/**
 * @file ocount.cpp
 * Tool for event counting using the new Linux Performance Events Subsystem.
 *
 * @remark Copyright 2013 OProfile authors
 * @remark Read the file COPYING
 *
 * Created on: May 21, 2013
 * @author Maynard Johnson
 * (C) Copyright IBM Corp. 2013
 *
 */

#include "config.h"

#include <iostream>
#include <stdexcept>
#include <fstream>
#include <vector>
#include <set>

#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <sys/time.h>
#include <math.h>

#include "op_pe_utils.h"
#include "ocount_counter.h"
#include "op_cpu_type.h"
#include "op_cpufreq.h"
#include "operf_event.h"
#include "cverb.h"
#include "op_libiberty.h"

// Globals
char * app_name = NULL;
bool use_cpu_minus_one = false;
std::vector<operf_event_t> events;
op_cpu cpu_type;

#define OCOUNT_MSECS_PER_SEC 1000
// Current implementation supports a display interval of 100 ms
#define OCOUNT_DSP_INTVL_MSECS 100
#define OCOUNT_DSP_INTVLS_PER_SEC (OCOUNT_MSECS_PER_SEC/OCOUNT_DSP_INTVL_MSECS)
#define OCOUNT_NSECS_PER_MSEC 1000000
#define OCOUNT_NSECS_PER_DSP_INTVL (OCOUNT_NSECS_PER_MSEC * OCOUNT_DSP_INTVL_MSECS)

static char * app_name_SAVE = NULL;
static char ** app_args = NULL;
static bool app_started;
static bool startApp;
static bool stop = false;
static std::ofstream outfile;
static pid_t my_uid;
static double cpu_speed;
static ocount_record * orecord;
static pid_t app_PID = -1;

using namespace std;
using namespace op_pe_utils;


typedef enum END_CODE {
	ALL_OK = 0,
	APP_ABNORMAL_END =  1,
	PERF_RECORD_ERROR = 2,
	PERF_READ_ERROR   = 4,
	PERF_BOTH_ERROR   = 8
} end_code_t;

namespace ocount_options {
bool verbose;
bool system_wide;
vector<pid_t> processes;
vector<pid_t> threads;
vector<int> cpus;
string outfile;
bool separate_cpu;
bool separate_thread;
set<string> evts;
bool csv_output;
long display_interval;
long num_intervals;
}


static enum op_runmode runmode = OP_MAX_RUNMODE;
static string runmode_options[] = { "<command> [command-args]", "--system-wide", "--cpu-list",
                                    "--process-list", "--thread-list"
};


struct option long_options [] =
{
 {"verbose", no_argument, NULL, 'V'},
 {"system-wide", no_argument, NULL, 's'},
 {"cpu-list", required_argument, NULL, 'C'},
 {"process-list", required_argument, NULL, 'p'},
 {"thread-list", required_argument, NULL, 'r'},
 {"events", required_argument, NULL, 'e'},
 {"output-file", required_argument, NULL, 'f'},
 {"separate-cpu", no_argument, NULL, 'c'},
 {"separate-thread", no_argument, NULL, 't'},
 {"brief-format", no_argument, NULL, 'b'},
 {"time-interval", required_argument, NULL, 'i'},
 {"help", no_argument, NULL, 'h'},
 {"usage", no_argument, NULL, 'u'},
 {"version", no_argument, NULL, 'v'},
 {NULL, 9, NULL, 0}
};

const char * short_options = "VsC:p:r:e:f:ctbi:huv";

static void cleanup(void)
{
	free(app_name_SAVE);
	free(app_args);
	events.clear();
	if (!ocount_options::outfile.empty())
		outfile.close();
}


// Signal handler for main (parent) process.
static void op_sig_stop(int sigval __attribute__((unused)))
{
	// Received a signal to quit, so we need to stop the
	// app being counted.
	size_t dummy __attribute__ ((__unused__));
	stop = true;
	if (cverb << vdebug)
		dummy = write(1, "in op_sig_stop\n", 15);
	if (startApp)
		kill(app_PID, SIGKILL);
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
		perror("ocount: install of SIGINT handler failed: ");
		exit(EXIT_FAILURE);
	}
}


