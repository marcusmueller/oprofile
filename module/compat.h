/* COPYRIGHT (C) 2002 Philippe Elie, based on discussion with John Levon
 * stuff here come from various source, linux kernel header, John's trick etc.
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

/* This file is intended to be up-to-date with the last linux version and
 * provide work-around for missing features in previous kernel version */

#ifndef COMPAT_H
#define COMPAT_H

#include <linux/version.h>

/* You want to keep this sorted by increasing linux version. Prefer checking
 * against linux version rather existence of macro */

/* missing header: work around are later in this file */

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,0)
#include <asm/apic.h>
#include <asm/apicdef.h>
#include <asm/mpspec.h>
#endif

#include "apic_up_compat.h"

/* provide a working smp_call_function when: < 2.2.8 || (!SMP && <= 2.2.20),
 * this means than we support all UP kernel from 2.2.0 and all SMP kernel from
 * 2.2.8 */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,2,8) || \
      (!defined(CONFIG_SMP) && LINUX_VERSION_CODE <= KERNEL_VERSION(2,2,20))

#undef smp_call_function
static int inline smp_call_function (void (*func) (void *info), void *info,
				     int retry, int wait)
{
	return 0;
}

#endif /* < 2.2.8 || (!SMP && <= 2.2.20) */


#if LINUX_VERSION_CODE < KERNEL_VERSION(2,2,2)

/* 2.2.2 introduced wait_event_interruptible */
#define __wait_event_interruptible(wq, condition, ret)	\
do {							\
	struct wait_queue __wait;			\
							\
	__wait.task = current;				\
	add_wait_queue(&wq, &__wait);			\
	for (;;) {					\
		current->state = TASK_INTERRUPTIBLE;	\
		if (condition)				\
			break;				\
		if (!signal_pending(current)) {		\
			schedule();			\
			continue;			\
		}					\
		ret = -ERESTARTSYS;			\
		break;					\
	}						\
	current->state = TASK_RUNNING;			\
	remove_wait_queue(&wq, &__wait);		\
} while (0)

#define wait_event_interruptible(wq, condition)				\
({									\
	int __ret = 0;							\
	if (!(condition))						\
		__wait_event_interruptible(wq, condition, __ret);	\
	__ret;								\
})

#endif /* LINUX_VERSION_CODE < KERNEL_VERSION(2,2,2) */

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,2,8)

/* 2.2.8 introduced rdmsr/wrmsr */
#define rdmsr(msr,val1,val2)				\
       __asm__ __volatile__("rdmsr"			\
			    : "=a" (val1), "=d" (val2)	\
			    : "c" (msr))
#define wrmsr(msr,val1,val2)					\
     __asm__ __volatile__("wrmsr"				\
			  : /* no outputs */			\
			  : "c" (msr), "a" (val1), "d" (val2))

#endif /* LINUX_VERSION_CODE < KERNEL_VERSION(2,2,8) */

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,2,18)

/* 2.2.18 introduced module_init */
/* Not sure what version aliases were introduced in, but certainly in 2.91.66.  */
#ifdef MODULE
  #if __GNUC__ > 2 || (__GNUC__ == 2 && __GNUC_MINOR__ >= 91)
    #define module_init(x)	int init_module(void) __attribute__((alias(#x)));
    #define module_exit(x)	void cleanup_module(void) __attribute__((alias(#x)));
  #else
    #define module_init(x)	int init_module(void) { return x(); }
    #define module_exit(x)	void cleanup_module(void) { x(); }
  #endif
#else
  #define module_init(x)
  #define module_exit(x)
#endif

/* 2.2.18 introduced vmalloc_32, FIXME is old vmalloc equivalent */
#define vmalloc_32 vmalloc

/* 2.2.18 add doubled linked list wait_queue and mutex */
#define DECLARE_WAIT_QUEUE_HEAD(q) struct wait_queue *q = NULL 
#define DECLARE_MUTEX(foo)	struct semaphore foo = MUTEX

/* 2.2.18 add THIS_MODULE */
#define THIS_MODULE (&__this_module)

#endif /* LINUX_VERSION_CODE < KERNEL_VERSION(2,2,18) */

/* 2.2.18 introduced the rtc lock */
#ifdef RTC_LOCK
#define lock_rtc(f) spin_lock_irqsave(&rtc_lock, f)
#define unlock_rtc(f) spin_unlock_irqrestore(&rtc_lock, f)
#else
#define lock_rtc(f) save_flags(f)
#define unlock_rtc(f) restore_flags(f)
#endif /* RTC_LOCK */
 
/* replacement of d_covers */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,4,0)
#define NEED_2_2_DENTRIES
extern int wind_dentries_2_2(struct dentry *dentry);
extern uint do_path_hash_2_2(struct dentry *dentry);
#define wind_dentries(d, v, r, m) wind_dentries_2_2(d)
#define hash_path(f) do_path_hash_2_2((f)->f_dentry)
#else
#define wind_dentries(d, v, r, m) wind_dentries_2_4(d, v, r, m)
#define hash_path(f) do_path_hash_2_4((f)->f_dentry, (f)->f_vfsmnt)
#endif /* LINUX_VERSION_CODE < KERNEL_VERSION(2,4,0) */

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,4,0)

