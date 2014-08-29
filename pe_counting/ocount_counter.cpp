/**
 * @file ocount_counter.cpp
 * Functions and classes for ocount tool.
 *
 * @remark Copyright 2013 OProfile authors
 * @remark Read the file COPYING
 *
 * Created on: May 22, 2013
 * @author Maynard Johnson
 * (C) Copyright IBM Corp. 2013
 *
 */
#include <time.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <dirent.h>
#include <stdlib.h>
#include <sys/types.h>
#include <signal.h>

#include <iostream>
#include <sstream>
#include <stdexcept>

#include "ocount_counter.h"
#include "op_pe_utils.h"
#include "operf_event.h"
#include "cverb.h"

extern verbose vdebug;
extern bool use_cpu_minus_one;
extern char * app_name;

using namespace std;

static string print_mask_modes(bool mode_specified,bool um_specified,
			       int no_kernel, int no_user,
			       string um_numeric_as_str, string umask_value)
{
	ostringstream qualifier_string;

	if (um_specified) {
		if (umask_value.size() == 0)
			umask_value = um_numeric_as_str;

		qualifier_string << ":" << umask_value;
	}

	if (mode_specified) {
		if (no_kernel)
			qualifier_string << ":0";
		else
			qualifier_string << ":1";

		if (no_user)
			qualifier_string << ":0";
		else
			qualifier_string << ":1";

	}

	return qualifier_string.str();
}

ocount_counter::ocount_counter(operf_event_t & evt,  bool enable_on_exec,
                               bool inherit)
{
	memset(&attr, 0, sizeof(attr));
	attr.size = sizeof(attr);
	attr.config = evt.evt_code;
#ifdef __s390__
	attr.type = PERF_TYPE_HARDWARE;
	if (evt.no_kernel && !evt.no_user)
		attr.config |= 32;
#else
	attr.type = PERF_TYPE_RAW;
#endif
	attr.exclude_hv = evt.no_hv;
	attr.inherit = inherit ? 1 : 0;
	attr.enable_on_exec = enable_on_exec ? 1 : 0;
	attr.disabled  = attr.enable_on_exec;
	attr.exclude_idle = 0;
	attr.exclude_kernel = evt.no_kernel;
	attr.exclude_user = evt.no_user;
	// This format allows us to tell user percent of time an event was scheduled
	// when multiplexing has been done by the kernel.
	attr.read_format = PERF_FORMAT_TOTAL_TIME_ENABLED |
			    PERF_FORMAT_TOTAL_TIME_RUNNING;
	event = evt;
	fd = cpu = pid = -1;
}

ocount_counter::~ocount_counter() {
}

#include <stdio.h>
int ocount_counter::perf_event_open(pid_t _pid, int _cpu)
{
	fd = op_perf_event_open(&attr, _pid, _cpu, -1, 0);
	if (fd < 0) {
		int ret = -1;
		cverb << vdebug << "perf_event_open failed: " << strerror(errno) << endl;
		if (errno == EBUSY) {
			cerr << "The performance monitoring hardware reports EBUSY. Is another profiling tool in use?" << endl
			     << "On some architectures, tools such as oprofile and perf being used in system-wide "
			     << "mode can cause this problem." << endl;
			ret = OP_PERF_HANDLED_ERROR;
		} else if (errno == ESRCH) {
			cerr << "!!!! No samples collected !!!" << endl;
			cerr << "The target program/command ended before profiling was started." << endl;
			ret = OP_PERF_HANDLED_ERROR;
		} else {
			cerr << "perf_event_open failed with " << strerror(errno) << endl;
		}
		return ret;
	}
	pid = _pid;
	cpu = _cpu;

	cverb << vdebug << "perf_event_open returning fd " << fd << endl;
	return fd;
}

int ocount_counter::read_count_data(ocount_accum_t * count_data)
{
	size_t len = 3 * sizeof(u64);
	char * buf = (char *)count_data;

	while (len) {
		int ret = read(fd, buf, len);

		if (ret <= 0)
			return ret;

		len -= ret;
		buf += ret;
	}

	return 0;
}