static void __print_usage_and_exit(const char * extra_msg)
{
	if (extra_msg)
		cerr << extra_msg << endl;
	cerr << "usage: ocount [ options ] [ --system-wide | -p <pids> | -r <tids> | -C <cpus> | [ command [ args ] ] ]" << endl;
	cerr << "See ocount man page for details." << endl;
	exit(EXIT_FAILURE);
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

static int app_ready_pipe[2], start_app_pipe[2];

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
	execvp(app_name, app_args);
	cerr <<  "Failed to exec " << app_fname << " " << arg_str << ": " << strerror(errno) << endl;
	/* We don't want any cleanup in the child */
	_exit(EXIT_FAILURE);

}

bool start_counting(void)
{
	vector<pid_t> proc_list; // May contain processes or threads

	// The only process that should return from this function is the process
	// which invoked it.  Any forked process must do _exit() rather than return().

	startApp = runmode == OP_START_APP;

	if (startApp) {
		if (pipe(app_ready_pipe) < 0 || pipe(start_app_pipe) < 0) {
			perror("Internal error: ocount-record could not create pipe");
			return false;
		}
		app_PID = fork();
		if (app_PID < 0) {
			perror("Internal error: fork failed");
			return false;
		} else if (app_PID == 0) { // child process for exec'ing app
			run_app();
		}
	}

	// parent
	int startup;
	if (startApp) {
		if (read(app_ready_pipe[0], &startup, sizeof(startup)) == -1) {
			perror("Internal error on app_ready_pipe");
			return false;
		} else if (startup != 1) {
			cerr << "app is not ready to start; exiting" << endl;
			return false;
		}
		proc_list.push_back(app_PID);
	} else if (!ocount_options::threads.empty()) {
		proc_list = ocount_options::threads;
	} else if (!ocount_options::processes.empty()) {
		proc_list = ocount_options::processes;
	}

	if (startApp) {
		// Tell app_PID to start the app
		cverb << vdebug << "telling child to start app" << endl;
		if (write(start_app_pipe[1], &startup, sizeof(startup)) < 0) {
			perror("Internal error on start_app_pipe");
			return false;
		}
		app_started = true;
	}

	orecord = new ocount_record(runmode, events, ocount_options::display_interval ? true : false);
	bool ret;
	switch (runmode) {
	case OP_START_APP:
		ret = orecord->start_counting_app_process(app_PID);
		break;
	case OP_SYSWIDE:
		ret = orecord->start_counting_syswide();
		break;
	case OP_CPULIST:
		ret = orecord->start_counting_cpulist(ocount_options::cpus);
		break;
	case OP_PROCLIST:
		ret = orecord->start_counting_tasklist(ocount_options::processes, false);
		break;
	case OP_THREADLIST:
		ret = orecord->start_counting_tasklist(ocount_options::threads, true);
		break;
	default:
		ret = false;
		break;   // impossible to get here, since we validate runmode prior to this point
	}
	if (!orecord->get_valid()) {
		/* If valid is false, it means that one of the "known" errors has
		 * occurred:
		 *   - monitored process has already ended
		 *   - passed PID was invalid
		 *   - device or resource busy
		 */
		cverb << vdebug << "ocount record init failed" << endl;
		ret = false;
	}

	return ret;
}

static void do_results(ostream & out)
{
	try {
		orecord->output_results(out, ocount_options::separate_cpu | ocount_options::separate_thread,
		                        ocount_options::csv_output);
	} catch (const runtime_error & e) {
		cerr << "Caught runtime error from ocount_record::output_results" << endl;
		cerr << e.what() << endl;
		cleanup();
		exit(EXIT_FAILURE);
	}
}

