/* COPYRIGHT (C) 2001 Philippe Elie
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59 Temple
 * Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * first written by P.Elie, many cleanup by John Levon
 */
#ifndef OPF_FILTER_H
#define OPF_FILTER_H

#include <stddef.h>
#include <iostream>
#include <string>

#include "oprofpp.h"
#include "opp_symbol.h"

//---------------------------------------------------------------------------
class symbol_container_impl;

/// a container of symbols: member function dispatch to symbol_container_impl
class symbol_container_t {
public:
	 symbol_container_t();
	~symbol_container_t();

	/// add a symbol to the underlined container. No attempt to detect
	/// duplicate symbol is made
	void push_back(const symbol_entry &);

	/// return the number of symbols in the underlined container
	size_t size() const;
	/// return the symbol at index. Index range checking is not performed
	const symbol_entry & operator[](size_t index) const;
	/// find the symbol at filename / linenr location
	const symbol_entry * find(std::string filename, size_t linenr) const;
	/// find the symbol at vma
	const symbol_entry * find_by_vma(bfd_vma vma) const;

	/// get a vector of symbol_entry sorted by increased count of samples
	void get_symbols_by_count(size_t counter, 
				  std::vector<const symbol_entry*>& v) const;

private:
	/// member function of this class are delegated to this implementation
	symbol_container_impl * impl;
};

//---------------------------------------------------------------------------

class sample_container_impl;

/// a container of samples: member function dispatch to sample_container_impl
class sample_container_t {
public:
	sample_container_t();
	~sample_container_t();

	/// add a sample to the underlined container. No attempt to detect
	/// duplicate sample is made
	void push_back(const sample_entry &);

	/// return the number of samples in the underlined container
	size_t size() const;
	/// return the sample at index. Index range checking is not performed
	const sample_entry & operator[](size_t index) const;
	/// calculate the total number of samples for a file. Return false
	/// if there is no sample for this file
	bool accumulate_samples(counter_array_t & counter, 
				const std::string & filename,
				uint max_counters) const;
	/// calculate the total number of samples for a file/linenr. Return
	/// false if there is no sample for this file
	bool accumulate_samples(counter_array_t &, const std::string & filename, 
				size_t linenr, uint max_counters) const;
	/// find a sample from a vma. Return NULL if no samples are available
	/// at this vma.
	const sample_entry * find_by_vma(bfd_vma vma) const;

private:
	/// member function of this class are delegated to this implementation
	sample_container_impl * impl;
};

//---------------------------------------------------------------------------
/// A container to store symbol/sample from samples files/image file
class samples_files_t {
public:
	 samples_files_t();
	~samples_files_t();

	/**
	 * add() -  record symbols/samples in the underlined container
	 * @samples_files: the samples files container
	 * @abf: the associated bfd object
	 * @add_zero_samples_symbols: must we add to the symbol container
	 *   symbols with zero samples count
	 * @flags: optimize hint to add samples. The flags is a promise on what
	 * will be required as information in future. Avoid to pass
	 * osf_linenr_info greatly improve performance of add. Avoiding
	 * osf_details is also an improvement.
	 * @add_shared_libs: add to the set of symbols/samples shared libs
	 *  which belongs to this image, only meaningfull if samples come from
	 *  a --separate-samples session
	 *
	 * add() is an helper for delayed ctor. Take care you can't safely
	 * make any call to add after any other member function call.
	 * Successive call to build must use the same boolean value and
	 * obviously you can add only samples files which are coherent (same
	 * sampling rate, same events etc.)
	 */
	void add(const opp_samples_files& samples_files,
		 const opp_bfd& abfd, bool add_zero_samples_symbols,
		 OutSymbFlag flags, bool add_shared_libs, int counter);

	/// Find a symbol from its vma, return zero if no symbol at this vma
	const symbol_entry* find_symbol(bfd_vma vma) const;
	/// Find a symbol from its filename, linenr, return zero if no symbol
	/// at this location
	const symbol_entry* find_symbol(const std::string & filename,
					size_t linenr) const;

	/// Find a sample by its vma, return zero if no sample at this vma
	const sample_entry * find_sample(bfd_vma vma) const;

	/// Return a sample_entry by its index, index must be valid
	const sample_entry & get_samples(size_t idx) const;

	/**
	 * select_symbols - create a set of symbols sorted by sample count
	 * @result: where to put result
	 * @threshold: the filter threshold
	 * @until_threshold: rather to get symbols with more than
	 * @threshold percent of samples get symbols until the amount
	 *   of samples reach @threshold
	 * @sort_by_vma: rather to sort symbols by samples count
	 *   sort them by vma
	 *
	 * @until_threshold and @threshold acts like the -w and -u options
	 * of op_to_source
	 * if you need to get all symbols call it with @threshold == 0.0
	 * and @until_threshold == false
	 */
	void select_symbols(std::vector<const symbol_entry*> & result,
			    size_t ctr, double threshold,
			    bool until_threshold,
			    bool sort_by_vma = false) const;

	/// Like select_symbols for filename without allowing sort by vma.
	void select_filename(std::vector<std::string> & result, size_t ctr,
			     double threshold, bool until_threshold) const;

	/// return the total number of samples for counter_nr
	u32 samples_count(size_t counter_nr) const;

	/// Get the samples count which belongs to filename. Return false if
	/// no samples found.
	bool samples_count(counter_array_t & result,
			   const std::string & filename) const;
	/// Get the samples count which belongs to filename, linenr. Return
	/// false if no samples found.
	bool samples_count(counter_array_t & result,
			   const std::string & filename,
			   size_t linenr) const;
	/// you can call this *after* the first call to add()
	uint get_nr_counters() const { return nr_counters; }
private:
	/// helper for add();
	void do_add(const opp_samples_files & samples_files,
		    const opp_bfd & abfd,
		    bool add_zero_samples_symbols,
		    bool build_samples_by_vma,
		    bool record_linenr_info);
	/// helper for do_add()
	void add_samples(const opp_samples_files& samples_files, 
			 const opp_bfd & abfd, size_t sym_index,
			 u32 start, u32 end, bfd_vma base_vma,
			 const std::string & image_name,
			 bool record_linenr_info);

	/**
	 * create an unique artificial symbol for an offset range. The range
	 * is only a hint of the maximum size of the created symbol. We
	 * give to the symbol an unique name as ?image_file_name#order and
	 * a range up to the nearest of syms or for the whole range if no
	 * syms exist after the start offset. the end parameter is updated
	 * to reflect the symbol range.
	 *
	 * The rationale here is to try to create symbols for alignment between
	 * function as little as possible and to create meaningfull symbols
	 * for special case such image w/o symbol.
	 * I am no longer convinced we really need that.
	 */
	std::string create_artificial_symbol(const opp_bfd & abfd, u32 start,
					     u32 & end, size_t & order);

	/// The symbols collected by oprofpp sorted by increased vma, provide
	/// also a sort order on samples count for each counter.
	symbol_container_t symbols;
	/// The samples count collected by oprofpp sorted by increased vma,
	/// provide also a sort order on (filename, linenr)
	sample_container_t samples;
	/// build() must count samples count for each counter so cache it here
	/// since user of samples_files_t often need it later.
	counter_array_t counter;
	/// maximum number of counter available
	uint nr_counters;
};

#endif /* !OPF_FILTER_H */
