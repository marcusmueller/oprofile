/**
 * @file oprofiled.c
 * Daemon set up and main loop
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author John Levon <moz@compsoc.man.ac.uk>
 * @author Philippe Elie <phil_el@wanadoo.fr>
 */

#include "opd_proc.h"
#include "opd_mapping.h"
#include "opd_stats.h"
#include "opd_sample_files.h"
#include "opd_image.h"
#include "opd_parse_proc.h"
#include "opd_kernel.h"
#include "opd_printf.h"

#include "version.h"
#include "op_popt.h"
#include "op_file.h"
#include "op_fileio.h"
#include "op_deviceio.h"
#include "op_lockfile.h"
#include "op_get_time.h"
#include "op_sample_file.h"
#include "op_events.h"
#include "op_libiberty.h"
#include "op_interface.h"
#include "op_hw_config.h"

#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>

// GNU libc bug
pid_t getpgid(pid_t pid);

u32 ctr_count[OP_MAX_COUNTERS];
u8 ctr_event[OP_MAX_COUNTERS];
u8 ctr_um[OP_MAX_COUNTERS];
double cpu_speed;
fd_t hashmapdevfd;

uint op_nr_counters = 2;
int verbose;
op_cpu cpu_type;
int separate_samples;
char * vmlinux;
int kernel_only;
unsigned long opd_stats[OPD_MAX_STATS] = { 0, };

static char * kernel_range;
static int showvers;
static u32 ctr_enabled[OP_MAX_COUNTERS];
/* Unfortunately popt does not have, on many versions, the POPT_ARG_DOUBLE type
 * so I must first store it as a string. */
static char const * cpu_speed_str;
static int opd_buf_size=OP_DEFAULT_BUF_SIZE;
static int opd_note_buf_size=OP_DEFAULT_NOTE_SIZE;
static pid_t mypid;
static pid_t pid_filter;
static pid_t pgrp_filter;
static sigset_t maskset;
static fd_t devfd;
static fd_t notedevfd;

static void opd_sighup(int val);
static void opd_open_logfile(void);

static struct poptOption options[] = {
	{ "pid-filter", 0, POPT_ARG_INT, &pid_filter, 0, "only profile the given process ID", "pid" },
	{ "pgrp-filter", 0, POPT_ARG_INT, &pgrp_filter, 0, "only profile the given process group", "pgrp" },
	{ "kernel-range", 'r', POPT_ARG_STRING, &kernel_range, 0, "Kernel VMA range", "start-end", },
	{ "vmlinux", 'k', POPT_ARG_STRING, &vmlinux, 0, "vmlinux kernel image", "file", },
	{ "cpu-speed", 0, POPT_ARG_STRING, &cpu_speed_str, 0, "cpu speed (MHz)", "cpu_mhz", },
	{ "separate-samples", 0, POPT_ARG_INT, &separate_samples, 0, "separate samples for each distinct application", "[0|1]", },
	{ "version", 'v', POPT_ARG_NONE, &showvers, 0, "show version", NULL, },
	{ "verbose", 'V', POPT_ARG_NONE, &verbose, 0, "be verbose in log file", NULL, },
	POPT_AUTOHELP
	{ NULL, 0, 0, NULL, 0, NULL, NULL, },
};

/**
 * opd_open_logfile - open the log file
 *
 * Open the logfile on stdout and stderr. This function
 * assumes that 1 and 2 are the lowest close()d file
 * descriptors. Failure to open on either descriptor is
 * a fatal error.
 */
static void opd_open_logfile(void)
{
	if (open(OP_LOG_FILE, O_WRONLY|O_CREAT|O_NOCTTY|O_APPEND, 0755) == -1) {
		perror("oprofiled: couldn't re-open stdout: ");
		exit(EXIT_FAILURE);
	}

	if (dup2(1,2) == -1) {
		perror("oprofiled: couldn't dup stdout to stderr: ");
		exit(EXIT_FAILURE);
	}
}

/**
 * op_open_files - open necessary files
 *
 * Open the device files and the log file,
 * and mmap() the hash map.
 */
