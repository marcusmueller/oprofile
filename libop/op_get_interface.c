/**
 * @file op_get_interface.c
 * Determine which oprofile kernel interface used
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author Will Cohen
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "op_cpu_type.h"

op_interface op_get_interface(void)
{
	static op_interface current_interface = OP_INTERFACE_NO_GOOD;
	FILE * fp;

	if (current_interface != OP_INTERFACE_NO_GOOD)
		return current_interface;

	/* Try 2.4's interface. */
	fp = fopen("/proc/sys/dev/oprofile/cpu_type", "r");
	if (fp) {
		fclose (fp);
		current_interface = OP_INTERFACE_24;
		return current_interface;
	}

	/* Try 2.6's oprofilefs one instead. */
	fp = fopen("/dev/oprofile/cpu_type", "r");
	if (fp) {
		fclose (fp);
		current_interface = OP_INTERFACE_26;
		return current_interface;
	}

	return OP_INTERFACE_NO_GOOD;
}