ocount_record::ocount_record(enum op_runmode _runmode, std::vector<operf_event_t> & _evts,
                             bool _with_time_interval)
{
	runmode = _runmode;
	with_time_interval = _with_time_interval;
	evts = _evts;
	valid = false;
	system_wide = false;
	tasks_are_threads = false;
	num_cpus = 0;
	app_pid = -1;
	start_time = 0ULL;
	total_bytes_recorded = 0;
}

bool ocount_record::start_counting_app_process(pid_t _pid)
{
	if (valid) {
		cerr << "ocount internal error: ocount_record already initialized" << endl;
		return false;
	}
	if (runmode != OP_START_APP) {
		cerr << "ocount internal error: Current run mode " << runmode << " is incompatible with "
		     "starting app." << endl;
		return false;
	}
	app_pid = _pid;
	setup();
	return true;
}

/*
 * There are separate ocount options for counting events for a set of processes ("--process-list")
 * or a set of threads ("--thread-list"). This function is used for passing the set of either
 * processes or threads to ocount_record, along with a boolean argument to indicate whether or not
 * the set of passed tasks are threads.  If they are threads, we set up perf_event_open to NOT
 * do "inherit".
 */
bool ocount_record::start_counting_tasklist(std::vector<pid_t> _tasks, bool _are_threads)
{
	if (valid) {
		cerr << "ocount internal error: ocount_record already initialized" << endl;
		return false;
	}
	tasks_are_threads = _are_threads;
	specified_tasks = _tasks;
	if (tasks_are_threads) {
		if (runmode != OP_THREADLIST) {
			cerr << "ocount internal error: Current run mode " << runmode << " is incompatible with "
			     "--thread-list option." << endl;
			return false;
		}
	} else {
		if (runmode != OP_PROCLIST) {
			cerr << "ocount internal error: Current run mode " << runmode << " is incompatible with "
			     "--process-list option." << endl;
			return false;
		}
	}
	setup();
	if (tasks_to_count.empty()) {
		cerr << "No valid tasks to monitor -- quitting." << endl;
		return false;
	}
	return true;
}

bool ocount_record::start_counting_cpulist(std::vector<int> _cpus)
{
	if (valid) {
		cerr << "ocount internal error: ocount_record already initialized" << endl;
		return false;
	}
	if (runmode != OP_CPULIST) {
		cerr << "ocount internal error: Current run mode " << runmode << " is incompatible with "
		     "--cpu-list option." << endl;
		return false;
	}
	specified_cpus = _cpus;
	setup();
	return true;
}

bool ocount_record::start_counting_syswide(void)
{
	if (valid) {
		cerr << "ocount internal error: ocount_record already initialized" << endl;
		return false;
	}
	if (runmode != OP_SYSWIDE) {
		cerr << "ocount internal error: Current run mode " << runmode << " is incompatible with "
		     "--system-wide option." << endl;
		return false;
	}
	system_wide = true;
	setup();
	return true;
}

int ocount_record::do_counting_per_task(void)
{
	string err_msg;
	int rc = 0;

	for (set<pid_t>::iterator it = tasks_to_count.begin(); it != tasks_to_count.end(); it++) {
		pid_t the_pid = *it;
		bool inherit = are_tasks_processes();
		cverb << vdebug << "calling perf_event_open for task " << the_pid << endl;
		for (unsigned event = 0; event < evts.size(); event++) {
			ocount_accum_t count_data = {0ULL, 0ULL, 0ULL};
			accum_counts.push_back(count_data);
			prev_accum_counts.push_back(0ULL);
			ocount_counter op_ctr(ocount_counter(evts[event], false, inherit));
			if ((rc = op_ctr.perf_event_open(the_pid, -1)) < 0) {
				err_msg = "Internal Error.  Perf event setup failed.";
				goto out;
			} else {
				rc = 0;
			}
			perfCounters.push_back(op_ctr);
		}
	}
out:
	if (rc && rc != OP_PERF_HANDLED_ERROR)
		throw runtime_error(err_msg);
	return rc;
}