static void op_open_files(void)
{
	hashmapdevfd = op_open_device(OP_HASH_DEVICE, 0);
	if (hashmapdevfd == -1) {
		perror("Failed to open hash map device");
		exit(EXIT_FAILURE);
	}

	notedevfd = op_open_device(OP_NOTE_DEVICE, 0);
	if (notedevfd == -1) {
		if (errno == EINVAL)
			fprintf(stderr, "Failed to open note device. Possibly you have passed incorrect\n"
				"parameters. Check /var/log/messages.");
		else
			perror("Failed to open note device");
		exit(EXIT_FAILURE);
	}

	devfd = op_open_device(OP_DEVICE, 0);
	if (devfd == -1) {
		if (errno == EINVAL)
			fprintf(stderr, "Failed to open device. Possibly you have passed incorrect\n"
				"parameters. Check /var/log/messages.");
		else
			perror("Failed to open profile device");
		exit(EXIT_FAILURE);
	}

	opd_init_hash_map();

	/* give output before re-opening stdout as the logfile */
	printf("Using log file " OP_LOG_FILE "\n");

	/* set up logfile */
	close(0);
	close(1);

	if (open("/dev/null", O_RDONLY) == -1) {
		perror("oprofiled: couldn't re-open stdin as /dev/null: ");
		exit(EXIT_FAILURE);
	}

	opd_open_logfile();

	printf("oprofiled started %s", op_get_time());
	fflush(stdout);
}

/**
 * opd_backup_samples_files - back up all the samples file
 *
 * move all files in samples dir to sub-directory
 * session-#nr/
 */
static void opd_backup_samples_files(void)
{
	char * dir_name;
	int gen = 0;
	struct stat stat_buf;
	DIR * dir;
	struct dirent * dirent;

	dir_name = xmalloc(strlen(OP_SAMPLES_DIR) + strlen("session-") + 10);
	strcpy(dir_name, OP_SAMPLES_DIR);

	do {
		sprintf(dir_name + strlen(OP_SAMPLES_DIR), "/session-%d", ++gen);
	} while (stat(dir_name, &stat_buf) == 0);

	if (mkdir(dir_name, 0755)) {
		/* That's a severe problem: if we continue we can overwrite
		 * samples files and produce wrong result. FIXME */
		printf("unable to create directory %s\n", dir_name);
		exit(EXIT_FAILURE);
	}

	if (!(dir = opendir(OP_SAMPLES_DIR))) {
		printf("unable to open directory " OP_SAMPLES_DIR "\n");
		exit(EXIT_FAILURE);
	}

	printf("Backing up samples file to directory %s\n", dir_name);

	while ((dirent = readdir(dir)) != 0) {
		if (op_move_regular_file(dir_name, OP_SAMPLES_DIR, dirent->d_name) < 0) {
			printf("unable to backup %s/%s to directory %s\n",
			       OP_SAMPLES_DIR, dirent->d_name, dir_name);
		}
	}

	closedir(dir);

	free(dir_name);
}

/**
 * opd_need_backup_samples_files - test if we need to
 * backup samples files
 *
 * We can't backup lazily samples files else it can
 * leads to detect that backup is needed after some
 * samples has been written (e.g. ctr 1 have the same
 * setting from the previous runs, ctr 0 have different
 * setting and the first samples output come from ctr1)
 *
 */
static int opd_need_backup_samples_files(void)
{
	DIR * dir;
	struct dirent * dirent;
	struct stat stat_buf;
	int need_backup;
	/* bitmaps: bit i is on if counter i is enabled */
	int counter_set, old_counter_set;
	uint i;

	if (!(dir = opendir(OP_SAMPLES_DIR))) {
		printf("unable to open directory " OP_SAMPLES_DIR "\n");
		exit(EXIT_FAILURE);
	}

	counter_set = old_counter_set = 0;
	need_backup = 0;

	while ((dirent = readdir(dir)) != 0 && need_backup == 0) {
		char * file = xmalloc(strlen(OP_SAMPLES_DIR) + strlen(dirent->d_name) + 2);
		strcpy(file, OP_SAMPLES_DIR);
		strcat(file, dirent->d_name);
		if (!stat(file, &stat_buf) && S_ISREG(stat_buf.st_mode)) {
			struct opd_header header;
			FILE * fp = fopen(file, "r");
			if (!fp)
				continue;

			if (fread(&header, sizeof( header), 1, fp) != 1)
				goto close;

			if (memcmp(&header.magic, OPD_MAGIC, sizeof(header.magic)) || header.version != OPD_VERSION)
				goto close;

			if (header.ctr_event != ctr_event[header.ctr] ||
			    header.ctr_um != ctr_um[header.ctr] ||
			    header.ctr_count != ctr_count[header.ctr] ||
			    header.cpu_type != (u32)cpu_type ||
			    header.separate_samples != separate_samples) {
				need_backup = 1;
			}

			old_counter_set |= 1 << header.ctr;

		close:
			fclose(fp);
		}

		free(file);
	}

	for (i = 0 ; i < op_nr_counters; ++i) {
		if (ctr_enabled[i])
			counter_set |= 1 << i;
	}

	/* old_counter_set == 0 means there is no samples file in the sample
	 * dir, so avoid to try to backup else we get an empty backup dir */
	if (old_counter_set && old_counter_set != counter_set)
		need_backup = 1;

	closedir(dir);

	return need_backup;
}

