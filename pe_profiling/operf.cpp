/**
 * @file operf.cpp
 * Front-end (containing main) for handling a user request to run a profile
 * using the new Linux Performance Events Subsystem.
 *
 * @remark Copyright 2011 OProfile authors
 * @remark Read the file COPYING
 *
 * Created on: Dec 7, 2011
 * @author Maynard Johnson
 * (C) Copyright IBM Corp. 2011
 *
 * Modified by Maynard Johnson <maynardj@us.ibm.com>
 * (C) Copyright IBM Corporation 2012, 2013, 2014
 *
 */

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <dirent.h>
#include <exception>
#include <pwd.h>
#include <errno.h>
#include <sys/time.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <ftw.h>
#include <getopt.h>
#include <iostream>
#include "operf_utils.h"
#include "op_pe_utils.h"
#include "op_libiberty.h"
#include "string_manip.h"
#include "cverb.h"
#include "operf_counter.h"
#include "op_cpu_type.h"
#include "op_cpufreq.h"
#include "op_events.h"
#include "op_string.h"
#include "operf_kernel.h"
#include "child_reader.h"
#include "op_get_time.h"
#include "operf_stats.h"
#include "op_netburst.h"
#include "utility.h"

using namespace std;
using namespace op_pe_utils;

typedef enum END_CODE {
	ALL_OK = 0,
	APP_ABNORMAL_END =  1,
	PERF_RECORD_ERROR = 2,
	PERF_READ_ERROR   = 4,
	PERF_BOTH_ERROR   = 8
} end_code_t;

// Globals
char * app_name = NULL;
bool use_cpu_minus_one = false;
pid_t app_PID = -1;
uint64_t kernel_start, kernel_end;
op_cpu cpu_type;
double cpu_speed;
uint op_nr_events;
verbose vmisc("misc");
uid_t my_uid;
bool no_vmlinux;
int kptr_restrict;
char * start_time_human_readable;
std::vector<operf_event_t> events;
operf_read operfRead(events);
/* With certain operf options, we have to take extra steps to track new threads
 * and processes that an app may create via pthread_create, fork, etc.  Note that
 * any such thread or process creation APIs will result in a PERF_RECORD_FORK event,
 * so we handle these new threads/processes in operf_utils::__handle_fork_event.
 */
bool track_new_forks;


#define DEFAULT_OPERF_OUTFILE "operf.data"
#define KERN_ADDR_SPACE_START_SYMBOL  "_text"
#define KERN_ADDR_SPACE_END_SYMBOL    "_etext"

static operf_record * operfRecord = NULL;
static char * app_name_SAVE = NULL;
static char ** app_args = NULL;
static 	pid_t jitconv_pid = -1;
static bool app_started;
static pid_t operf_record_pid;
static pid_t operf_read_pid;
static string samples_dir;
static bool startApp;
static string outputfile;
static char start_time_str[32];
static bool jit_conversion_running;
static void convert_sample_data(void);
static int sample_data_pipe[2];
static int app_ready_pipe[2], start_app_pipe[2], operf_record_ready_pipe[2];
// The operf_convert_record_write_pipe is used for the convert process to send
// forked PID data to the record process.
static int operf_convert_record_write_pipe[2];
// The operf_record_convert_write_pipe is used for the record process to send
// data to the convert process in response to the forked PID data.
static int operf_record_convert_write_pipe[2];
// The operf_post_profiling_pipe is used by the main process to inform the operf_read_pid
// that profiling is done.  The operf_read_pid will then print its progress in
// finishing the conversion.
static int operf_post_profiling_pipe[2];

bool ctl_c = false;
bool pipe_closed = false;


namespace operf_options {
bool system_wide;
bool append;
int pid;
bool callgraph;
int mmap_pages_mult;
string session_dir;
string vmlinux;
bool separate_cpu;
bool separate_thread;
bool post_conversion;
set<string> evts;
}

static const char * valid_verbose_vals[] = { "debug", "record", "convert", "misc", "sfile", "arcs", "all"};
#define NUM_VERBOSE_OPTIONS (sizeof(valid_verbose_vals)/sizeof(char *))

struct option long_options [] =
{
 {"verbose", required_argument, NULL, 'V'},
 {"session-dir", required_argument, NULL, 'd'},
 {"vmlinux", required_argument, NULL, 'k'},
 {"callgraph", no_argument, NULL, 'g'},
 {"system-wide", no_argument, NULL, 's'},
 {"append", no_argument, NULL, 'a'},
 {"pid", required_argument, NULL, 'p'},
 {"events", required_argument, NULL, 'e'},
 {"separate-cpu", no_argument, NULL, 'c'},
 {"separate-thread", no_argument, NULL, 't'},
 {"lazy-conversion", no_argument, NULL, 'l'},
 {"help", no_argument, NULL, 'h'},
 {"version", no_argument, NULL, 'v'},
 {"usage", no_argument, NULL, 'u'},
 {NULL, 9, NULL, 0}
};

const char * short_options = "V:d:k:gsap:e:ctlhuv";

vector<string> verbose_string;

void __set_event_throttled(int index)
{
	if (index < 0) {
		cerr << "Unable to determine if throttling occurred for ";
		cerr << "event " << events[index].name << endl;
	} else {
		throttled = true;
		events[index].throttled = true;
	}
}

static void __print_usage_and_exit(const char * extra_msg)
{
	if (extra_msg)
		cerr << extra_msg << endl;
	cerr << "usage: operf [ options ] [ --system-wide | --pid <pid> | [ command [ args ] ] ]" << endl;
	cerr << "See operf man page for details." << endl;
	exit(EXIT_FAILURE);
}

// Signal handler for main (parent) process.
static void op_sig_stop(int val __attribute__((unused)))
{
	// Received a signal to quit, so we need to stop the
	// app being profiled.
	size_t dummy __attribute__ ((__unused__));
	ctl_c = true;
	if (cverb << vdebug)
		dummy = write(1, "in op_sig_stop\n", 15);
	if (startApp)
		kill(app_PID, SIGKILL);
}

// For child processes to manage a controlled stop after Ctl-C is done
static void _handle_sigint(int val __attribute__((unused)))
{
	size_t dummy __attribute__ ((__unused__));
	/* Each process (parent and each forked child) will have their own copy of
	 * the ctl_c variable, so this can be used by each process in managing their
	 * shutdown procedure.
	 */
	ctl_c = true;
	if (cverb << vdebug)
		dummy = write(1, "in _handle_sigint\n", 19);
	return;
}


void _set_basic_SIGINT_handler_for_child(void)
{
	struct sigaction act;
	sigset_t ss;

	sigfillset(&ss);
	sigprocmask(SIG_UNBLOCK, &ss, NULL);

	act.sa_handler = _handle_sigint;
	act.sa_flags = 0;
	sigemptyset(&act.sa_mask);
	sigaddset(&act.sa_mask, SIGINT);
	if (sigaction(SIGINT, &act, NULL)) {
		perror("operf: install of SIGINT handler failed: ");
		exit(EXIT_FAILURE);
	}
}