int ocount_record::do_counting_per_cpu(void)
{
	string err_msg;
	int rc = 0;

	/* We'll do this sanity check here, but we also do it at the front-end where user
	 * args are being validated.  If we wait until we get here, the invalid CPU argument
	 * becomes an ugly thrown exception.
	 */
	set<int> available_cpus = op_pe_utils::op_get_available_cpus(num_cpus);
	if (runmode == OP_CPULIST) {
		size_t k;
		for (k = 0; k < specified_cpus.size(); k++) {
			if (available_cpus.find(specified_cpus[k]) == available_cpus.end()) {
				ostringstream err_msg_ostr;
				err_msg_ostr << "Specified CPU " << specified_cpus[k] << " is not valid";
				err_msg = err_msg_ostr.str();
				rc = -1;
				goto out;
			} else {
				cpus_to_count.insert(specified_cpus[k]);
			}
		}
	} else {
		cpus_to_count = available_cpus;
	}

	for (set<pid_t>::iterator it = cpus_to_count.begin(); it != cpus_to_count.end(); it++) {
		int the_cpu = *it;
		cverb << vdebug << "calling perf_event_open for cpu " << the_cpu << endl;
		for (unsigned event = 0; event < evts.size(); event++) {
			ocount_accum_t count_data = {0ULL, 0ULL, 0ULL};
			accum_counts.push_back(count_data);
			prev_accum_counts.push_back(0ULL);
			ocount_counter op_ctr(ocount_counter(evts[event], false, true));
			if ((rc = op_ctr.perf_event_open(-1, the_cpu)) < 0) {
				err_msg = "Internal Error.  Perf event setup failed.";
				goto out;
			} else {
				rc = 0;
			}
			perfCounters.push_back(op_ctr);
		}
	}
out:
	if (rc && rc != OP_PERF_HANDLED_ERROR)
		throw runtime_error(err_msg);
	return rc;
}

void ocount_record::setup()
{
	int rc = 0;
	string err_msg;

	if (!specified_tasks.empty()) {
		if ((rc = get_process_info(specified_tasks)) < 0) {
			if (rc == OP_PERF_HANDLED_ERROR)
				return;
			else
				throw runtime_error("Unexpected error in ocount_record setup");
		}
	}

	/* To set up to count events for an existing thread group, we need call perf_event_open
	 * for each thread, and we need to pass cpu=-1 on the syscall.
	 */
	use_cpu_minus_one = use_cpu_minus_one ? true : ((system_wide || (runmode == OP_CPULIST)) ? false : true);
	num_cpus = use_cpu_minus_one ? 1 : sysconf(_SC_NPROCESSORS_ONLN);
	if (num_cpus < 1) {
		char int_str[256];
		sprintf(int_str, "Number of online CPUs is %d; cannot continue", num_cpus);
		throw runtime_error(int_str);
	}
	if (system_wide || (runmode == OP_CPULIST)) {
		rc = do_counting_per_cpu();
	} else if (!specified_tasks.empty()) {
		rc = do_counting_per_task();
	} else {
		cverb << vdebug << "calling perf_event_open for pid " << app_pid << endl;
		for (unsigned event = 0; event < evts.size(); event++) {
			ocount_accum_t count_data = {0ULL, 0ULL, 0ULL};
			accum_counts.push_back(count_data);
			prev_accum_counts.push_back(0ULL);
			ocount_counter op_ctr(ocount_counter(evts[event], true, true));
			if ((rc = op_ctr.perf_event_open(app_pid, -1)) < 0) {
				err_msg = "Internal Error.  Perf event setup failed.";
				goto error;
			} else {
				rc = 0;
			}
			perfCounters.push_back(op_ctr);
		}
	}
	if (!rc) {
		cverb << vdebug << "perf counter setup complete" << endl;
		// Set bit to indicate we're set to go.
		valid = true;
		// Now that all events are programmed to start counting, init the start time
		struct timespec tspec;
		clock_gettime(CLOCK_MONOTONIC, &tspec);
		start_time = tspec.tv_sec * 1000000000ULL + tspec.tv_nsec;

		return;
	}

error:
	if (rc != OP_PERF_HANDLED_ERROR)
		throw runtime_error(err_msg);
}

