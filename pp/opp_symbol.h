/**
 * @file opp_symbol.h
 * Symbol containers
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author Philippe Elie
 * @author John Levon
 */

#ifndef OPP_SYMBOL_H
#define OPP_SYMBOL_H

#include "config.h"

#include <string>
#include <vector>
#include <iostream>

#include <bfd.h>

#include "counter_array.h"

typedef size_t sample_index_t;

/// A simple container for a fileno:linenr location.
struct file_location {
	/// From where image come this file location
	std::string image_name;
	/// owning application name: identical to image name if profiling
	/// session did not separate samples for shared libs or if image_name
	// is not a shared libs
	std::string app_name;
	/// empty if not valid.
	std::string filename;
	/// 0 means invalid or code is generated internally by the compiler
	int linenr;

	bool operator<(file_location const & rhs) const {
		return filename < rhs.filename ||
			(filename == rhs.filename && linenr < rhs.linenr);
	}
};


/// associate vma address with a file location and a samples count
struct sample_entry {
	/// From where file location comes the samples
	file_location file_loc;
	/// From where virtual memory address comes the samples
	bfd_vma vma;
	/// the samples count
	counter_array_t counter;
};


/// associate a symbol with a file location, samples count and vma address
struct symbol_entry {
	/// file location, vma and cumulated samples count for this symbol
	sample_entry sample;
	/// name of symbol
	std::string name;
	/// [first, last[ gives the range of sample_entry.
	sample_index_t first;
	sample_index_t last;
	/// symbol size as calculated by op_bfd, start of symbol is sample.vma
	size_t size;
};

#endif /* !OPP_SYMBOL_H */