/* different request_region */
#define request_region_check compat_request_region
void *compat_request_region (unsigned long start, unsigned long n, const char *name);
 
/* on 2.2, the APIC is never enabled on UP */
/* FIXME: what about smp running on an UP kernel */
#define NO_MPTABLE_CHECK_NEEDED
 
/* 2.2 has no cpu_number_map on UP */
#ifdef CONFIG_SMP
#error sorry, it will crash your machine right now 
#define op_cpu_id() cpu_number_map[smp_processor_id()]
#else
#define op_cpu_id() smp_processor_id()
#endif /* CONFIG_SMP */

#else

#define request_region_check request_region
 
#define op_cpu_id() cpu_number_map(smp_processor_id())

#endif /* LINUX_VERSION_CODE < KERNEL_VERSION(2,4,0) */

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,4,0)

/* TODO: add __cache_line_aligned_in_smp and put this stuff in its own file */
/* 2.4.0 have introduced __cacheline_aligned */

#include <asm/cache.h>

#ifndef L1_CACHE_ALIGN
#define L1_CACHE_ALIGN(x) (((x)+(L1_CACHE_BYTES-1))&~(L1_CACHE_BYTES-1))
#endif

#ifndef SMP_CACHE_BYTES
#define SMP_CACHE_BYTES L1_CACHE_BYTES
#endif

#ifndef ____cacheline_aligned
#define ____cacheline_aligned __attribute__((__aligned__(SMP_CACHE_BYTES)))
#endif

#ifndef __cacheline_aligned
#ifdef MODULE
#define __cacheline_aligned ____cacheline_aligned
#else
#define __cacheline_aligned					\
  __attribute__((__aligned__(SMP_CACHE_BYTES),			\
		 __section__(".data.cacheline_aligned")))
#endif
#endif /* __cacheline_aligned */

/* 2.4.0 introduced __exit, __init */
#define __exit
#define __init

/* 2.4.0 introduce virt_to_page */
#define virt_to_page(va) MAP_NR(va)

#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,0) */

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,0)

/* FIXME: consider apic_up_compat ... ok I take this later (phe) */
 
/* 2.4.0 introduced vfsmount cross mount point */
#define HAVE_CROSS_MOUNT_POINT

/* 2.4.0 introduced mmap2 syscall */
#define HAVE_MMAP2

#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,0) */


#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,0)
/* 2.4.0 change proto of pte_page */
#define pte_page_address(x) page_address(pte_page(x))
#else
/* 2.2.? to 2.2.20 make page_address(pte_page(x) and pte_page(x) equivalent */
#define pte_page_address(x) pte_page(x)
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,0)
#define GET_VM_OFFSET(v) ((v)->vm_pgoff << PAGE_SHIFT)
#define PTRACE_OFF(t) ((t)->ptrace &= ~PT_DTRACE)
#else
#define GET_VM_OFFSET(v) ((v)->vm_offset)
#define PTRACE_OFF(t) ((t)->flags &= ~PF_DTRACE)
#endif

/* 2.4.3 introduced rw mmap semaphore  */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,3)
#define take_mmap_sem(mm) down_read(&mm->mmap_sem)
#define release_mmap_sem(mm) up_read(&mm->mmap_sem)
#else
#define take_mmap_sem(mm) down(&mm->mmap_sem)
#define release_mmap_sem(mm) up(&mm->mmap_sem)
#endif

/* 2.4.7 introduced the completion API */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,7)
#include <linux/completion.h>
#else
#define DECLARE_COMPLETION(x)	DECLARE_MUTEX_LOCKED(x)
#define init_completion(x)
#define complete_and_exit(x, y) up_and_exit((x), (y))
#define wait_for_completion(x) down(x)
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,4,10)

/* 2.4.10 introduced MODULE_LICENSE */
#define MODULE_LICENSE(x)

/* 2.4.10 introduced APIC setup under normal APIC config */
#ifndef CONFIG_X86_UP_APIC
#define NEED_FIXMAP_HACK
#endif

#endif /* LINUX_VERSION_CODE < KERNEL_VERSION(2,4,10) */

/* Things that can not rely on a particular linux version */

/* 2.4/2.5 kernel can be  patched with the preempt patch. We support only
 * recent version of this patch */
#ifndef preempt_disable
#define preempt_disable()    do { } while (0)
#define preempt_enable_no_resched() do { } while (0)
#define preempt_enable()     do { } while (0)
#endif

/* Compiler work-around */

/* work-around compiler bug in gcc 2.91.66, just mark all input register as
 * magically cloberred by wrmsr */
#if __GNUC__ == 2 && __GNUC_MINOR__ == 91
#undef wrmsr
#define wrmsr(msr,val1,val2)					\
     __asm__ __volatile__("wrmsr"				\
			  : /* no outputs */			\
			  : "c" (msr), "a" (val1), "d" (val2)	\
			  : "ecx", "eax", "edx")
#endif

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

#endif /* COMPAT_H */
