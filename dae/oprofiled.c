/* $Id: oprofiled.c,v 1.11 2000/08/26 22:19:27 moz Exp $ */

#include "oprofiled.h"

extern struct opd_footer footer;

static int showvers;

static u8 ctr0_type_val;
static u8 ctr1_type_val;
/* unit masks used for the samples file output */
static int ctr0_um;
static int ctr1_um;
static int cpu_type;
static int ignore_myself;
static char *ctr0_type="INST_RETIRED";
static char *ctr1_type="";
static int opd_buf_size=OPD_DEFAULT_BUF_SIZE;
static char *opd_dir="/var/opd/";
static char *logfilename="oprofiled.log";
char *smpdir="/var/opd/samples/";
static char *devfilename="opdev";
static char *devmapfilename="opmapdev";
static char *devhashmapfilename="ophashmapdev";
char *vmlinux;
static char *systemmapfilename;
static pid_t mypid;
static sigset_t maskset;
static fd_t devfd;
fd_t mapdevfd;
char *hashmap;

static void opd_sighup(int val);
static void opd_open_logfile(void);

unsigned long opd_stats[OPD_MAX_STATS] = { 0, };

static struct poptOption options[] = {
	{ "buffer-size", 'b', POPT_ARG_INT, &opd_buf_size, 0, "nr. of entries in kernel buffer", "num", },
	{ "ctr0-unit-mask", 'u', POPT_ARG_INT, &ctr0_um, 0, "unit mask for ctr0", "val", },
	{ "ctr1-unit-mask", 't', POPT_ARG_INT, &ctr1_um, 0, "unit mask for ctr1", "val", },
	{ "ctr0-event", '0', POPT_ARG_STRING, &ctr0_type, 0, "symbolic event name for ctr0", "name", },
	{ "ctr1-event", '1', POPT_ARG_STRING, &ctr1_type, 0, "symbolic event name for ctr1", "name", },
	{ "use-cpu", 'p', POPT_ARG_INT, &cpu_type, 0, "0 for PPro, 1 for PII, 2 for PIII", "[0|1|2]" },
	{ "ignore-myself", 'm', POPT_ARG_INT, &ignore_myself, 0, "ignore samples of oprofile driver", "[0|1]"},
	{ "log-file", 'l', POPT_ARG_STRING, &logfilename, 0, "log file", "file", },
	{ "base-dir", 'd', POPT_ARG_STRING, &opd_dir, 0, "base directory of daemon", "dir", },
	{ "samples-dir", 's', POPT_ARG_STRING, &smpdir, 0, "output samples dir", "file", },
	{ "device-file", 'd', POPT_ARG_STRING, &devfilename, 0, "profile device file", "file", },
	{ "map-device-file", 'd', POPT_ARG_STRING, &devmapfilename, 0, "profile mapping device file", "file", },
	{ "hash-map-device-file", 'h', POPT_ARG_STRING, &devhashmapfilename, 0, "profile hashmap device file", "file", },
	{ "map-file", 'f', POPT_ARG_STRING, &systemmapfilename, 0, "System.map for running kernel file", "file", },
	{ "vmlinux", 'k', POPT_ARG_STRING, &vmlinux, 0, "vmlinux kernel image", "file", },
	{ "version", 'v', POPT_ARG_NONE, &showvers, 0, "show version", NULL, },
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
	if (open(logfilename,O_WRONLY|O_CREAT|O_NOCTTY|O_APPEND,0755)==-1) {
		perror("oprofiled: couldn't re-open stdout: ");
		exit(1);
	}

	if (dup2(1,2)==-1) {
		perror("oprofiled: couldn't dup stdout to stderr: ");
		exit(1);
	}

}

/**
 * opd_open_files - open necessary files
 *
 * Open the three device files and the log file,
 * and mmap() the hash map.
 */
static void opd_open_files(void)
{
	fd_t hashmapdevfd;

	devfd = opd_open_device(devfilename,1);
	mapdevfd = opd_open_device(devmapfilename,1);
	hashmapdevfd = opd_open_device(devhashmapfilename,1);

	hashmap = mmap(0, OP_HASH_MAP_SIZE, PROT_READ, MAP_SHARED, hashmapdevfd, 0);
	if ((long)hashmap==-1) {
		perror("oprofiled: couldn't mmap hash map: ");
		exit(1);
	}

	/* set up logfile */
	close(0);
	close(1);

	if (open("/dev/null",O_RDONLY)==-1) {
		perror("oprofiled: couldn't re-open stdin as /dev/null: ");
		exit(1);
	}

	opd_open_logfile();

	opd_read_system_map(systemmapfilename);
	printf("oprofiled started %s",opd_get_time());
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
static void opd_options(int argc, char *argv[])
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

	if (!vmlinux || streq("",vmlinux)) {
		fprintf(stderr, "oprofiled: no vmlinux specified.\n");
		poptPrintHelp(optcon, stderr, 0);
		exit(1);
	}

	if (!systemmapfilename || streq("",systemmapfilename)) {
		fprintf(stderr, "oprofiled: no System.map specified.\n");
		poptPrintHelp(optcon, stderr, 0);
		exit(1);
	}

	ret = op_check_events_str(ctr0_type, ctr1_type, (u8)ctr0_um, (u8)ctr1_um, cpu_type, &ctr0_type_val, &ctr1_type_val);

        if (ret&OP_CTR0_NOT_FOUND) fprintf(stderr, "oprofiled: ctr0: no such event\n");
        if (ret&OP_CTR1_NOT_FOUND) fprintf(stderr, "oprofiled: ctr1: no such event\n");
        if (ret&OP_CTR0_NO_UM) fprintf(stderr, "oprofiled: ctr0: invalid unit mask\n");
        if (ret&OP_CTR1_NO_UM) fprintf(stderr, "oprofiled: ctr1: invalid unit mask\n");
        if (ret&OP_CTR0_NOT_ALLOWED) fprintf(stderr, "oprofiled: ctr0: can't count event\n");
        if (ret&OP_CTR1_NOT_ALLOWED) fprintf(stderr, "oprofiled: ctr1: can't count event\n");
        if (ret&OP_CTR0_PII_EVENT) fprintf(stderr, "oprofiled: ctr0: event only available on PII\n");
        if (ret&OP_CTR1_PII_EVENT) fprintf(stderr, "oprofiled: ctr1: event only available on PII\n");
        if (ret&OP_CTR0_PIII_EVENT) fprintf(stderr, "oprofiled: ctr0: event only available on PIII\n");
        if (ret&OP_CTR1_PIII_EVENT) fprintf(stderr, "oprofiled: ctr1: event only available on PIII\n");

	if (ret!=OP_EVENTS_OK) {
		poptPrintHelp(optcon, stderr, 0);
		exit(1);
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
		fprintf(stderr,"oprofiled: opd_go_daemon: couldn't chdir to %s: %s",opd_dir,strerror(errno));
		exit(1);
	}

	if (setsid() < 0) {
		perror("oprofiled: opd_go_daemon: couldn't setsid: ");
		exit(1);
	}

	opd_fork();
	mypid = getpid();
}

