/**
 * @file symbol_functors.h
 * Functors for symbol/sample comparison
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author Philippe Elie
 * @author John Levon
 */

#ifndef SYMBOL_FUNCTORS_H
#define SYMBOL_FUNCTORS_H

#include "opp_symbol.h"

/// compare based on vma value (address)
struct less_sample_entry_by_vma {
	bool operator()(sample_entry const & lhs, sample_entry const & rhs) const {
		return lhs.vma < rhs.vma;
	}

	bool operator()(symbol_entry const & lhs, symbol_entry const & rhs) const {
		return (*this)(lhs.sample, rhs.sample);
	}
	bool operator()(symbol_entry const * lhs, symbol_entry const * rhs) const {
		return (*this)(lhs->sample, rhs->sample);
	}
};

/// compare based on number of accumulated samples
struct less_symbol_entry_by_samples_nr {
	// Precondition: index < op_nr_counters. Used also as default ctr.
	less_symbol_entry_by_samples_nr(size_t index_ = 0) : index(index_) {}

	bool operator()(symbol_entry const * lhs, symbol_entry const * rhs) const {
		// sorting by vma when samples count are identical is better
		if (lhs->sample.counter[index] != rhs->sample.counter[index])
			return lhs->sample.counter[index] > rhs->sample.counter[index];

		return lhs->sample.vma > rhs->sample.vma;
	}

	size_t index;
};

/// compare based on same symbol name
struct equal_symbol_by_name {
	equal_symbol_by_name(std::string const & name_) : name(name_) {}

	bool operator()(symbol_entry const & entry) const {
		return name == entry.name;
	}

	std::string name;
};

/// compare based on file location
struct less_by_file_loc {
	bool operator()(sample_entry const * lhs,
			sample_entry const * rhs) const {
		return lhs->file_loc < rhs->file_loc;
	}

	bool operator()(symbol_entry const * lhs,
			symbol_entry const * rhs) const {
		return lhs->sample.file_loc < rhs->sample.file_loc;
	}
};

#endif /* SYMBOL_FUNCTORS_H */
