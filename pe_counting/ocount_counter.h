/**
 * @file ocount_counter.h
 * Definitions and prototypes for ocount tool.
 *
 * @remark Copyright 2013 OProfile authors
 * @remark Read the file COPYING
 *
 * Created on: May 22, 2013
 * @author Maynard Johnson
 * (C) Copyright IBM Corp. 2013
 *
 */

#ifndef OCOUNT_COUNTER_H_
#define OCOUNT_COUNTER_H_

#include <linux/perf_event.h>
#include <unistd.h>
#include <sys/syscall.h>

#include <vector>
#include <set>
#include <string>

#include "operf_event.h"

#define OP_PERF_HANDLED_ERROR -101

enum op_runmode {
	OP_START_APP,
	OP_SYSWIDE,
	OP_CPULIST,
	OP_PROCLIST,
	OP_THREADLIST,
	OP_MAX_RUNMODE
};

typedef struct ocount_accum {
	u64 count;
	u64 enabled_time;
	u64 running_time;
} ocount_accum_t;

static inline int
op_perf_event_open(struct perf_event_attr * attr,
		      pid_t pid, int cpu, int group_fd,
		      unsigned long flags)
{
	return syscall(__NR_perf_event_open, attr, pid, cpu,
			       group_fd, flags);
}


class ocount_record;
class ocount_counter {
public:
	ocount_counter(operf_event_t & evt, bool enable_on_exec,
	               bool inherit);
	~ocount_counter();
	int perf_event_open(pid_t pid, int cpu);
	int get_cpu(void) { return cpu; }
	pid_t get_pid(void) { return pid; }
	const std::string get_umask_value(void) const { return event.um_name; }
	const std::string get_event_name(void) const { return event.name; }
	std::string get_um_numeric_val_as_str(void)
		{ return event.um_numeric_val_as_str; }
	int get_no_user(void) const { return attr.exclude_user; }
	int get_no_kernel(void) const { return attr.exclude_kernel; }
	bool get_mode_specified(void) { return event.mode_specified; }
	bool get_um_specified(void) { return event.umask_specified; }
	int read_count_data(ocount_accum_t * accum);

private:
	operf_event_t event;
	struct perf_event_attr attr;
	int fd;
	int cpu;
	pid_t pid;
};

class ocount_record {
public:
	ocount_record(enum op_runmode _runmode, std::vector<operf_event_t> & _evts,
	              bool _with_time_interval);
	~ocount_record();
	bool start_counting_app_process(pid_t _pid);
	bool start_counting_tasklist(std::vector<pid_t> _tasks, bool _are_threads);
	bool start_counting_cpulist(std::vector<int> _cpus);
	bool start_counting_syswide(void);
	void add_process(pid_t proc) { tasks_to_count.insert(proc); }
	void output_results(std::ostream & out, bool use_separation, bool short_format);
	bool get_valid(void) { return valid; }
	bool are_tasks_processes(void) { return !tasks_are_threads; }

private:
	void setup(void);
	int get_process_info(const std::vector<pid_t> & _procs);
	int _get_one_process_info(pid_t pid);
	int do_counting_per_cpu(void);
	int do_counting_per_task(void);
	void output_short_results(std::ostream & out, bool use_separation, bool scaled);
	void output_long_results(std::ostream & out, bool use_separation,
                                 int longest_event_name,
                                 bool scaled, u64 time_enabled);

	enum op_runmode runmode;
	bool tasks_are_threads;
	int num_cpus;
	pid_t app_pid;
	std::set<pid_t> tasks_to_count;
	std::set<int> cpus_to_count;
	bool system_wide;
	std::vector<ocount_counter> perfCounters;
	unsigned int total_bytes_recorded;
	std::vector<operf_event_t> evts;
	std::vector<pid_t> specified_tasks;
	std::vector<int> specified_cpus;
	std::vector<ocount_accum_t> accum_counts;  // accumulated across threads or cpus; one object per event

	/* The prev_accum_counts vector is used with time intervals for computing count values for just
	 * the current time interval. The number of elements in this vector depends on the run mode:
	 *   - For <command> [args] : vector size == evts.size()
	 *   - For system-wide or cpu list : vector size is number of processors
	 *   - For process or thread list : vector size is number of tasks
	 */
	std::vector<u64> prev_accum_counts;
	bool valid;
	bool with_time_interval;
	u64 start_time;
};


#endif /* OCOUNT_COUNTER_H_ */