end_code_t _get_waitpid_status(int waitpid_status, int wait_rc)
{
	end_code_t rc = ALL_OK;
	if (wait_rc < 0) {
		if (errno == EINTR) {
			//  Ctrl-C will only kill the monitored app.  See the op_sig_stop signal handler.
			cverb << vdebug << "Caught ctrl-C.  Killed app process." << endl;
		} else {
			cerr << "waitpid for app process failed: " << strerror(errno) << endl;
			rc = APP_ABNORMAL_END;
		}
	} else if (wait_rc) {
		if (WIFEXITED(waitpid_status) && (!WEXITSTATUS(waitpid_status))) {
			cverb << vdebug << "app process ended normally." << endl;
		} else if (WIFEXITED(waitpid_status)) {
			cerr << "app process exited with the following status: "
					<< WEXITSTATUS(waitpid_status) << endl;
			rc = APP_ABNORMAL_END;
		}  else if (WIFSIGNALED(waitpid_status)) {
			if (WTERMSIG(waitpid_status) != SIGKILL) {
				cerr << "app process killed by signal "
						<< WTERMSIG(waitpid_status) << endl;
				rc = APP_ABNORMAL_END;
			}
		}
	}
	return rc;
}

end_code_t _wait_for_app(ostream & out)
{
	int wait_rc;
	end_code_t rc = ALL_OK;
	int waitpid_status = 0;

	bool done = false;
	cverb << vdebug << "going into waitpid on monitored app " << app_PID << endl;
	if (ocount_options::display_interval) {
		long number_intervals = ocount_options::num_intervals;
		do {
			struct timeval mytime;
			unsigned int countdown, dsp_intvl_in_100ms_units = 0;
			struct timespec ts_req;

			/* The display_interval is in milliseconds; but at this time, we only allow
			 * 100ms granularity. */
			dsp_intvl_in_100ms_units = (int)round(((double)ocount_options::display_interval/
					OCOUNT_DSP_INTVL_MSECS));
			if (dsp_intvl_in_100ms_units == 0)
				dsp_intvl_in_100ms_units = 1; // special case of rounding up; prevent 0 time interval
			countdown = dsp_intvl_in_100ms_units;
			cverb << vdebug << "Actual display interval used: " << dsp_intvl_in_100ms_units
					<< "x100ms" << endl;
			ts_req.tv_nsec = countdown * OCOUNT_NSECS_PER_DSP_INTVL;
			ts_req.tv_sec = 0;
			while (countdown) {
				/* We want to avoid the scenario where, say, the user requests a
				 * 10 second time interval and the app being counted ends
				 * immediately after we call sleep().  If we called sleep() with
				 * the full time interval, we'd be sleeping unnecessarily for
				 * 10 seconds.  So, for any time interval of one second or longer,
				 * we do a one second sleep(), wake up and check if the app is
				 * still alive.
				 */
				if (countdown >= OCOUNT_DSP_INTVLS_PER_SEC) {
					countdown -= OCOUNT_DSP_INTVLS_PER_SEC;
					sleep(1);
					if (countdown && (countdown < OCOUNT_DSP_INTVLS_PER_SEC)) {
						/* The next time through the loop, we'll be taking the
						 * 'else' leg of this if-statement -- i.e., we will have
						 * finished the specified time interval, modulo
						 * OCOUNT_DSP_INTVLS_PER_SEC. So we set sleep time
						 * accordingly and force dsp_intvl_in_100ms_units to be
						 * equal to the current countdown value so the "-=" op
						 * below makes the countdown finally zero.
						 */
						ts_req.tv_nsec = countdown * OCOUNT_NSECS_PER_DSP_INTVL;
						dsp_intvl_in_100ms_units  = countdown;
					}

				} else {
					/* We don't bother keeping track of remaining time, since signals delivered
					 * to this process will be rare (see nanosleep man page). The real time (in
					 * the supported granularity of 100 ms) is printed out for each time interval,
					 * so we leave it to the user to note any time gaps.
					 */
					(void)nanosleep(&ts_req, NULL);
					countdown -= dsp_intvl_in_100ms_units;
				}
				if (countdown == 0) {
					if (gettimeofday(&mytime, NULL) < 0) {
						cleanup();
						perror("gettimeofday");
						exit(EXIT_FAILURE);
					}
					if (!ocount_options::csv_output)
						out << endl << "Current time (seconds since epoch): ";
					else
						out << endl << "timestamp,";
					if (dsp_intvl_in_100ms_units % OCOUNT_DSP_INTVLS_PER_SEC) {
						int tenths_secs = (int)round(((double)mytime.tv_usec/100000));
						if (tenths_secs == 10)
							out << dec << mytime.tv_sec + 1 << "." << "0";
						else
							out << dec << mytime.tv_sec << "." << tenths_secs;
					} else {
						out << dec << mytime.tv_sec;
					}
					do_results(out);
				}
				wait_rc = waitpid(app_PID, &waitpid_status, WNOHANG);
				if (wait_rc) {
					rc = _get_waitpid_status(waitpid_status, wait_rc);
					done = true;
					countdown = 0;
				}
			}
			if (--number_intervals == 0) {
				done = true;
				kill(app_PID, SIGKILL);
			}
		} while (!done);
	} else {
		wait_rc = waitpid(app_PID, &waitpid_status, 0);
		rc = _get_waitpid_status(waitpid_status, wait_rc);
	}
	return rc;
}