/**
 * opd_pmc_options - read sysctls for pmc options
 */
static void opd_pmc_options(void)
{
	int ret;
	uint i;
	/* should be sufficient to hold /proc/sys/dev/oprofile/%d/yyyy */
	char filename[PATH_MAX + 1];

	for (i = 0 ; i < op_nr_counters ; ++i) {
		sprintf(filename, "/proc/sys/dev/oprofile/%d/event", i);
		ctr_event[i]= op_read_int_from_file(filename);

		sprintf(filename, "/proc/sys/dev/oprofile/%d/count", i);
		ctr_count[i]= op_read_int_from_file(filename);

		sprintf(filename, "/proc/sys/dev/oprofile/%d/unit_mask", i);
		ctr_um[i]= op_read_int_from_file(filename);

		sprintf(filename, "/proc/sys/dev/oprofile/%d/enabled", i);
		ctr_enabled[i]= op_read_int_from_file(filename);

		if (!ctr_enabled[i])
			continue;

		ret = op_check_events(i, ctr_event[i], ctr_um[i], cpu_type);

		if (ret & OP_INVALID_EVENT)
			fprintf(stderr, "oprofiled: ctr%d: %d: no such event for cpu %s\n",
				i, ctr_event[i], op_get_cpu_type_str(cpu_type));

		if (ret & OP_INVALID_UM)
			fprintf(stderr, "oprofiled: ctr%d: 0x%.2x: invalid unit mask for cpu %s\n",
				i, ctr_um[i], op_get_cpu_type_str(cpu_type));

		if (ret & OP_INVALID_COUNTER)
			fprintf(stderr, "oprofiled: ctr%d: %d: can't count event for this counter\n",
				i, ctr_count[i]);

		if (ret != OP_OK_EVENT)
			exit(EXIT_FAILURE);
	}
}

/**
 * opd_rtc_options - read sysctls + set up for rtc options
 */
static void opd_rtc_options(void)
{
	uint i;

	for (i = 0 ; i < op_nr_counters ; ++i) {
		ctr_event[i] = ctr_count[i] =  ctr_um[i] = ctr_enabled[i] = 0;
	}

	ctr_count[0] = op_read_int_from_file("/proc/sys/dev/oprofile/rtc_value");
	/* FIXME ugly ... */
	ctr_event[0] = 0xff;
}

/**
 * opd_options - parse command line options
 * @param argc  argc
 * @param argv  argv array
 *
 * Parse all command line arguments, and sanity
 * check what the user passed. Incorrect arguments
 * are a fatal error.
 */
static void opd_options(int argc, char const * argv[])
{
	poptContext optcon;

	optcon = op_poptGetContext(NULL, argc, argv, options, 0);

	if (showvers) {
		show_version(argv[0]);
	}

	cpu_type = op_get_cpu_type();
	if (cpu_type == CPU_ATHLON)
		op_nr_counters = 4;

	if (!vmlinux || !strcmp("", vmlinux)) {
		fprintf(stderr, "oprofiled: no vmlinux specified.\n");
		poptPrintHelp(optcon, stderr, 0);
		exit(EXIT_FAILURE);
	}

	if (!kernel_range || !strcmp("", kernel_range)) {
		fprintf(stderr, "oprofiled: no kernel VMA range specified.\n");
		poptPrintHelp(optcon, stderr, 0);
		exit(EXIT_FAILURE);
	}

	opd_buf_size = op_read_int_from_file("/proc/sys/dev/oprofile/bufsize");
	opd_note_buf_size = op_read_int_from_file("/proc/sys/dev/oprofile/notesize");
	kernel_only = op_read_int_from_file("/proc/sys/dev/oprofile/kernel_only");

	if (cpu_type != CPU_RTC) {
		opd_pmc_options();
	} else {
		opd_rtc_options();
	}

	if (cpu_speed_str && strlen(cpu_speed_str))
		sscanf(cpu_speed_str, "%lf", &cpu_speed);

	opd_parse_kernel_range(kernel_range);
	poptFreeContext(optcon);
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
			exit(EXIT_FAILURE);
			break;
		case 0:
			break;
		default:
			/* parent */
			_exit(EXIT_SUCCESS);
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

	if (chdir(OP_BASE_DIR)) {
		fprintf(stderr,"oprofiled: opd_go_daemon: couldn't chdir to "
			OP_BASE_DIR ": %s", strerror(errno));
		exit(EXIT_FAILURE);
	}

	if (setsid() < 0) {
		perror("oprofiled: opd_go_daemon: couldn't setsid: ");
		exit(EXIT_FAILURE);
	}

	opd_fork();
	mypid = getpid();
}

