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

EXPORT_NO_SYMBOLS;

MODULE_PARM(force_rtc, "i");
MODULE_PARM_DESC(force_rtc, "force RTC mode.");
static int force_rtc;

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
				if (model <= 3)
					/* a Pentium 4 processor */
					return CPU_P4;
				else
					/* Do not know what it is */
					return CPU_RTC;
			}
			
		default:
			return CPU_RTC;
	}
}
