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

#include "oprofpp.h"
#include "op_events.h"

using std::string;
 
opp_samples_files::opp_samples_files(string const & sample_file,
	int counter_)
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

	struct opd_header const * header = samples[first_file]->header();
	mtime = header->mtime;

	/* determine how many counters are possible via the sample file */
	op_cpu cpu = static_cast<op_cpu>(header->cpu_type);
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
			check_event(samples[i]->header());
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

void opp_samples_files::set_sect_offset(u32 sect_offset)
{
	for (uint k = 0; k < nr_counters; ++k) {
		if (is_open(k)) {
			samples[k]->sect_offset = sect_offset;
		}
	}
}

samples_file_t::samples_file_t(string const & filename)
	: sect_offset(0)
{
	db_open(&db_tree, filename.c_str(), DB_RDONLY, sizeof(struct opd_header));
}

samples_file_t::~samples_file_t()
{
	if (db_tree.base_memory)
		db_close(&db_tree);
}

bool samples_file_t::check_headers(samples_file_t const & rhs) const
{
/* 
 * FIXME: is this going to be changed to have a meaning to
 * the return value ? (and get rid of the global check_headers() ?) 
 */
	::check_headers(header(), rhs.header());

	return true;
}

static void db_tree_callback(db_key_t, db_value_t value, void * data)
{
	u32 * count = (u32 *)data;

	*count += value;
}

u32 samples_file_t::count(uint start, uint end) const
{
	u32 count = 0;

	db_travel(&db_tree, start - sect_offset, end - sect_offset,
		  db_tree_callback, &count);

	return count;
}
