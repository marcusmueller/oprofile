/**
 * @file pe_profiling/operf_counter.h
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
 */

#ifndef OPERF_COUNTER_H_
#define OPERF_COUNTER_H_

#include <sys/mman.h>
#include <asm/unistd.h>
#include <stdint.h>
#include <poll.h>
#include <string>
#include <vector>
#include <map>
#include <stdexcept>
#include <limits.h>
#include <signal.h>
#include <istream>
#include <fstream>
#include <dirent.h>
#include "operf_event.h"
#include "op_cpu_type.h"

class operf_record;

#define OP_BASIC_SAMPLE_FORMAT (PERF_SAMPLE_ID | PERF_SAMPLE_IP \
    | PERF_SAMPLE_TID | PERF_SAMPLE_CPU | PERF_SAMPLE_STREAM_ID)

static inline int
op_perf_event_open(struct perf_event_attr * attr,
		      pid_t pid, int cpu, int group_fd,
		      unsigned long flags)
{
	return syscall(__NR_perf_event_open, attr, pid, cpu,
			       group_fd, flags);
}

#define NUM_MMAP_PAGES 8
#define OP_PERF_HANDLED_ERROR -101


class operf_counter {
public:
	operf_counter(operf_event_t evt);
	~operf_counter();
	int perf_event_open(pid_t ppid, int cpu, unsigned counter, operf_record * pr);
	const struct perf_event_attr * the_attr(void) const { return &attr; }
	int get_fd(void) const { return fd; }
	int get_id(void) const { return id; }
	const string get_event_name(void) const { return event_name; }

private:
	struct perf_event_attr attr;
	int fd;
	int id;
	string event_name;
};


class operf_record {
public:
	operf_record(string outfile, pid_t the_pid, vector<operf_event_t> & evts);
	~operf_record();
	void recordPerfData(void);
	int out_fd(void) const { return outputFile; }
	void add_to_total(int n) { total_bytes_recorded += n; }
	int get_total_bytes_recorded(void) const { return total_bytes_recorded; }
	void register_perf_event_id(unsigned counter, u64 id, perf_event_attr evt_attr);
	bool get_valid(void) { return valid; }

private:
	void setup(void);
	int prepareToRecord(int counter, int cpu, int fd);
	void write_op_header_info(void);
	int outputFile;
	struct pollfd * poll_data;
	vector< vector<struct mmap_data> > samples_array;
	int num_cpus;
	pid_t pid;
	vector< vector<operf_counter> > perfCounters;
	int total_bytes_recorded;
	int poll_count;
	struct OP_header opHeader;
	vector<operf_event_t> evts;
	bool valid;
};

class operf_read {
public:
	operf_read(void) { valid = false; }
	void init(string infile, string samples_dir, op_cpu cputype, vector<operf_event_t> & evts);
	~operf_read();
	int readPerfHeader(void);
	int convertPerfData(void);
	bool is_valid(void) {return valid; }
	int get_eventnum_by_perf_event_id(u64 id) const;
	inline const operf_event * get_event_by_counter(u32 counter) { return &evts[counter]; }

private:
	string inputFname;
	string sampledir;
	ifstream istrm;
	struct OP_header opHeader;
	vector<operf_event_t> evts;
	bool valid;
	op_cpu cpu_type;
	void read_op_header_info_with_ifstream(void);
};


#endif /* OPERF_COUNTER_H_ */
