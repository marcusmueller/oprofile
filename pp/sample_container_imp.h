/**
 * @file sample_container_imp.h
 * Internal implementation of sample container
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author Philippe Elie <phil_el@wanadoo.fr>
 * @author John Levon <moz@compsoc.man.ac.uk>
 */

#ifndef SAMPLE_CONTAINER_IMP_H 
#define SAMPLE_CONTAINER_IMP_H

#include <vector>
#include <string>

class sample_container_imp_t {
public:
	sample_entry const & operator[](sample_index_t index) const;

	sample_index_t size() const;

	bool accumulate_samples(counter_array_t & counter,
				std::string const & filename,
				uint max_counters) const;

	sample_entry const * find_by_vma(bfd_vma vma) const;
 
	bool accumulate_samples(counter_array_t &, std::string const & filename,
				size_t linenr, uint max_counters) const;
 
	void push_back(sample_entry const &);
 
private:
	void flush_input_counter() const;

	std::vector<sample_entry> samples;

	typedef std::multiset<sample_entry const *, less_by_file_loc> set_sample_entry_t;

	// must be declared after the vector to ensure a correct life-time.
	// sample_entry sorted by increasing (filename, linenr).
	// lazily build when necessary from const function so mutable
	mutable set_sample_entry_t samples_by_file_loc;
};

#endif /* SAMPLE_CONTAINER_IMP_H */
