/**
 * @file daemon/oprofiled.c
 * Daemon set up and main loop
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author John Levon
 * @author Philippe Elie
 */

#include <config.h>
 
#include "opd_stats.h"
#include "opd_sfile.h"
#include "opd_util.h"
#include "opd_kernel.h"
#include "opd_trans.h"

#include "op_version.h"
#include "op_config.h"
#include "op_popt.h"
#include "op_file.h"
#include "op_fileio.h"
#include "op_deviceio.h"
#include "op_lockfile.h"
#include "op_get_time.h"
#include "op_libiberty.h"
#include "op_events.h"
#ifdef OPROF_ABI
#include "op_abi.h"
#endif
#include "op_cpufreq.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <unistd.h>
#include <signal.h>
#include <dirent.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>

// GNU libc bug
pid_t getpgid(pid_t pid);

u32 ctr_count[OP_MAX_COUNTERS];
u8 ctr_event[OP_MAX_COUNTERS];
u16 ctr_um[OP_MAX_COUNTERS];
double cpu_speed;

uint op_nr_counters;
int verbose;
op_cpu cpu_type;
int separate_lib;
int separate_kernel;
int separate_thread;
int separate_cpu;
int no_vmlinux;
char * vmlinux;
size_t kernel_pointer_size;

static char * kernel_range;
static int showvers;
static u32 ctr_enabled[OP_MAX_COUNTERS];
static int opd_buf_size;
static fd_t devfd;

static void opd_sighup(void);
static void opd_alarm(void);
static void opd_sigterm(void);

static struct poptOption options[] = {
	{ "kernel-range", 'r', POPT_ARG_STRING, &kernel_range, 0, "Kernel VMA range", "start-end", },
	{ "vmlinux", 'k', POPT_ARG_STRING, &vmlinux, 0, "vmlinux kernel image", "file", },
	{ "no-vmlinux", 0, POPT_ARG_NONE, &no_vmlinux, 0, "vmlinux kernel image file not available", NULL, },
	{ "separate-lib", 0, POPT_ARG_INT, &separate_lib, 0, "separate library samples for each distinct application", "[0|1]", },
	{ "separate-kernel", 0, POPT_ARG_INT, &separate_kernel, 0, "separate kernel samples for each distinct application", "[0|1]", },
	{ "separate-thread", 0, POPT_ARG_INT, &separate_thread, 0, "thread-profiling mode", "[0|1]" },
	{ "separate-cpu", 0, POPT_ARG_INT, &separate_cpu, 0, "separate samples for each CPU", "[0|1]" },
	{ "version", 'v', POPT_ARG_NONE, &showvers, 0, "show version", NULL, },
	{ "verbose", 'V', POPT_ARG_NONE, &verbose, 0, "be verbose in log file", NULL, },
	POPT_AUTOHELP
	{ NULL, 0, 0, NULL, 0, NULL, NULL, },
};
 

/**
 * opd_open_files - open necessary files
 *
 * Open the device files and the log file,
 * and mmap() the hash map.
 */
static void opd_open_files(void)
{
	devfd = op_open_device("/dev/oprofile/buffer", 0);
	if (devfd == -1) {
		if (errno == EINVAL)
			fprintf(stderr, "Failed to open device. Possibly you have passed incorrect\n"
				"parameters. Check /var/log/messages.");
		else
			perror("Failed to open profile device");
		exit(EXIT_FAILURE);
	}

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
	printf("kernel pointer size: %lu\n",
		(unsigned long)kernel_pointer_size);
	fflush(stdout);
}
 

/** return the int in the given counter's oprofilefs file */
static int opd_read_fs_int_pmc(int ctr, char const * name)
{
	char filename[PATH_MAX + 1];
	snprintf(filename, PATH_MAX, "/dev/oprofile/%d/%s", ctr, name);
	return op_read_int_from_file(filename);
}

 
/** return the int in the given oprofilefs file */
static int opd_read_fs_int(char const * name)
{
	char filename[PATH_MAX + 1];
	snprintf(filename, PATH_MAX, "/dev/oprofile/%s", name);
	return op_read_int_from_file(filename);
}


/**
 * opd_pmc_options - read sysctls for pmc options
 */
