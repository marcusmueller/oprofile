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
#include "opd_sample_files.h"
#include "opd_image.h"
#include "opd_kernel.h"
#include "opd_printf.h"
#include "opd_util.h"

#include "op_version.h"
#include "op_popt.h"
#include "op_file.h"
#include "op_fileio.h"
#include "op_deviceio.h"
#include "op_lockfile.h"
#include "op_get_time.h"
#include "op_sample_file.h"
#include "op_events.h"
#include "op_libiberty.h"
#include "op_interface_25.h"
#include "op_config_25.h"
#include "op_hw_config.h"
#ifdef OPROF_ABI
#include "op_abi.h"
#endif
#include "op_cpufreq.h"

#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <elf.h>

// GNU libc bug
pid_t getpgid(pid_t pid);

u32 ctr_count[OP_MAX_COUNTERS];
u8 ctr_event[OP_MAX_COUNTERS];
u16 ctr_um[OP_MAX_COUNTERS];
double cpu_speed;

uint op_nr_counters;
int verbose;
op_cpu cpu_type;
int separate_lib_samples;
int separate_kernel_samples;
int no_vmlinux;
char * vmlinux;
size_t kernel_pointer_size;

static char * kernel_range;
static int showvers;
static u32 ctr_enabled[OP_MAX_COUNTERS];
static char const * mount = OP_MOUNT;
static int opd_buf_size;
static pid_t pid_filter;
static pid_t pgrp_filter;
static sigset_t maskset;
static fd_t devfd;

static void opd_sighup(int val);

static struct poptOption options[] = {
	{ "mount", 'm', POPT_ARG_STRING, &mount, 0, "path to mounted oprofilefs", "dir" },
	{ "pid-filter", 0, POPT_ARG_INT, &pid_filter, 0, "only profile the given process ID", "pid" },
	{ "pgrp-filter", 0, POPT_ARG_INT, &pgrp_filter, 0, "only profile the given process tty group", "pgrp" },
	{ "kernel-range", 'r', POPT_ARG_STRING, &kernel_range, 0, "Kernel VMA range", "start-end", },
	{ "vmlinux", 'k', POPT_ARG_STRING, &vmlinux, 0, "vmlinux kernel image", "file", },
	{ "no-vmlinux", 0, POPT_ARG_NONE, &no_vmlinux, 0, "vmlinux kernel image file not available", NULL, },
	{ "separate-lib-samples", 0, POPT_ARG_INT, &separate_lib_samples, 0, "separate library samples for each distinct application", "[0|1]", },
	{ "separate-kernel-samples", 0, POPT_ARG_INT, &separate_kernel_samples, 0, "separate kernel samples for each distinct application", "[0|1]", },
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
	devfd = op_open_device(OP_DEVICE, 0);
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
	fflush(stdout);
}
 

/** return the int in the given counter's oprofilefs file */
static int opd_read_fs_int_pmc(int ctr, char const * name)
{
	char filename[PATH_MAX + 1];
	snprintf(filename, PATH_MAX, "%s/%d/%s", mount, ctr, name);
	return op_read_int_from_file(filename);
}

 
/** return the int in the given oprofilefs file */
static int opd_read_fs_int(char const * name)
{
	char filename[PATH_MAX + 1];
	snprintf(filename, PATH_MAX, "%s/%s", mount, name);
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

	if (separate_kernel_samples) {
		separate_lib_samples = 1;
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

		opd_parse_kernel_range(kernel_range);
	}

	opd_buf_size = opd_read_fs_int("buffer_size");

	if (cpu_type != CPU_TIMER_INT) {
		opd_pmc_options();
	}

	cpu_speed = op_cpu_frequency();

	poptFreeContext(optcon);
}

 
/** Done writing out the samples, indicate with complete_dump file */
static void complete_dump()
{
	FILE * status_file = fopen(OP_DUMP_STATUS, "w");

	if (!status_file) {
		fprintf(stderr, "Couldn't set %s !\n", OP_DUMP_STATUS);
		exit(EXIT_FAILURE);
	}

        fprintf(status_file, "1\n");
        fclose(status_file);
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
 * to the relevant sample file.
 */
static void opd_do_samples(char const * opd_buf, ssize_t count)
{
	size_t num = count / kernel_pointer_size;
 
	/* prevent signals from messing us up */
	sigprocmask(SIG_BLOCK, &maskset, NULL);

	opd_stats[OPD_DUMP_COUNT]++;

	printf("Read buffer of %d entries.\n", (unsigned int)num);
 
	opd_process_samples(opd_buf, num);

	complete_dump();

	sigprocmask(SIG_UNBLOCK, &maskset, NULL);
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
		}

		opd_do_samples(buf, count);
	}
}


/**
 * opd_alarm - sync files and report stats
 */
static void opd_alarm(int val __attribute__((unused)))
{
	opd_for_each_image(opd_sync_image_samples_files);
	opd_print_stats();
	alarm(60*10);
}
 

/* re-open logfile for logrotate */
static void opd_sighup(int val __attribute__((unused)))
{
	printf("Received SIGHUP.\n");
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
	opd_print_stats();
	printf("oprofiled stopped %s", op_get_time());
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


static size_t opd_pointer_size(void)
{
	size_t size;
	char elf_header[EI_NIDENT];
	FILE * fp;

	if (!op_file_readable("/proc/kcore")) {
		fprintf(stderr, "oprofiled: /proc/kcore not readable.");
		goto guess;
	}

	fp = op_open_file("/proc/kcore", "r");
	op_read_file(fp, &elf_header, 16);
	op_close_file(fp);

	/* CONFIG_KCORE_AOUT exists alas */
	if (memcmp(elf_header, ELFMAG, SELFMAG) != 0) {
		fprintf(stderr, "oprofiled: /proc/kcore not in ELF format.");
		goto guess;
	}

	switch (elf_header[EI_CLASS]) {
		case ELFCLASS32:
			return 4;
		case ELFCLASS64:
			return 8;
	}

	fprintf(stderr, "oprofiled: /proc/kcore has bad class.\n");

guess:
	size = sizeof(unsigned long);
	fprintf(stderr, "oprofiled: assuming pointer size is %u\n",
		(unsigned int)size);
	return size;
}


int main(int argc, char const * argv[])
{
	char * sbuf;
	size_t s_buf_bytesize;
	int i;

	opd_options(argc, argv);

	kernel_pointer_size = opd_pointer_size();

	s_buf_bytesize = opd_buf_size * kernel_pointer_size;

 	sbuf = xmalloc(s_buf_bytesize);

	opd_init_images();
	opd_init_kernel_image();
	opd_reread_module_info();

	opd_write_abi();

	if (atexit(clean_exit)) {
		fprintf(stderr, "Couldn't set exit cleanup !\n");
		exit(EXIT_FAILURE);
	}

	opd_go_daemon();

	opd_open_files();

	for (i = 0; i < OPD_MAX_STATS; i++) {
		opd_stats[i] = 0;
	}

	setup_signals();
 
	if (op_write_lock_file(OP_LOCK_FILE)) {
		fprintf(stderr, "oprofiled: could not create lock file "
			OP_LOCK_FILE "\n");
		exit(EXIT_FAILURE);
	}

	/* simple sleep-then-process loop */
	opd_do_read(sbuf, s_buf_bytesize);

	opd_print_stats();
	printf("oprofiled stopped %s", op_get_time());

	free(sbuf);
	opd_image_cleanup();
	free(vmlinux);

	return 0;
}