void ocount_record::output_short_results(ostream & out, bool use_separation, bool scaled)
{
	size_t num_iterations = use_separation ? perfCounters.size() : evts.size();
	out << endl;
	for (size_t num = 0; num < num_iterations; num++) {
		ostringstream count_str;
		ocount_accum_t tmp_accum;
		double fraction_time_running;
		string qual_string;

		if (use_separation) {
			if (cpus_to_count.size()) {
				out << perfCounters[num].get_cpu();
			} else {
				out << perfCounters[num].get_pid();
			}
			out << "," << perfCounters[num].get_event_name();

			qual_string =
			  print_mask_modes(perfCounters[num].get_mode_specified(),
			                   perfCounters[num].get_um_specified(),
			                   perfCounters[num].get_no_kernel(),
			                   perfCounters[num].get_no_user(),
			                   perfCounters[num].get_um_numeric_val_as_str(),
			                   perfCounters[num].get_umask_value());
			out << qual_string;
			out  << ",";

			errno = 0;
			cverb << vdebug << "Reading counter data for event " << perfCounters[num].get_event_name() << endl;
			if (perfCounters[num].read_count_data(&tmp_accum) < 0) {
				string err_msg = "Internal error: read of perfCounter fd failed with ";
				err_msg += errno ? strerror(errno) : "unknown error";
				throw runtime_error(err_msg);
			}
			fraction_time_running = scaled ? (double)tmp_accum.running_time/tmp_accum.enabled_time : 1;
			if (with_time_interval) {
				u64 save_prev = prev_accum_counts[num];
				prev_accum_counts[num] = tmp_accum.count;
				tmp_accum.count -= save_prev;
			}
			u64 scaled_count = tmp_accum.count ? tmp_accum.count/fraction_time_running : 0;
			out << dec << scaled_count << ",";
		} else {
			fraction_time_running = scaled ? (double)accum_counts[num].running_time/accum_counts[num].enabled_time : 1;
			u64 scaled_count = accum_counts[num].count ? accum_counts[num].count/fraction_time_running : 0;
			out << perfCounters[num].get_event_name();

			qual_string = print_mask_modes(perfCounters[num].get_mode_specified(),
			                               perfCounters[num].get_um_specified(),
			                               perfCounters[num].get_no_kernel(),
			                               perfCounters[num].get_no_user(),
			                               perfCounters[num].get_um_numeric_val_as_str(),
			                               perfCounters[num].get_umask_value());
			out << qual_string;
			out << "," << dec << scaled_count << ",";
		}
		ostringstream strm_tmp;
		if (use_separation) {
			if (!tmp_accum.enabled_time) {
				out << 0 << endl;
			} else {
				strm_tmp.precision(2);
				strm_tmp << fixed << fraction_time_running * 100
				         << endl;
				out << strm_tmp.str();
			}
		} else {
			if (!accum_counts[num].enabled_time) {
				out << "Event not counted" << endl;
			} else {
				strm_tmp.precision(2);
				strm_tmp << fixed << fraction_time_running * 100
				         << endl;
				out << strm_tmp.str();
			}
		}
	}
}

