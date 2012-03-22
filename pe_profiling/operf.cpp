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
 * (C) Copyright IBM Corporation 2012
 *
 */

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
#include <sys/wait.h>
#include <ftw.h>
#include <iostream>
#include "operf_utils.h"
#include "popt_options.h"
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

using namespace std;

typedef enum END_CODE {
	ALL_OK = 0,
	APP_ABNORMAL_END = -2,
	PERF_RECORD_ERROR = -3
} end_code_t;

// Globals
char * app_name = NULL;
uint64_t kernel_start, kernel_end;
operf_read operfRead;
op_cpu cpu_type;
double cpu_speed;
char op_samples_current_dir[PATH_MAX];


#define DEFAULT_OPERF_OUTFILE "operf.data"
static char full_pathname[PATH_MAX];
static char * app_name_SAVE = NULL;
static char * app_args = NULL;
static pid_t app_PID = -1;
static bool app_started;
static pid_t operf_pid;
static string samples_dir;
static string outputfile;
static bool startApp;
static bool reset_done = false;
uint op_nr_counters;
vector<operf_event_t> events;

verbose vmisc("misc");

namespace operf_options {
bool system_wide;
bool reset;
int pid;
int callgraph_depth;
int mmap_pages_mult;
string session_dir;
string vmlinux;
bool separate_cpu;
vector<string> evts;
}

namespace {
vector<string> verbose_string;

popt::option options_array[] = {
	popt::option(verbose_string, "verbose", 'V',
	             "verbose output", "debug,perf_events,misc,all"),
	popt::option(operf_options::session_dir, "session-dir", 'd',
	             "session path to hold sample data", "path"),
	popt::option(operf_options::vmlinux, "vmlinux", 'k',
	             "pathname for vmlinux file to use for symbol resolution and debuginfo", "path"),
	popt::option(operf_options::callgraph_depth, "callgraph", 'g',
	             "callgraph depth", "depth"),
	popt::option(operf_options::system_wide, "system-wide", 's',
	             "profile entire system"),
	popt::option(operf_options::reset, "reset", 'r',
	             "clear out old profile data"),
	popt::option(operf_options::pid, "pid", 'p',
	             "process ID to profile", "PID"),
	popt::option(operf_options::mmap_pages_mult, "kernel-buffersize-multiplier", 'b',
	             "factor by which kernel buffer size should be increased", "buffersize"),
	popt::option(operf_options::evts, "events", 'e',
	             "comma-separated list of event specifications for profiling. Event spec form is:\n"
	             "name:count[:unitmask[:kernel[:user]]]",
	             "events"),
	popt::option(operf_options::separate_cpu, "separate-cpu", 'c',
	             "Categorize samples by cpu"),
};
}


static void op_sig_stop(int val __attribute__((unused)))
{
	// Received a signal to quit, so we need to stop the
	// app being profiled.
	if (cverb << vdebug)
		write(1, "in op_sig_stop ", 15);
	if (startApp)
		kill(app_PID, SIGKILL);
}

void set_signals(void)
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

static int app_ready_pipe[2], start_app_pipe[2], operf_record_ready_pipe[2];

void run_app(void)
{
	char * app_fname = rindex(app_name, '/') + 1;
	if (!app_fname) {
		cerr << "Error trying to parse app name " <<  app_name << endl;
		cerr << "usage: operf [options] --pid=<PID> | appname [args]" << endl;
		exit(EXIT_FAILURE);
	}

	vector<string> exec_args_str;
	if (app_args) {
		size_t end_pos;
		string app_args_str = app_args;
		// Since the strings returned from substr would otherwise be ephemeral, we
		// need to store them into the exec_args_str vector so we can reference
		// them later when we call execvp.
		do {
			end_pos = app_args_str.find_first_of(' ', 0);
			if (end_pos != string::npos) {
				exec_args_str.push_back(app_args_str.substr(0, end_pos));
				app_args_str = app_args_str.substr(end_pos + 1);
			} else {
				exec_args_str.push_back(app_args_str);
			}
		} while (end_pos != string::npos);
	}

	vector<const char *> exec_args;
	exec_args.push_back(app_fname);
	vector<string>::iterator it;
	cverb << vdebug << "Exec args are: " << app_fname << " ";
	// Now transfer the args from the intermediate exec_args_str container to the
	// exec_args container that can be passed to execvp.
	for (it = exec_args_str.begin(); it != exec_args_str.end(); it++) {
		exec_args.push_back((*it).c_str());
		cverb << vdebug << (*it).c_str() << " ";
	}
	exec_args.push_back((char *) NULL);
	cverb << vdebug << endl;
	// Fake an exec to warm-up the resolver
	execvp("", ((char * const *)&exec_args[0]));
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
		goto fail;

	cverb << vdebug << "parent says start app " << app_name << endl;
	//sleep(1);
	execvp(app_name, ((char * const *)&exec_args[0]));
	cerr <<  "Failed to exec " << exec_args[0] << ": " << strerror(errno) << endl;
	fail:
	/* We don't want any cleanup in the child */
	_exit(EXIT_FAILURE);

}

