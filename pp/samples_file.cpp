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

/**
 * opp_samples_files - construct an opp_samples_files object
 * @param sample_file the base name of sample file
 * @param counter which samples files to open, -1 means try to open
 * all samples files.
 *
 * at least one sample file (based on sample_file name)
 * must be opened. If more than one sample file is open
 * their header must be coherent. Each header is also
 * sanitized.
 *
 * all error are fatal
 */
opp_samples_files::opp_samples_files(const std::string & sample_file,
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

	const struct opd_header * header = samples[first_file]->header();
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

/**
 * ~opp_samples_files - destroy an object opp_samples
 *
 * close and free all related resource to the samples file(s)
 */
opp_samples_files::~opp_samples_files()
{
	uint i;

	for (i = 0 ; i < OP_MAX_COUNTERS; ++i) {
		delete samples[i];
	}
}

/**
 * open_samples_file - ctor helper
 * @param counter the counter number
 * @param can_fail allow to fail gracefully
 *
 * open and mmap the given samples files,
 * the member var samples[counter], header[counter]
 * etc. are updated in case of success.
 * The header is checked but coherence between
 * header can not be sanitized at this point.
 *
 * if can_fail == false all error are fatal.
 */
void opp_samples_files::open_samples_file(u32 counter, bool can_fail)
{
	std::ostringstream filename;
	filename << sample_filename << "#" << counter;
	std::string temp = filename.str();

	if (access(temp.c_str(), R_OK) == 0) {
		samples[counter] = new samples_file_t(temp);
	}
	else {
		if (can_fail == false) {
			/* FIXME: nicer message if e.g. wrong counter */ 
			fprintf(stderr, "oprofpp: Opening %s failed. %s\n", temp.c_str(), strerror(errno));
			exit(EXIT_FAILURE);
		}
	}
}

/**
 * accumulate_samples - lookup samples from a vma address
 * @param counter where to accumulate the samples
 * @param index index of the samples.
 *
 * return false if no samples has been found
 */
bool opp_samples_files::accumulate_samples(counter_array_t& counter, uint index) const
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

/**
 * accumulate_samples - lookup samples from a range of vma address
 * @param counter where to accumulate the samples
 * @param start start index of the samples.
 * @param end end index of the samples.
 *
 * return false if no samples has been found
 */
bool opp_samples_files::accumulate_samples(counter_array_t& counter,
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

/**
 * samples_file_t - construct a samples_file_t object
 * @param filename the full path of sample file
 *
 * open and mmap the samples file specified by filename
 * samples file header coherence are checked
 *
 * all error are fatal
 *
 */
samples_file_t::samples_file_t(const std::string & filename)
	:
	sect_offset(0)
{
	db_open(&db_tree, filename.c_str(), DB_RDONLY, sizeof(struct opd_header));
}

/**
 * ~samples_file_t - destroy a samples_file_t object
 *
 * close and unmap the samples file
 *
 */
samples_file_t::~samples_file_t()
{
	if (db_tree.base_memory)
		db_close(&db_tree);
}

/**
 * check_headers - check than the lhs and rhs headers are
 * coherent (same size, same mtime etc.)
 * @param rhs the other samples_file_t
 *
 * all error are fatal
 *
 */
bool samples_file_t::check_headers(const samples_file_t & rhs) const
{
	::check_headers(header(), rhs.header());

	return true;
}

void db_tree_callback(db_key_t, db_value_t value, void * data)
{
	u32 * count = (u32 *)data;

	*count += value;
}

/**
 * count - return the number of samples in given range
 * @param start start samples nr of range
 * @param end end samples br of range
 *
 * return the number of samples in the the range [start, end]
 * no range checking is performed.
 *
 * This actually code duplicate partially accumulate member of
 * opp_samples_files which in future must use this as it internal
 * implementation
 */
u32 samples_file_t::count(uint start, uint end) const
{
	u32 count = 0;

	db_travel(&db_tree, start - sect_offset, end - sect_offset,
		  db_tree_callback, &count);

	return count;
}