void ocount_record::output_long_results(ostream & out, bool use_separation,
                                        int evt_name_col_size, bool scaled,
                                        u64 time_enabled)
{
#define COUNT_COLUMN_WIDTH 25
#define SEPARATION_ELEMENT_COLUMN_WIDTH 10
#define MIN_NAME_COLUMN_SPACING 8

	char space_padding[64], temp[64];
	char const * cpu, * task, * scaling;
	u64 num_seconds_enabled = time_enabled/1000000000;
	unsigned int num_minutes_enabled = num_seconds_enabled/60;
	cpu = "CPU";
	task = "Task ID";
	scaling = scaled ? "(scaled) " : "(actual) ";

	unsigned int begin_second_col;
	unsigned int num_pads;
	ostringstream debug_string;

	/* Need to account for any events that will be printing user/kernel
	 * mode or unit mask names when setting up the columns of the data.
	 */
	begin_second_col = evt_name_col_size + MIN_NAME_COLUMN_SPACING;
	num_pads = begin_second_col - strlen("Event");

	memset(space_padding, ' ', 64);
	strncpy(temp, space_padding, num_pads);
	temp[num_pads] = '\0';
	out << endl;
	if (!with_time_interval) {
		ostringstream strm;
		strm << "Events were actively counted for ";
		if (num_minutes_enabled) {
			strm << " ";
			strm << num_minutes_enabled;
			if (num_minutes_enabled > 1)
				strm << " minutes and ";
			else
				strm << " minute and ";
			strm << num_seconds_enabled % 60;
			strm << " seconds.";
		} else {
			if (num_seconds_enabled) {
				// Show 1/10's of seconds
				strm.precision(1);
				strm << fixed << (double)time_enabled/1000000000;
				strm << " seconds.";
			} else {
				// Show full nanoseconds
				strm << time_enabled << " nanoseconds.";
			}
		}
		out << strm.str() << endl;
	}
	out << "Event counts " << scaling;
	if (app_name)
		out << "for " << app_name << ":";
	else if (system_wide)
		out << "for the whole system:";
	else if (!cpus_to_count.empty())
		out << "for the specified CPU(s):";
	else if (tasks_are_threads)
		out << "for the specified thread(s):";
	else
		out << "for the specified process(es):";
	out << endl;

	out << "\tEvent" << temp;
	if (use_separation) {
		if (cpus_to_count.size()) {
			out << cpu;
			num_pads = SEPARATION_ELEMENT_COLUMN_WIDTH - strlen(cpu);
		} else {
			out << task;
			num_pads = SEPARATION_ELEMENT_COLUMN_WIDTH - strlen(task);

		}
		strncpy(temp, space_padding, num_pads);
		temp[num_pads] = '\0';
		out << temp;
	}
	out << "Count";
	num_pads = COUNT_COLUMN_WIDTH - strlen("Count");
	strncpy(temp, space_padding, num_pads);
	temp[num_pads] = '\0';
	out << temp << "% time counted" << endl;

	/* If counting per-cpu or per-thread, I refer generically to cpu or thread values
	 * as "elements of separation".  We will have one ocount_counter object per element of
	 * separation per event.  So if we're counting 2 events for 4 processes (or threads),
	 * we'll have 2x4 (8) ocount_counter objects.
	 *
	 * If 'use_separation' is true, then we need to print individual counts for
	 * each element of separation for each event; otherwise, we print aggregated counts
	 * for each event.
	 */
	size_t num_iterations = use_separation ? perfCounters.size() : evts.size();
	for (size_t num = 0; num < num_iterations; num++) {
		double fraction_time_running;
		string qual_string;

		out << "\t" << perfCounters[num].get_event_name();
		qual_string = print_mask_modes(perfCounters[num].get_mode_specified(),
		                               perfCounters[num].get_um_specified(),
		                               perfCounters[num].get_no_kernel(),
		                               perfCounters[num].get_no_user(),
		                               perfCounters[num].get_um_numeric_val_as_str(),
		                               perfCounters[num].get_umask_value());
		out << qual_string;
		num_pads = begin_second_col - qual_string.size()
			- perfCounters[num].get_event_name().size();

		strncpy(temp, space_padding, num_pads);
		temp[num_pads] = '\0';
		out << temp;

		ostringstream count_str;
		ocount_accum_t tmp_accum;
		if (use_separation) {
			ostringstream separation_element_str;
			strncpy(temp, space_padding, num_pads);
			temp[num_pads] = '\0';
			if (cpus_to_count.size()) {
				separation_element_str << dec << perfCounters[num].get_cpu();
				out << perfCounters[num].get_cpu();
			} else {
				separation_element_str << dec << perfCounters[num].get_pid();
				out << perfCounters[num].get_pid();
			}
			num_pads = SEPARATION_ELEMENT_COLUMN_WIDTH - separation_element_str.str().length();
			strncpy(temp, space_padding, num_pads);
			temp[num_pads] = '\0';
			out << temp;

			errno = 0;
			cverb << vdebug << "Reading counter data for event " << perfCounters[num].get_event_name() << endl;
			if (perfCounters[num].read_count_data(&tmp_accum) < 0) {
				string err_msg = "Internal error: read of perfCounter fd failed with ";
				err_msg += errno ? strerror(errno) : "unknown error";
				throw runtime_error(err_msg);
			}
			fraction_time_running = scaled ? (double)tmp_accum.running_time/tmp_accum.enabled_time : 1;

			if (with_time_interval) {
				u64 save_prev = prev_accum_counts[num];
				prev_accum_counts[num] = tmp_accum.count;
				tmp_accum.count -= save_prev;
			}
			u64 scaled_count = tmp_accum.count ? tmp_accum.count/fraction_time_running : 0;
			count_str << dec << scaled_count;
		} else {
			fraction_time_running = scaled ? (double)accum_counts[num].running_time/accum_counts[num].enabled_time : 1;
			u64 scaled_count = accum_counts[num].count ? accum_counts[num].count/fraction_time_running : 0;
			count_str << dec << scaled_count;
		}
		string count = count_str.str();
		for (int i = count.size() - 3; i > 0; i-=3) {
			count.insert(i, 1, ',');
		}
		out << count;
		num_pads = COUNT_COLUMN_WIDTH - count.size();
		strncpy(temp, space_padding, num_pads);
		temp[num_pads] = '\0';
		out << temp;
		ostringstream strm_tmp;
		if (use_separation) {
			if (!tmp_accum.enabled_time) {
				out << "Event not counted" << endl;
			} else {
				strm_tmp.precision(2);
				strm_tmp << fixed << fraction_time_running * 100
				         << endl;
				out << strm_tmp.str();
			}
		} else {
			if (!accum_counts[num].enabled_time) {
				out << "Event not counted" << endl;
			} else {
				strm_tmp.precision(2);
				strm_tmp << fixed << fraction_time_running * 100
				         << endl;
				out << strm_tmp.str();
			}
		}
	}
}

