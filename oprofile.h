/* $Id: oprofile.h,v 1.61 2001/10/16 21:31:03 movement Exp $ */
/* COPYRIGHT (C) 2000 THE VICTORIA UNIVERSITY OF MANCHESTER and John Levon
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59 Temple
 * Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <linux/module.h>
#include <linux/config.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/vmalloc.h>
#include <linux/sched.h>
#include <linux/sysctl.h>

#include <asm/uaccess.h>
#include <asm/smplock.h>
#include <asm/apic.h>

/* userspace/module interface */
#include "op_user.h"

#undef min
#undef max
 
#define OP_NR_ENTRY (SMP_CACHE_BYTES/sizeof(struct op_sample))

struct op_entry {
	struct op_sample samples[OP_NR_ENTRY];
} __attribute__((__aligned__(SMP_CACHE_BYTES)));

/* per-cpu dynamic data */
struct _oprof_data {
	struct op_entry *entries; /* hash table */
	struct op_sample *buffer; /* eviction buffer */
	uint hash_size; /* nr. in hash table */
	uint buf_size; /* nr. in buffer */
	uint nextbuf; /* next in buffer (atomic) */
	uint next; /* next sample in entry */
	uint ctr_count[OP_MAX_COUNTERS]; /* reset counter values */
} __attribute__((__aligned__(SMP_CACHE_BYTES)));

/* MSRs */
#ifndef MSR_IA32_PERFCTR0
#define MSR_IA32_PERFCTR0 0xc1
#endif
#ifndef MSR_IA32_PERFCTR1
#define MSR_IA32_PERFCTR1 0xc2
#endif
#ifndef MSR_IA32_EVNTSEL0
#define MSR_IA32_EVNTSEL0 0x186
#endif
#ifndef MSR_IA32_EVNTSEL1
#define MSR_IA32_EVNTSEL1 0x187
#endif
#ifndef MSR_IA32_APICBASE
#define MSR_IA32_APICBASE 0x1B
#endif
#ifndef MSR_K7_PERFCTL0
#define MSR_K7_PERFCTL0 0xc0010000
#endif
#ifndef MSR_K7_PERFCTL1
#define MSR_K7_PERFCTL1 0xc0010001
#endif
#ifndef MSR_K7_PERFCTL2
#define MSR_K7_PERFCTL2 0xc0010002
#endif
#ifndef MSR_K7_PERFCTL3
#define MSR_K7_PERFCTL3 0xc0010003
#endif
#ifndef MSR_K7_PERFCTR0
#define MSR_K7_PERFCTR0 0xc0010004
#endif
#ifndef MSR_K7_PERFCTR1
#define MSR_K7_PERFCTR1 0xc0010005
#endif
#ifndef MSR_K7_PERFCTR2
#define MSR_K7_PERFCTR2 0xc0010006
#endif
#ifndef MSR_K7_PERFCTR3
#define MSR_K7_PERFCTR3 0xc0010007
#endif

#ifndef APIC_SPIV_APIC_ENABLED
#define APIC_SPIV_APIC_ENABLED (1<<8)
#endif

#define streq(a, b) (!strcmp((a), (b)))
#define streqn(a, b, len) (!strncmp((a), (b), (len)))

/* ready will be set this many samples before the end of the 
 * eviction buffer.
 * The purpose of this is to avoid overflowing the sample
 * buffer - though if we do overflow, nothing too bad will
 * happen.
 */
#define OP_PRE_WATERMARK 2048

/* ready will be set this many notes before the end of the 
 * note buffer.
 */
#define OP_PRE_NOTE_WATERMARK 64

/* maximum depth of dname trees - this is just a page */
#define DNAME_STACK_MAX 1024

/* maximum number of events between
 * interrupts. Counters are 40 bits, but
 * for convenience we only use 32 bits.
 * The top bit is used for overflow detection,
 * so user can set up to (2^31)-1 */
#define OP_MAX_PERF_COUNT 2147483647UL

/* is the count at maximal value ? */
#define op_full_count(c) (((c) & OP_COUNT_MASK) == OP_COUNT_MASK)

/* no check for ctr needed as one of the three will differ in the hash */
#define op_miss(ops)  \
	((ops).eip != regs->eip || \
	(ops).pid != current->pid || \
	op_full_count((ops).count))

/* the top half of pid is likely to remain static,
   so it's masked off. the ctr bit is used to separate
   the two counters */
#define op_hash(eip, pid, ctr) \
	((((((eip&0xff000)>>3) ^ eip) ^ (pid&0xff)) ^ (eip<<9)) \
	^ (ctr<<8)) & (data->hash_size - 1)

