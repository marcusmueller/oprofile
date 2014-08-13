/**
 * @file libperf_events/operf_counter.h
 * C++ class definition that abstracts the user-to-kernel interface
 * for using Linux Performance Events Subsystem.
 *
 * @remark Copyright 2011 OProfile authors
 * @remark Read the file COPYING
 *
 * Created on: Dec 7, 2011
 * @author Maynard Johnson
 * (C) Copyright IBM Corp. 2011
 *
 * Modified by Maynard Johnson <maynardj@us.ibm.com>
 * (C) Copyright IBM Corporation 2012, 2014
 *
 */

#ifndef OPERF_COUNTER_H_
#define OPERF_COUNTER_H_

#include <sys/mman.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <stdint.h>
#include <poll.h>
#include <string>
#include <vector>
#include <map>
#include <stdexcept>
#include <limits.h>
#include <istream>
#include <fstream>
#include "operf_event.h"
#include "op_cpu_type.h"
#include "operf_utils.h"

extern char * start_time_human_readable;

class operf_record;

#define OP_BASIC_SAMPLE_FORMAT (PERF_SAMPLE_ID | PERF_SAMPLE_IP \
    | PERF_SAMPLE_TID)

static inline int
op_perf_event_open(struct perf_event_attr * attr,
		      pid_t pid, int cpu, int group_fd,
		      unsigned long flags)
{
	return syscall(__NR_perf_event_open, attr, pid, cpu,
			       group_fd, flags);
}

#define OP_PERF_HANDLED_ERROR -101
#define OP_PERF_NO_SAMPLE_ID 0xdeadbeefdeadbeefULL


class operf_counter {
public:
	operf_counter(operf_event_t & evt, bool enable_on_exec, bool callgraph,
	              bool separate_by_cpu, bool inherit, int event_number);
	~operf_counter();
	int perf_event_open(pid_t pid, int cpu, operf_record * pr, bool print_error);
	const struct perf_event_attr * the_attr(void) const { return &attr; }
	int get_fd(void) const { return fd; }
	int get_id(void) const { return id; }
	int get_evt_num(void) const { return evt_num; }
	const std::string get_event_name(void) const { return event_name; }

private:
	struct perf_event_attr attr;
	int fd;
	int id;
	int evt_num;
	std::string event_name;
};


class operf_record {
public:
	/* For system-wide profiling, set sys_wide=true, the_pid=-1, and pid_running=false.
	 * For single app profiling, set sys_wide=false, the_pid=<processID-to-profile>,
	 * and pid_running=true if profiling an already active process; otherwise false.
	 */
	operf_record(int output_fd, bool sys_wide, pid_t the_pid, bool pid_running,
	             std::vector<operf_event_t> & evts, OP_perf_utils::vmlinux_info_t vi,
	             bool callgraph, bool separate_by_cpu, bool output_fd_is_file,
	             int _convert_read_pipe, int _convert_write_pipe);
	~operf_record();
	void recordPerfData(void);
	int out_fd(void) const { return output_fd; }
	void add_to_total(int n) { total_bytes_recorded += n; }
	void add_process(struct comm_event proc) { procs[proc.tid] = proc; }
	unsigned int get_total_bytes_recorded(void) const { return total_bytes_recorded; }
	void register_perf_event_id(unsigned counter, u64 id, perf_event_attr evt_attr);
	bool get_valid(void) { return valid; }

private:
	void create(std::string outfile, std::vector<operf_event_t> & evts);
	void setup(void);
	int prepareToRecord(void);
	int _prepare_to_record_one_fd(int idx, int fd);
	int _start_recoding_new_thread(pid_t id);
	void record_process_info(void);
	void write_op_header_info(void);
	int _write_header_to_file(void);
	int _write_header_to_pipe(void);
	int output_fd;
	int read_comm_pipe;
	int write_comm_pipe;
	bool write_to_file;
	// Array of size 'num_cpus_used_for_perf_event_open * num_pids * num_events'
	struct pollfd * poll_data;
	std::vector<struct mmap_data> samples_array;
	int num_cpus;
	pid_t pid_to_profile;
	/* When doing --pid or --system-wide profiling, we'll obtain process information
	 * for all processes to be profiled (including forked/cloned processes) and store
	 * that information in a collection of type 'comm_event'.  We'll use this collection
	 * for synthesizing PERF_RECORD_COMM events into the profile data stream.
	 */
	std::map<u32, struct comm_event> procs;
	bool pid_started;
	bool system_wide;
	bool callgraph;
	bool separate_cpu;
	std::vector<operf_counter> perfCounters;
	unsigned int total_bytes_recorded;
	int poll_count;
	struct OP_header opHeader;
	std::vector<operf_event_t> evts;
	bool valid;
	std::string vmlinux_file;
	u64 kernel_start, kernel_end;
};

class operf_read {
public:
	operf_read(std::vector<operf_event_t> & _evts)
	: sample_data_fd(-1), inputFname(""), evts(_evts), cpu_type(CPU_NO_GOOD)
	  { valid = syswide = false;
	  write_comm_pipe = read_comm_pipe = 1;
	  post_profiling_pipe = -1; }
	void init(int sample_data_pipe_fd, std::string input_filename, std::string samples_dir, op_cpu cputype,
	          bool systemwide, int _record_write_pipe, int _record_read_pipe,
	          int _post_profiling_pipe);
	~operf_read();
	int readPerfHeader(void);
	unsigned int convertPerfData(void);
	bool is_valid(void) {return valid; }
	int get_eventnum_by_perf_event_id(u64 id) const;
	inline const operf_event_t * get_event_by_counter(u32 counter) { return &evts[counter]; }
	int get_write_comm_pipe(void) { return write_comm_pipe; }
	int get_read_comm_pipe(void)  { return read_comm_pipe; }
	void add_sample_id_to_opHeader(u64 sample_id);

private:
	int sample_data_fd;
	int write_comm_pipe;
	int read_comm_pipe;
	int post_profiling_pipe;
	std::string inputFname;
	std::string sampledir;
	std::ifstream istrm;
	struct OP_header opHeader;
	std::vector<operf_event_t> & evts;
	bool valid;
	bool syswide;
	op_cpu cpu_type;
	int _read_header_info_with_ifstream(void);
	int _read_perf_header_from_file(void);
	int _read_perf_header_from_pipe(void);
};


#endif /* OPERF_COUNTER_H_ */