void opd_do_samples(const struct op_sample *opd_buf);

/**
 * opd_do_read - enter processing loop
 * @buf: buffer to read into
 * @size: size of buffer
 *
 * Read a full buffer from the device and process
 * the contents.
 *
 * Never returns.
 */
static void opd_do_read(struct op_sample *buf, size_t size)
{
	while (1) {
		opd_read_device(devfd,buf,size,TRUE);
		opd_do_samples(buf);
	}
}

/**
 * opd_is_mapping - is the entry a notification
 * @sample: sample to use
 *
 * Returns positive if the sample is actually a notification,
 * zero otherwise.
 */
inline static u16 opd_is_mapping(const struct op_sample *sample)
{
	return (sample->count & OP_MAPPING);
}

/**
 * opd_do_samples - process a full sample buffer
 * @opd_buf: buffer to process
 *
 * Process a buffer full of opd_buf_size samples.
 * The signals specified by the global variable maskset are
 * masked. Samples for oprofiled are ignored if the global
 * variable ignore_myself is set.
 *
 * If the sample could be processed correctly, it is written
 * to the relevant sample file.
 */
void opd_do_samples(const struct op_sample *opd_buf)
{
	uint i;

	/* prevent signals from messing us up */
	sigprocmask(SIG_BLOCK,&maskset,NULL);

	opd_stats[OPD_DUMP_COUNT]++;

	//printf("Reading %d entries.\n",opd_buf->eip);

	/* opd_buf->eip contains how many to read */
	for (i=1; i <= opd_buf->eip; i++) {
		//printf("%u: eip 0x%x, pid %u, count %u\n",i,((struct op_sample *)opd_buf)[i].eip,((struct op_sample *)opd_buf)[i].pid,((struct op_sample *)opd_buf)[i].count); 
		if (ignore_myself && opd_buf[i].pid==mypid)
			continue;

		if (opd_is_mapping(&opd_buf[i])) {
			switch (opd_buf[i].count) {
				case OP_FORK:
					opd_handle_fork(&opd_buf[i]);
					break;

				case OP_DROP:
					opd_handle_drop_mappings(&opd_buf[i]);
					break;

				case OP_MAP:
					opd_handle_mapping(&opd_buf[i]);
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

	sigprocmask(SIG_UNBLOCK,&maskset,NULL);
}

/* re-open logfile for logrotate */
static void opd_sighup(int val __attribute__((unused)))
{
	close(1);
	close(2);
	opd_open_logfile();
}

int main(int argc, char *argv[])
{
	struct op_sample *opd_buf;
	size_t opd_buf_bytesize;
	struct sigaction act;
	int i;

	opd_options(argc, argv);

	footer.ctr0_type_val = ctr0_type_val;
	footer.ctr0_um = (u8)ctr0_um;
	footer.ctr1_type_val = ctr1_type_val;
	footer.ctr1_um = (u8)ctr1_um;

	/* one extra for the "how many" header */
	opd_buf_bytesize=(opd_buf_size+1)*sizeof(struct op_sample);

 	opd_buf = opd_malloc(opd_buf_bytesize);

	opd_init_images();

	/* yes, this is racey. */
	opd_get_ascii_procs();

	opd_go_daemon();

	opd_open_files();

	for (i=0; i< OPD_MAX_STATS; i++) {
		opd_stats[i] = 0;
	}

	act.sa_handler=opd_alarm;
	act.sa_flags=0;
	sigemptyset(&act.sa_mask);

	if (sigaction(SIGALRM, &act, NULL)) {
		perror("oprofiled: install of SIGALRM handler failed: ");
		exit(1);
	}

	act.sa_handler=opd_sighup;
	act.sa_flags=0;
	sigemptyset(&act.sa_mask);
	sigaddset(&act.sa_mask,SIGALRM);

	if (sigaction(SIGHUP, &act, NULL)) {
		perror("oprofiled: install of SIGHUP handler failed: ");
		exit(1);
	}

	sigemptyset(&maskset);
	sigaddset(&maskset,SIGALRM);
	sigaddset(&maskset,SIGHUP);

	/* clean up every 20 minutes */
	alarm(60*20);

	/* simple sleep-then-process loop */
	opd_do_read(opd_buf,opd_buf_bytesize);

	return 0;
}
