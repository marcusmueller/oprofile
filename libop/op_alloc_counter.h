/**
 * @file op_alloc_counter.h
 * hardware counter allocation
 *
 * You can have silliness here.
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author John Levon
 * @author Philippe Elie
 */

#ifndef OP_ALLOC_COUNTER_H
#define OP_ALLOC_COUNTER_H

#include <stddef.h>

#include "op_cpu_type.h"

#ifdef __cplusplus
extern "C" {
#endif

size_t * map_event_to_counter(struct op_event const * pev[], int nr_events,
                              op_cpu cpu_type);

#ifdef __cplusplus
}
#endif

#endif /* !OP_ALLOC_COUNTER_H */