static end_code_t _run(ostream & out)
{
	end_code_t rc = ALL_OK;

	// Fork processes with signals blocked.
	sigset_t ss;
	sigfillset(&ss);
	sigprocmask(SIG_BLOCK, &ss, NULL);

	try {
		if (!start_counting()) {
			return PERF_RECORD_ERROR;
		}
	} catch (const runtime_error & e) {
		cerr << "Caught runtime error while setting up counters" << endl;
		cerr << e.what() << endl;
		return PERF_RECORD_ERROR;
	}
	// parent continues here
	if (startApp)
		cverb << vdebug << "app " << app_PID << " is running" << endl;

	set_signals_for_parent();
	if (startApp) {
		rc = _wait_for_app(out);
	} else {
		cout << "ocount: Press Ctl-c or 'kill -SIGINT " << getpid() << "' to stop counting" << endl;
		if (ocount_options::display_interval) {
			long number_intervals = ocount_options::num_intervals;
			struct timeval mytime;
			/* The display_interval is in milliseconds; but at this time, we only allow
			 * 100ms granularity. */
			struct timespec ts_req;
			unsigned int dsp_intvl_in_100ms_units = (int)round(((double)ocount_options::display_interval/
					OCOUNT_DSP_INTVL_MSECS));
			if (dsp_intvl_in_100ms_units == 0)
				dsp_intvl_in_100ms_units = 1; // special case of rounding up; prevent 0 time interval
			cverb << vdebug << "Actual display interval used: " << dsp_intvl_in_100ms_units
					<< "x100ms" << endl;
			// 10 dsp_intvl_in_100ms_units is one second, so we set ts_req accordingly
			ts_req.tv_sec = dsp_intvl_in_100ms_units/10;
			ts_req.tv_nsec = (dsp_intvl_in_100ms_units % 10) * OCOUNT_NSECS_PER_DSP_INTVL;
			while (!stop) {
				(void)nanosleep(&ts_req, NULL);
				if (gettimeofday(&mytime, NULL) < 0) {
					cleanup();
					perror("gettimeofday");
					exit(EXIT_FAILURE);
				}
				if (!ocount_options::csv_output)
					out << endl << "Current time (seconds since epoch): ";
				else
					out << endl << "t:";
				if (dsp_intvl_in_100ms_units % OCOUNT_DSP_INTVLS_PER_SEC) {
					int tenths_secs = (int)round(((double)mytime.tv_usec/100000));
					if (tenths_secs == 10)
						out << dec << mytime.tv_sec + 1 << "." << "0";
					else
						out << dec << mytime.tv_sec << "." << tenths_secs;
				} else {
					out << dec << mytime.tv_sec;
				}
				do_results(out);
				if (--number_intervals == 0)
					stop = true;
			}
		} else {
			while (!stop)
				sleep(1);
		}
	}
	return rc;
}

