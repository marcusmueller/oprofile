/**
 * @file symbol_sort.h
 * Sorting symbols
 *
 * @remark Copyright 2002, 2003 OProfile authors
 * @remark Read the file COPYING
 *
 * @author Philippe Elie
 * @author John Levon
 */

#ifndef SYMBOL_SORT_H
#define SYMBOL_SORT_H

#include "profile_container.h"

#include <vector>
#include <string>

class symbol_entry;

struct sort_options {
	enum sort_order {
		vma,
		sample,
		symbol,
		image,
		debug
	};

	sort_options() {}

	void add_sort_option(std::string const & name);
	void add_sort_option(sort_order order);

	/**
	 * Sort the vector by the given criteria.
	 */
	void sort(symbol_collection & syms, bool reverse_sort,
		  bool long_filenames) const;

	std::vector<sort_order> options;
};

#endif // SYMBOL_SORT_H