void opd_do_samples(struct op_buffer_head const * buf);
void opd_do_notes(struct op_note const * opd_buf, size_t count);

/**
 * do_shutdown - shutdown cleanly, reading as much remaining data as possible.
 * @param buf  sample buffer area
 * @param size  size of sample buffer
 * @param nbuf  note buffer area
 * @param nsize  size of note buffer
 */
static void opd_shutdown(struct op_buffer_head * buf, size_t size, struct op_note * nbuf, size_t nsize)
{
	ssize_t count = -1;
	ssize_t ncount = -1;

	/* the dump may have added no samples, so we must set
	 * non-blocking */
	if (fcntl(devfd, F_SETFL, fcntl(devfd, F_GETFL) | O_NONBLOCK) < 0) {
		perror("Failed to set non-blocking read for device: ");
		exit(EXIT_FAILURE);
	}

	/* it's always OK to read the note device */
	while (ncount < 0) {
		ncount = op_read_device(notedevfd, nbuf, nsize);
	}

	if (ncount > 0) {
		opd_do_notes(nbuf, ncount);
	}

	/* read as much as we can until we have exhausted the data
	 * (EAGAIN is returned).
	 *
	 * This will not livelock as the profiler has been partially
	 * shut down by now.
	 */
	while (1) {
		count = op_read_device(devfd, buf, size);
		if (count < 0 && errno == EAGAIN)
			break;
		verbprintf("Shutting down, state %d\n", buf->state);
		opd_do_samples(buf);
	}
}

/**
 * opd_do_read - enter processing loop
 * @param buf  buffer to read into
 * @param size  size of buffer
 * @param nbuf  note buffer
 * @param nsize  size of note buffer
 *
 * Read some of a buffer from the device and process
 * the contents.
 *
 * Never returns.
 */
static void opd_do_read(struct op_buffer_head * buf, size_t size, struct op_note * nbuf, size_t nsize)
{
	while (1) {
		ssize_t count = -1;
		ssize_t ncount = -1;

		/* loop to handle EINTR */
		while (count < 0) {
			count = op_read_device(devfd, buf, size);

		}

		while (ncount < 0) {
			ncount = op_read_device(notedevfd, nbuf, nsize);
		}

		opd_do_notes(nbuf, ncount);
		opd_do_samples(buf);
 
		/* request to stop arrived */
		if (buf->state == STOPPING) {
			verbprintf("Shutting down by request.\n");
			opd_shutdown(buf, size, nbuf, nsize);
			return;
		}
	}
}

/**
 * opd_do_notes - process a notes buffer
 * @param opd_buf  buffer to process
 * @param count  number of bytes in buffer
 *
 * Process a buffer of notes.
 */
void opd_do_notes(struct op_note const * opd_buf, size_t count)
{
	uint i;
	struct op_note const * note;

	/* prevent signals from messing us up */
	sigprocmask(SIG_BLOCK, &maskset, NULL);

	for (i = 0; i < count/sizeof(struct op_note); i++) {
		note = &opd_buf[i];

		opd_stats[OPD_NOTIFICATIONS]++;

		switch (note->type) {
			case OP_MAP:
			case OP_EXEC:
				if (note->type == OP_EXEC)
					opd_handle_exec(note->pid);
				opd_handle_mapping(note);
				break;

			case OP_FORK:
				opd_handle_fork(note);
				break;

			case OP_DROP_MODULES:
				opd_clear_module_info();
				break;

			case OP_EXIT:
				opd_handle_exit(note);
				break;

			default:
				fprintf(stderr, "Received unknown notification type %u\n", note->type);
				exit(EXIT_FAILURE);
				break;
		}
	}
	sigprocmask(SIG_UNBLOCK, &maskset, NULL);
}

/**
 * opd_do_samples - process a sample buffer
 * @param opd_buf  buffer to process
 *
 * Process a buffer of samples.
 * The signals specified by the global variable maskset are
 * masked.
 *
 * If the sample could be processed correctly, it is written
 * to the relevant sample file. Additionally mapping and
 * process notifications are handled here.
 */
