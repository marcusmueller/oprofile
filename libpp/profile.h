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
#include <iterator>

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
 
	/// return the header of the last opened samples file
	opd_header const & get_header() const {
		return *file_header;
	}

	/**
	 * count samples count w/o recording them
	 * @param filename sample filename
	 *
	 * convenience interface for raw access to sample count w/o recording
	 * them. It's placed here so all access to samples files go through
	 * profile_t static or non static member.
	 */
	static unsigned int sample_count(string const & filename);

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

	class const_iterator;
	typedef std::pair<const_iterator, const_iterator> iterator_pair;

	/**
	 * @param start  start offset
	 * @param end  end offset
	 *
	 * return an iterator pair to [start, end) range
	 */
	iterator_pair
	samples_range(unsigned int start, unsigned int end) const;

	/// return a pair of iterator for all samples
	iterator_pair samples_range() const;

private:
	/// helper for sample_count() and add_sample_file(). All error launch
	/// an exception.
	static void open_sample_file(string const & filename, samples_odb_t &);

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


// It will be easier to derive profile_t::const_iterator from
// std::iterator<std::input_iterator_tag, unsigned int> but this doesn't
// work for gcc <= 2.95 so we provide the neccessary typedef in the hard way.
// See ISO C++ 17.4.3.1 § 1 and 14.7.3 § 9.
namespace std {
	template <>
		struct iterator_traits<profile_t::const_iterator> {
			typedef ptrdiff_t difference_type;
			typedef unsigned int value_type;
			typedef unsigned int* pointer;
			typedef unsigned int& reference;
			typedef input_iterator_tag iterator_category;
		};
}


class profile_t::const_iterator
{
	typedef ordered_samples_t::const_iterator iterator_t;
public:
	const_iterator() : start_offset(0) {}
	const_iterator(iterator_t it_, u32 start_offset_)
		: it(it_), start_offset(start_offset_) {}

	unsigned int operator*() const { return it->second; }
	const_iterator & operator++() { ++it; return *this; }

	unsigned int vma() const { return it->first + start_offset; }
	unsigned int count() const { return **this; }

	bool operator!=(const_iterator const & rhs) const {
		return it != rhs.it;
	}

private:
	iterator_t it;
	u32 start_offset;
};

#endif /* !PROFILE_H */
