/**
 * @file counter_profile.cpp
 * Encapsulation of one samples files
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author Philippe Elie
 * @author John Levon
 */

#include "op_file.h"
#include "op_config.h"
#include "op_print_event.h"
#include "op_sample_file.h"
#include "string_manip.h"

#include "counter_array.h"

#include "counter_profile.h"

#include <cerrno>
#include <unistd.h>

#include <iostream>

using namespace std;

counter_profile_t::counter_profile_t(string const & filename)
	:
	start_offset(0)
{
	build_ordered_samples(filename);
}

// empty dtor but needed due to the use of scoped_ptr<opd_header>, if we don't
// provide once compiler generate the dtor and all source file declaring
// counter_profile_t will need the complete type of opd_header
counter_profile_t::~counter_profile_t()
{
}

void counter_profile_t::check_headers(counter_profile_t const & rhs) const
{
	opd_header const & f1 = header();
	opd_header const & f2 = rhs.header();
	if (f1.mtime != f2.mtime) {
		cerr << "oprofpp: header timestamps are different ("
		     << f1.mtime << ", " << f2.mtime << ")\n";
		exit(EXIT_FAILURE);
	}

	if (f1.is_kernel != f2.is_kernel) {
		cerr << "oprofpp: header is_kernel flags are different\n";
		exit(EXIT_FAILURE);
	}

	if (f1.cpu_speed != f2.cpu_speed) {
		cerr << "oprofpp: header cpu speeds are different ("
		     << f1.cpu_speed << ", " << f2.cpu_speed << ")\n";
		exit(EXIT_FAILURE);
	}

	if (f1.separate_lib_samples != f2.separate_lib_samples) {
		cerr << "oprofpp: header separate_lib_samples are different ("
		     << f1.separate_lib_samples << ", " 
		     << f2.separate_lib_samples << ")\n";
		exit(EXIT_FAILURE);
	}

	if (f1.separate_kernel_samples != f2.separate_kernel_samples) {
		cerr << "oprofpp: header separate_kernel_samples are different ("
		     << f1.separate_kernel_samples << ", " 
		     << f2.separate_kernel_samples << ")\n";
		exit(EXIT_FAILURE);
	}
}

void counter_profile_t::build_ordered_samples(string const & filename)
{
	samples_db_t samples_db;

	db_open(&samples_db, filename.c_str(), DB_RDONLY, sizeof(struct opd_header));

	opd_header const & head = *static_cast<opd_header *>(samples_db.base_memory);

	if (head.version != OPD_VERSION) {
		cerr << "oprofpp: samples files version mismatch, are you "
			"running a daemon and post-profile tools with version "
			"mismatch ?" << endl;
		exit(EXIT_FAILURE);
	}

	file_header.reset(new opd_header(head));

	db_node_nr_t node_nr, pos;
	db_node_t * node = db_get_iterator(&samples_db, &node_nr);

	for ( pos = 0 ; pos < node_nr ; ++pos) {
		if (node[pos].key) {
			ordered_samples_t::value_type val(node[pos].key,
							node[pos].value);
			ordered_samples.insert(val);
		}
	}

	db_close(&samples_db);
}

u32 counter_profile_t::count(uint start, uint end) const
{
	u32 count = 0;

	ordered_samples_t::const_iterator first, last;
	first = ordered_samples.lower_bound(start - start_offset);
	last = ordered_samples.lower_bound(end - start_offset);
	for ( ; first != last ; ++first) {
		count += first->second;
	}

	return count;
}