void set_signals_for_parent(void)
{
	struct sigaction act;
	sigset_t ss;

	sigfillset(&ss);
	sigprocmask(SIG_UNBLOCK, &ss, NULL);

	act.sa_handler = op_sig_stop;
	act.sa_flags = 0;
	sigemptyset(&act.sa_mask);
	sigaddset(&act.sa_mask, SIGINT);

	if (sigaction(SIGINT, &act, NULL)) {
		perror("operf: install of SIGINT handler failed: ");
		exit(EXIT_FAILURE);
	}
}

static string args_to_string(void)
{
	string ret;
	char * const * ptr = app_args + 1;
	while (*ptr != NULL) {
		ret.append(*ptr);
		ret += ' ';
		ptr++;
	}
	return ret;
}

void run_app(void)
{
	// ASSUMPTION: app_name is a fully-qualified pathname
	char * app_fname = rindex(app_name, '/') + 1;
	app_args[0] = app_fname;

	string arg_str = args_to_string();
	cverb << vdebug << "Exec args are: " << app_fname << " " << arg_str << endl;
	// Fake an exec to warm-up the resolver
	execvp("", app_args);
	// signal to the parent that we're ready to exec
	int startup = 1;
	if (write(app_ready_pipe[1], &startup, sizeof(startup)) < 0) {
		perror("Internal error on app_ready_pipe");
		_exit(EXIT_FAILURE);
	}

	// wait for parent to tell us to start
	int startme = 0;
	if (read(start_app_pipe[0], &startme, sizeof(startme)) == -1) {
		perror("Internal error in run_app on start_app_pipe");
		_exit(EXIT_FAILURE);
	}
	if (startme != 1)
		_exit(EXIT_SUCCESS);

	cverb << vdebug << "parent says start app " << app_name << endl;
	app_started = true;
	execvp(app_name, app_args);
	cerr <<  "Failed to exec " << app_fname << " " << arg_str << ": " << strerror(errno) << endl;
	/* We don't want any cleanup in the child */
	_exit(EXIT_FAILURE);

}

int start_profiling(void)
{
	// The only process that should return from this function is the process
	// which invoked it.  Any forked process must do _exit() rather than return().
	struct timeval tv;
	unsigned long long start_time = 0ULL;
	gettimeofday(&tv, NULL);
	start_time = 0ULL;
	start_time = tv.tv_sec;
	sprintf(start_time_str, "%llu", start_time);
	start_time_human_readable = op_get_time();
	startApp = ((app_PID != operf_options::pid) && (operf_options::system_wide == false));

	if (startApp) {
		if (pipe(app_ready_pipe) < 0 || pipe(start_app_pipe) < 0) {
			perror("Internal error: operf-record could not create pipe");
			_exit(EXIT_FAILURE);
		}
		app_PID = fork();
		if (app_PID < 0) {
			perror("Internal error: fork failed");
			_exit(EXIT_FAILURE);
		} else if (app_PID == 0) { // child process for exec'ing app
			if (!operf_options::post_conversion) {
				close(sample_data_pipe[0]);
				close(sample_data_pipe[1]);
			}
			run_app();
		}
	}

	// parent
	if (pipe(operf_record_ready_pipe) < 0) {
		perror("Internal error: could not create pipe");
		return -1;
	}
	if (pipe2(operf_convert_record_write_pipe, O_NONBLOCK) < 0) {
		perror("Internal error: could not create pipe");
		return -1;
	}
	if (pipe(operf_record_convert_write_pipe) < 0) {
		perror("Internal error: could not create pipe");
		return -1;
	}
	operf_record_pid = fork();
	if (operf_record_pid < 0) {
		return -1;
	} else if (operf_record_pid == 0) { // operf-record process
		int ready = 0;
		int exit_code = EXIT_SUCCESS;
		_set_basic_SIGINT_handler_for_child();
		close(operf_record_ready_pipe[0]);
		close(operf_convert_record_write_pipe[1]);
		close(operf_record_convert_write_pipe[0]);
		if (!operf_options::post_conversion)
			close(sample_data_pipe[0]);
		/*
		 * Since an informative message will be displayed to the user if
		 * an error occurs, we don't want to blow chunks here; instead, we'll
		 * exit gracefully.  Clear out the operf.data file as an indication
		 * to the parent process that the profile data isn't valid.
		 */
		try {
			OP_perf_utils::vmlinux_info_t vi;
			int outfd;
			int flags = O_WRONLY | O_CREAT | O_TRUNC;
			vi.image_name = operf_options::vmlinux;
			vi.start = kernel_start;
			vi.end = kernel_end;
			if (operf_options::post_conversion) {
				outfd = open(outputfile.c_str(), flags, S_IRUSR|S_IWUSR);
				if (outfd < 0) {
					string errmsg = "Internal error: Could not create temporary output file. errno is ";
					errmsg += strerror(errno);
					throw runtime_error(errmsg);
				}
			} else {
				outfd = sample_data_pipe[1];
			}
			operfRecord = new operf_record(outfd, operf_options::system_wide, app_PID,
			                         (operf_options::pid == app_PID), events, vi,
			                         operf_options::callgraph,
			                         operf_options::separate_cpu, operf_options::post_conversion,
			                         operf_convert_record_write_pipe[0], operf_record_convert_write_pipe[1]);
			if (operfRecord->get_valid() == false) {
				/* If valid is false, it means that one of the "known" errors has
				 * occurred:
				 *   - profiled process has already ended
				 *   - passed PID was invalid
				 *   - device or resource busy
				 *   - failure to mmap kernel profile data
				 */
				cerr << "operf record init failed" << endl;
				cerr << "usage: operf [ options ] [ --system-wide | --pid <pid> | [ command [ args ] ] ]" << endl;
				// Exit with SUCCESS to avoid the unnecessary "operf-record process ended
				// abnormally" message
				goto fail_out;
			}

			ready = 1;
			if (write(operf_record_ready_pipe[1], &ready, sizeof(ready)) < 0) {
				perror("Internal error on operf_record_ready_pipe");
				exit_code = EXIT_FAILURE;
				goto fail_out;
			}

			// start recording
			operfRecord->recordPerfData();
			cverb << vdebug << "Total bytes recorded from perf events: " << dec
					<< operfRecord->get_total_bytes_recorded() << endl;
			delete operfRecord;
		} catch (const runtime_error & re) {
			/* If the user does ctl-c, the operf-record process may get interrupted
			 * in a system call, causing problems with writes to the sample data pipe.
			 * So we'll ignore such errors unless the user requests debug info.
			 */
			if (!ctl_c || (cverb << vmisc)) {
				cerr << "Caught runtime_error: " << re.what() << endl;
				exit_code = EXIT_FAILURE;
			}
			goto fail_out;
		}
		// done
		_exit(exit_code);

fail_out:
		if (operfRecord)
			try {
				delete operfRecord;
			} catch (const runtime_error & re) {
				// We're already in failure mode here; if we get a runtime_error while
				// deleting operfRecord, we'll only print it if user requests "-V misc"
				if (cverb << vmisc) {
					cerr << "Caught runtime_error: " << re.what() << endl;
					exit_code = EXIT_FAILURE;
				}
			}

		if (!ready){
			/* ready==0 means we've not yet told parent we're ready,
			 * but the parent is reading our pipe.  So we tell the
			 * parent we're not ready so it can continue.
			 */
			if (write(operf_record_ready_pipe[1], &ready, sizeof(ready)) < 0) {
				perror("Internal error on operf_record_ready_pipe");
			}
		}
		_exit(exit_code);
	} else {  // parent
		int recorder_ready = 0;
		int startup;
		close(operf_record_ready_pipe[1]);
		if (startApp) {
			if (read(app_ready_pipe[0], &startup, sizeof(startup)) == -1) {
				perror("Internal error on app_ready_pipe");
				return -1;
			} else if (startup != 1) {
				cerr << "app is not ready to start; exiting" << endl;
				return -1;
			}
		}

		if (read(operf_record_ready_pipe[0], &recorder_ready, sizeof(recorder_ready)) == -1) {
			perror("Internal error on operf_record_ready_pipe");
			return -1;
		} else if (recorder_ready != 1) {
			cverb << vdebug << "operf record process failure; exiting" << endl;
			if (startApp) {
				cverb << vdebug << "telling child to abort starting of app" << endl;
				startup = 0;
				if (write(start_app_pipe[1], &startup, sizeof(startup)) < 0) {
					perror("Internal error on start_app_pipe");
				}
			}
			return -1;
		}

		if (startApp) {
			// Tell app_PID to start the app
			cverb << vdebug << "telling child to start app" << endl;
			if (write(start_app_pipe[1], &startup, sizeof(startup)) < 0) {
				perror("Internal error on start_app_pipe");
				return -1;
			}
		}

	}
	if (!operf_options::system_wide)
		app_started = true;

	// parent returns
	return 0;
}