static void _parse_cpu_list(void)
{
	char * comma_sep;
	char * endptr;
	char * aCpu = strtok_r(optarg, ",", &comma_sep);
	do {
		int tmp = strtol(aCpu, &endptr, 10);
		if ((endptr >= aCpu) && (endptr <= (aCpu + strlen(aCpu) - 1))) {
			// Check if user has passed a range of cpu numbers:  e.g., '3-8'
			char * dash_sep;
			char * ending_cpu_str, * starting_cpu_str = strtok_r(aCpu, "-", &dash_sep);
			int starting_cpu, ending_cpu;
			if (starting_cpu_str) {
				ending_cpu_str = strtok_r(NULL, "-", &dash_sep);
				if (!ending_cpu_str) {
					__print_usage_and_exit("ocount: Invalid cpu range.");
				}
				starting_cpu = strtol(starting_cpu_str, &endptr, 10);
				if ((endptr >= starting_cpu_str) &&
						(endptr <= (starting_cpu_str + strlen(starting_cpu_str) - 1))) {
					__print_usage_and_exit("ocount: Invalid numeric value for --cpu-list option.");
				}
				ending_cpu = strtol(ending_cpu_str, &endptr, 10);
				if ((endptr >= ending_cpu_str) &&
						(endptr <= (ending_cpu_str + strlen(ending_cpu_str) - 1))) {
					__print_usage_and_exit("ocount: Invalid numeric value for --cpu-list option.");
				}
				for (int i = starting_cpu; i < ending_cpu + 1; i++)
					ocount_options::cpus.push_back(i);
			} else {
				__print_usage_and_exit("ocount: Invalid numeric value for --cpu-list option.");
			}
		} else {
			ocount_options::cpus.push_back(tmp);
		}
	} while ((aCpu = strtok_r(NULL, ",", &comma_sep)));
}

static void _parse_time_interval(void)
{
	char * endptr;
	char * num_intervals, * interval = strtok(optarg, ":");
	ocount_options::display_interval = strtol(interval, &endptr, 10);
	if (((endptr >= interval) && (endptr <= (interval + strlen(interval) - 1))) ||
			(ocount_options::display_interval < 0))
		__print_usage_and_exit("ocount: Invalid numeric value for interval_length.");

	// User has specified num_intervals: e.g., '-i 5:10'
	num_intervals = strtok(NULL, ":");
	if (num_intervals) {
		ocount_options::num_intervals = strtol(num_intervals, &endptr, 10);
		if (((endptr >= num_intervals) && (endptr <= (num_intervals + strlen(num_intervals) - 1))) ||
				(ocount_options::num_intervals < 0))
			__print_usage_and_exit("ocount: Invalid numeric value for num_intervals.");
	}
}

static int _process_ocount_and_app_args(int argc, char * const argv[])
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
			cerr << "ocount: non-option detected at optind " << optind << endl;
			keep_trying = false;
			idx_of_non_options = -1;
			break;
		case 'V':
			ocount_options::verbose = true;
			break;
		case 's':
			ocount_options::system_wide = true;
			break;
		case 'C':
			_parse_cpu_list();
			break;
		case 'p':
		{
			char * aPid = strtok(optarg, ",");
			do {
				ocount_options::processes.push_back(strtol(aPid, &endptr, 10));
				if ((endptr >= aPid) && (endptr <= (aPid + strlen(aPid) - 1)))
					__print_usage_and_exit("ocount: Invalid numeric value for --process-list option.");
			} while ((aPid = strtok(NULL, ",")));
			break;
		}
		case 'r':
		{
			char * aTid = strtok(optarg, ",");
			do {
				ocount_options::threads.push_back(strtol(aTid, &endptr, 10));
				if ((endptr >= aTid) && (endptr <= (aTid + strlen(aTid) - 1)))
					__print_usage_and_exit("ocount: Invalid numeric value for --thread-list option.");
			} while ((aTid = strtok(NULL, ",")));
			break;
		}
		case 'e':
			event = strtok(optarg, ",");
			do {
				ocount_options::evts.insert(event);
			} while ((event = strtok(NULL, ",")));
			break;
		case 'f':
			ocount_options::outfile = optarg;
			break;
		case 'c':
			ocount_options::separate_cpu = true;
			break;
		case 't':
			ocount_options::separate_thread = true;
			break;
		case 'b':
			ocount_options::csv_output = true;
			break;
		case 'i':
			_parse_time_interval();
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
			__print_usage_and_exit("ocount: unexpected end of arg parsing");
		}
	}

	if (prev_env == NULL)
		unsetenv("POSIXLY_CORRECT");

	return idx_of_non_options;
}


