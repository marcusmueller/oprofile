/* $Id: oprofiled.c,v 1.32 2001/06/22 03:16:24 movement Exp $ */
/* COPYRIGHT (C) 2000 THE VICTORIA UNIVERSITY OF MANCHESTER and John Levon
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59 Temple
 * Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include "oprofiled.h"

extern struct opd_footer footer;

static int showvers;
int verbose;

int kernel_only;
/* Unfortunately popt does not have, on many versions, the POPT_ARG_DOUBLE type
 * so I must first store it as a string. */
static const char *cpu_speed_str;
static double cpu_speed;
static int cpu_type;
static int ignore_myself;
static int opd_buf_size=OP_DEFAULT_BUF_SIZE;
static char *opd_dir="/var/opd/";
static char *logfilename="oprofiled.log";
char *smpdir="/var/opd/samples/";
static char *devfilename="opdev";
static char *devhashmapfilename="ophashmapdev";
char *vmlinux;
static char *systemmapfilename;
static pid_t mypid;
static sigset_t maskset;
static fd_t devfd;
struct op_hash *hashmap;

static void opd_sighup(int val);
static void opd_open_logfile(void);

unsigned long opd_stats[OPD_MAX_STATS] = { 0, };

static struct poptOption options[] = {
	{ "buffer-size", 'b', POPT_ARG_INT, &opd_buf_size, 0, "nr. of entries in kernel buffer", "num", },
	{ "use-cpu", 'p', POPT_ARG_INT, &cpu_type, 0, "0 for PPro, 1 for PII, 2 for PIII", "[0|1|2]" },
	{ "ignore-myself", 'm', POPT_ARG_INT, &ignore_myself, 0, "ignore samples of oprofile driver", "[0|1]"},
	{ "log-file", 'l', POPT_ARG_STRING, &logfilename, 0, "log file", "file", },
	{ "base-dir", 'd', POPT_ARG_STRING, &opd_dir, 0, "base directory of daemon", "dir", },
	{ "samples-dir", 's', POPT_ARG_STRING, &smpdir, 0, "output samples dir", "file", },
	{ "device-file", 'd', POPT_ARG_STRING, &devfilename, 0, "profile device file", "file", },
	{ "hash-map-device-file", 'h', POPT_ARG_STRING, &devhashmapfilename, 0, "profile hashmap device file", "file", },
	{ "map-file", 'f', POPT_ARG_STRING, &systemmapfilename, 0, "System.map for running kernel file", "file", },
	{ "vmlinux", 'k', POPT_ARG_STRING, &vmlinux, 0, "vmlinux kernel image", "file", },
	{ "kernel-only", 'o', POPT_ARG_INT, &kernel_only, 0, "profile only kernel", "file", },
	{ "cpu-speed", 0, POPT_ARG_STRING, &cpu_speed_str, 0, "cpu speed (MHz)", "cpu_mhz", },
	{ "version", 'v', POPT_ARG_NONE, &showvers, 0, "show version", NULL, },
	{ "verbose", 'V', POPT_ARG_NONE, &verbose, 0, "be verbose in log file", NULL, },
	POPT_AUTOHELP
	{ NULL, 0, 0, NULL, 0, NULL, NULL, },
};

/**
 * opd_open_logfile - open the log file
 *
 * Open the logfile on stdout and stderr. This function
 * assumes that 1 and 2 are the lowest closed()d file
 * descriptors. Failure to open on either descriptor is
 * a fatal error.
 */
static void opd_open_logfile(void)
{
	if (open(logfilename, O_WRONLY|O_CREAT|O_NOCTTY|O_APPEND, 0755) == -1) {
		perror("oprofiled: couldn't re-open stdout: ");
		exit(1);
	}

	if (dup2(1,2) == -1) {
		perror("oprofiled: couldn't dup stdout to stderr: ");
		exit(1);
	}

}

/**
 * opd_open_files - open necessary files
 *
 * Open the device files and the log file,
 * and mmap() the hash map. Also read the System.map
 * file.
 */