static end_code_t _waitfor_operf_read_pid(end_code_t rc)
{
	// Now wait for the operf-read process to finish
	int wait_rc, waitpid_status, post_processing = 1;

	if (write(operf_post_profiling_pipe[1], &post_processing, sizeof(post_processing)) < 0) {
		perror("Internal error:  Failed to write to operf_post_profiling_pipe");
		rc = rc ? PERF_BOTH_ERROR : PERF_READ_ERROR;
		return rc;
	}
	waitpid_status = 0;
	if ((wait_rc = waitpid(operf_read_pid, &waitpid_status, 0)) < 0) {
		if (errno != ECHILD) {
			perror("waitpid for operf-read process failed");
			rc = rc ? PERF_BOTH_ERROR : PERF_READ_ERROR;
		}
	} else if (wait_rc) {
		if (WIFEXITED(waitpid_status)) {
			if (!WEXITSTATUS(waitpid_status)) {
				cverb << vdebug << "operf-read process returned OK" << endl;
			} else {
				/* If user did ctl-c, operf-read may get spurious errors, like
				 * broken pipe, etc.  We ignore these unless the user asks for
				 * debug output.
				 */
				if (!ctl_c || cverb << vdebug) {
					cerr <<  "operf-read process ended abnormally.  Status = "
							<< WEXITSTATUS(waitpid_status) << endl;
					rc = rc ? PERF_BOTH_ERROR : PERF_READ_ERROR;
				}
			}
		}  else if (WIFSIGNALED(waitpid_status)) {
			/* If user did ctl-c, operf-read may get spurious errors, like
			 * broken pipe, etc.  We ignore these unless the user asks for
			 * debug output.
			 */
			if (!ctl_c || cverb << vdebug) {
				cerr << "operf-read process killed by signal "
						<< WTERMSIG(waitpid_status) << endl;
				rc = PERF_RECORD_ERROR;
			}
		}
	}
	return rc;
}

static end_code_t _kill_operf_record_pid(void)
{
	int waitpid_status = 0;
	end_code_t rc = ALL_OK;

	// stop operf-record process
	errno = 0;
	if (kill(operf_record_pid, SIGUSR1) < 0) {
		// If operf-record process is already ended, don't consider this an error.
		if (errno != ESRCH) {
			perror("Attempt to stop operf-record process failed");
			rc = PERF_RECORD_ERROR;
		}
	} else {
		if (waitpid(operf_record_pid, &waitpid_status, 0) < 0) {
			perror("waitpid for operf-record process failed");
			rc = PERF_RECORD_ERROR;
		} else {
			if (WIFEXITED(waitpid_status) && (!WEXITSTATUS(waitpid_status))) {
				cverb << vdebug << "operf-record process returned OK" << endl;
			} else if (WIFEXITED(waitpid_status)) {
				/* If user did ctl-c, operf-record may get spurious errors, like
				 * broken pipe, etc.  We ignore these unless the user asks for
				 * debug output.
				 */
				if (!ctl_c || cverb << vdebug) {
					cerr <<  "operf-record process ended abnormally: "
							<< WEXITSTATUS(waitpid_status) << endl;
					rc = PERF_RECORD_ERROR;
				}
			} else if (WIFSIGNALED(waitpid_status)) {
				if (!ctl_c || cverb << vdebug) {
					cerr << "operf-record process killed by signal "
					     << WTERMSIG(waitpid_status) << endl;
					rc = PERF_RECORD_ERROR;
				}
			}
		}
	}
	return rc;
}