static enum op_runmode _get_runmode(int starting_point)
{
	enum op_runmode ret_rm = OP_MAX_RUNMODE;
	for (int i = starting_point; i < OP_MAX_RUNMODE && ret_rm == OP_MAX_RUNMODE; i++) {
		switch (i) {
		// There is no option to check for OP_START_APP; we include a case
		// statement here just to silence Coverity.
		case OP_START_APP:
			break;
		case OP_SYSWIDE:
			if (ocount_options::system_wide)
				ret_rm = OP_SYSWIDE;
			break;
		case OP_CPULIST:
			if (!ocount_options::cpus.empty())
				ret_rm = OP_CPULIST;
			break;
		case OP_PROCLIST:
			if (!ocount_options::processes.empty())
				ret_rm = OP_PROCLIST;
			break;
		case OP_THREADLIST:
			if (!ocount_options::threads.empty())
				ret_rm = OP_THREADLIST;
			break;
		default:
			break;
		}
	}
	return ret_rm;
}

static void _validate_args(void)
{
	if (ocount_options::verbose && !verbose::setup("debug")) {
		cerr << "unknown --verbose= options\n";
		__print_usage_and_exit(NULL);
	}
	if (runmode == OP_START_APP) {
		enum op_runmode conflicting_mode = OP_MAX_RUNMODE;
		if (ocount_options::system_wide)
			conflicting_mode = OP_SYSWIDE;
		else if (!ocount_options::cpus.empty())
			conflicting_mode = OP_CPULIST;
		else if (!ocount_options::processes.empty())
			conflicting_mode = OP_PROCLIST;
		else if (!ocount_options::threads.empty())
			conflicting_mode = OP_THREADLIST;

		if (conflicting_mode != OP_MAX_RUNMODE) {
			cerr << "Run mode " << runmode_options[OP_START_APP] << " is incompatible with "
			     << runmode_options[conflicting_mode] << endl;
			__print_usage_and_exit(NULL);
		}
	} else {
		enum op_runmode rm2;
		runmode = _get_runmode(OP_SYSWIDE);
		if (runmode == OP_MAX_RUNMODE) {
			__print_usage_and_exit("You must either pass in the name of a command or app to run or specify a run mode");
		}
		rm2 = _get_runmode(runmode + 1);
		if (rm2 != OP_MAX_RUNMODE) {
			cerr << "Run mode " << runmode_options[rm2] << " is incompatible with "
			     << runmode_options[runmode] << endl;
			__print_usage_and_exit(NULL);
		}

	}

	if (ocount_options::separate_cpu && !(ocount_options::system_wide || !ocount_options::cpus.empty())) {
		cerr << "The --separate-cpu option is only valid with --system-wide or --cpu-list." << endl;
		__print_usage_and_exit(NULL);
	}

	if (ocount_options::separate_thread && !(!ocount_options::threads.empty() || !ocount_options::processes.empty())) {
		cerr << "The --separate-thread option is only valid with --process_list or --thread_list." << endl;
		__print_usage_and_exit(NULL);
	}

	if (runmode == OP_CPULIST) {
		int num_cpus = use_cpu_minus_one ? 1 : sysconf(_SC_NPROCESSORS_ONLN);
		if (num_cpus < 1) {
			cerr << "System config says number of online CPUs is " << num_cpus << "; cannot continue" << endl;
			exit(EXIT_FAILURE);
		}

		set<int> available_cpus = op_pe_utils::op_get_available_cpus(num_cpus);
		size_t k;
		for (k = 0; k < ocount_options::cpus.size(); k++) {
			if (available_cpus.find(ocount_options::cpus[k]) == available_cpus.end()) {
				cerr << "Specified CPU " << ocount_options::cpus[k] << " is not valid" << endl;
				__print_usage_and_exit(NULL);
			}
		}
	}
}

static void process_args(int argc, char * const argv[])
{
	int non_options_idx  = _process_ocount_and_app_args(argc, argv);

	if (non_options_idx < 0) {
		__print_usage_and_exit(NULL);
	} else if ((non_options_idx) > 0) {
		runmode = OP_START_APP;
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
	}
	_validate_args();

	/*  At this point, we know which of the three counting modes the user requested:
	 *    - count events in named app
	 *    - count events in app by PID
	 *    - count events in whole system
	 */

	if (ocount_options::evts.empty()) {
		// Use default event
		op_pe_utils::op_get_default_event(false);
	} else  {
		op_pe_utils::op_process_events_list(ocount_options::evts, false, false);
	}
	cverb << vdebug << "Number of events passed is " << events.size() << endl;
	return;
}

