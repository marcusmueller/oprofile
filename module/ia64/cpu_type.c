/**
 * @file cpu_type.c
 * CPU determination
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author John Levon <moz@compsoc.man.ac.uk>
 * @author Philippe Elie <phil_el@wanadoo.fr>
 * @author Will Cohen <wcohen@redhat.com>
 */

#include "oprofile.h"

EXPORT_NO_SYMBOLS;

__init op_cpu get_cpu_type(void)
{
	__u8 family = local_cpu_data->family;

	/* FIXME: There should be a bit more checking here. */
	switch (family) {
	case 0x07: /* Itanium */
		return CPU_IA64_1;
		break;
	case 0x1f: /* Itanium 2 */
		return CPU_IA64_2;
		break;
	}
	/* Go for the basic generic IA64 */
	return CPU_IA64;
}