static end_code_t _run(void)
{
	int waitpid_status = 0;
	end_code_t rc = ALL_OK;
	bool kill_record = true;

	// Fork processes with signals blocked.
	sigset_t ss;
	sigfillset(&ss);
	sigprocmask(SIG_BLOCK, &ss, NULL);

	/* By default (unless the user specifies --lazy-conversion), the operf-record process
	 * writes the sample data to a pipe, from which the operf-read process reads.
	 */
	if (!operf_options::post_conversion && pipe(sample_data_pipe) < 0) {
		perror("Internal error: operf-record could not create pipe");
		_exit(EXIT_FAILURE);
	}

	if (start_profiling() < 0) {
		return PERF_RECORD_ERROR;
	}
	// parent continues here
	if (startApp)
		cverb << vdebug << "app " << app_PID << " is running" << endl;

	/* If we're not doing system wide profiling and no app is started, then
	 * there's no profile data to convert. So if this condition is NOT true,
	 * then we'll do the convert.
	 * Note that if --lazy-conversion is passed, then operf_options::post_conversion
	 * will be set, and we will defer conversion until after the operf-record
	 * process is done.
	 */
	if (!operf_options::post_conversion) {
		if (!(!app_started && !operf_options::system_wide)) {
			cverb << vdebug << "Forking read pid" << endl;
			if (pipe(operf_post_profiling_pipe) < 0) {
				perror("Internal error: operf-record could not create pipe");
				_exit(EXIT_FAILURE);
			}
			operf_read_pid = fork();
			if (operf_read_pid < 0) {
				perror("Internal error: fork failed");
				_exit(EXIT_FAILURE);
			} else if (operf_read_pid == 0) { // child process
				close(sample_data_pipe[1]);
				close(operf_post_profiling_pipe[1]);
				_set_basic_SIGINT_handler_for_child();
				convert_sample_data();
				_exit(EXIT_SUCCESS);
			}
			// parent
			close(sample_data_pipe[0]);
			close(sample_data_pipe[1]);
			close(operf_convert_record_write_pipe[0]);
			close(operf_convert_record_write_pipe[1]);
			close(operf_record_convert_write_pipe[0]);
			close(operf_record_convert_write_pipe[1]);
			close(operf_post_profiling_pipe[0]);
		}
	}

	set_signals_for_parent();
	if (startApp) {
		/* The user passed in a command or program name to start, so we'll need to do waitpid on that
		 * process.  However, while that user-requested process is running, it's possible we
		 * may get an error in the operf-record process.  If that happens, we want to know it right
		 * away so we can stop profiling and kill the user app.  Therefore, we must use WNOHANG
		 * on the waitpid call and bounce back and forth between the user app and the operf-record
		 * process, checking their status.  The profiled app may end normally, abnormally, or by way
		 * of ctrl-C.  The operf-record process should not end here, except abnormally.  The normal
		 * flow is:
		 *    1. profiled app ends or is stopped via ctrl-C
		 *    2. keep_trying is set to false, so we drop out of while loop and proceed to end of function
		 *    3. call _kill_operf_record_pid and _kill_operf_read_pid
		 */
		bool keep_trying = true;
		const char * app_process = "profiled app";
		const char * record_process = "operf-record process";
		waitpid_status = 0;
		cverb << vdebug << "going into waitpid on profiled app " << app_PID << endl;

		// We'll try the waitpid with WNOHANG once every 100 ms (100,000,000 nsecs).
		while (keep_trying) {
			pid_t the_pid = app_PID;
			int wait_rc;
			const char * the_process = app_process;
			bool trying_user_app = true;
			struct timespec ts_req;
			ts_req.tv_sec = 0;
			ts_req.tv_nsec = 100000000;

			(void)nanosleep(&ts_req, NULL);
again:
			if ((wait_rc = waitpid(the_pid, &waitpid_status, WNOHANG)) < 0) {
				keep_trying = false;
				if (errno == EINTR) {
					//  Ctrl-C will only kill the profiled app.  See the op_sig_stop signal handler.
					cverb << vdebug << "Caught ctrl-C.  Killed " << the_process << "." << endl;
				} else {
					cerr << "waitpid for " << the_process << " failed: " << strerror(errno) << endl;
					rc = trying_user_app ? APP_ABNORMAL_END : PERF_RECORD_ERROR;
				}
			} else if (wait_rc) {
				keep_trying = false;
				if (WIFEXITED(waitpid_status) && (!WEXITSTATUS(waitpid_status))) {
					cverb << vdebug << the_process << " ended normally." << endl;
				} else if (WIFEXITED(waitpid_status)) {
					cerr <<  the_process << " exited with the following status: "
					     << WEXITSTATUS(waitpid_status) << endl;
					rc = trying_user_app ? APP_ABNORMAL_END : PERF_RECORD_ERROR;
				}  else if (WIFSIGNALED(waitpid_status)) {
					if (WTERMSIG(waitpid_status) != SIGKILL) {
						cerr << the_process << " killed by signal "
						     << WTERMSIG(waitpid_status) << endl;
						rc = trying_user_app ? APP_ABNORMAL_END : PERF_RECORD_ERROR;
					}
				} else {
					keep_trying = true;
				}
			}
			if (trying_user_app && (rc == ALL_OK)) {
				trying_user_app = false;
				the_pid = operf_record_pid;
				the_process = record_process;
				goto again;
			} else if (rc != ALL_OK) {
				// If trying_user_app == true, implies profiled app ended; otherwise, operf-record process abended.
				if (!trying_user_app)
					kill_record = false;
			}
		}
	} else {
		// User passed in --pid or --system-wide
		cout << "operf: Press Ctl-c or 'kill -SIGINT " << getpid() << "' to stop profiling" << endl;
		cverb << vdebug << "going into waitpid on operf record process " << operf_record_pid << endl;
		if (waitpid(operf_record_pid, &waitpid_status, 0) < 0) {
			if (errno == EINTR) {
				cverb << vdebug << "Caught ctrl-C. Killing operf-record process . . ." << endl;
			} else {
				cerr << "waitpid errno is " << errno << endl;
				perror("waitpid for operf-record process failed");
				kill_record = false;
				rc = PERF_RECORD_ERROR;
			}
		} else {
			if (WIFEXITED(waitpid_status) && (!WEXITSTATUS(waitpid_status))) {
				cverb << vdebug << "waitpid for operf-record process returned OK" << endl;
			} else if (WIFEXITED(waitpid_status)) {
				kill_record = false;
				cerr <<  "operf-record process ended abnormally: "
				     << WEXITSTATUS(waitpid_status) << endl;
				rc = PERF_RECORD_ERROR;
			} else if (WIFSIGNALED(waitpid_status)) {
				kill_record = false;
				cerr << "operf-record process killed by signal "
				     << WTERMSIG(waitpid_status) << endl;
				rc = PERF_RECORD_ERROR;
			}
		}
	}
	if (kill_record) {
		if (operf_options::post_conversion)
			rc = _kill_operf_record_pid();
		else
			rc = _waitfor_operf_read_pid(_kill_operf_record_pid());
	} else {
		if (!operf_options::post_conversion)
			rc = _waitfor_operf_read_pid(rc);
	}

	return rc;
}

static void cleanup(void)
{
	free(app_name_SAVE);
	free(app_args);
	events.clear();
	verbose_string.clear();
	if (operf_options::post_conversion) {
		string cmd = "rm -f " + outputfile;
		if (system(cmd.c_str()) != 0)
			cerr << "Unable to remove " << outputfile << endl;
	}
}

static void _jitconv_complete(int val __attribute__((unused)))
{
	int child_status;
	pid_t the_pid = wait(&child_status);
	if (the_pid != jitconv_pid) {
		return;
	}
	jit_conversion_running = false;
	if (WIFEXITED(child_status) && (!WEXITSTATUS(child_status))) {
		cverb << vdebug << "JIT dump processing complete." << endl;
	} else {
		 if (WIFSIGNALED(child_status)) {
			 if (ctl_c)
				 cerr << "JIT conversion stopped by request of user via ctl-c" << endl;
			 else
				 cerr << "child received signal " << WTERMSIG(child_status) << endl;
		 } else {
			 cerr << "JIT dump processing exited abnormally: "
			      << WEXITSTATUS(child_status) << endl;
		 }
	}
}

