/* COPYRIGHT (C) 2002 Philippe Elie, based on discussion with John Levon
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

/* replacement of d_covers: version is a guess */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,3,21)
#define NEED_2_2_DENTRIES
extern int wind_dentries_2_2(struct dentry *dentry);
extern uint do_path_hash_2_2(struct dentry *dentry);
#define wind_dentries(d, v, r, m) wind_dentries_2_2(d)
#define hash_path(f) do_path_hash_2_2((f)->f_dentry)
#else
#define wind_dentries(d, v, r, m) wind_dentries_2_4(d, v, r, m)
#define hash_path(f) do_path_hash_2_4((f)->f_dentry, (f)->f_vfsmnt)
#endif /* LINUX_VERSION_CODE < KERNEL_VERSION(2,3,21) */

// 2.2's UP version of this is broken
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,3,1)
#ifndef __SMP__
#undef smp_call_function
inline static int smp_call_function(void (*f)(void *in), void *i, int n, int w)
{
	return 0;
}
#endif /* __SMP__ */
#endif /* LINUX_VERSION_CODE < KERNEL_VERSION(2,3,1) */
 
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,4,0)

/* on 2.2, the APIC is never enabled on UP */
#define NO_MPTABLE_CHECK_NEEDED
 
/* 2.2 has no cpu_number_map on UP */
#ifdef __SMP__
#define op_cpu_id() cpu_number_map[smp_processor_id()]
#else
#define op_cpu_id() smp_processor_id()
#endif /* __SMP__ */
#else
#define op_cpu_id() cpu_number_map(smp_processor_id())
#endif /* LINUX_VERSION_CODE < KERNEL_VERSION(2,4,0) */

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,4,0)

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

#ifndef __SMP__
#include "apic_up_compat.h"
#else
/* even on SMP, some defines are missing in 2.2 */
#define	APIC_LVR		0x30
#define	APIC_LVTPC		0x340
#define	APIC_LVTERR		0x370
#define	GET_APIC_VERSION(x)	((x)&0xFF)
#define	GET_APIC_MAXLVT(x)	(((x)>>16)&0xFF)
#define	APIC_INTEGRATED(x)	((x)&0xF0)
#endif /* !__SMP__ */

/* 2.4.0 introduce virt_to_page */
#define virt_to_page(va) MAP_NR(va)

#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,0) */

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,0)

/* FIXME: consider apic_up_compat ... */
 
/* 2.4.0 introduced vfsmount cross mount point */
#define HAVE_CROSS_MOUNT_POINT

/* 2.4.0 introduced mmap2 syscall */
#define HAVE_MMAP2

#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,0) */

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,0)
/* 2.4.0 change proto of pte_page and make it unusable param to page_address */
#define pte_page_address(x) page_address(pte_page(x))
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(2,2,7)
/* 2.2.0 make pte_page_address and pte_page equivalent */
#define pte_page_address(x) pte_page(pte)
#else
/* < 2.2.20 use MAP_NR(x) as arg of page_address() */
#define pte_page_address(x) page_address(MAP_NR(x))
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
/* no fixmapping is done in UP 2.2 */
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,4,0)
#define NEED_FIXMAP_HACK
#endif
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
