/**
 * @file samples_file.h
 * Encapsulation for samples files
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author Philippe Elie <phil_el@wanadoo.fr>
 * @author John Levon <moz@compsoc.man.ac.uk>
 */

#include <stdio.h>
#include <errno.h>

#include <sstream>
#include <iostream>

#include "oprofpp.h"
#include "op_events.h"
#include "op_events_desc.h"
#include "op_print_event.h"

using std::string;
using std::cout;
using std::endl;
 
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
		fprintf(stderr, "Can not open any samples files for %s last error %s\n", sample_filename.c_str(), strerror(errno));
		exit(EXIT_FAILURE);
	}

	opd_header const & header = samples[first_file]->header();
	mtime = header.mtime;

	/* determine how many counters are possible via the sample file */
	op_cpu cpu = static_cast<op_cpu>(header.cpu_type);
	nr_counters = op_get_cpu_nr_counters(cpu);

	/* check sample files match */
	for (j = first_file + 1; j < OP_MAX_COUNTERS; ++j) {
		if (samples[j] == 0)
			continue;
		samples[first_file]->check_headers(*samples[j]);
	}

	/* sanity check on ctr_um, ctr_event and cpu_type */
	for (i = 0 ; i < OP_MAX_COUNTERS; ++i) {
		if (samples[i] != 0)
			check_event(&samples[i]->header());
	}
}

opp_samples_files::~opp_samples_files()
{
	uint i;

	for (i = 0 ; i < OP_MAX_COUNTERS; ++i) {
		delete samples[i];
	}
}

void opp_samples_files::open_samples_file(u32 counter, bool can_fail)
{
	std::ostringstream filename;
	filename << sample_filename << "#" << counter;
	string temp = filename.str();

	if (access(temp.c_str(), R_OK) == 0) {
		samples[counter] = new samples_file_t(temp);
	} else {
		if (can_fail == false) {
			/* FIXME: nicer message if e.g. wrong counter */ 
			fprintf(stderr, "oprofpp: Opening %s failed. %s\n", temp.c_str(), strerror(errno));
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
			samples[k]->start_offset = start_offset;
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
	if (db_tree.base_memory)
		db_close(&db_tree);
}

void samples_file_t::check_headers(samples_file_t const & rhs) const
{
	opd_header const & f1 = header();
	opd_header const & f2 = rhs.header();
	if (f1.mtime != f2.mtime) {
		fprintf(stderr, "oprofpp: header timestamps are different (%ld, %ld)\n", f1.mtime, f2.mtime);
		exit(EXIT_FAILURE);
	}

	if (f1.is_kernel != f2.is_kernel) {
		fprintf(stderr, "oprofpp: header is_kernel flags are different\n");
		exit(EXIT_FAILURE);
	}

	if (f1.cpu_speed != f2.cpu_speed) {
		fprintf(stderr, "oprofpp: header cpu speeds are different (%f, %f)",
			f2.cpu_speed, f2.cpu_speed);
		exit(EXIT_FAILURE);
	}

	if (f1.separate_samples != f2.separate_samples) {
		fprintf(stderr, "oprofpp: header separate_samples are different (%d, %d)",
			f2.separate_samples, f2.separate_samples);
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