static void opd_pmc_options(void)
{
	int ret;
	uint i;
	unsigned int min_count;

	for (i = 0 ; i < op_nr_counters ; ++i) {
		ctr_event[i] = opd_read_fs_int_pmc(i, "event");
		ctr_count[i] = opd_read_fs_int_pmc(i, "count");
		ctr_um[i] = opd_read_fs_int_pmc(i, "unit_mask");
		ctr_enabled[i] = opd_read_fs_int_pmc(i, "enabled");

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

		min_count = op_min_count(ctr_event[i], cpu_type);
		if (ctr_count[i] < min_count) {
			fprintf(stderr, "oprofiled: ctr%d: count is too low: %d, minimum is %d\n",
				i, ctr_count[i], min_count);
			exit(EXIT_FAILURE);
		}
	}

	op_free_events();
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

	if (separate_kernel) {
		separate_lib = 1;
	}

	cpu_type = op_get_cpu_type();
	op_nr_counters = op_get_nr_counters(cpu_type);

	if (!no_vmlinux) {
		if (!vmlinux || !strcmp("", vmlinux)) {
			fprintf(stderr, "oprofiled: no vmlinux specified.\n");
			poptPrintHelp(optcon, stderr, 0);
			exit(EXIT_FAILURE);
		}

		/* canonicalise vmlinux filename. fix #637805 */
		vmlinux = op_relative_to_absolute_path(vmlinux, NULL);

		if (!kernel_range || !strcmp("", kernel_range)) {
			fprintf(stderr, "oprofiled: no kernel VMA range specified.\n");
			poptPrintHelp(optcon, stderr, 0);
			exit(EXIT_FAILURE);
		}
	}

	opd_create_vmlinux(vmlinux, kernel_range);

	opd_buf_size = opd_read_fs_int("buffer_size");

	if (cpu_type != CPU_TIMER_INT) {
		opd_pmc_options();
	}

	cpu_speed = op_cpu_frequency();

	poptFreeContext(optcon);
}

 
/** Done writing out the samples, indicate with complete_dump file */
static void complete_dump(void)
{
	FILE * status_file;

retry:
       	status_file = fopen(OP_DUMP_STATUS, "w");

	if (!status_file && errno == EMFILE) {
		if (sfile_lru_clear()) {
			printf("LRU cleared but file open fails for %s.\n",
			       OP_DUMP_STATUS);
			abort();
		}
		goto retry;
	}

	if (!status_file) {
		perror("warning: couldn't set complete_dump: ");
		return;
	}

        fprintf(status_file, "1\n");
        fclose(status_file);
}

 
/**
 * opd_do_samples - process a sample buffer
 * @param opd_buf  buffer to process
 *
 * Process a buffer of samples.
 *
 * If the sample could be processed correctly, it is written
 * to the relevant sample file.
 */
static void opd_do_samples(char const * opd_buf, ssize_t count)
{
	size_t num = count / kernel_pointer_size;
 
	opd_stats[OPD_DUMP_COUNT]++;

	printf("Read buffer of %d entries.\n", (unsigned int)num);
 
	opd_process_samples(opd_buf, num);

	complete_dump();
}
 

/**
 * opd_do_read - enter processing loop
 * @param buf  buffer to read into
 * @param size  size of buffer
 *
 * Read some of a buffer from the device and process
 * the contents.
 */
static void opd_do_read(char * buf, size_t size)
{
	while (1) {
		ssize_t count = -1;

		/* loop to handle EINTR */
		while (count < 0) {
			count = op_read_device(devfd, buf, size);

			/* we can lose an alarm or a hup but
			 * we don't care.
			 */
			if (signal_alarm) {
				signal_alarm = 0;
				opd_alarm();
			}

			if (signal_hup) {
				signal_hup = 0;
				opd_sighup();
			}

			if (signal_term)
				opd_sigterm();
		}

		opd_do_samples(buf, count);
	}
}


/** opd_alarm - sync files and report stats */
static void opd_alarm(void)
{
	sfile_sync_files();
	opd_print_stats();
	alarm(60*10);
}
 

/** re-open files for logrotate/opcontrol --reset */
static void opd_sighup(void)
{
	printf("Received SIGHUP.\n");
	/* We just close them, and re-open them lazily as usual. */
	sfile_close_files();
	close(1);
	close(2);
	opd_open_logfile();
}


static void clean_exit(void)
{
	unlink(OP_LOCK_FILE);
}


static void opd_sigterm(void)
{
	opd_print_stats();
	printf("oprofiled stopped %s", op_get_time());
	exit(EXIT_FAILURE);
}
 

static size_t opd_pointer_size(void)
{
	unsigned long val = 0;
	FILE * fp;

	if (!op_file_readable("/dev/oprofile/pointer_size")) {
		fprintf(stderr, "/dev/oprofile/pointer_size not readable\n");
		exit(EXIT_FAILURE);
	}


	fp = op_open_file("/dev/oprofile/pointer_size", "r");
	if (!fp) {
		fprintf(stderr, "oprofiled: couldn't open "
			"/dev/oprofile/pointer_size.\n");
		exit(EXIT_FAILURE);
	}

	fscanf(fp, "%lu\n", &val);
	if (!val) {
		fprintf(stderr, "oprofiled: couldn't read "
			"/dev/oprofile/pointer_size.\n");
		exit(EXIT_FAILURE);
	}

	op_close_file(fp);
	return val;
}


int main(int argc, char const * argv[])
{
	char * sbuf;
	size_t s_buf_bytesize;
	int i;
	int err;
	struct rlimit rlim = { 2048, 2048 };

	opd_options(argc, argv);

	kernel_pointer_size = opd_pointer_size();

	s_buf_bytesize = opd_buf_size * kernel_pointer_size;

 	sbuf = xmalloc(s_buf_bytesize);

	opd_reread_module_info();

	opd_write_abi();

	if (atexit(clean_exit)) {
		perror("oprofiled: couldn't set exit cleanup: ");
		exit(EXIT_FAILURE);
	}

	opd_go_daemon();

	opd_open_files();

	for (i = 0; i < OPD_MAX_STATS; i++) {
		opd_stats[i] = 0;
	}

	opd_setup_signals();
 
	err = setrlimit(RLIMIT_NOFILE, &rlim);
	if (err) {
		perror("warning: could not set RLIMIT_NOFILE to 2048: ");
	}

	if (op_write_lock_file(OP_LOCK_FILE)) {
		fprintf(stderr, "oprofiled: could not create lock file "
			OP_LOCK_FILE "\n");
		exit(EXIT_FAILURE);
	}

	cookie_init();
	sfile_init();

	/* simple sleep-then-process loop */
	opd_do_read(sbuf, s_buf_bytesize);

	opd_print_stats();
	printf("oprofiled stopped %s", op_get_time());

	free(sbuf);
	free(vmlinux);
	/* FIXME: free kernel images, sfiles etc. */

	return 0;
}
