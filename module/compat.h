/**
 * @file compat.h
 * This file is intended to be up-to-date with the last linux version and
 * provide work-arounds for missing features in previous kernel version
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author John Levon <moz@compsoc.man.ac.uk>
 * @author Philippe Elie <phil_el@wanadoo.fr>
 */


#ifndef COMPAT_H
#define COMPAT_H

#include <linux/version.h>
#include <linux/module.h>
#ifdef HAVE_LINUX_SPINLOCK_HEADER
#include <linux/spinlock.h> /* ensure visiblity of preempt_disable */
#endif

#define V_BEFORE(a,b,c) (LINUX_VERSION_CODE < KERNEL_VERSION(a,b,c))
#define V_EQUAL(a,b,c) (LINUX_VERSION_CODE == KERNEL_VERSION(a,b,c))
#define V_AT_LEAST(a,b,c) (LINUX_VERSION_CODE >= KERNEL_VERSION(a,b,c))

#if V_BEFORE(2,4,0)
	#include "compat22.h"
#else
	#include "compat24.h"
#endif

#include "op_cache.h"

#ifndef preempt_disable
#define preempt_disable()		do { } while (0)
#define preempt_enable()		do { } while (0)
#endif

#if V_BEFORE(2, 5, 23)
#define OP_MAX_CPUS	smp_num_cpus
#define for_each_online_cpu(i) for (i = 0 ; i < smp_num_cpus ; ++i)
#else
#define OP_MAX_CPUS	NR_CPUS
static inline int next_cpu(int i)
{ 
	while (i < NR_CPUS && !cpu_online(i))
		++i;
	return i;
}
#define for_each_online_cpu(i) \
		for (i = next_cpu(0) ; i < NR_CPUS ; i = next_cpu(++i))
#endif

#if V_BEFORE(2, 5, 14)
#define op_pfn_pte(x, y) mk_pte_phys((x), (y))
#else
#define op_pfn_pte(x, y) pfn_pte((x) >> PAGE_SHIFT, (y))
#endif

#if V_AT_LEAST(2, 5, 8)
#include <asm/tlbflush.h>
#endif

/* 2.5.5 change pte_offset */
#if V_AT_LEAST(2, 5, 5)
#define pte_offset pte_offset_kernel
#endif

/* 2.5.3 change prototype of remap_page_range */
#if V_BEFORE(2,5,3)
#define REMAP_PAGE_RANGE(vma, start, page, page_size, flags) \
		remap_page_range((start), (page), (page_size), (flags))
#else
#define REMAP_PAGE_RANGE(vma, start, page, page_size, flags) \
		remap_page_range((vma), (start), (page), (page_size), (flags))
#endif


/* 2.5.2 change the dev_t definition */
#if V_BEFORE(2,5,2)
#define minor(d)	MINOR(d)
#endif

/* Things that cannot rely on a particular linux version or are needed between
 * major release */

#ifndef BUG_ON
#define BUG_ON(p) do { if (p) BUG(); } while (0)
#endif

#ifndef MODULE_LICENSE
#define MODULE_LICENSE(x)
#endif

/* 2.4/2.5 kernel can be  patched with the preempt patch. We support only
 * recent version of this patch */
#ifndef preempt_disable
	#define preempt_disable()    do { } while (0)
	#define preempt_enable_no_resched() do { } while (0)
	#define preempt_enable()     do { } while (0)
#endif

/* Compiler work-around */

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
