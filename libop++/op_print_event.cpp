/**
 * @file op_print_event.cpp
 * Output a header describing a perf counter event
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author John Levon <moz@compsoc.man.ac.uk>
 * @author Philippe Elie <phil_el@wanadoo.fr>
 */
 
#include "op_print_event.h"
#include "op_events_desc.h" 
 
#include <iostream>
#include <iomanip>
 
using std::ostream;
using std::endl;
using std::hex;
using std::dec;
using std::setw;
using std::setfill;

void op_print_event(ostream & out, int counter_nr, op_cpu cpu_type,
		    u8 type, u8 um, u32 count)
{
	char * typenamep;
	char * typedescp;
	char * umdescp;

	op_get_event_desc(cpu_type, type, um, 
			  &typenamep, &typedescp, &umdescp);

	out << "Counter " << counter_nr << " counted "
	    << typenamep << " events (" << typedescp << ")";
	if (cpu_type != CPU_RTC) {
		out << " with a unit mask of 0x"
		    << hex << setw(2) << setfill('0') << unsigned(um) << " ("
		    << (umdescp ? umdescp : "Not set") << ")";
	}
	out << " count " << dec << count << endl;
}
