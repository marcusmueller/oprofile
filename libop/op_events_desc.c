/**
 * @file op_events_desc.c
 * Human-readable descriptions of PMC events
 *
 * Adapted from libpperf 0.5 by M. Patrick Goda and Michael S. Warren
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author John Levon
 * @author Philippe Elie
 */

/* See IA32 Vol. 3 Appendix A */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "op_events.h"
#include "op_events_desc.h"

/**
 * op_get_um_desc - verify and get unit mask description
 * @param op_events_index  the index of the events in op_events array
 * @param um  unit mask
 *
 * Try to get the associated unit mask given the event index and unit
 * mask value. No error can occur.
 *
 * The function return the associated help string about this um or
 * NULL if um is invalid.
 * This string is in text section so should not be freed.
 */
static const char * op_get_um_desc(u32 op_events_index, u8 um)
{
	struct op_unit_mask const * op_um_mask;
	int um_mask_desc_index;

	op_um_mask = op_events[op_events_index].unit;
	um_mask_desc_index = op_check_unit_mask(op_um_mask, um);

	if (um_mask_desc_index == -1)
		return NULL;
	else if (um_mask_desc_index == 0) {
		/* avoid dynamic alloc to simplify caller's life */
		return "set with multiple units, check the documentation";
	}

	return op_um_mask->um[um_mask_desc_index-1].desc;
}

/**
 * op_get_event_desc - get event name and description
 * @param cpu_type  the cpu_type
 * @param type  event value
 * @param um  unit mask
 * @param typenamep  returned event name string
 * @param typedescp  returned event description string
 * @param umdescp  returned unit mask description string
 *
 * Get the associated event name and descriptions given
 * the cpu type, event value and unit mask value. It is a fatal error
 * to supply a non-valid @type value, but an invalid @um
 * will not exit.
 *
 * @typenamep, @typedescp, @umdescp are filled in with pointers
 * to the relevant name and descriptions. @umdescp can be set to
 * NULL when @um is invalid for the given @type value.
 * These strings are in text section so should not be freed.
 */
void op_get_event_desc(op_cpu cpu_type, u8 type, u8 um,
		       char const ** typenamep, 
		       char const ** typedescp, 
		       char const ** umdescp)
{
	u32 i;
	int cpu_mask = 1 << cpu_type;

	*typenamep = *typedescp = *umdescp = NULL;

	for (i=0; i < op_nr_events; i++) {
		if (op_events[i].val == type && (op_events[i].cpu_mask & cpu_mask)) {
			*typenamep = op_events[i].name;
			*typedescp = op_events[i].desc;

			*umdescp = op_get_um_desc(i, um);
			break;
		}
	}

	if (!*typenamep) {
		fprintf(stderr,"op_get_event_desc: no such event 0x%.2x\n",type);
		exit(EXIT_FAILURE);
	}
}
