/**
 * @file samples_file.h
 * Encapsulation of samples files
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author Philippe Elie
 * @author John Levon
 */

#ifndef SAMPLES_FILE_H
#define SAMPLES_FILE_H

#include <string>

#include "db.h"
#include "op_types.h"
#include "op_hw_config.h"
#include "utility.h"

class counter_array_t;
class opd_header;

/** A class to store one samples file */
struct samples_file_t /*:*/ noncopyable
{
	/**
	 * samples_file_t - construct a samples_file_t object
	 * @param filename the full path of sample file
	 *
	 * open and mmap the samples file specified by filename
	 * samples file header coherence are checked
	 *
	 * all error are fatal
	 */
	samples_file_t(std::string const & filename);

	/**
	 * ~samples_file_t - destroy a samples_file_t object
	 *
	 * close and unmap the samples file
	 */
	~samples_file_t();

	/**
	 * check_headers - check that the lhs and rhs headers are
	 * coherent (same size, same mtime etc.)
	 * @param headers the other samples_file_t
	 *
	 * all errors are fatal
	 */
	void check_headers(samples_file_t const & headers) const;

	/// return the sample count at the given position
	u32 count(uint start) const {
		return count(start, start + 1);
	}

	/**
	 * count - return the number of samples in given range
	 * @param start start samples nr of range
	 * @param end end samples nr of range
	 *
	 * return the number of samples in the the range [start, end)
	 * no range checking is performed.
	 *
	 * This actually code duplicate partially accumulate member of
	 * opp_samples_files which in future must use this as it internal
	 * implementation
	 */
	u32 count(uint start, uint end) const;

	/// return the header of this sample file
	opd_header const & header() const {
		return *static_cast<opd_header *>(db_tree.base_memory);
	}

	/// see member variable start_offset
	void set_start_offset(u32 start_offset_) {
		start_offset = start_offset_;
	}
private:
	/// the underlying db object
	db_tree_t db_tree;

	/**
	 * For the kernel and kernel modules, this value is non-zero and
	 * equal to the offset of the .text section. This is done because
	 * we use the information provided in /proc/ksyms, which only gives
	 * the mapped position of .text, and the symbol _text from
	 * vmlinux. This value is used to fix up the sample offsets
	 * for kernel code as a result of this difference (in user-space
	 * samples, the sample offset is from the start of the mapped
	 * file, as seen in /proc/pid/maps).
	 */
	u32 start_offset;
};

#endif /* !SAMPLES_FILE_H */