int main(int argc, char * const argv[])
{
	int rc;
	bool get_results = true;
	int perf_event_paranoid = op_get_sys_value("/proc/sys/kernel/perf_event_paranoid");

	my_uid = geteuid();
	rc = op_check_perf_events_cap(use_cpu_minus_one);
	if (rc == EACCES) {
		/* Early perf_events kernels required the cpu argument to perf_event_open
		 * to be '-1' when setting up to monitor a single process if 1) the user is
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
	if (rc == EBUSY)
		cerr << "Performance monitor unit is busy.  Ensure that no other profilers are running on the system." << endl
		     << "Note: For example, the obsolete opcontrol profiler (available in earlier oprofile releases)" << endl
		     << "does not allow other performance tools to run simultaneously. To check for this, look for the" << endl
		     << "'oprofiled' process using the 'ps' command." << endl;
	else if (rc == ENOSYS)
		cerr << "Your kernel does not implement a required syscall"
		     << " for the ocount program." << endl;
	else if (rc == ENOENT)
		cerr << "Your kernel's Performance Events Subsystem does not support"
		     << " your processor type." << endl;
	else if (rc)
		cerr << "Unexpected error running ocount: " << strerror(rc) << endl;

	if (rc)
		exit(1);

	cpu_type = op_get_cpu_type();

	if (cpu_type == CPU_NO_GOOD) {
		cerr << "Unable to ascertain cpu type.  Exiting." << endl;
		cleanup();
		exit(1);
	}

	if (cpu_type == CPU_TIMER_INT) {
		cerr << "CPU type 'timer' was detected, but ocount does not support 'timer' as a cpu type." << endl
		     << "Ensure the obsolete opcontrol profiler (available in pre-1.0 oprofile releases)" << endl
		     << "is not running on the system.  To check for this, look for the file" << endl
		     << "/dev/oprofile/cpu_type; if this file exists, locate the pre-1.0 oprofile" << endl
		     << "installation, and use its 'opcontrol' command with the --deinit option." << endl;
		cleanup();
		exit(1);
	}
	cpu_speed = op_cpu_frequency();
	try {
		process_args(argc, argv);
	} catch (const runtime_error & e) {
		cerr << "Caught runtime error while processing args" << endl;
		cerr << e.what() << endl;
		cleanup();
		exit(EXIT_FAILURE);
	}

	if ((runmode == OP_SYSWIDE || runmode == OP_CPULIST) && ((my_uid != 0) && (perf_event_paranoid > 0))) {
		cerr << "To do ";
		if (runmode == OP_SYSWIDE)
			cerr << "system-wide ";
		else
			cerr << "cpu-list ";
		cerr << "event counting, either you must be root or" << endl;
		cerr << "/proc/sys/kernel/perf_event_paranoid must be set to 0 or -1." << endl;
		cleanup();
		exit(1);
	}

	if (!ocount_options::outfile.empty()) {
		outfile.open(ocount_options::outfile.c_str());
	}
	ostream & out = !ocount_options::outfile.empty() ? outfile : cout;

	end_code_t run_result;
	if ((run_result = _run(out))) {
		get_results = false;
		if (startApp && app_started && (run_result != APP_ABNORMAL_END)) {
			int rc;
			cverb << vdebug << "Killing monitored app . . ." << endl;
			rc = kill(app_PID, SIGKILL);
			if (rc) {
				if (errno == ESRCH)
					cverb << vdebug
					      << "Unable to kill monitored app because it has already ended"
					      << endl;
				else
					perror("Attempt to kill monitored app failed.");
			}
		}
		if ((run_result == PERF_RECORD_ERROR) || (run_result == PERF_BOTH_ERROR)) {
			cerr <<  "Error running ocount" << endl;
		} else {
			get_results = true;
			cverb << vdebug << "WARNING: Results may be incomplete due to to abend of monitored app." << endl;
		}
	}
	if (get_results)
		// We don't do a final display of results if we've been doing it on an interval already.
		if (!ocount_options::display_interval)
			do_results(out);

	cleanup();
	return 0;
}
