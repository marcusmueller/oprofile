/**
 * @file opp_samples_files.h
 * Encapsulation of samples files belonging to the same image and same session
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author Philippe Elie
 * @author John Levon
 */

#ifndef OPP_SAMPLES_FILES_H
#define OPP_SAMPLES_FILES_H

#include <string>

#include "op_types.h"
#include "op_hw_config.h"
#include "utility.h"
#include "samples_file.h"

/** Store multiple samples files belonging to the same image and the same
 * session can hold OP_MAX_COUNTERS samples files */
struct opp_samples_files /*:*/  noncopyable {
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
	opp_samples_files(std::string const & sample_file, int counter);

	~opp_samples_files();
 
	/**
	 * check_mtime - check mtime of samples file against file
	 */
	void check_mtime(std::string const & file) const;

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
	 * accumulate_samples - lookup samples from a vma address
	 * @param counter where to accumulate the samples
	 * @param vma index of the samples.
	 *
	 * return false if no samples has been found
	 */
	bool accumulate_samples(counter_array_t & counter, uint vma) const;

	/**
	 * accumulate_samples - lookup samples from a range of vma address
	 * @param counter where to accumulate the samples
	 * @param start start index of the samples.
	 * @param end end index of the samples.
	 *
	 * return false if no samples has been found
	 */
	bool accumulate_samples(counter_array_t & counter,
				uint start, uint end) const;

	/**
	 * output_header() - output counter setup
	 *
	 * output to stdout the cpu type, cpu speed
	 * and all counter description available
	 */
	void output_header() const;

	/// return the header of the first opened samples file
	opd_header const & first_header() const {
		return samples[first_file]->header();
	}

	/**
	 * Set the start offset of the underlying samples files
	 * to non-zero (derived from the BFD) iff this contains
	 * the kernel or kernel module sample files.
	 */
	void set_start_offset(u32 start_offset);

	// TODO privatize when we can
	samples_file_t * samples[OP_MAX_COUNTERS];
	uint nr_counters;
private:
	std::string sample_filename;

	// used in do_list_xxxx/do_dump_gprof.
	size_t counter_mask;

	// cached value: index to the first opened file, setup as nearly as we
	// can in ctor.
	int first_file;

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
	 * if !can_fail all errors are fatal.
	 */
	void open_samples_file(u32 counter, bool can_fail);
};

#endif /* !OPP_SAMPLES_FILES_H */