static void opd_open_files(void)
{
	fd_t hashmapdevfd;

	hashmapdevfd = opd_open_device(devhashmapfilename, 0);
	if (hashmapdevfd == -1) {
		perror("Failed to open hash map device: ");
		exit(1);
	}
 
	devfd = opd_open_device(devfilename, 0);
	if (devfd == -1) {
		if (errno == EINVAL)
			fprintf(stderr, "Failed to open device. Possibly you have passed incorrect\n"
				"parameters. Check /var/log/messages.");
		else
			perror("Failed to open hash map device: ");
		exit(1);
	} 
 
	hashmap = mmap(0, OP_HASH_MAP_SIZE, PROT_READ, MAP_SHARED, hashmapdevfd, 0);
	if ((long)hashmap == -1) {
		perror("oprofiled: couldn't mmap hash map: ");
		exit(1);
	}

	/* give output before re-opening stdout as the logfile */
	printf("Using log file \"%s\"\n", logfilename);
 
	/* set up logfile */
	close(0);
	close(1);

	if (open("/dev/null",O_RDONLY) == -1) {
		perror("oprofiled: couldn't re-open stdin as /dev/null: ");
		exit(1);
	}

	opd_open_logfile();

	opd_read_system_map(systemmapfilename);
	printf("oprofiled started %s", opd_get_time());
	fflush(stdout);
}

/**
 * opd_options - parse command line options
 * @argc: argc
 * @argv: argv array
 *
 * Parse all command line arguments, and sanity
 * check what the user passed. Incorrect arguments
 * are a fatal error.
 */
static void opd_options(int argc, char const *argv[])
{
	poptContext optcon;
	int ret;
	char c;

	optcon = poptGetContext(NULL, argc, argv, options, 0);

	c=poptGetNextOpt(optcon);

	if (c<-1) {
		fprintf(stderr, "oprofiled: %s: %s\n",
			poptBadOption(optcon, POPT_BADOPTION_NOALIAS),
			poptStrerror(c));
		poptPrintHelp(optcon, stderr, 0);
		exit(1);
	}

	if (showvers) {
		printf(VERSION_STRING " compiled on " __DATE__ " " __TIME__ "\n");
		exit(0);
	}

	if (!vmlinux || streq("", vmlinux)) {
		fprintf(stderr, "oprofiled: no vmlinux specified.\n");
		poptPrintHelp(optcon, stderr, 0);
		exit(1);
	}

	if (!systemmapfilename || streq("", systemmapfilename)) {
		fprintf(stderr, "oprofiled: no System.map specified.\n");
		poptPrintHelp(optcon, stderr, 0);
		exit(1);
	}

	ret = op_check_events(footer.ctr0_type_val, footer.ctr1_type_val, footer.ctr0_um, footer.ctr1_um, cpu_type);

        if (ret & OP_CTR0_NOT_FOUND) fprintf(stderr, "oprofiled: ctr0: %d: no such event\n",footer. ctr0_type_val);
        if (ret & OP_CTR1_NOT_FOUND) fprintf(stderr, "oprofiled: ctr1: %d: no such event\n", footer.ctr1_type_val);
        if (ret & OP_CTR0_NO_UM) fprintf(stderr, "oprofiled: ctr0: 0x%.2x: invalid unit mask\n", footer.ctr0_um);
        if (ret & OP_CTR1_NO_UM) fprintf(stderr, "oprofiled: ctr1: 0x%.2x: invalid unit mask\n", footer.ctr1_um);
        if (ret & OP_CTR0_NOT_ALLOWED) fprintf(stderr, "oprofiled: ctr0: %d: can't count event\n", footer.ctr0_type_val);
        if (ret & OP_CTR1_NOT_ALLOWED) fprintf(stderr, "oprofiled: ctr1: %d: can't count event\n", footer.ctr1_type_val);
        if (ret & OP_CTR0_PII_EVENT) fprintf(stderr, "oprofiled: ctr0: %d: event only available on PII\n", footer.ctr0_type_val);
        if (ret & OP_CTR1_PII_EVENT) fprintf(stderr, "oprofiled: ctr1: %d: event only available on PII\n", footer.ctr1_type_val);
        if (ret & OP_CTR0_PIII_EVENT) fprintf(stderr, "oprofiled: ctr0: %d: event only available on PIII\n", footer.ctr0_type_val);
        if (ret & OP_CTR1_PIII_EVENT) fprintf(stderr, "oprofiled: ctr1: %d: event only available on PIII\n", footer.ctr1_type_val);

	if (ret != OP_EVENTS_OK) {
		poptPrintHelp(optcon, stderr, 0);
		exit(1);
	}

	if (cpu_speed_str && strlen(cpu_speed_str)) {
		sscanf(cpu_speed_str, "%lf", &cpu_speed);
	}
}

