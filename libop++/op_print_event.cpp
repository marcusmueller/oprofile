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
#include "op_events.h"

using namespace std;

void op_print_event(ostream & out, int counter_nr, op_cpu cpu_type,
		    u8 type, u16 um, u32 count)
{
	if (cpu_type == CPU_TIMER_INT) {
		out << "Profiling through timer interrupt\n";
		return;
	}

	struct op_event * event = op_find_event(cpu_type, type);

	char const * um_desc = 0;

	for (size_t i = 0; i < event->unit->num; ++i) {
		if (event->unit->um[i].value == um)
			um_desc = event->unit->um[i].desc;
	}

	out << "Counter " << counter_nr << " counted "
	    << event->name << " events (" << event->desc << ")";
	if (cpu_type != CPU_RTC) {
		out << " with a unit mask of 0x"
		    << hex << setw(2) << setfill('0') << unsigned(um) << " ("
		    << um_desc << ")";
	}
	out << " count " << dec << count << endl;
}
