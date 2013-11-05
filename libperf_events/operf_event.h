/*
 * @file pe_profiling/operf_event.h
 * Definitions of structures and methods for handling perf_event data
 * from the kernel.
 *
 * @remark Copyright 2011 OProfile authors
 * @remark Read the file COPYING
 *
 * Created on: Dec 7, 2011
 * @author Maynard Johnson
 * (C) Copyright IBM Corp. 2011
 */

#ifndef OPERF_EVENT_H_
#define OPERF_EVENT_H_

#include <limits.h>
#include <linux/perf_event.h>
#include <vector>
#include "op_types.h"


#define OP_MAX_EVT_NAME_LEN 64
#define OP_MAX_UM_NAME_LEN 64
#define OP_MAX_UM_NAME_STR_LEN 17
#define OP_MAX_NUM_EVENTS 512

struct ip_event {
	struct perf_event_header header;
	u64 ip;
	u32 pid, tid;
	unsigned char __more_data[];
};

struct mmap_event {
	struct perf_event_header header;
	u32 pid, tid;
	u64 start;
	u64 len;
	u64 pgoff;
	char filename[PATH_MAX];
};

struct comm_event {
	struct perf_event_header header;
	u32 pid, tid;
	char comm[16];
};

struct fork_event {
	struct perf_event_header header;
	u32 pid, ppid;
	u32 tid, ptid;
	u64 time;
};

struct lost_event {
	struct perf_event_header header;
	u64 id;
	u64 lost;
};

struct read_event {
	struct perf_event_header header;
	u32 pid, tid;
	u64 value;
	u64 time_enabled;
	u64 time_running;
	u64 id;
};

struct sample_event {
	struct perf_event_header header;
	u64 array[];
};

struct throttle_event {
	struct perf_event_header header;
	u64 time;
	u64 id;
	u64 stream_id;
};

typedef union event_union {
	struct perf_event_header header;
	struct ip_event	ip;
	struct mmap_event mmap;
	struct comm_event comm;
	struct fork_event fork;
	struct lost_event lost;
	struct read_event read;
	struct sample_event sample;
	struct throttle_event throttle;
} event_t;

struct mmap_data {
	void *base;
	u64 mask;
	u64 prev;
};

struct ip_callchain {
	u64 nr;
	u64 ips[0];
};
struct sample_data {
	u64 ip;
	u32 pid, tid;
	u64 time;
	u64 addr;
	u64 id;
	u32 cpu;
	u64 period;
	u32 raw_size;
	void *raw_data;
	struct ip_callchain * callchain;
};


typedef struct operf_event {
	char name[OP_MAX_EVT_NAME_LEN];
	// code for perf_events
	u64 evt_code;
	/* Base event code for oprofile sample file management; may be the same as evt_code,
	 * but different for certain architectures (e.g., ppc64).  Also, when unit masks
	 * are used, the evt_code to be passed to perf_events includes both the
	 * base code from op_evt_code and the left-shifted unit mask bits.
	 */
	u64 op_evt_code;
	// Make the evt_um and count fields unsigned long to match op_default_event_descr
	unsigned long evt_um;
	char um_name[OP_MAX_UM_NAME_LEN];
	unsigned long count;
	bool no_kernel;
	bool no_user;
	bool no_hv;
	bool mode_specified; /* user specified user or kernel modes */
	bool umask_specified; /* user specified a unit mask */
	char um_numeric_val_as_str[OP_MAX_UM_NAME_STR_LEN];
	bool throttled;  /* set to true if the event is ever throttled */
} operf_event_t;

struct mmap_info {
	u64 offset, file_data_size, file_data_offset, head;
	char * buf;
	int traceFD;
};


struct op_file_section {
	u64 size;
	u64 offset;
};

struct op_file_attr {
	struct perf_event_attr	attr;
	struct op_file_section ids;
};

struct op_header_evt_info {
	struct perf_event_attr attr;
	std::vector<u64> ids;
	off_t id_offset;
};

struct OP_file_header {
	u64				magic;
	u64				size;
	u64				attr_size;
	struct op_file_section	attrs;
	struct op_file_section	data;
};

struct OP_header {
	struct op_header_evt_info h_attrs[OP_MAX_NUM_EVENTS];
	off_t			attr_offset;
	off_t			data_offset;
	u64			data_size;
};
/* Some of the above definitions were borrowed from the perf tool's util/event.h file. */

#endif /* OPERF_EVENT_H_ */