/**
 * opd_fork - fork and return as child
 *
 * fork() and exit the parent with _exit().
 * Failure is fatal.
 */
static void opd_fork(void)
{
	switch (fork()) {
		case -1:
			perror("oprofiled: fork() failed: ");
			exit(1);
			break;
		case 0:
			break;
		default:
			/* parent */
			_exit(0);
			break;
	}
}

/**
 * opd_go_daemon - become daemon process
 *
 * Become an un-attached daemon in the standard
 * way (fork(),chdir(),setsid(),fork()). Sets
 * the global variable mypid to the pid of the second
 * child. Parents perform _exit().
 *
 * Any failure is fatal.
 */
static void opd_go_daemon(void)
{
	opd_fork();

	if (chdir(opd_dir)) {
		fprintf(stderr,"oprofiled: opd_go_daemon: couldn't chdir to %s: %s", opd_dir, strerror(errno));
		exit(1);
	}

	if (setsid() < 0) {
		perror("oprofiled: opd_go_daemon: couldn't setsid: ");
		exit(1);
	}

	opd_fork();
	mypid = getpid();
}

void opd_do_samples(const struct op_sample *opd_buf, size_t count);

/**
 * opd_do_read - enter processing loop
 * @buf: buffer to read into
 * @size: size of buffer
 *
 * Read some of a buffer from the device and process
 * the contents.
 *
 * Never returns.
 */
static void opd_do_read(struct op_sample *buf, size_t size)
{
	size_t count;
 
	while (1) {
		count = opd_read_device(devfd, buf, size, TRUE);
		opd_do_samples(buf, count);
	}
}

/**
 * opd_is_mapping - is the entry a notification
 * @sample: sample to use
 *
 * Returns positive if the sample is actually a notification,
 * zero otherwise.
 */
inline static u16 opd_is_notification(const struct op_sample *sample)
{
	return (sample->count & OP_NOTE);
}

/**
 *  opd_unpack_mapping - unpack a mapping notification
 *  @mapping: map structure to fill in
 *  @samples: two contiguous samples containing the packed data
 *
 * Unpacks two samples into the map structure for further processing.
 */ 
static void opd_unpack_mapping(struct op_mapping *map, const struct op_sample *samples)
{
	map->addr = samples[0].eip;
	map->pid = samples[0].pid;
	map->hash = samples[0].count & ~OP_EXEC;
	map->len = samples[1].eip;
	map->offset = samples[1].pid | (((u32)samples[1].count) << 16);
}
 
/**
 * opd_do_samples - process a sample buffer
 * @opd_buf: buffer to process
 * @count: number of bytes in buffer 
 *
 * Process a buffer of samples.
 * The signals specified by the global variable maskset are
 * masked. Samples for oprofiled are ignored if the global
 * variable ignore_myself is set.
 *
 * If the sample could be processed correctly, it is written
 * to the relevant sample file. Additionally mapping and
 * process notifications are handled here.
 */
