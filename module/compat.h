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
#include <linux/module.h>

#define V_BEFORE(a,b,c) (LINUX_VERSION_CODE < KERNEL_VERSION(a,b,c))
#define V_EQUAL(a,b,c) (LINUX_VERSION_CODE == KERNEL_VERSION(a,b,c))
#define V_AT_LEAST(a,b,c) (LINUX_VERSION_CODE >= KERNEL_VERSION(a,b,c))
 
#if V_BEFORE(2,4,0)
	#include "compat22.h"
#else
	#include "compat24.h"
#endif

#include "op_cache.h"

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
#define minor(dev)	MINOR(dev)
#endif
 
/* Things that cannot rely on a particular linux version or are needed between
 * major release */

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
 
/* MSRs */
#ifndef MSR_P6_PERFCTR0
#define MSR_P6_PERFCTR0 0xc1
#endif
#ifndef MSR_P6_PERFCTR1
#define MSR_P6_PERFCTR1 0xc2
#endif
#ifndef MSR_P6_EVNTSEL0
#define MSR_P6_EVNTSEL0 0x186
#endif
#ifndef MSR_P6_EVNTSEL1
#define MSR_P6_EVNTSEL1 0x187
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

#ifndef APIC_DEFAULT_PHYS_BASE
#define APIC_DEFAULT_PHYS_BASE 0xfee00000
#endif
 
#endif /* COMPAT_H */
