/**
 * @file op_print_event.h
 * Output a header describing a perf counter event
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author John Levon <moz@compsoc.man.ac.uk>
 * @author Philippe Elie <phil_el@wanadoo.fr>
 */
 
#ifndef OP_PRINT_EVENT_H
#define OP_PRINT_EVENT_H
 
#include <iosfwd>
 
#include "op_types.h"
#include "op_interface.h"
 
/**
 * Output a description of the given event paramters
 * to the stream.
 */
void op_print_event(std::ostream & out, int counter_nr,
	op_cpu cpu_type, u8 type, u8 um, u32 count);

#endif // OP_PRINT_EVENT 
