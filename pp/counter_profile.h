/**
 * @file counter_profile.h
 * Encapsulation of one samples file
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author Philippe Elie
 * @author John Levon
 */

#ifndef COUNTER_PROFILE_H
#define COUNTER_PROFILE_H

#include <string>
#include <map>

#include "db_hash.h"
#include "op_types.h"
#include "op_hw_config.h"
#include "utility.h"

class counter_array_t;
class opd_header;

/** A class to store a sample file for one counter */
class counter_profile_t /*:*/ noncopyable
{
public:
	/**
	 * counter_profile_t - construct a counter_profile_t object
	 * @param filename the full path of sample file
	 *
	 * open and mmap the samples file specified by filename
	 * samples file header coherence are checked
	 *
	 * all error are fatal
	 */
	counter_profile_t(std::string const & filename);

	/**
	 * ~counter_profile_t - destroy a counter_profile_t object
	 *
	 * relax resource used by a counter_profile_t object
	 */
	~counter_profile_t();

	/**
	 * check_headers - check that the lhs and rhs headers are
	 * coherent (same size, same mtime etc.)
	 * @param headers the other counter_profile_t
	 *
	 * all errors are fatal
	 */
	void check_headers(counter_profile_t const & headers) const;

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
	 */
	u32 count(uint start, uint end) const;

	/// return the header of this sample file
	opd_header const & header() const {
		return *file_header;
	}

	/// see member variable start_offset
	void set_start_offset(u32 start_offset_) {
		start_offset = start_offset_;
	}

private:
	/// storage type for samples sorted by eip
	typedef std::map<db_key_t, db_value_t> ordered_samples_t;

	/// helper to build ordered samples by eip
	void build_ordered_samples(string const & filename);

	/// copy of the samples file header
	scoped_ptr<opd_header> file_header;

	/**
	 * Samples are stored in hash table, iterating over hash table don't
	 * provide any ordering, the above count() interface rely on samples
	 * ordered by eip. This map is only a temporary storage where samples
	 * are ordered by eip.
	 */
	ordered_samples_t ordered_samples;

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

#endif /* !COUNTER_PROFILE_H */
