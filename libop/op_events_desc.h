/**
 * @file op_events_desc.h
 * Human-readable descriptions of PMC events
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author John Levon
 * @author Philippe Elie
 */

#ifndef OP_EVENTS_DESC_H
#define OP_EVENTS_DESC_H

#ifdef __cplusplus
extern "C" {
#endif

#include "op_cpu_type.h"

/**
 * get event name and description
 * @param cpu_type the cpu_type
 * @param type event value
 * @param um unit mask
 * @param typenamep returned event name string
 * @param typedescp returned event description string
 * @param umdescp returned unit mask description string
 *
 * Get the associated event name and descriptions given
 * the cpu type, event value and unit mask value. It is a fatal error
 * to supply a non-valid type value, but an invalid um will not exit.
 *
 * typenamep, typedescp, umdescp are filled in with pointers
 * to the relevant name and descriptions. umdescp can be set to
 * NULL when um is invalid for the given type value.
 * These strings are static and should not be freed.
 */
void op_get_event_desc(op_cpu cpu_type, u8 type, u8 um,
		       char const ** typenamep, 
		       char const ** typedescp, 
		       char const ** umdescp);

/** description of events for all processor type */
extern struct op_event op_events[];
/** the total number of events for all processor type, allowing to iterate
 * on the op_events[] decription */
extern u32 op_nr_events;

#ifdef __cplusplus
}
#endif

#endif /* OP_EVENTS_DESC_H */