static void _set_signals_for_convert(void)
{
	struct sigaction act;
	sigset_t ss;

	sigfillset(&ss);
	sigprocmask(SIG_UNBLOCK, &ss, NULL);

	act.sa_handler = _jitconv_complete;
	act.sa_flags = 0;
	sigemptyset(&act.sa_mask);
	sigaddset(&act.sa_mask, SIGCHLD);
	if (sigaction(SIGCHLD, &act, NULL)) {
		perror("operf: install of SIGCHLD handler failed: ");
		exit(EXIT_FAILURE);
	}
}

static void _do_jitdump_convert()
{
	int arg_num;
	unsigned long long end_time = 0ULL;
	struct timeval tv;
	char end_time_str[32];
	char opjitconv_path[PATH_MAX + 1];
	char * exec_args[9];

	jitconv_pid = fork();
	switch (jitconv_pid) {
	case -1:
		perror("Error forking JIT dump process!");
		break;
	case 0: {
		const char * jitconv_pgm = "opjitconv";
		const char * debug_option = "-d";
		const char * non_root_user = "--non-root";
		const char * delete_jitdumps = "--delete-jitdumps";
		const char * sess_dir =  "--session-dir";
		gettimeofday(&tv, NULL);
		end_time = tv.tv_sec;
		sprintf(end_time_str, "%llu", end_time);
		sprintf(opjitconv_path, "%s/%s", OP_BINDIR, jitconv_pgm);
		arg_num = 0;
		exec_args[arg_num++] = (char *)jitconv_pgm;
		if (cverb << vdebug)
			exec_args[arg_num++] = (char *)debug_option;
		if (my_uid != 0)
			exec_args[arg_num++] = (char *)non_root_user;
		exec_args[arg_num++] = (char *)delete_jitdumps;
		exec_args[arg_num++] = (char *)sess_dir;
		exec_args[arg_num++] = (char *)operf_options::session_dir.c_str();
		exec_args[arg_num++] = start_time_str;
		exec_args[arg_num++] = end_time_str;
		exec_args[arg_num] = (char *) NULL;
		execvp(opjitconv_path, exec_args);
		fprintf(stderr, "Failed to exec %s: %s\n",
		        exec_args[0], strerror(errno));
		/* We don't want any cleanup in the child */
		_exit(EXIT_FAILURE);
		break;
	}
	default: // parent
		jit_conversion_running = true;
		break;
	}

}

static int __delete_old_previous_sample_data(const char *fpath,
                                const struct stat *sb  __attribute__((unused)),
                                int tflag  __attribute__((unused)),
                                struct FTW *ftwbuf __attribute__((unused)))
{
	if (remove(fpath)) {
		perror("sample data removal error");
		return FTW_STOP;
	} else {
		return FTW_CONTINUE;
	}
}

/* Read perf_events sample data written by the operf-record process through
 * the sample_data_pipe or file (dependent on 'lazy-conversion' option)
 * and convert the perf format sample data to to oprofile format sample files.
 *
 * If not invoked with --lazy-conversion option, this function is executed by
 * the "operf-read" child process.  If user does a ctrl-C, the parent will
 * execute _kill_operf_read_pid which will try to allow the conversion process
 * to complete, waiting 5 seconds before it forcefully kills the operf-read
 * process via 'kill SIGUSR1'.
 *
 * But if --lazy-conversion option is used, then it's the parent process that's
 * running convert_sample_data.  If the user does a ctrl-C during this procedure,
 * the ctrl-C is handled via op_sig_stop which essentially does nothing to stop
 * the conversion procedure, which in general is fine.  On the very rare chance
 * that the procedure gets stuck (hung) somehow, the user will have to do a
 * 'kill -KILL'.
 */