void ocount_record::output_results(ostream & out, bool use_separation, bool short_format)
{
#define MODE_FIELD_SIZE  3    /* space for :KU in the output */

	size_t evt_name_col_size = 0;
	u64 time_enabled = 0ULL;
	bool scaled = false;
	bool mode_specified = false;

	for (unsigned long evt_num = 0; evt_num < evts.size(); evt_num++) {
		unsigned int length = 0;

		/* calculate the longest name + unit mask + mode specifier */
		length = strlen(evts[evt_num].um_name) +
		  strlen(evts[evt_num].name) + 1; /* for colon */

		if ((strlen(evts[evt_num].um_numeric_val_as_str)
		     + strlen(evts[evt_num].name)) > length)
			length = strlen(evts[evt_num].um_numeric_val_as_str) +
			  strlen(evts[evt_num].name) + 1;  /* for colon */

		if (evts[evt_num].mode_specified)
			length += MODE_FIELD_SIZE;

		if (length > evt_name_col_size)
			evt_name_col_size = length;

		mode_specified = mode_specified ||
			evts[evt_num].mode_specified;
	}

	if (with_time_interval) {
		// reset the accum count values
		for (size_t i = 0; i < evts.size(); i++) {
			ocount_accum_t accum = accum_counts[i];
			accum.count = 0ULL;
			accum_counts[i] = accum;
		}
	}

	/* We need to inspect all of the count data now to ascertain if scaling
	 * is required, so we also collect aggregated counts into the accum_counts
	 * vector (if needed).
	 */
	for (unsigned long ocounter = 0; ocounter < perfCounters.size(); ocounter++) {
		ocount_accum_t tmp_accum;
		int evt_key = ocounter % evts.size();
		errno = 0;
		cverb << vdebug << "Reading counter data for event " << evts[evt_key].name << endl;
		if (perfCounters[ocounter].read_count_data(&tmp_accum) < 0) {
			string err_msg = "Internal error: read of perfCounter fd failed with ";
			err_msg += errno ? strerror(errno) : "unknown error";
			throw runtime_error(err_msg);
		}
		if (!use_separation) {
			ocount_accum_t real_accum = accum_counts[evt_key];
			real_accum.count += tmp_accum.count;
			real_accum.enabled_time += tmp_accum.enabled_time;
			real_accum.running_time += tmp_accum.running_time;
			accum_counts[evt_key] = real_accum;
		}
		if (tmp_accum.enabled_time != tmp_accum.running_time) {
			if (((double)(tmp_accum.enabled_time - tmp_accum.running_time)/tmp_accum.enabled_time) > 0.01)
				scaled = true;
		}
	}

	if (with_time_interval && !use_separation) {
		for (size_t i = 0; i < evts.size(); i++) {
			u64 save_prev = prev_accum_counts[i];
			ocount_accum_t real_accum = accum_counts[i];
			prev_accum_counts[i] = real_accum.count;
			real_accum.count -= save_prev;
			accum_counts[i] = real_accum;
		}
	}
	struct timespec tspec;
	clock_gettime(CLOCK_MONOTONIC, &tspec);
	time_enabled = (tspec.tv_sec * 1000000000ULL + tspec.tv_nsec) - start_time;


	if (short_format)
		output_short_results(out, use_separation, scaled);
	else
		output_long_results(out, use_separation, evt_name_col_size,
				    scaled, time_enabled);
}

