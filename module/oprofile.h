/**
 * @file oprofile.h
 * Main driver code
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author John Levon <moz@compsoc.man.ac.uk>
 * @author Philippe Elie <phil_el@wanadoo.fr>
 */

#ifndef OPROFILE_H
#define OPROFILE_H

#include <linux/version.h>
#include <linux/module.h>
#include <linux/config.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/vmalloc.h>
#include <linux/sched.h>
#include <linux/sysctl.h>
#include <linux/smp_lock.h>

#include <asm/uaccess.h>

#include "compat.h"

#include "op_config.h"
#include "op_hw_config.h"
#include "op_interface.h"

#undef min
#undef max

#define streq(a, b) (!strcmp((a), (b)))
#define streqn(a, b, len) (!strncmp((a), (b), (len)))

#define regparm3 __attribute__((regparm(3)))

#define OP_NR_ENTRY (SMP_CACHE_BYTES/sizeof(struct op_sample))

struct op_entry {
	struct op_sample samples[OP_NR_ENTRY];
} __cacheline_aligned;

/* per-cpu dynamic data */
struct _oprof_data {
	/* hash table */
	struct op_entry * entries;
	/* eviction buffer */
	struct op_sample * buffer;
	/* nr. in hash table */
	uint hash_size;
	/* nr. in buffer */
	uint buf_size;
	/* next in buffer (atomic) */
	uint nextbuf;
	/* next sample in entry */
	uint next;
	/* number of IRQs for this CPU */
	uint nr_irq;
	/* reset counter values */
	uint ctr_count[OP_MAX_COUNTERS];
} __cacheline_aligned;

/* reflect /proc/sys/dev/oprofile/#counter files */
struct oprof_counter {
	int count;
	int enabled;
	int event;
	int kernel;
	int user;
	int unit_mask;
};

/* reflect /proc/sys/dev/oprofile files */
struct oprof_sysctl {
	/* nr. in eviction buffser */
	int buf_size;
	/* nr. in hash table */
	int hash_size;
	/* sysctl dump */
	int dump;
	/* dump and stop */
	int dump_stop;
	/* is profiling kernel only */
	int kernel_only;
	/* nr. in note buffer */
	int note_size;
	/* nr. interrupts occured */
	int nr_interrupts;
	/* the cpu core type: CPU_PPRO, CPU_PII ... */
	int cpu_type;
	/* counter setup */
	struct oprof_counter ctr[OP_MAX_COUNTERS];
};

/**
 * A interrupt handler must implement these routines.
 * When an interrupt arrives, it must eventually call
 * op_do_profile().
 */
struct op_int_operations {
	/* initialise the handler on module load, if this function fail
	 * oprofile try to fall back to RTC mode, on failure deinit handler is
	 * not called so all resource allocated by init() must be freed
	 * before returning an error code */
	int (*init)(void);
	/* deinitialise on module unload */
	void (*deinit)(void);
	/* add any handler-specific sysctls at the position given by @next */
	int (*add_sysctls)(ctl_table *next);
	/* remove handler-specific sysctls */
	void (*remove_sysctls)(ctl_table *next);
	/* check given profiling parameters are correct */
	int (*check_params)(void);
	/* setup the handler from profiling parameters */
	int (*setup)(void);
	/* start profiling on all CPUs */
	void (*start)(void);
	/* stop profiling on all CPUs */
	void (*stop)(void);
	/* start profiling on the given CPU */
	void (*start_cpu)(uint);
	/* stop profiling on the given CPU */
	void (*stop_cpu)(uint);
};

/* maximum depth of dname trees - this is just a page */
#define DNAME_STACK_MAX 1024

/* is the count at maximal value ? */
#define op_full_count(c) (((c) & OP_COUNT_MASK) == OP_COUNT_MASK)

/* no check for ctr needed as one of the three will differ in the hash */
#define op_miss(ops)  \
	((ops).eip != regs->eip || \
	(ops).pid != current->pid || \
	op_full_count((ops).count))

/* the ctr bit is used to separate the two counters.
 * Simple and effective hash. If you can do better, prove it ...
 */
#define op_hash(eip, pid, ctr) \
	(((eip ) + (pid << 5) + (ctr)) & (data->hash_size - 1))

/* oprof_start() copy here the sysctl settable parameters */
extern struct oprof_sysctl sysctl;

int oprof_init(void);
void oprof_exit(void);
unsigned long is_map_ready(void);
int oprof_hash_map_open(void);
int oprof_hash_map_release(void);
int oprof_hash_map_mmap(struct file *file, struct vm_area_struct *vma);
int oprof_map_open(void);
int oprof_map_release(void);
int oprof_init_hashmap(void);
void oprof_free_hashmap(void);

/* used by interrupt handlers if the underlined harware doesn't support
 * performance counter */
extern struct op_int_operations op_rtc_ops;

void regparm3 op_do_profile(uint cpu, struct pt_regs *regs, int ctr);
extern struct _oprof_data oprof_data[NR_CPUS];
extern int partial_stop;
extern struct oprof_sysctl sysctl_parms;
extern int nr_oprof_static;
extern int lproc_dointvec(ctl_table *table, int write, struct file *filp, void *buffer, size_t *lenp);

/* functionnality provided by the architecture dependant file */
/* must return OP_RTC if the hardware doesn't support something like
 * perf counter */
op_cpu get_cpu_type(void);
/* return an interface pointer, this function is called only if get_cpu_type
 * doesn't return OP_RTC */
struct op_int_operations const * op_int_interface(void);
/* intercept the needed syscall */
void op_intercept_syscalls(void);
void op_restore_syscalls(void);
void op_save_syscalls(void);

#endif /* OPROFILE_H */
