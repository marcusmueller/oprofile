#ifndef OPF_FILTER_H
#define OPF_FILTER_H

#include <stddef.h>
#include <iostream>
#include <string>

#include "oprofpp.h"

//---------------------------------------------------------------------------
/// A simple container for a fileno:linr location
struct file_location {
	/// the image name from where come the samples or the symbol
	string image_name;
	/// empty if not valid.
	string filename;
	/// 0 means invalid or code is generated internally by the compiler
	int linenr;
};

//---------------------------------------------------------------------------
/// associate vma address with a file location and a samples count
struct sample_entry {
	/// From where file location comes the samples
	file_location file_loc;
	/// From where virtual memory address comes the samples
	bfd_vma vma;
	/// the samples count
	counter_array_t counter;
};

//---------------------------------------------------------------------------
/// associate a symbol with a file location, samples count and vma address
struct symbol_entry {
	/// file location, vma and cumulated samples count for this symbol
	sample_entry sample;
	/// name of symbol
	string name;
	/// [first, last[ gives the range of sample_entry.
	size_t first;
	size_t last;
};

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
	const symbol_entry * find(string filename, size_t linenr) const;
	/// find the symbol at vma
	const symbol_entry * find_by_vma(bfd_vma vma) const;

	/// get a vector of symbol_entry sorted by increased count of samples
	void get_symbols_by_count(size_t counter, 
				  vector<const symbol_entry*>& v) const;

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
	bool accumulate_samples_for_file(counter_array_t & counter, 
					 const string & filename) const;
	/// calculate the total number of samples for a file/linenr. Return
	/// false if there is no sample for this file
	bool accumulate_samples(counter_array_t &, const string & filename, 
				size_t linenr) const;
	/// find a sample from a vma. Return NULL if no samples are available
	/// at this vma.
	const sample_entry * find_by_vma(bfd_vma vma) const;

private:
	/// member function of this class are delegated to this implementation
	sample_container_impl * impl;
};

#endif /* !OPF_FILTER_H */
