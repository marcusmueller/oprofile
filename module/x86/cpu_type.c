/**
 * @file cpu_type.c
 * CPU determination
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author John Levon
 * @author Philippe Elie
 */

#include "oprofile.h"
#include "op_msr.h"

EXPORT_NO_SYMBOLS;

MODULE_PARM(force_rtc, "i");
MODULE_PARM_DESC(force_rtc, "force RTC mode.");
static int force_rtc;

/**
 * p4_threads - determines the number of logical processor threads in a die
 * 
 * returns number of threads in p4 die (1 for non-HT processors)
 */
static int p4_threads(void)
{
	int processor_threads = 1;

#ifdef CONFIG_SMP
	if (test_bit(X86_FEATURE_HT, &current_cpu_data.x86_capability)) {
		/* This it a Pentium 4 with HT, find number of threads */
		int eax, ebx, ecx, edx;

		cpuid (1, &eax, &ebx, &ecx, &edx);
		processor_threads = (ebx >> 16) & 0xff;
	}
#endif /* CONFIG_SMP */

	return processor_threads;
}

__init op_cpu get_cpu_type(void)
{
	__u8 vendor = current_cpu_data.x86_vendor;
	__u8 family = current_cpu_data.x86;
	__u8 model = current_cpu_data.x86_model;

	if (force_rtc) {
		return CPU_RTC;
	}

	switch (vendor) {
		case X86_VENDOR_AMD:
			/* Needs to be at least an Athlon (or hammer in 32bit mode) */
			if (family < 6)
				return CPU_RTC;
			/* FIXME: Test for hammer in longmode and warn. */
			return CPU_ATHLON;

		case X86_VENDOR_INTEL:
			switch (family) {
			default:
				return CPU_RTC;
			case 6:
				/* A P6-class processor */
				if (model > 5)
					return CPU_PIII;
				else if (model > 2)
					return CPU_PII;
				return CPU_PPRO;
			case 0xf:
				if (model <= 3) {
					/* Cannot handle HT P4 hardware */
					if (p4_threads()>1 )
						return CPU_RTC;
					else
				  		return CPU_P4;
				} else
					/* Do not know what it is */
					return CPU_RTC;
			}
			
		default:
			return CPU_RTC;
	}
}
