/**
 * @file symbol_container_imp.h
 * Internal container for symbols
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author Philippe Elie
 * @author John Levon
 */

#ifndef SYMBOL_CONTAINER_IMP_H
#define SYMBOL_CONTAINER_IMP_H

#include <vector>
#include <string>
#include <set>

#include "samples_container.h"

class symbol_container_imp_t {
public:
	symbol_index_t size() const;

	symbol_entry const & operator[](symbol_index_t index) const;

	void push_back(symbol_entry const &);

	symbol_entry const * find(std::string filename, size_t linenr) const;

	symbol_entry const * find(std::string name) const;

	symbol_entry const * find_by_vma(bfd_vma vma) const;

	void get_symbols_by_count(size_t counter, samples_container_t::symbol_collection& v) const;

private:
	void build_by_file_loc() const;

	/// the main container of symbols. multiple symbols with the same
	/// name are allowed.
	std::vector<symbol_entry> symbols;

	/// different named symbol at same file location are allowed e.g.
	/// template instanciation
	typedef std::multiset<symbol_entry const *, less_by_file_loc>
		set_symbol_by_file_loc;

	// must be declared after the vector to ensure a correct life-time.

	/// symbol_entry sorted by location order lazily build when necessary
	/// from const function so mutable
	mutable set_symbol_by_file_loc symbol_entry_by_file_loc;
};

#endif /* SYMBOL_CONTAINER_IMP_H */
