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
#include <asm/unistd.h>

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
	u64 aggregated_count;
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
	const std::string get_event_name(void) const { return event_name; }
	int read_count_data(ocount_accum_t * accum);

private:
	struct perf_event_attr attr;
	int fd;
	int cpu;
	pid_t pid;
	std::string event_name;
};

class ocount_record {
public:
	ocount_record(enum op_runmode _runmode, std::vector<operf_event_t> & _evts);
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
	void output_short_results(std::ostream & out, bool use_separation);
	void output_long_results(std::ostream & out, bool use_separation,
                                 int longest_event_name, bool scaled);

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
	std::vector<ocount_accum_t> accum_counts;  // same size as evts; one object for each event
	bool valid;
};


#endif /* OCOUNT_COUNTER_H_ */
