/**
 * @file cpu_type.c
 * CPU determination
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author John Levon <moz@compsoc.man.ac.uk>
 * @author Philippe Elie <phil_el@wanadoo.fr>
 */

#include "oprofile.h"

EXPORT_NO_SYMBOLS;

__init op_cpu get_cpu_type(void)
{
	__u8 vendor = current_cpu_data.x86_vendor;
	__u8 family = current_cpu_data.x86;
	__u8 model = current_cpu_data.x86_model;

	/* unknown vendor */
	if (vendor != X86_VENDOR_INTEL && vendor != X86_VENDOR_AMD) {
		return CPU_RTC;
	}

	/* not a P6-class processor */
	if (family != 6)
		return CPU_RTC;

	if (vendor == X86_VENDOR_AMD)
		return CPU_ATHLON;

	if (model > 5)
		return CPU_PIII;
	else if (model > 2)
		return CPU_PII;

	return CPU_PPRO;
}