int start_profiling_app(void)
{
	// The only process that should return from this function is the process
	// which invoked it.  Any forked process must do _exit() rather than return().
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
			run_app();
		}
		// parent
		if (pipe(operf_record_ready_pipe) < 0) {
			perror("Internal error: could not create pipe");
			return -1;
		}
	}

	//parent
	operf_pid = fork();
	if (operf_pid < 0) {
		return -1;
	} else if (operf_pid == 0) { // operf-record process
		// setup operf recording
		try {
			OP_perf_utils::vmlinux_info_t vi;
			vi.image_name = operf_options::vmlinux;
			vi.start = kernel_start;
			vi.end = kernel_end;
			operf_record operfRecord(outputfile, operf_options::system_wide, app_PID,
			                         (operf_options::pid == app_PID), events, vi);
			if (operfRecord.get_valid() == false) {
				/* If valid is false, it means that one of the "known" errors has
				 * occurred:
				 *   - profiled process has already ended
				 *   - passed PID was invalid
				 *   - device or resource busy
				 * Since an informative message has already been displayed to
				 * the user, we don't want to blow chunks here; instead, we'll
				 * exit gracefully.  Clear out the operf.data file as an indication
				 * to the parent process that the profile data isn't valid.
				 */
				ofstream of;
				of.open(outputfile.c_str(), ios_base::trunc);
				of.close();
				cerr << "operf record init failed" << endl;
				cerr << "usage: operf [options] --pid=<PID> | appname [args]" << endl;
				// Exit with SUCCESS to avoid the unnecessary "operf-record process ended
				// abnormally" message
				_exit(EXIT_SUCCESS);
			}
			if (startApp) {
				int ready = 1;
				if (write(operf_record_ready_pipe[1], &ready, sizeof(ready)) < 0) {
					perror("Internal error on operf_record_ready_pipe");
					_exit(EXIT_FAILURE);
				}
			}

			// start recording
			operfRecord.recordPerfData();
			cerr << "Total bytes recorded from perf events: "
					<< operfRecord.get_total_bytes_recorded() << endl;

			operfRecord.~operf_record();
			// done
			_exit(EXIT_SUCCESS);
		} catch (runtime_error re) {
			cerr << "Caught runtime_error: " << re.what() << endl;
			if (startApp)
				kill(app_PID, SIGKILL);
			_exit(EXIT_FAILURE);
		}
	} else {  // parent
		if (startApp) {
			int startup;
			if (read(app_ready_pipe[0], &startup, sizeof(startup)) == -1) {
				perror("Internal error on app_ready_pipe");
				return -1;
			} else if (startup != 1) {
				cerr << "app is not ready to start; exiting" << endl;
				return -1;
			}

			int recorder_ready;
			if (read(operf_record_ready_pipe[0], &recorder_ready, sizeof(recorder_ready)) == -1) {
				perror("Internal error on operf_record_ready_pipe");
				return -1;
			} else if (recorder_ready != 1) {
				cerr << "operf record process failure; exiting" << endl;
				return -1;
			}

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

static end_code_t _kill_operf_pid(void)
{
	int waitpid_status = 0;
	end_code_t rc = ALL_OK;

	// stop operf-record process
	if (kill(operf_pid, SIGUSR1) < 0) {
		perror("Attempt to stop operf-record process failed");
		rc = PERF_RECORD_ERROR;
	} else {
		if (waitpid(operf_pid, &waitpid_status, 0) < 0) {
			perror("waitpid for operf-record process failed");
			rc = PERF_RECORD_ERROR;
		} else {
			if (WIFEXITED(waitpid_status) && (!WEXITSTATUS(waitpid_status))) {
				cverb << vdebug << "waitpid for operf-record process returned OK" << endl;
			} else {
				cerr <<  "operf-record process ended abnormally: "
						<< WEXITSTATUS(waitpid_status) << endl;
				rc = PERF_RECORD_ERROR;
			}
		}
	}
	return rc;
}

static end_code_t _run(void)
{
	int waitpid_status = 0;
	end_code_t rc = ALL_OK;

	// Fork processes with signals blocked.
	sigset_t ss;
	sigfillset(&ss);
	sigprocmask(SIG_BLOCK, &ss, NULL);

	if (start_profiling_app() < 0) {
		perror("Internal error: fork failed");
		return PERF_RECORD_ERROR;
	}
	// parent continues here
	if (startApp)
		cverb << vdebug << "app " << app_PID << " is running" << endl;
	set_signals();
	if (startApp) {
		// User passed in command or program name to start
		cverb << vdebug << "going into waitpid on profiled app " << app_PID << endl;
		if (waitpid(app_PID, &waitpid_status, 0) < 0) {
			if (errno == EINTR) {
				cverb << vdebug << "Caught ctrl-C.  Killed profiled app." << endl;
			} else {
				cerr << "waitpid errno is " << errno << endl;
				perror("waitpid for profiled app failed");
				rc = APP_ABNORMAL_END;
			}
		} else {
			if (WIFEXITED(waitpid_status) && (!WEXITSTATUS(waitpid_status))) {
				cverb << vdebug << "waitpid for profiled app returned OK" << endl;
			} else if (WIFEXITED(waitpid_status)) {
				cerr <<  "profiled app ended abnormally: "
						<< WEXITSTATUS(waitpid_status) << endl;
				rc = APP_ABNORMAL_END;
			}
		}
		rc = _kill_operf_pid();
	} else {
		// User passed in --pid or --system-wide
		cverb << vdebug << "going into waitpid on operf record process " << operf_pid << endl;
		if (waitpid(operf_pid, &waitpid_status, 0) < 0) {
			if (errno == EINTR) {
				cverb << vdebug << "Caught ctrl-C. Killing operf-record process . . ." << endl;
				rc = _kill_operf_pid();
			} else {
				cerr << "waitpid errno is " << errno << endl;
				perror("waitpid for operf-record process failed");
				rc = PERF_RECORD_ERROR;
			}
		} else {
			if (WIFEXITED(waitpid_status) && (!WEXITSTATUS(waitpid_status))) {
				cverb << vdebug << "waitpid for operf-record process returned OK" << endl;
			} else if (WIFEXITED(waitpid_status)) {
				cerr <<  "operf-record process ended abnormally: "
						<< WEXITSTATUS(waitpid_status) << endl;
				rc = PERF_RECORD_ERROR;
			} else if (WIFSIGNALED(waitpid_status)) {
				cerr << "operf-record process killed by signal "
				     << WTERMSIG(waitpid_status) << endl;
				rc = PERF_RECORD_ERROR;
			}
		}
	}
	return rc;
}

static void cleanup(void)
{
	string cmd = "rm -f " + outputfile;
	free(app_name_SAVE);
	free(app_args);
	events.clear();
	verbose_string.clear();
	system(cmd.c_str());
}

static int __delete_sample_data(const char *fpath,
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

static void complete(void)
{
	int rc;
	string current_sampledir = samples_dir + "/current/";
	current_sampledir.copy(op_samples_current_dir, current_sampledir.length(), 0);

	if (!app_started && !operf_options::system_wide) {
		cleanup();
		return;
	}
	if (operf_options::reset) {
		int flags = FTW_DEPTH | FTW_ACTIONRETVAL;

		if (nftw(current_sampledir.c_str(), __delete_sample_data, 32, flags) !=0 &&
				errno != ENOENT) {
			cerr << "Problem encountered clearing old sample data."
			     << " Possible permissions problem." << endl;
			cerr << "Try a manual removal of " << current_sampledir << endl;
			cleanup();
			exit(1);
		}
		reset_done = true;
	}
	rc = mkdir(current_sampledir.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
	if (rc && (errno != EEXIST)) {
		cerr << "Error trying to create " << current_sampledir << " dir." << endl;
		perror("mkdir failed with");
		exit(EXIT_FAILURE);
	}

	operfRead.init(outputfile, current_sampledir, cpu_type, events);
	if ((rc = operfRead.readPerfHeader()) < 0) {
		if (rc != OP_PERF_HANDLED_ERROR)
			cerr << "Error: Cannot create read header info for sample file " << outputfile << endl;
		cleanup();
		exit(1);
	}
	cverb << vdebug << "Successfully read header info for sample file " << outputfile << endl;
	// TODO:  We may want to do incremental conversion of the perf data, since the perf sample format
	// is very inefficient to store.  For example, using a simple test program that does many
	// millions of memcpy's over a 12 second span of time, a profile taken via legacy oprofile,
	// with --separate=all and --image=<app_name> requires ~300K of storage.  Using the perf tool
	// (not operf) to profile the same application creates an 18MB perf.data file!!
	if (operfRead.is_valid()) {
		try {
			operfRead.convertPerfData();
			cerr << endl << "Use '--session-dir=" << operf_options::session_dir << "'" << endl
			     << "with opreport and other post-processing tools to view your profile data."
			     << endl;
			if (operf_options::system_wide)
				cerr << "\nNOTE: The system-wide profile you requested was collected "
				"on a per-process basis." << endl
				<< "Adding '--merge=tgid' when using post-processing tools will make the output"
				<< endl << "more readable." << endl;

			cerr << "^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^" << endl;
		} catch (runtime_error e) {
			cerr << "Caught exception from operf_read::convertPerfData" << endl;
			cerr << e.what() << endl;
		}
	}
	cleanup();
}


static int find_app_file_in_dir(const struct dirent * d)
{
	if (!strcmp(d->d_name, app_name))
		return 1;
	else
		return 0;
}

static int get_PATH_based_pathname(char * path_holder, size_t n)
{
	int retval = -1;

	char * real_path = getenv("PATH");
	char * path = (char *) xstrdup(real_path);
	char * segment = strtok(path, ":");
	while (segment) {
		struct dirent ** namelist;
		int rc = scandir(segment, &namelist, find_app_file_in_dir, NULL);
		if (rc < 0) {
			cerr << app_name << " cannot be found in your PATH." << endl;
			break;
		} else if (rc == 1) {
			size_t applen = strlen(app_name);
			size_t dirlen = strlen(segment);

			if (applen + dirlen + 2 > n) {
				cerr << "Path segment " << segment
				     << " prepended to the passed app name is too long"
				     << endl;
				retval = -1;
				break;
			}
			strncpy(path_holder, segment, dirlen);
			strcat(path_holder, "/");
			strncat(path_holder, app_name, applen);
			retval = 0;
			break;
		}
		segment = strtok(NULL, ":");
	}
	return retval;
}
int validate_app_name(void)
{
	int rc = 0;
	struct stat filestat;
	size_t len = strlen(app_name);

	if (len > (size_t) (OP_APPNAME_LEN - 1)) {
		cerr << "app name longer than max allowed (" << OP_APPNAME_LEN
		     << " chars)\n";
		cerr << app_name << endl;
		rc = -1;
		goto out;
	}

	if (index(app_name, '/') == app_name) {
		// Full pathname of app was specified, starting with "/".
		strncpy(full_pathname, app_name, len);
	} else if ((app_name[0] == '.') && (app_name[1] == '/')) {
		// Passed app is in current directory; e.g., "./myApp"
		getcwd(full_pathname, PATH_MAX);
		strcat(full_pathname, "/");
		strcat(full_pathname, (app_name + 2));
	} else if (index(app_name, '/')) {
		// Passed app is in a subdirectory of cur dir; e.g., "test-stuff/myApp"
		getcwd(full_pathname, PATH_MAX);
		strcat(full_pathname, "/");
		strcat(full_pathname, app_name);
	} else {
		// Pass app name, at this point, MUST be found in PATH
		rc = get_PATH_based_pathname(full_pathname, PATH_MAX);
	}

	if (rc) {
		cerr << "Problem finding app name " << app_name << ". Aborting."
		     << endl;
		goto out;
	}
	app_name_SAVE = app_name;
	app_name = full_pathname;
	if (stat(app_name, &filestat)) {
		char msg[OP_APPNAME_LEN + 50];
		snprintf(msg, OP_APPNAME_LEN + 50, "Non-existent app name \"%s\"",
		         app_name);
		perror(msg);
		rc = -1;
	}

	out: return rc;
}

static void __print_usage_and_exit(char * extra_msg)
{
	if (extra_msg)
		cerr << extra_msg << endl;
	cerr << "usage: operf [options] --pid=<PID> | appname [args]" << endl;
	exit(EXIT_FAILURE);

}

static u32 _get_event_code(char name[])
{
	FILE * fp;
	char oprof_event_code[9];
	string command;
	command = "ophelp ";
	command += name;

	fp = popen(command.c_str(), "r");
	if (fp == NULL) {
		cerr << "Unable to execute ophelp to get info for event "
		     << name << endl;
		exit(EXIT_FAILURE);
	}
	if (fgets(oprof_event_code, sizeof(oprof_event_code), fp) == NULL) {
		cerr << "Unable to find info for event "
		     << name << endl;
		exit(EXIT_FAILURE);
	}

	return atoi(oprof_event_code);
}

static void _process_events_list(void)
{
	string cmd = "ophelp --check-events ";
	for (unsigned int i = 0; i <  operf_options::evts.size(); i++) {
		FILE * fp;
		string full_cmd = cmd;
		string event_spec = operf_options::evts[i];
		full_cmd += event_spec;
		fp = popen(full_cmd.c_str(), "r");
		if (fp == NULL) {
			cerr << "Unable to execute ophelp to get info for event "
			     << event_spec << endl;
			exit(EXIT_FAILURE);
		}
		if (fgetc(fp) == EOF) {
			cerr << "Unable to find info for event "
			     << event_spec << endl;
			exit(EXIT_FAILURE);
		}
		char * event_str = op_xstrndup(event_spec.c_str(), event_spec.length());
		operf_event_t event;
		strncpy(event.name, strtok(event_str, ":"), OP_MAX_EVT_NAME_LEN);
		event.count = atoi(strtok(NULL, ":"));
		/* Name and count are required in the event spec in order for
		 * 'ophelp --check-events' to pass.  But since unit mask is
		 * optional, we need to ensure the result of strtok is valid.
		 */
		char * um = strtok(NULL, ":");
		if (um)
			event.evt_um = atoi(um);
		else
			event.evt_um = 0;
		event.op_evt_code = _get_event_code(event.name);
		event.evt_code = event.op_evt_code;
		events.push_back(event);
	}
#if (defined(__powerpc__) || defined(__powerpc64__))
	{
		/* This section of code is for architectures such as ppc[64] for which
		 * the oprofile event code needs to be converted to the appropriate event
		 * code to pass to the perf_event_open syscall.
		 */

		using namespace OP_perf_utils;
		if (!op_convert_event_vals(&events)) {
			cerr << "Unable to convert all oprofile event values to perf_event values" << endl;
			exit(EXIT_FAILURE);
		}
	}
#endif
}

static void get_default_event(void)
{
	operf_event_t dft_evt;
	struct op_default_event_descr descr;
	vector<operf_event_t> tmp_events;

	op_default_event(cpu_type, &descr);
	if (descr.name[0] == '\0') {
		cerr << "Unable to find default event" << endl;
		exit(EXIT_FAILURE);
	}

	memset(&dft_evt, 0, sizeof(dft_evt));
	dft_evt.count = descr.count;
	dft_evt.evt_um = descr.um;
	strncpy(dft_evt.name, descr.name, OP_MAX_EVT_NAME_LEN - 1);
	dft_evt.op_evt_code = _get_event_code(dft_evt.name);
	dft_evt.evt_code = dft_evt.op_evt_code;
	events.push_back(dft_evt);

#if (defined(__powerpc__) || defined(__powerpc64__))
	{
		/* This section of code is for architectures such as ppc[64] for which
		 * the oprofile event code needs to be converted to the appropriate event
		 * code to pass to the perf_event_open syscall.
		 */

		using namespace OP_perf_utils;
		if (!op_convert_event_vals(&events)) {
			cerr << "Unable to convert all oprofile event values to perf_event values" << endl;
			exit(EXIT_FAILURE);
		}
	}
#endif
}

static void _process_session_dir(void)
{
	if (operf_options::session_dir.empty()) {
		char * cwd = NULL;
		int rc;
		cwd = (char *) xmalloc(PATH_MAX);
		// set default session dir
		cwd = getcwd(cwd, PATH_MAX);
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
		string tmp = operf_options::session_dir + "/oprofile_data";
		rc = mkdir(tmp.c_str(), S_IRWXU);
		if (rc && (errno != EEXIST)) {
			cerr << "Error trying to create " << tmp << " dir." << endl;
			perror("mkdir failed with");
			exit(EXIT_FAILURE);
		}
		samples_dir = tmp + "/samples";
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

static void process_args(int argc, char const ** argv)
{
	vector<string> non_options;
	popt::parse_options(argc, argv, non_options, true/*non-options IS an app*/);

	if (operf_options::callgraph_depth) {
		cerr << "The --callgraph option is not yet supported." << endl;
		exit(EXIT_FAILURE);
	}

	if (!non_options.empty()) {
		if (operf_options::pid)
			__print_usage_and_exit(NULL);

		vector<string>::iterator it = non_options.begin();
		app_name = (char *) xmalloc((*it).length() + 1);
		strncpy(app_name, ((char *)(*it).c_str()), (*it).length() + 1);
		if (it++ != non_options.end()) {
			if ((*it).length() > 0) {
				app_args = (char *) xmalloc((*it).length() + 1);
				strncpy(app_args, ((char *)(*it).c_str()), (*it).length() + 1);
			}
		}
		if (validate_app_name() < 0) {
			exit(1);
		}
	} else if (operf_options::pid) {
		if (operf_options::system_wide)
			__print_usage_and_exit(NULL);
		app_PID = operf_options::pid;
	} else if (operf_options::system_wide) {
		app_PID = -1;
	} else {
		__print_usage_and_exit(NULL);
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
	outputfile = samples_dir + "/" + DEFAULT_OPERF_OUTFILE;

	// TODO: Need to examine ocontrol to see what (if any) additional
	// event verification is needed here.
	if (operf_options::evts.empty()) {
		// Use default event
		get_default_event();
	} else  {
		_process_events_list();
	}

	if (operf_options::vmlinux.empty()) {
		no_vmlinux = true;
		operf_create_vmlinux(NULL, NULL);
	} else {
		string startEnd = _process_vmlinux(operf_options::vmlinux);
		operf_create_vmlinux(operf_options::vmlinux.c_str(), startEnd.c_str());
	}

	return;
}

static int _check_perf_events_cap(void)
{
	/* If perf_events syscall is not implemented, the syscall below will fail
	 * with ENOSYS (38).  If implemented, but the processor type on which this
	 * program is running is not supported by perf_events, the syscall returns
	 * ENOENT (2).
	 */
	struct perf_event_attr attr;
	pid_t pid ;
        memset(&attr, 0, sizeof(attr));
        attr.size = sizeof(attr);
        attr.sample_type = PERF_SAMPLE_IP;

	pid = getpid();
	syscall(__NR_perf_event_open, &attr, pid, 0, -1, 0);
	return errno;

}

bool no_vmlinux;
int main(int argc, char const *argv[])
{
	int rc;
	if ((rc = _check_perf_events_cap())) {
		if (rc == ENOSYS) {
			cerr << "Your kernel does not implement a required syscall"
			     << "  for the operf program." << endl;
		} else if (rc == ENOENT) {
			cerr << "Your kernel's Performance Events Subsystem does not support"
			     << " your processor type." << endl;
		} else {
			cerr << "Unexpected error running operf: " << strerror(rc) << endl;
		}
		cerr << "Please use the opcontrol command instead of operf." << endl;
		exit(1);
	}

	cpu_type = op_get_cpu_type();
	cpu_speed = op_cpu_frequency();
	process_args(argc, argv);
	uid_t uid = geteuid();
	if (operf_options::system_wide && uid != 0) {
		cerr << "You must be root to do system-wide profiling." << endl;
		exit(1);
	}

	if (cpu_type == CPU_NO_GOOD) {
		cerr << "Unable to ascertain cpu type.  Exiting." << endl;
		exit(1);
	}
	op_nr_counters = op_get_nr_counters(cpu_type);
	end_code_t run_result;
	if ((run_result = _run())) {
		if (app_started && (run_result != APP_ABNORMAL_END)) {
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
		if (run_result == PERF_RECORD_ERROR) {
			cerr <<  "Error running profiler" << endl;
			exit(1);
		} else {
			cerr << "WARNING: Profile results may be incomplete due to to abend of profiled app." << endl;
		}
	}
	complete();
	if (operf_options::reset && reset_done == false)
		cerr << "Requested reset was not performed due to problem running operf command." << endl;
	return 0;
}