static void convert_sample_data(void)
{
	int inputfd;
	string inputfname;
	int rc = EXIT_SUCCESS;
	string current_sampledir = samples_dir + "/current/";
	string previous_sampledir = samples_dir + "/previous";
	string stats_dir = "";
	current_sampledir.copy(op_samples_current_dir, current_sampledir.length(), 0);

	if (!app_started && !operf_options::system_wide)
		return;

	if (!operf_options::append) {
                int flags = FTW_DEPTH | FTW_ACTIONRETVAL;
		errno = 0;
		if (nftw(previous_sampledir.c_str(), __delete_old_previous_sample_data, 32, flags) !=0 &&
				errno != ENOENT) {
			cerr << "Unable to remove old sample data at " << previous_sampledir << "." << endl;
			if (errno)
				cerr << strerror(errno) << endl;
			rc = EXIT_FAILURE;
			goto out;
		}
		if (rename(current_sampledir.c_str(), previous_sampledir.c_str()) < 0) {
			if (errno && (errno != ENOENT)) {
				cerr << "Unable to move old profile data to " << previous_sampledir << endl;
				cerr << strerror(errno) << endl;
				rc = EXIT_FAILURE;
				goto out;
			}
		}
	}
	rc = mkdir(current_sampledir.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
	if (rc && (errno != EEXIST)) {
		cerr << "Error trying to create " << current_sampledir << " dir." << endl;
		perror("mkdir failed with");
		rc = EXIT_FAILURE;
		goto out;
	}

	if (operf_options::post_conversion) {
		inputfd = -1;
		inputfname = outputfile;
	} else {
		inputfd = sample_data_pipe[0];
		inputfname = "";
	}
	close(operf_record_convert_write_pipe[1]);
	close(operf_convert_record_write_pipe[0]);
	operfRead.init(inputfd, inputfname, current_sampledir, cpu_type,
	               operf_options::system_wide, operf_convert_record_write_pipe[1],
	               operf_record_convert_write_pipe[0], operf_post_profiling_pipe[0]);
	if ((rc = operfRead.readPerfHeader()) < 0) {
		if (rc != OP_PERF_HANDLED_ERROR)
			cerr << "Error: Cannot create read header info for sample data " << endl;
		rc = EXIT_FAILURE;
		goto out;
	}
	cverb << vdebug << "Successfully read header info for sample data " << endl;
	if (operfRead.is_valid()) {
		try {
			unsigned int num = operfRead.convertPerfData();
			cverb << vdebug << "operf_read: Total bytes received from operf_record process: " << dec << num << endl;
		} catch (const runtime_error & e) {
			cerr << "Caught runtime error from operf_read::convertPerfData" << endl;
			cerr << e.what() << endl;
			rc = EXIT_FAILURE;
			goto out;
		}
	}

	_set_signals_for_convert();
	cverb << vdebug << "Calling _do_jitdump_convert" << endl;
	_do_jitdump_convert();
	while (jit_conversion_running) {
		sleep(1);
	}
out:
	if (!operf_options::post_conversion)
		_exit(rc);
}


static void _process_session_dir(void)
{
	if (operf_options::session_dir.empty()) {
		char * cwd = NULL;
		int rc;
		cwd = (char *) xmalloc(PATH_MAX);
		// set default session dir
		cwd = getcwd(cwd, PATH_MAX);
		if (cwd == NULL) {
			perror("Error calling getcwd");
			exit(EXIT_FAILURE);
		}
		operf_options::session_dir = cwd;
		operf_options::session_dir +="/oprofile_data";
		samples_dir = operf_options::session_dir + "/samples";
		free(cwd);
		rc = mkdir(operf_options::session_dir.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
		if (rc && (errno != EEXIST)) {
			cerr << "Error trying to create " << operf_options::session_dir << " dir." << endl;
			perror("mkdir failed with");
			exit(EXIT_FAILURE);
		}
		rc = mkdir(samples_dir.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
		if (rc && (errno != EEXIST)) {
			cerr << "Error trying to create " << samples_dir << " dir." << endl;
			perror("mkdir failed with");
			exit(EXIT_FAILURE);
		}
	} else {
		struct stat filestat;
		int rc;
		if (stat(operf_options::session_dir.c_str(), &filestat)) {
			perror("stat operation on passed session-dir failed");
			exit(EXIT_FAILURE);
		}
		if (!S_ISDIR(filestat.st_mode)) {
			cerr << "Passed session-dir " << operf_options::session_dir
			     << " is not a directory" << endl;
			exit(EXIT_FAILURE);
		}
		samples_dir = operf_options::session_dir + "/samples";
		rc = mkdir(samples_dir.c_str(), S_IRWXU);
		if (rc && (errno != EEXIST)) {
			cerr << "Error trying to create " << samples_dir << " dir." << endl;
			perror("mkdir failed with");
			exit(EXIT_FAILURE);
		}
	}

	cverb << vdebug << "Using samples dir " << samples_dir << endl;
}

bool _get_vmlinux_address_info(vector<string> args, string cmp_val, string &str)
{
	bool found = false;
	child_reader reader("objdump", args);
	if (reader.error()) {
		cerr << "An error occurred while trying to get vmlinux address info:\n\n";
		cerr << reader.error_str() << endl;
		exit(EXIT_FAILURE);
	}

	while (reader.getline(str)) {
		if (str.find(cmp_val.c_str()) != string::npos) {
			found = true;
			break;
		}
	}
	// objdump always returns SUCCESS so we must rely on the stderr state
	// of objdump. If objdump error message is cryptic our own error
	// message will be probably also cryptic
	ostringstream std_err;
	ostringstream std_out;
	reader.get_data(std_out, std_err);
	if (std_err.str().length()) {
		cerr << "An error occurred while getting vmlinux address info:\n\n";
		cerr << std_err.str() << endl;
		// If we found the string we were looking for in objdump output,
		// treat this as non-fatal error.
		if (!found)
			exit(EXIT_FAILURE);
	}

	// force error code to be acquired
	reader.terminate_process();

	// required because if objdump stop by signal all above things suceeed
	// (signal error message are not output through stdout/stderr)
	if (reader.error()) {
		cerr << "An error occur during the execution of objdump to get vmlinux address info:\n\n";
		cerr << reader.error_str() << endl;
		if (!found)
			exit(EXIT_FAILURE);
	}
	return found;
}

static bool _add_kernel_entry(string start_addr_str, string end_addr_str, string image_name)
{
	string str, start_end;
	unsigned long long start_addr, end_addr;

	errno = 0;
	start_addr = strtoull(start_addr_str.c_str(), NULL, 16);
	if (errno) {
		cerr << "Unable to convert kallsyms start address " << start_addr_str
		     << " to a valid hex value. errno is " << strerror(errno) << endl;
		return false;
	}

	errno = 0;
	end_addr =  strtoull(end_addr_str.c_str(), NULL, 16);
	if (errno) {
		cerr << "Unable to convert kallsyms end address " << end_addr_str
		     << " to a valid hex value. errno is " << strerror(errno) << endl;
		return false;
	}

	if ((start_addr == 0) || (end_addr == 0)) {
		no_vmlinux = true;
		cerr << "Kernel profiling is not possible with current system "
		     << "config." << endl
		     << "Set /proc/sys/kernel/kptr_restrict to 0 to "
		     << "collect kernel samples." << endl;
		return false;
	}

	/* Do not assign kernel_start and kernel_end until the addresses
	 * have been validated.
	 */
	kernel_start = start_addr;
	kernel_end = end_addr;

	start_end = start_addr_str;
	start_end.append(",");
	start_end.append(end_addr_str);

	no_vmlinux = false;  // set to false or the operf_get_vmlinux_name() returns "no-vmlinux"
	operf_create_vmlinux(image_name.c_str(), start_end.c_str());
	return true;
}

static bool _process_kallsyms(void)
{
	ifstream  infile;
	string start_addr_str, end_addr_str;
	string address_str;
	string str, start_end;
	std::string line;
	stringstream iss;
	string name;
	string kall_syms_file = KALL_SYM_FILE;
	char type;
	int rtn = false;

	infile.open(kall_syms_file.c_str());
	if (!infile) {
		cerr << "Internal Error: Could not open kallsyms file." << endl;
		return false;
	}

	start_addr_str.clear();
	end_addr_str.clear();

	/* get the start and end  address of the kernel address range */
	while ( !infile.eof() ) {
		getline(infile, line);
		iss.clear();
		iss << line;
		address_str.clear();

		iss >> address_str;
		iss >> type;
		iss >> name;

		if (strncmp(name.c_str(), KERN_ADDR_SPACE_START_SYMBOL,
			    strlen(name.c_str())) == 0) {
			/* found the symbol for the start of the kernel
			 * address space.
			*/
			start_addr_str.assign(address_str);
		}

		if (strncmp(name.c_str(), KERN_ADDR_SPACE_END_SYMBOL,
			    strlen(name.c_str())) == 0) {
			/* found the symbol for the end of the kernel
			 * address space.
			 */
			end_addr_str.assign(address_str);
			rtn = _add_kernel_entry(start_addr_str,
						  end_addr_str, KALL_SYM_FILE);
			break;
		}
	}
	infile.close();
	return rtn;
}

string _process_vmlinux(string vmlinux_file)
{
	vector<string> args;
	char start[17], end[17];
	string str, start_end;
	bool found;
	int ret;

	no_vmlinux = false;
	args.push_back("-h");
	args.push_back(vmlinux_file);
	if ((found = _get_vmlinux_address_info(args, " .text", str))) {
		cverb << vmisc << str << endl;
		ret = sscanf(str.c_str(), " %*s %*s %*s %s", start);
	}
	if (!found || ret != 1){
		cerr << "Unable to obtain vmlinux start address." << endl;
		cerr << "The specified vmlinux file (" << vmlinux_file << ") "
		     << "does not seem to be valid." << endl;
		cerr << "Make sure you are using a non-compressed image file "
		     << "(e.g. vmlinux not vmlinuz)" << endl;
		exit(EXIT_FAILURE);
	}

	args.clear();
	args.push_back("-t");
	args.push_back(vmlinux_file);
	if ((found = _get_vmlinux_address_info(args, " _etext", str))) {
		cverb << vmisc << str << endl;
		ret = sscanf(str.c_str(), "%s", end);
	}
	if (!found || ret != 1){
		cerr << "Unable to obtain vmlinux end address." << endl;
		cerr << "The specified vmlinux file (" << vmlinux_file << ") "
		     << "does not seem to be valid." << endl;
		cerr << "Make sure you are using a non-compressed image file "
		     << "(e.g. vmlinux not vmlinuz)" << endl;
		exit(EXIT_FAILURE);
	}

	errno = 0;
	kernel_start = strtoull(start, NULL, 16);
	if (errno) {
		cerr << "Unable to convert vmlinux start address " << start
		     << " to a valid hex value. errno is " << strerror(errno) << endl;
		exit(EXIT_FAILURE);
	}
	errno = 0;
	kernel_end =  strtoull(end, NULL, 16);
	if (errno) {
		cerr << "Unable to convert vmlinux end address " << start
		     << " to a valid hex value. errno is " << strerror(errno) << endl;
		exit(EXIT_FAILURE);
	}

	start_end = start;
	start_end.append(",");
	start_end.append(end);
	return start_end;
}

static void _print_valid_verbose_options(void)
{
	cerr << "Valid verbosity options are: ";
	for (unsigned i = 0; i < (NUM_VERBOSE_OPTIONS - 1); i++)
		cerr << valid_verbose_vals[i] << ",";
	cerr << valid_verbose_vals[NUM_VERBOSE_OPTIONS - 1] << endl;
}

static bool _validate_verbose_args(char * verbosity)
{
	bool valid_verbosity = true;

	char * verbose_cand = strtok(verbosity, ",");
	do {
		unsigned i;
		for (i = 0; i < (NUM_VERBOSE_OPTIONS); i++) {
			if (!strcmp(verbose_cand, valid_verbose_vals[i])) {
				verbose_string.push_back(verbose_cand);
				break;
			}
		}
		if (i == NUM_VERBOSE_OPTIONS) {
			valid_verbosity = false;
			cerr << "Verbosity argument " << verbose_cand << " is not valid." << endl;
			_print_valid_verbose_options();
		}
	} while ((verbose_cand = strtok(NULL, ",")) && valid_verbosity);
	return valid_verbosity;
}

static int _process_operf_and_app_args(int argc, char * const argv[])
{
	bool keep_trying = true;
	int idx_of_non_options = 0;
	char * prev_env = getenv("POSIXLY_CORRECT");
	setenv("POSIXLY_CORRECT", "1", 0);
	while (keep_trying) {
		int option_idx = 0;
		int c = getopt_long(argc, argv, short_options, long_options, &option_idx);
		switch (c) {
		char * endptr;
		char * event;

		case -1:
			if (optind != argc) {
				idx_of_non_options = optind;
			}
			keep_trying = false;
			break;
		case '?':
			cerr << "non-option detected at optind " << optind << endl;
			keep_trying = false;
			idx_of_non_options = -1;
			break;
		case 'V':
			if (!_validate_verbose_args(optarg))
				__print_usage_and_exit("NULL");
			break;
		case 'd':
			operf_options::session_dir = optarg;
			break;
		case 'k':
			operf_options::vmlinux = optarg;
			break;
		case 'g':
			operf_options::callgraph = true;
			break;
		case 's':
			operf_options::system_wide = true;
			break;
		case 'a':
			operf_options::append = true;
			break;
		case 'p':
			operf_options::pid = strtol(optarg, &endptr, 10);
			if ((endptr >= optarg) && (endptr <= (optarg + strlen(optarg) - 1)))
				__print_usage_and_exit("operf: Invalid numeric value for --pid option.");
			break;
		case 'e':
			event = strtok(optarg, ",");
			do {
				operf_options::evts.insert(event);
			} while ((event = strtok(NULL, ",")));
			break;
		case 'c':
			operf_options::separate_cpu = true;
			break;
		case 't':
			operf_options::separate_thread = true;
			break;
		case 'l':
			operf_options::post_conversion = true;
			break;
		case 'h':
			__print_usage_and_exit(NULL);
			break;
		case 'u':
			__print_usage_and_exit(NULL);
			break;
		case 'v':
			cout << argv[0] << ": " << PACKAGE << " " << VERSION << " compiled on " << __DATE__
			     << " " << __TIME__ << endl;
			exit(EXIT_SUCCESS);
			break;
		default:
			__print_usage_and_exit("unexpected end of arg parsing");
		}
	}

	if (prev_env == NULL)
		unsetenv("POSIXLY_CORRECT");

	return idx_of_non_options;
}

static void process_args(int argc, char * const argv[])
{
	int non_options_idx  = _process_operf_and_app_args(argc, argv);

	if (non_options_idx < 0) {
		__print_usage_and_exit(NULL);
	} else if ((non_options_idx) > 0) {
		if (operf_options::pid || operf_options::system_wide)
			__print_usage_and_exit(NULL);

		app_name = (char *) xmalloc(strlen(argv[non_options_idx]) + 1);
		strcpy(app_name, argv[non_options_idx]);
		// Note 1: app_args[0] is placeholder for app_fname (filled in later).
		// Note 2: app_args[<end>] is set to NULL (required by execvp)
		if (non_options_idx < (argc -1)) {
			app_args = (char **) xmalloc((sizeof *app_args) *
			                             (argc - non_options_idx + 1));
			for(int i = non_options_idx + 1; i < argc; i++) {
				app_args[i - non_options_idx] = argv[i];
			}
			app_args[argc - non_options_idx] = NULL;
		} else {
			app_args = (char **) xmalloc((sizeof *app_args) * 2);
			app_args[1] = NULL;
		}
		if (op_validate_app_name(&app_name, &app_name_SAVE) < 0) {
			__print_usage_and_exit(NULL);
		}
	} else {  // non_options_idx == 0
		if (operf_options::pid) {
			if (operf_options::system_wide)
				__print_usage_and_exit(NULL);
			app_PID = operf_options::pid;
		} else if (operf_options::system_wide) {
			app_PID = -1;
		} else {
			__print_usage_and_exit(NULL);
		}
	}
	/*  At this point, we know which of the three kinds of profiles the user requested:
	 *    - profile app by name
	 *    - profile app by PID
	 *    - profile whole system
	 */

	if (!verbose::setup(verbose_string)) {
		cerr << "unknown --verbose= options\n";
		exit(EXIT_FAILURE);
	}

	_process_session_dir();
	if (operf_options::post_conversion)
		outputfile = samples_dir + "/" + DEFAULT_OPERF_OUTFILE;

	if (operf_options::evts.empty()) {
		// Use default event
		op_get_default_event(operf_options::callgraph);
	} else  {
		op_process_events_list(operf_options::evts, true, operf_options::callgraph);
	}
	op_nr_events = events.size();

	if (operf_options::vmlinux.empty()) {
		/* get the begining and end of the kernel addr space */
		if (!_process_kallsyms()) {
			/* Do not have permission to read
			 * kernel addresses from /proc/kallsyms.
			 */
			no_vmlinux = true;
			operf_create_vmlinux(NULL, NULL);
		}
	} else {
		string startEnd = _process_vmlinux(operf_options::vmlinux);
		operf_create_vmlinux(operf_options::vmlinux.c_str(), startEnd.c_str());
	}
	if (operf_options::pid && !operf_options::post_conversion)
		track_new_forks = true;
	else
		track_new_forks = false;

	return;
}

static void _precheck_permissions_to_samplesdir(string sampledir, bool for_current)
{
	/* Pre-check to make sure we have permission to remove old sample data
	 * or to create new sample data in the specified sample data directory.
	 * If the user wants us to remove old data, we don't actually do it now,
	 * since the profile session may fail for some reason or the user may do ctl-c.
	 * We should exit without unnecessarily removing the old sample data as
	 * the user may expect it to still be there after an aborted run.
	 */
	string sampledir_testfile = sampledir + "/.xxxTeStFiLe";
	ofstream afile;
	errno = 0;
	afile.open(sampledir_testfile.c_str());
	if (!afile.is_open() && (errno != ENOENT)) {
		if (operf_options::append && for_current)
			cerr << "Unable to write to sample data directory at "
			     << sampledir << "." << endl;
		else
			cerr << "Unable to remove old sample data at "
			     << sampledir << "." << endl;
		if (errno)
			cerr << strerror(errno) << endl;
		cerr << "Try a manual removal of " << sampledir << endl;
		cleanup();
		exit(1);
	}
	afile.close();

}

int main(int argc, char * const argv[])
{
	int rc;
	int perf_event_paranoid = op_get_sys_value("/proc/sys/kernel/perf_event_paranoid");

	my_uid = geteuid();
	throttled = false;
	rc = op_check_perf_events_cap(use_cpu_minus_one);
	if (rc == EACCES) {
		/* Early perf_events kernels required the cpu argument to perf_event_open
		 * to be '-1' when setting up to profile a single process if 1) the user is
		 * not root; and 2) perf_event_paranoid is > 0.  An EACCES error would be
		 * returned if passing '0' or greater for the cpu arg and the above criteria
		 * was not met.  Unfortunately, later kernels turned this requirement around
		 * such that the passed cpu arg must be '0' or greater when the user is not
		 * root.
		 *
		 * We don't really have a good way to check whether we're running on such an
		 * early kernel except to try the perf_event_open with different values to see
		 * what works.
		 */
		if (my_uid != 0 && perf_event_paranoid > 0) {
			use_cpu_minus_one = true;
			rc = op_check_perf_events_cap(use_cpu_minus_one);
		}
	}
	if (rc == EBUSY) {
		cerr << "Performance monitor unit is busy.  Ensure that no other profilers are running on the system." << endl
		     << "Note: For example, the obsolete opcontrol profiler (available in earlier oprofile releases)" << endl
		     << "does not allow other perforrmance tools to run simultaneously. To check for this, look for the" << endl
		     << "'oprofiled' process using the 'ps' command." << endl;
	} else if (rc == ENOSYS) {
		cerr << "Your kernel does not implement a required syscall"
		     << " for the operf program." << endl;
	} else if (rc == ENOENT) {
		cerr << "Your kernel's Performance Events Subsystem does not support"
		     << " your processor type." << endl;
	} else if (rc) {
		cerr << "Unexpected error running operf: " << strerror(rc) << endl;
	}

	if (rc)
		exit(1);

	cpu_type = op_get_cpu_type();
	if (cpu_type == CPU_NO_GOOD) {
		cerr << "Unable to ascertain cpu type.  Exiting." << endl;
		cleanup();
		exit(1);
	}

	if (cpu_type == CPU_TIMER_INT) {
		cerr << "CPU type 'timer' was detected, but operf does not support timer mode." << endl
		     << "Ensure the obsolete opcontrol profiler (available in earlier oprofile releases)" << endl
		     << "is not running on the system.  To check for this, look for the file" << endl
		     << "/dev/oprofile/cpu_type; if this file exists, locate the pre-1.0 oprofile" << endl
		     << "installation, and use its 'opcontrol' command with the --deinit option." << endl;
		cleanup();
		exit(1);
	}

	cpu_speed = op_cpu_frequency();
	process_args(argc, argv);

	if (operf_options::system_wide && ((my_uid != 0) && (perf_event_paranoid > 0))) {
		cerr << "To do system-wide profiling, either you must be root or" << endl;
		cerr << "/proc/sys/kernel/perf_event_paranoid must be set to 0 or -1." << endl;
		cleanup();
		exit(1);
	}

	if (my_uid != 0) {
		bool for_current = true;
		string current_sampledir = samples_dir + "/current";
		_precheck_permissions_to_samplesdir(current_sampledir, for_current);
		if (!operf_options::append) {
			string previous_sampledir = samples_dir + "/previous";
			for_current = false;
			_precheck_permissions_to_samplesdir(previous_sampledir, for_current);
		}
	}
	kptr_restrict = op_get_sys_value("/proc/sys/kernel/kptr_restrict");
	end_code_t run_result;
	if ((run_result = _run())) {
		if (startApp && app_started && (run_result != APP_ABNORMAL_END)) {
			int rc;
			cverb << vdebug << "Killing profiled app . . ." << endl;
			rc = kill(app_PID, SIGKILL);
			if (rc) {
				if (errno == ESRCH)
					cverb << vdebug
					      << "Unable to kill profiled app because it has already ended"
					      << endl;
				else
					perror("Attempt to kill profiled app failed.");
			}
		}
		if ((run_result == PERF_RECORD_ERROR) || (run_result == PERF_BOTH_ERROR)) {
			cerr <<  "Error running profiler" << endl;
		} else if (run_result == PERF_READ_ERROR) {
			cerr << "Error converting operf sample data to oprofile sample format" << endl;
		} else {
			cerr << "WARNING: Profile results may be incomplete due to to abend of profiled app." << endl;
		}
	} else {
		cerr << endl << "Profiling done." << endl;
	}
	if (operf_options::post_conversion) {
		if (!(!app_started && !operf_options::system_wide))
			convert_sample_data();
	}
	cleanup();
	return run_result;;
}
