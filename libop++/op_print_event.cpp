/**
 * @file op_print_event.cpp
 * Output a header describing a perf counter event
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author John Levon
 * @author Philippe Elie
 */

#include <iostream>
#include <iomanip>

#include "op_print_event.h"
#include "op_events_desc.h"

using namespace std;

void op_print_event(ostream & out, int counter_nr, op_cpu cpu_type,
		    u8 type, u16 um, u32 count)
{
	char const * typenamep;
	char const * typedescp;
	char const * umdescp;

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
