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
#include "opd_perfmon.h"
#include "opd_printf.h"

#include "op_version.h"
#include "op_config.h"
#include "op_file.h"
#include "op_fileio.h"
#include "op_string.h"
#include "op_deviceio.h"
#include "op_lockfile.h"
#include "op_get_time.h"
#include "op_libiberty.h"
#include "op_events.h"
#ifdef OPROF_ABI
#include "op_abi.h"
#endif

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

size_t kernel_pointer_size;

static fd_t devfd;

static void opd_sighup(void);
static void opd_alarm(void);
static void opd_sigterm(void);

/**
 * opd_open_files - open necessary files
 *
 * Open the device files and the log file,
 * and mmap() the hash map.
 */
static void opd_open_files(void)
{
	devfd = op_open_device("/dev/oprofile/buffer");
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
 

/** return the int in the given oprofilefs file */
static int opd_read_fs_int(char const * name)
{
	char filename[PATH_MAX + 1];
	snprintf(filename, PATH_MAX, "/dev/oprofile/%s", name);
	return op_read_int_from_file(filename);
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

			if (signal_usr1) {
				signal_usr1 = 0;
				perfmon_start();
			}

			if (signal_usr2) {
				signal_usr2 = 0;
				perfmon_stop();
			}
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
	perfmon_exit();
	unlink(OP_LOCK_FILE);
}


static void opd_sigterm(void)
{
	opd_print_stats();
	printf("oprofiled stopped %s", op_get_time());
	exit(EXIT_FAILURE);
}
 

int main(int argc, char const * argv[])
{
	char * sbuf;
	size_t s_buf_bytesize;
	int opd_buf_size;
	int i;
	int err;
	struct rlimit rlim = { 2048, 2048 };

	opd_options(argc, argv);

	opd_create_vmlinux(vmlinux, kernel_range);

	opd_buf_size = opd_read_fs_int("buffer_size");
	kernel_pointer_size = opd_read_fs_int("pointer_size");

	s_buf_bytesize = opd_buf_size * kernel_pointer_size;

 	sbuf = xmalloc(s_buf_bytesize);

	opd_reread_module_info();

	opd_write_abi();

	opd_go_daemon();

	opd_open_files();

	for (i = 0; i < OPD_MAX_STATS; i++)
		opd_stats[i] = 0;

	perfmon_init();
	cookie_init();
	sfile_init();

	opd_setup_signals();
 
	err = setrlimit(RLIMIT_NOFILE, &rlim);
	if (err) {
		perror("warning: could not set RLIMIT_NOFILE to 2048: ");
	}

	/* must be /after/ perfmon_init() at least */
	if (atexit(clean_exit)) {
		perror("oprofiled: couldn't set exit cleanup: ");
		exit(EXIT_FAILURE);
	}

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
	free(vmlinux);
	/* FIXME: free kernel images, sfiles etc. */

	return 0;
}