int ocount_record::_get_one_process_info(pid_t pid)
{
	char fname[PATH_MAX];
	DIR *tids;
	struct dirent dirent, *next;
	int ret = 0;

	add_process(pid);
	if (are_tasks_processes()) {
		snprintf(fname, sizeof(fname), "/proc/%d/task", pid);
		tids = opendir(fname);
		if (tids == NULL) {
			// process must have exited
			ret = -1;
			cverb << vdebug << "Process " << pid << " apparently exited while "
					<< "process info was being collected"<< endl;
			goto out;
		}

		while (!readdir_r(tids, &dirent, &next) && next) {
			char *end;
			pid = strtol(dirent.d_name, &end, 10);
			if (*end)
				continue;
			add_process(pid);
		}
		closedir(tids);
	}

out:
	return ret;
}

/* Obtain process information for one or more active process, where the user has
 * either passed in a set of processes via the --process-list option or has specified
 * --system_wide.
 */
int ocount_record::get_process_info(const vector<pid_t> & _procs)
{
	int ret = 0;
	if (cverb << vdebug)
		cout << "op_get_process_info" << endl;
	for (size_t i = 0; i < _procs.size(); i++) {
		errno = 0;
		if (kill(_procs[i], 0) < 0) {
			if (errno == EPERM) {
				string errmsg = "You do not have permission to monitor ";
				errmsg += are_tasks_processes() ? "process " : "thread ";
				cerr << errmsg << _procs[i] << endl;
				ret = OP_PERF_HANDLED_ERROR;
			}
			break;
		}
		if ((ret = _get_one_process_info(_procs[i])) < 0)
			break;
	}
	return ret;
}
