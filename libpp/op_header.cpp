/**
 * @file op_header.cpp
 * various free function acting on a sample file header
 *
 * @remark Copyright 2004 OProfile authors
 * @remark Read the file COPYING
 *
 * @author John Levon
 * @author Philippe Elie
 */

#include <iostream>
#include <cstdlib>
#include <iomanip>
#include <set>
#include <sstream>

#include "op_exception.h"
#include "odb_hash.h"
#include "op_cpu_type.h"
#include "op_file.h"
#include "op_header.h"
#include "op_events.h"

using namespace std;

void op_check_header(opd_header const & h1, opd_header const & h2,
		     string const & filename)
{
	if (h1.mtime != h2.mtime) {
		ostringstream os;
		os << "header timestamps are different ("
		   << h1.mtime << ", " << h2.mtime << ") for "
		   << filename << "\n";
		throw op_fatal_error(os.str());
	}

	if (h1.is_kernel != h2.is_kernel) {
		ostringstream os;
		os << "header is_kernel flags are different for "
		   << filename << "\n";
		throw op_fatal_error(os.str());
	}

	if (h1.separate_lib_samples != h2.separate_lib_samples) {
		ostringstream os;
		os << "header separate_lib_samples are different ("
		   << h1.separate_lib_samples << ", " 
		   << h2.separate_lib_samples << ") for "
		   << filename << "\n";
		throw op_fatal_error(os.str());
	}

	if (h1.separate_kernel_samples != h2.separate_kernel_samples) {
		ostringstream os;
		os << "header separate_kernel_samples are different ("
		   << h1.separate_kernel_samples << ", " 
		   << h2.separate_kernel_samples << ") for"
		   << filename << "\n";
		throw op_fatal_error(os.str());
	}

	// Note that we don't check CPU speed since that can vary
	// freely on the same machine
}


namespace {

set<string> warned_files;

}


void check_mtime(string const & file, opd_header const & header)
{
	time_t const newmtime = op_get_mtime(file.c_str());

	if (newmtime == header.mtime)
	       return;

	if (warned_files.find(file) != warned_files.end())
		return;

	warned_files.insert(file);

	// Files we couldn't get mtime of have zero mtime
	if (!header.mtime) {
		cerr << "warning: could not check that the binary file "
		     << file << " has not been modified since "
			"the profile was taken. Results may be inaccurate.\n";
	} else {
		static bool warned_already = false;

		cerr << "warning: the last modified time of the binary file "
		     "does not match that of the sample file for " << file
		     << "\n";

		if (!warned_already) {
			cerr << "Either this is the wrong binary or the binary "
			"has been modified since the sample file was created.\n";
			warned_already = true;
		}
	}
}


opd_header read_header(string const & sample_filename)
{
	samples_odb_t samples_db;

	int rc = odb_open(&samples_db, sample_filename.c_str(), ODB_RDONLY,
		sizeof(struct opd_header));

	if (rc != EXIT_SUCCESS) {
		ostringstream os;
		os << samples_db.err_msg << endl;
		throw op_fatal_error(os.str());
	}

	opd_header head = *static_cast<opd_header *>(samples_db.base_memory);

	odb_close(&samples_db);

	return head;
}


namespace {

void op_print_event(ostream & out, op_cpu cpu_type, u8 type, u16 um, u32 count)
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

	out << "Counted " << event->name << " events (" << event->desc << ")";
	if (cpu_type != CPU_RTC) {
		int old_width = out.width();
		char old_fill = out.fill();
		out << " with a unit mask of 0x"
		    << hex << setw(2) << setfill('0') << unsigned(um) << " ("
		    << (um_desc ? um_desc : "multiple flags") << ")";
		out << setfill(old_fill) << setw(old_width) << dec;
	}
	out << " count " << dec << count << endl;
}

}


ostream & operator<<(ostream & out, opd_header const & header)
{
	op_cpu cpu = static_cast<op_cpu>(header.cpu_type);

	out << "CPU: " << op_get_cpu_type_str(cpu);
	out << ", speed " << header.cpu_speed << " MHz (estimated)" << endl;

	op_print_event(out, cpu, header.ctr_event,
	               header.ctr_um, header.ctr_count);

	return out;
}