void opd_do_samples(const struct op_sample *opd_buf, size_t count)
{
	uint i;
	struct op_mapping mapping; 

	/* prevent signals from messing us up */
	sigprocmask(SIG_BLOCK, &maskset, NULL);

	opd_stats[OPD_DUMP_COUNT]++;

	for (i=0; i < count/sizeof(struct op_sample); i++) {
		verbprintf("%.6u: EIP: 0x%.8x pid: %.6d count: %.6d\n", i, opd_buf[i].eip, opd_buf[i].pid, opd_buf[i].count);

		if (ignore_myself && opd_buf[i].pid == mypid)
			continue;

		if (opd_is_notification(&opd_buf[i])) {
			opd_stats[OPD_NOTIFICATIONS]++;
			 
			/* is a mapping type notification ? */
			if (IS_OP_MAP(opd_buf[i].count)) {
				if (IS_OP_EXEC(opd_buf[i].count))
					opd_handle_exec(opd_buf[i].pid);

				if (i + 2 > count / sizeof(struct op_sample)) {
					verbprintf("Partial mapping ignored.\n");
					i = count / sizeof(struct op_sample);
					break;
				}

				opd_unpack_mapping(&mapping, &opd_buf[i]);
				opd_handle_mapping(&mapping);
				i++;
			} else switch (opd_buf[i].count) {
				case OP_FORK:
					opd_handle_fork(&opd_buf[i]);
					break;

				case OP_DROP_MODULES:
					opd_clear_module_info();
					break;

				case OP_EXIT:
					opd_handle_exit(&opd_buf[i]);
					break;

				default:
					fprintf(stderr, "Received unknown notification type %u\n",opd_buf[i].count);
					exit(1);
					break;
			}
		} else
			opd_put_sample(&opd_buf[i]);
	}

	sigprocmask(SIG_UNBLOCK, &maskset, NULL);
}

/* re-open logfile for logrotate */
static void opd_sighup(int val __attribute__((unused)))
{
	close(1);
	close(2);
	opd_open_logfile();
}

int main(int argc, char const *argv[])
{
	struct op_sample *opd_buf;
	size_t opd_buf_bytesize;
	struct sigaction act;
	int i;

	footer.ctr0_type_val = opd_read_int_from_file("/proc/sys/dev/oprofile/0/0/event");
	footer.ctr0_um = (u8) opd_read_int_from_file("/proc/sys/dev/oprofile/0/0/unit_mask");
	footer.ctr1_type_val = opd_read_int_from_file("/proc/sys/dev/oprofile/0/1/event");
	footer.ctr1_um = (u8) opd_read_int_from_file("/proc/sys/dev/oprofile/0/1/unit_mask");
	
	footer.cpu_speed = cpu_speed;

	footer.ctr0_count = opd_read_int_from_file("/proc/sys/dev/oprofile/0/0/count");
	footer.ctr1_count = opd_read_int_from_file("/proc/sys/dev/oprofile/0/1/count");

	opd_options(argc, argv);

	/* one extra for the "how many" header */
	opd_buf_bytesize = (opd_buf_size + 1) * sizeof(struct op_sample);

 	opd_buf = opd_malloc(opd_buf_bytesize);

	opd_init_images();

	opd_go_daemon();

	opd_open_files();

	/* yes, this is racey. */
	opd_get_ascii_procs();

	for (i=0; i< OPD_MAX_STATS; i++) {
		opd_stats[i] = 0;
	}

	act.sa_handler = opd_alarm;
	act.sa_flags = 0;
	sigemptyset(&act.sa_mask);

	if (sigaction(SIGALRM, &act, NULL)) {
		perror("oprofiled: install of SIGALRM handler failed: ");
		exit(1);
	}

	act.sa_handler = opd_sighup;
	act.sa_flags = 0;
	sigemptyset(&act.sa_mask);
	sigaddset(&act.sa_mask, SIGALRM);

	if (sigaction(SIGHUP, &act, NULL)) {
		perror("oprofiled: install of SIGHUP handler failed: ");
		exit(1);
	}

	sigemptyset(&maskset);
	sigaddset(&maskset, SIGALRM);
	sigaddset(&maskset, SIGHUP);

	/* clean up every 10 minutes */
	alarm(60*10);

	/* simple sleep-then-process loop */
	opd_do_read(opd_buf, opd_buf_bytesize);

	return 0;
}