/* read/write of perf counters */
#define get_perfctr(l,h,c) do { rdmsr(perfctr_msr[(c)], (l), (h)); } while (0)
#define set_perfctr(l,c) do { wrmsr(perfctr_msr[(c)], -(u32)(l), -1); } while (0)
#define ctr_overflowed(n) (!((n) & (1U<<31)))

#define op_check_range(val,l,h,str) do { \
        if ((val) < (l) || (val) > (h)) { \
                printk(str, (val), (l), (h)); \
                return 0; \
        } } while (0);

asmlinkage void op_nmi(void);

/* for installing and restoring the NMI handler */

#define store_idt(addr) \
	do { \
		__asm__ __volatile__ ( "sidt %0" \
			: "=m" (addr) \
			: : "memory" ); \
	} while (0)

#define _set_gate(gate_addr,type,dpl,addr) \
	do { \
		int __d0, __d1; \
		__asm__ __volatile__ ( \
			"movw %%dx,%%ax\n\t" \
			"movw %4,%%dx\n\t" \
			"movl %%eax,%0\n\t" \
			"movl %%edx,%1" \
			:"=m" (*((long *) (gate_addr))), \
			"=m" (*(1+(long *) (gate_addr))), "=&a" (__d0), "=&d" (__d1) \
			:"i" ((short) (0x8000 + (dpl<<13) + (type<<8))), \
			"3" ((char *) (addr)),"2" (__KERNEL_CS << 16)); \
	} while (0)

struct _descr { u16 limit; u32 base; } __attribute__((__packed__));
struct _idt_descr { u32 a; u32 b; } __attribute__((__packed__));

#define op_cpu_id() (cpu_number_map(smp_processor_id()))

/* branch prediction */
#ifndef likely
#ifdef EXPECT_OK
#define likely(a) __builtin_expect((a), 1)
#else
#define likely(a) (a)
#endif
#endif
#ifndef unlikely
#ifdef EXPECT_OK
#define unlikely(a) __builtin_expect((a), 0)
#else
#define unlikely(a) (a)
#endif
#endif
 
/* we can't unload safely on SMP */
#ifdef CONFIG_SMP
#define smp_can_unload() (allow_unload)
#else
#define smp_can_unload() 1
#endif
 
// 2.4.3 introduced rw mmap semaphore 
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,4,3)
#define take_mmap_sem(mm) down(&mm->mmap_sem)
#define release_mmap_sem(mm) up(&mm->mmap_sem)
#else
#define take_mmap_sem(mm) down_read(&mm->mmap_sem)
#define release_mmap_sem(mm) up_read(&mm->mmap_sem)
#endif

#ifndef COMPLETION_H
#define DECLARE_COMPLETION(x)	DECLARE_MUTEX_LOCKED(x)
#define init_completion(x)
#define complete_and_exit(x, y) up_and_exit((x), (y))
#define wait_for_completion(x) down(x)
#else
#include <linux/completion.h>
#endif

// 2.4.10 introduced APIC setup under normal APIC config
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,4,10)
#ifndef CONFIG_X86_UP_APIC
#define NEED_FIXMAP_HACK
#endif
#endif
 
// 2.4.10 introduced MODULE_LICENSE
#ifndef MODULE_LICENSE
#define MODULE_LICENSE(x)
#endif
 
/* These arrays are filled by hw_ok() */
extern uint perfctr_msr[OP_MAX_COUNTERS];
extern uint eventsel_msr[OP_MAX_COUNTERS];

void * rvmalloc(signed long size); 
void rvfree(void * mem, signed long size);
unsigned long kvirt_to_pa(unsigned long adr); 
int oprof_init(void);
void oprof_exit(void);
void my_set_fixmap(void);
void op_intercept_syscalls(void);
void op_replace_syscalls(void);
void op_save_syscalls(void);
int is_map_ready(void);
int oprof_hash_map_open(void);
int oprof_hash_map_release(void);
int oprof_hash_map_mmap(struct file *file, struct vm_area_struct *vma);
int oprof_map_open(void);
int oprof_map_release(void);
int oprof_map_read(char *buf, size_t count, loff_t *ppos);
int oprof_init_hashmap(void);
void oprof_free_hashmap(void);
void find_intel_smp(void);
void lvtpc_apic_setup(void *dummy);
void lvtpc_apic_restore(void *dummy);
void install_nmi(void);
void restore_nmi(void);
int apic_setup(void);
