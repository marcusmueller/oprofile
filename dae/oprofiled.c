/**
 * @file dae/oprofiled.c
 * Daemon set up and main loop
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author John Levon
 * @author Philippe Elie
 */

#include <config.h>
 
#include "opd_proc.h"
#include "opd_mapping.h"
#include "opd_stats.h"
#include "opd_sample_files.h"
#include "opd_image.h"
#include "opd_parse_proc.h"
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
#include "op_interface.h"
#include "op_hw_config.h"
#include "op_config_24.h"
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
fd_t hashmapdevfd;

uint op_nr_counters;
int verbose;
op_cpu cpu_type;
int separate_lib;
int separate_kernel;
int no_vmlinux;
char * vmlinux;
int kernel_only;

static char * kernel_range;
static int showvers;
static u32 ctr_enabled[OP_MAX_COUNTERS];
static char const * mount = OP_MOUNT;
static int opd_buf_size=OP_DEFAULT_BUF_SIZE;
static int opd_note_buf_size=OP_DEFAULT_NOTE_SIZE;
static pid_t pid_filter;
static pid_t pgrp_filter;
static sigset_t maskset;
static fd_t devfd;
static fd_t notedevfd;

static void opd_sighup(int val);

static struct poptOption options[] = {
	{ "pid-filter", 0, POPT_ARG_INT, &pid_filter, 0, "only profile the given process ID", "pid" },
	{ "pgrp-filter", 0, POPT_ARG_INT, &pgrp_filter, 0, "only profile the given process tty group", "pgrp" },
	{ "kernel-range", 'r', POPT_ARG_STRING, &kernel_range, 0, "Kernel VMA range", "start-end", },
	{ "vmlinux", 'k', POPT_ARG_STRING, &vmlinux, 0, "vmlinux kernel image", "file", },
	{ "no-vmlinux", 0, POPT_ARG_NONE, &no_vmlinux, 0, "vmlinux kernel image file not available", NULL, },
	{ "separate-lib", 0, POPT_ARG_INT, &separate_lib, 0, "separate library samples for each distinct application", "[0|1]", },
	{ "separate-kernel", 0, POPT_ARG_INT, &separate_kernel, 0, "separate kernel samples for each distinct application, this option imply --separate-lib=1", "[0|1]", },
	{ "version", 'v', POPT_ARG_NONE, &showvers, 0, "show version", NULL, },
	{ "verbose", 'V', POPT_ARG_NONE, &verbose, 0, "be verbose in log file", NULL, },
	POPT_AUTOHELP
	{ NULL, 0, 0, NULL, 0, NULL, NULL, },
};
 

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
 * opd_rtc_options - read sysctls + set up for rtc options
 */
static void opd_rtc_options(void)
{
	uint i;

	for (i = 0 ; i < op_nr_counters ; ++i) {
		ctr_event[i] = ctr_count[i] =  ctr_um[i] = ctr_enabled[i] = 0;
	}

	ctr_count[0] = op_read_int_from_file("/proc/sys/dev/oprofile/rtc_value");
	ctr_enabled[0] = 1;

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

		opd_parse_kernel_range(kernel_range);
	}

	opd_buf_size = opd_read_fs_int("bufsize");
	opd_note_buf_size = opd_read_fs_int("notesize");
	kernel_only = opd_read_fs_int("kernel_only");

	if (cpu_type != CPU_RTC) {
		opd_pmc_options();
	} else {
		opd_rtc_options();
	}

	cpu_speed = op_cpu_frequency();

	poptFreeContext(optcon);
}

 
static void opd_do_samples(struct op_buffer_head const * buf);
static void opd_do_notes(struct op_note const * opd_buf, size_t count);

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
static void opd_do_notes(struct op_note const * opd_buf, size_t count)
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
				abort();
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
static void opd_do_samples(struct op_buffer_head const * opd_buf)
{
	uint i;
	struct op_sample const * buffer = opd_buf->buffer; 

	/* prevent signals from messing us up */
	sigprocmask(SIG_BLOCK, &maskset, NULL);

	opd_stats[OPD_DUMP_COUNT]++;

	verbprintf("Read buffer of %d entries.\n",
		   (unsigned int)opd_buf->count);
 
	for (i = 0; i < opd_buf->count; i++) {
		verbprintf("%.6u: EIP: 0x%.8lx pid: %.6d\n",
			i, buffer[i].eip, buffer[i].pid);

		/* FIXME : we can try to remove cast by using in module pid_t
		 * as type for op_sample.pid but this don't work if kernel
		 * space definition of pid_t was 16 bits in past */
		if (pid_filter && (u32)pid_filter != buffer[i].pid)
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


int main(int argc, char const * argv[])
{
	struct op_buffer_head * sbuf;
	size_t s_buf_bytesize;
	struct op_note * nbuf;
	size_t n_buf_bytesize;
	int i;
	int err;
	struct rlimit rlim = { 8192, 8192 };

	opd_options(argc, argv);

	s_buf_bytesize = sizeof(struct op_buffer_head) + opd_buf_size * sizeof(struct op_sample);

 	sbuf = xmalloc(s_buf_bytesize);

	n_buf_bytesize = opd_note_buf_size * sizeof(struct op_note);
	nbuf = xmalloc(n_buf_bytesize);

	opd_init_kernel_image();

	opd_write_abi();

	if (atexit(clean_exit)) {
		perror("oprofiled: couldn't set exit cleanup: ");
		exit(EXIT_FAILURE);
	}

	opd_go_daemon();

	op_open_files();

	/* yes, this is racey. */
	opd_get_ascii_procs();

	for (i=0; i< OPD_MAX_STATS; i++) {
		opd_stats[i] = 0;
	}

	setup_signals();
 
	err = setrlimit(RLIMIT_NOFILE, &rlim);
	if (err) {
		perror("warning: could not set RLIMIT_NOFILE to 8192: ");
	}

	if (op_write_lock_file(OP_LOCK_FILE)) {
		fprintf(stderr, "oprofiled: could not create lock file "
			OP_LOCK_FILE "\n");
		exit(EXIT_FAILURE);
	}

	/* simple sleep-then-process loop */
	opd_do_read(sbuf, s_buf_bytesize, nbuf, n_buf_bytesize);

	opd_print_stats();
	printf("oprofiled stopped %s", op_get_time());

	free(sbuf);
	free(nbuf);
	opd_clear_module_info();
	opd_proc_cleanup();
	opd_image_cleanup();
	free(vmlinux);

	return 0;
}