void opd_do_samples(struct op_buffer_head const * opd_buf)
{
	uint i;
	struct op_sample const * buffer = opd_buf->buffer; 

	/* prevent signals from messing us up */
	sigprocmask(SIG_BLOCK, &maskset, NULL);

	opd_stats[OPD_DUMP_COUNT]++;

	for (i = 0; i < opd_buf->count; i++) {
		verbprintf("%.6u: EIP: 0x%.8x pid: %.6d count: %.6d\n",
			i, buffer[i].eip, buffer[i].pid, buffer[i].count);

		/* happens during initial startup whilst the
		 * hash table is being filled
		 */
		if (buffer[i].eip == 0)
			continue;

		if (pid_filter && pid_filter != buffer[i].pid)
			continue;
		if (pgrp_filter && pgrp_filter != getpgid(buffer[i].pid))
			continue;

		opd_put_sample(&buffer[i]);
	}

	sigprocmask(SIG_UNBLOCK, &maskset, NULL);
}

/**
 * opd_alarm - clean up old procs, msync, and report stats
 */
static void opd_alarm(int val __attribute__((unused)))
{
	opd_for_each_image(opd_sync_image_samples_files);

	opd_age_procs();

	opd_print_stats();

	alarm(60*10);
}

/* re-open logfile for logrotate */
static void opd_sighup(int val __attribute__((unused)))
{
	close(1);
	close(2);
	opd_open_logfile();
	/* We just close them, and re-open them lazily as usual. */
	opd_for_each_image(opd_close_image_samples_files);
}


static void clean_exit(void)
{
	unlink(OP_LOCK_FILE);
}


static void opd_sigterm(int val __attribute__((unused)))
{
	clean_exit();
	exit(EXIT_FAILURE);
}

static void setup_signals(void)
{
	struct sigaction act;
 
	act.sa_handler = opd_alarm;
	act.sa_flags = 0;
	sigemptyset(&act.sa_mask);

	if (sigaction(SIGALRM, &act, NULL)) {
		perror("oprofiled: install of SIGALRM handler failed: ");
		exit(EXIT_FAILURE);
	}

	act.sa_handler = opd_sighup;
	act.sa_flags = 0;
	sigemptyset(&act.sa_mask);
	sigaddset(&act.sa_mask, SIGALRM);

	if (sigaction(SIGHUP, &act, NULL)) {
		perror("oprofiled: install of SIGHUP handler failed: ");
		exit(EXIT_FAILURE);
	}

	act.sa_handler = opd_sigterm;
	act.sa_flags = 0;
	sigemptyset(&act.sa_mask);
	sigaddset(&act.sa_mask, SIGTERM);

	if (sigaction(SIGTERM, &act, NULL)) {
		perror("oprofiled: install of SIGTERM handler failed: ");
		exit(EXIT_FAILURE);
	}

	sigemptyset(&maskset);
	sigaddset(&maskset, SIGALRM);
	sigaddset(&maskset, SIGHUP);

	/* clean up every 10 minutes */
	alarm(60*10);
}


int main(int argc, char const * argv[])
{
	struct op_buffer_head * sbuf;
	size_t s_buf_bytesize;
	struct op_note * nbuf;
	size_t n_buf_bytesize;
	int i;

	opd_options(argc, argv);

	s_buf_bytesize = sizeof(struct op_buffer_head) + opd_buf_size * sizeof(struct op_sample);

 	sbuf = xmalloc(s_buf_bytesize);

	n_buf_bytesize = opd_note_buf_size * sizeof(struct op_note);
	nbuf = xmalloc(n_buf_bytesize);

	opd_init_kernel_image();

	if (atexit(clean_exit)) {
		fprintf(stderr, "Couldn't set exit cleanup !\n");
		unlink(OP_LOCK_FILE);
		exit(EXIT_FAILURE);
	}

	opd_go_daemon();

	if (opd_need_backup_samples_files()) {
		opd_backup_samples_files();
	}

	op_open_files();

	/* yes, this is racey. */
	opd_get_ascii_procs();

	for (i=0; i< OPD_MAX_STATS; i++) {
		opd_stats[i] = 0;
	}

	setup_signals();
 
	if (op_write_lock_file(OP_LOCK_FILE)) {
		fprintf(stderr,
			"oprofiled: could not create lock file "
			OP_LOCK_FILE "\n");
		exit(EXIT_FAILURE);
	}

	/* simple sleep-then-process loop */
	opd_do_read(sbuf, s_buf_bytesize, nbuf, n_buf_bytesize);

	opd_print_stats();
	printf("oprofiled stopped %s", op_get_time());

	free(sbuf);
	free(nbuf);
	opd_proc_cleanup();
	opd_image_cleanup();

	return 0;
}
