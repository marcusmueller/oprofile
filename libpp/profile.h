/**
 * @file profile.h
 * Encapsulation for samples files over all counter belonging to the
 * same binary image
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author Philippe Elie
 * @author John Levon
 */

#ifndef PROFILE_H
#define PROFILE_H

#include <string>
#include <map>

#include "odb_hash.h"
#include "op_types.h"
#include "op_hw_config.h"
#include "utility.h"

class opd_header;

/**
 * Class containing a single sample file contents.
 * i.e. set of count values for VMA offsets for
 * a particular binary.
 */
class profile_t : noncopyable {
public:
	/**
	 * profile_t - construct an empty  profile_t object
	 */
	profile_t();

	~profile_t();
 
	/**
	 * accumulate_samples - lookup samples from a vma address
	 * @param vma index of the samples.
	 *
	 * return zero if no samples has been found
	 */
	unsigned int accumulate_samples(uint vma) const;

	/**
	 * accumulate_samples - lookup samples from a range of vma address
	 * @param start start index of the samples.
	 * @param end end index of the samples.
	 *
	 * return zero if no samples has been found
	 */
	unsigned int accumulate_samples(uint start, uint end) const;

	/// return the header of the last opened samples file
	opd_header const & get_header() const {
		return *file_header;
	}

	/**
	 * cumulate sample file to our container of samples
	 * @param filename  sample file name
	 * @param offset the offset for kernel files, \sa start_offset
	 *
	 * store samples for one sample file, sample file header is sanitized.
	 *
	 * all error are fatal
	 */
	void add_sample_file(std::string const & filename, u32 offset);

private:

	/// copy of the samples file header
	scoped_ptr<opd_header> file_header;

	/// storage type for samples sorted by eip
	typedef std::map<odb_key_t, odb_value_t> ordered_samples_t;

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

#endif /* !PROFILE_H */
