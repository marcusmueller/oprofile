/**
 * @file profile.cpp
 * Encapsulation for samples files over all counter belonging to the
 * same binary image
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author Philippe Elie
 * @author John Levon
 */

#include <unistd.h>

#include <iostream>
#include <string>
#include <sstream>

#include <cerrno>

#include "op_exception.h"
#include "op_header.h"
#include "op_config.h"
#include "op_sample_file.h"

#include "profile.h"

using namespace std;

profile_t::profile_t()
	: start_offset(0)
{
}


profile_t::~profile_t()
{
}


void profile_t::add_sample_file(string const & filename, u32 offset)
{
	samples_odb_t samples_db;

	int rc = odb_open(&samples_db, filename.c_str(), ODB_RDONLY,
		sizeof(struct opd_header));

	if (rc != EXIT_SUCCESS) {
		ostringstream os;
		os << samples_db.err_msg << endl;
		throw op_fatal_error(os.str());
	}

	opd_header const & head = *static_cast<opd_header *>(samples_db.base_memory);

	if (head.version != OPD_VERSION) {
		ostringstream os;
		os << "oprofpp: samples files version mismatch, are you "
		   << "running a daemon and post-profile tools with version "
		   <<  "mismatch ?\n";
		throw op_fatal_error(os.str());
	}

	// if we already read a sample file header pointer is non null
	if (file_header.get()) {
		op_check_header(head, *file_header, filename);
	}

	file_header.reset(new opd_header(head));

	odb_node_nr_t node_nr, pos;
	odb_node_t * node = odb_get_iterator(&samples_db, &node_nr);

	for (pos = 0; pos < node_nr; ++pos) {
		if (node[pos].key) {
			ordered_samples_t::iterator it = 
				ordered_samples.find(node[pos].key);
			if (it != ordered_samples.end()) {
				it->second += node[pos].value;
			} else {
				ordered_samples_t::value_type
					val(node[pos].key, node[pos].value);
				ordered_samples.insert(val);
			}
		}
	}

	odb_close(&samples_db);

	if (!get_header().is_kernel)
		return;

	start_offset = offset;
}


profile_t::iterator_pair
profile_t::samples_range(unsigned int start, unsigned int end) const
{
	// if the image contains no symbol the vma range is [0 - filesize]
	// in this case we can't substract start_offset else we will underflow
	// and the iterator range will be empty.
	if (start) {
		start -= start_offset;
	}
	end -= start_offset;

	ordered_samples_t::const_iterator first = 
		ordered_samples.lower_bound(start);
	ordered_samples_t::const_iterator last =
		last = ordered_samples.lower_bound(end);

	return make_pair(const_iterator(first, start_offset),
		const_iterator(last, start_offset));
}


profile_t::iterator_pair profile_t::samples_range() const
{
	ordered_samples_t::const_iterator first = ordered_samples.begin();
	ordered_samples_t::const_iterator last = ordered_samples.end();

	return make_pair(const_iterator(first, start_offset),
		const_iterator(last, start_offset));
}
