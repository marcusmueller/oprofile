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

#ifndef COMPAT22_H
#define COMPAT22_H

#include "apic_up_compat.h"
 
/* FIXME: didn't this change in 2.2.21 ? */ 
#define pte_page_address(x) pte_page(x)
#define GET_VM_OFFSET(v) ((v)->vm_offset) 
#define take_mmap_sem(mm) down(&mm->mmap_sem) 
#define release_mmap_sem(mm) up(&mm->mmap_sem)
#define MODULE_LICENSE(l)
 
#define INC_USE_COUNT_MAYBE MOD_INC_USE_COUNT
#define DEC_USE_COUNT_MAYBE MOD_DEC_USE_COUNT
 
extern int wind_dentries_2_2(struct dentry *dentry);
extern uint do_path_hash_2_2(struct dentry *dentry);
#define wind_dentries(d, v, r, m) wind_dentries_2_2(d)
#define hash_path(f) do_path_hash_2_2((f)->f_dentry)

/* different request_region */
#define request_region_check compat_request_region
void *compat_request_region (unsigned long start, unsigned long n, const char *name);
 
#define __exit
#define __init
#define virt_to_page(va) MAP_NR(va)

/* 2.2 has no cpu_number_map on UP */
#ifdef CONFIG_SMP
	#error sorry, it will crash your machine right now 
	#define op_cpu_id() cpu_number_map[smp_processor_id()]
#else
	#define op_cpu_id() smp_processor_id()
	/* on 2.2, the APIC is never enabled on UP */
	/* FIXME: what about smp running on an UP kernel */
	#define NO_MPTABLE_CHECK_NEEDED
#endif /* CONFIG_SMP */

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

 
/* provide a working smp_call_function when: < 2.2.8 || (!SMP && <= 2.2.20),
 * this means than we support all UP kernel from 2.2.0 and all SMP kernel from
 * 2.2.8 */
#if VBEFORE(2,2,8) || (!defined(CONFIG_SMP) && VBEFORE(2,2,21))

	#undef smp_call_function
	static int inline smp_call_function (void (*func) (void *info), void *info,
					     int retry, int wait)
	{
		return 0;
	}

#endif /* < 2.2.8 || (!SMP && <= 2.2.20) */


#if VBEFORE(2,2,3)

	/* 2.2.3 introduced wait_event_interruptible */
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

#endif /* VBEFORE(2,2,3) */

#if VBEFORE(2,2,8)

	/* 2.2.8 introduced rdmsr/wrmsr */
	#define rdmsr(msr,val1,val2)				\
	       __asm__ __volatile__("rdmsr"			\
				    : "=a" (val1), "=d" (val2)	\
				    : "c" (msr))
	#define wrmsr(msr,val1,val2)					\
	     __asm__ __volatile__("wrmsr"				\
				  : /* no outputs */			\
				  : "c" (msr), "a" (val1), "d" (val2))

#endif /* VBEFORE(2,2,8) */

#if VBEFORE(2,2,18)

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

#endif /* VBEFORE(2,2,18) */

/* 2.2.18 introduced the rtc lock */
#ifdef RTC_LOCK
	#define lock_rtc(f) spin_lock_irqsave(&rtc_lock, f)
	#define unlock_rtc(f) spin_unlock_irqrestore(&rtc_lock, f)
#else
	#define lock_rtc(f) save_flags(f)
	#define unlock_rtc(f) restore_flags(f)
#endif /* RTC_LOCK */
 
#if VAFTER(2,2,20)
	#define PTRACE_OFF(t) ((t)->ptrace &= ~PT_DTRACE)
#else
	#define PTRACE_OFF(t) ((t)->flags &= ~PF_DTRACE)
#endif

#endif /* COMPAT22_H */
