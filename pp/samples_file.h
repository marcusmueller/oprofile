/**
 * @file samples_file.h
 * Encapsulation of samples files
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author Philippe Elie <phil_el@wanadoo.fr>
 * @author John Levon <moz@compsoc.man.ac.uk>
 */

#ifndef SAMPLES_FILE_H
#define SAMPLES_FILE_H

#include <string>

#include "db.h"
#include "op_types.h"
#include "op_hw_config.h"

class counter_array_t;

/** A class to store one samples file */
struct samples_file_t
{
	samples_file_t(const std::string & filename);
	~samples_file_t();

	bool check_headers(const samples_file_t & headers) const;

	u32 count(uint start) const { 
		return  count(start, start + 1);
	}

	u32 count(uint start, uint end) const;

	const struct opd_header * header() const {
		return static_cast<opd_header *>(db_tree.base_memory);
	}

	// probably needs to be private and create the neccessary member
	// function (not simple getter), make private and compile to see
	// what operation we need later. I've currently not a clear view
	// of what we need
//private:
	db_tree_t db_tree;

	// this offset is zero for user space application and the file pos
	// of text section for kernel and module.
	u32 sect_offset;

private:
	// neither copy-able or copy constructible
	samples_file_t(const samples_file_t &);
	samples_file_t& operator=(const samples_file_t &);
};

/** Store multiple samples files belonging to the same image and the same
 * session can hold OP_MAX_COUNTERS samples files */
struct opp_samples_files {
	/**
	 * @param sample_file name of sample file to open w/o the #nr suffix
	 * @param counter a bit mask specifying which sample file to open
	 *
	 * Open all samples files specified through sample_file and counter.
	 * Currently all error are fatal
	 */
	opp_samples_files(const std::string & sample_file, int counter);

	/** Close all opened samples files and free all related resource. */
	~opp_samples_files();

	/**
	 * is_open - test if a samples file is open
	 * @param i index of the samples file to check.
	 *
	 * return true if the samples file index is open
	 */ 
	bool is_open(int i) const {
		return samples[i] != 0;
	}
 
	/**
	 * @param i index of the samples files
	 * @param sample_nr number of the samples to test.
	 *
	 * return the number of samples for samples file index at position
	 * sample_nr. return 0 if the samples file is close or there is no
	 * samples at position sample_nr
	 */
	uint samples_count(int i, int sample_nr) const {
		return is_open(i) ? samples[i]->count(sample_nr) : 0;
	}

	/**
	 * @param counter where to accumulate the samples
	 * @param vma FIXME
	 *
	 * return false if no samples has been found
	 */

	bool accumulate_samples(counter_array_t & counter, uint vma) const;
	/**
	 * @param counter where to accumulate the samples
	 * @param start start index of the samples.
	 * @param end end index of the samples.
	 *
	 * return false if no samples has been found
	 */
	bool accumulate_samples(counter_array_t& counter,
				uint start, uint end) const;

	// this look like a free fun
	void output_header() const;

	/// return a struct opd_header * of the first openened samples file
	const struct opd_header * first_header() const {
		return samples[first_file]->header();
	}

	void set_sect_offset(u32 sect_offset);

	// TODO privatize as we can.
	samples_file_t * samples[OP_MAX_COUNTERS];
	uint nr_counters;
	std::string sample_filename;

	// used in do_list_xxxx/do_dump_gprof.
	size_t counter_mask;

private:
	// cached value: index to the first opened file, setup as nearly as we
	// can in ctor.
	int first_file;

	// ctor helper
	void open_samples_file(u32 counter, bool can_fail);
};

#endif /* !SAMPLES_FILE_H */
