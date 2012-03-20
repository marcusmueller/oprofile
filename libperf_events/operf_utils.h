/**
 * @file operf_utils.h
 * Header file containing definitions for handling a user request to profile
 * using the new Linux Performance Events Subsystem.
 *
 * @remark Copyright 2011 OProfile authors
 * @remark Read the file COPYING
 *
 * Created on: Dec 7, 2011
 * @author Maynard Johnson
 * (C) Copyright IBM Corp. 2011
 *
 */

#ifndef OPERF_H_
#define OPERF_H_

#include <linux/perf_event.h>
#include <dirent.h>
#include <vector>
#include "config.h"
#include "op_config.h"
#include "op_types.h"
#include "operf_event.h"
#include <signal.h>

namespace operf_options {
extern bool system_wide;
extern bool reset;
extern int pid;
extern int callgraph_depth;
extern int mmap_pages_mult;
extern std::string session_dir;
extern bool separate_cpu;
}

extern bool no_vmlinux;

#define OP_APPNAME_LEN 1024

#if BITS_PER_LONG == 64
#define MMAP_WINDOW_SZ ULLONG_MAX
#else
#define MMAP_WINDOW_SZ (32 * 1024 * 1024ULL)
#endif

extern unsigned int op_nr_counters;

static inline size_t align_64bit(u64 x)
{
	u64 mask = 7ULL;
	return (x + mask) & ~mask;
}

class operf_record;
namespace OP_perf_utils {
typedef struct vmlinux_info {
	std::string image_name;
	u64 start, end;
} vmlinux_info_t;
void op_record_kernel_info(std::string vmlinux_file, u64 start_addr, u64 end_addr,
                           int output_fd, operf_record * pr);
void op_get_kernel_event_data(struct mmap_data *md, operf_record * pr);
void op_perfrecord_sigusr1_handler(int sig __attribute__((unused)),
		siginfo_t * siginfo __attribute__((unused)),
		void *u_context __attribute__((unused)));
int op_record_process_info(pid_t pid, operf_record * pr, int output_fd);
int op_write_output(int output, void *buf, size_t size);
int op_write_event(event_t * event);
int op_read_from_stream(std::ifstream & is, char * buf, std::streamsize sz);
int op_mmap_trace_file(struct mmap_info & info);
event_t * op_get_perf_event(struct mmap_info & info);
int op_get_next_online_cpu(DIR * dir, struct dirent *entry);
bool op_convert_event_vals(std::vector<operf_event_t> * evt_vec);
bool get_build_id(char * buildid);
u64 get_checksum_for_file(std::string filename);
}

// The rmb() macros were borrowed from perf.h in the kernel tree
#if defined(__i386__)
#include <asm/unistd.h>
#define rmb()		asm volatile("lock; addl $0,0(%%esp)" ::: "memory")
#define cpu_relax()	asm volatile("rep; nop" ::: "memory");
#endif

#if defined(__x86_64__)
#include <asm/unistd.h>
#define rmb()		asm volatile("lfence" ::: "memory")
#define cpu_relax()	asm volatile("rep; nop" ::: "memory");
#endif

#ifdef __powerpc__
#include <asm/unistd.h>
#define rmb()		asm volatile ("sync" ::: "memory")
#define cpu_relax()	asm volatile ("" ::: "memory");
#endif

#ifdef __s390__
#include <asm/unistd.h>
#define rmb()		asm volatile("bcr 15,0" ::: "memory")
#define cpu_relax()	asm volatile("" ::: "memory");
#endif

#ifdef __sh__
#include <asm/unistd.h>
#if defined(__SH4A__) || defined(__SH5__)
# define rmb()		asm volatile("synco" ::: "memory")
#else
# define rmb()		asm volatile("" ::: "memory")
#endif
#define cpu_relax()	asm volatile("" ::: "memory")
#endif

#ifdef __hppa__
#include <asm/unistd.h>
#define rmb()		asm volatile("" ::: "memory")
#define cpu_relax()	asm volatile("" ::: "memory");
#endif

#ifdef __sparc__
#include <asm/unistd.h>
#define rmb()		asm volatile("":::"memory")
#define cpu_relax()	asm volatile("":::"memory")
#endif

#ifdef __alpha__
#include <asm/unistd.h>
#define rmb()		asm volatile("mb" ::: "memory")
#define cpu_relax()	asm volatile("" ::: "memory")
#endif

#ifdef __ia64__
#include <asm/unistd.h>
#define rmb()		asm volatile ("mf" ::: "memory")
#define cpu_relax()	asm volatile ("hint @pause" ::: "memory")
#endif

#ifdef __arm__
#include <asm/unistd.h>
/*
 * Use the __kuser_memory_barrier helper in the CPU helper page. See
 * arch/arm/kernel/entry-armv.S in the kernel source for details.
 */
#define rmb()		((void(*)(void))0xffff0fa0)()
#define cpu_relax()	asm volatile("":::"memory")
#endif

#ifdef __mips__
#include <asm/unistd.h>
#define rmb()		asm volatile(					\
				".set	mips2\n\t"			\
				"sync\n\t"				\
				".set	mips0"				\
				: /* no output */			\
				: /* no input */			\
				: "memory")
#define cpu_relax()	asm volatile("" ::: "memory")
#endif

#endif // OPERF_H_
