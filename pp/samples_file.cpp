/**
 * @file samples_file.h
 * Encapsulation for samples files
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author Philippe Elie
 * @author John Levon
 */

#include "op_file.h"

#include "op_events.h"
#include "op_events_desc.h"
#include "op_print_event.h"
#include "op_sample_file.h"
#include "string_manip.h"

#include "counter_array.h"

#include "samples_file.h"

#include <cerrno>
#include <unistd.h>

#include <iostream>

using namespace std;

opp_samples_files::opp_samples_files(string const & sample_file, int counter_)
	:
	nr_counters(2),
	sample_filename(sample_file),
	counter_mask(counter_),
	first_file(-1)
{
	uint i, j;
	time_t mtime = 0;

	/* no samplefiles open initially */
	for (i = 0; i < OP_MAX_COUNTERS; ++i) {
		samples[i] = 0;
	}

	for (i = 0; i < OP_MAX_COUNTERS ; ++i) {
		if ((counter_mask &  (1 << i)) != 0) {
			/* if only the i th bit is set in counter spec we do
			 * not allow opening failure to get a more precise
			 * error message */
			open_samples_file(i, (counter_mask & ~(1 << i)) != 0);
		}
	}

	/* find first open file */
	for (first_file = 0; first_file < OP_MAX_COUNTERS ; ++first_file) {
		if (samples[first_file] != 0)
			break;
	}

	if (first_file == OP_MAX_COUNTERS) {
		cerr << "Can not open any samples files for " << sample_filename
			<< ". Last error " << strerror(errno) << endl;
		exit(EXIT_FAILURE);
	}

	opd_header const & header = samples[first_file]->header();
	mtime = header.mtime;

	/* determine how many counters are possible via the sample file */
	op_cpu cpu = static_cast<op_cpu>(header.cpu_type);
	nr_counters = op_get_nr_counters(cpu);

	/* check sample files match */
	for (j = first_file + 1; j < OP_MAX_COUNTERS; ++j) {
		if (samples[j] == 0)
			continue;
		samples[first_file]->check_headers(*samples[j]);
	}
}

opp_samples_files::~opp_samples_files()
{
	uint i;

	for (i = 0 ; i < OP_MAX_COUNTERS; ++i) {
		delete samples[i];
	}
}

void opp_samples_files::check_mtime(string const & file) const
{
	time_t const newmtime = op_get_mtime(file.c_str());
	if (newmtime != first_header().mtime) {
		cerr << "oprofpp: WARNING: the last modified time of the binary file "
			<< file << " does not match\n"
			<< "that of the sample file. Either this is the wrong binary or the binary\n"
			<< "has been modified since the sample file was created.\n";
	}
}


void opp_samples_files::open_samples_file(u32 counter, bool can_fail)
{
	string filename = ::sample_filename(string(), sample_filename, counter);

	if (access(filename.c_str(), R_OK) == 0) {
		samples[counter] = new samples_file_t(filename);
	} else {
		if (!can_fail) {
			cerr << "oprofpp: Opening " << filename <<  "failed."
			     << strerror(errno) << endl;
			exit(EXIT_FAILURE);
		}
	}
}

bool opp_samples_files::accumulate_samples(counter_array_t & counter, uint index) const
{
	bool found_samples = false;

	for (uint k = 0; k < nr_counters; ++k) {
		u32 count = samples_count(k, index);
		if (count) {
			counter[k] += count;
			found_samples = true;
		}
	}

	return found_samples;
}

bool opp_samples_files::accumulate_samples(counter_array_t & counter,
	uint start, uint end) const
{
	bool found_samples = false;

	for (uint k = 0; k < nr_counters; ++k) {
		if (is_open(k)) {
			counter[k] += samples[k]->count(start, end);
			if (counter[k])
				found_samples = true;
		}
	}

	return found_samples;
}

void opp_samples_files::set_start_offset(u32 start_offset)
{
	if (!first_header().is_kernel)
		return;

	for (uint k = 0; k < nr_counters; ++k) {
		if (is_open(k))
			samples[k]->set_start_offset(start_offset);
	}
}

/**
 * output_header() - output counter setup
 *
 * output to stdout the cpu type, cpu speed
 * and all counter description available
 */
void opp_samples_files::output_header() const
{
	opd_header const & header = first_header();

	op_cpu cpu = static_cast<op_cpu>(header.cpu_type);

	cout << "Cpu type: " << op_get_cpu_type_str(cpu) << endl;

	cout << "Cpu speed was (MHz estimation) : " << header.cpu_speed << endl;

	for (uint i = 0 ; i < OP_MAX_COUNTERS; ++i) {
		if (samples[i] != 0) {
			op_print_event(cout, i, cpu, header.ctr_event,
				       header.ctr_um, header.ctr_count);
		}
	}
}

samples_file_t::samples_file_t(string const & filename)
	: start_offset(0)
{
	db_open(&db_tree, filename.c_str(), DB_RDONLY, sizeof(struct opd_header));
}

samples_file_t::~samples_file_t()
{
	db_close(&db_tree);
}

void samples_file_t::check_headers(samples_file_t const & rhs) const
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

	if (f1.separate_samples != f2.separate_samples) {
		cerr << "oprofpp: header separate_samples are different ("
		     << f1.separate_samples << ", " 
		     << f2.separate_samples << ")\n";
		exit(EXIT_FAILURE);
	}
}

static void db_tree_callback(db_key_t, db_value_t value, void * data)
{
	u32 * count = (u32 *)data;

	*count += value;
}

u32 samples_file_t::count(uint start, uint end) const
{
	u32 count = 0;

	db_travel(&db_tree, start - start_offset, end - start_offset,
		  db_tree_callback, &count);

	return count;
}
