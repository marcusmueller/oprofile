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
 */

#include <vector>
#include <set>
#include <algorithm>
#include <numeric>
#include <string>
#include <iostream>

#include <limits.h>

#include "../util/file_manip.h"

using std::string;
using std::cerr;
using std::endl;
using std::list;
using std::multiset;
using std::set;
using std::accumulate;
using std::vector;
using std::pair;
using std::sort;

#include "opf_filter.h"

extern uint op_nr_counters;

namespace {

inline double do_ratio(size_t counter, size_t total)
{
	return total == 0 ? 1.0 : ((double)counter / total);
}

}

//---------------------------------------------------------------------------
// Functors used as predicate for vma comparison.
struct less_sample_entry_by_vma {
	bool operator()(const sample_entry & lhs, const sample_entry & rhs) const {
		return lhs.vma < rhs.vma;
	}

	bool operator()(const symbol_entry & lhs, const symbol_entry & rhs) const {
		return (*this)(lhs.sample, rhs.sample);
	}
	bool operator()(const symbol_entry * lhs, const symbol_entry * rhs) const {
		return (*this)(lhs->sample, rhs->sample);
	}
};

//---------------------------------------------------------------------------
// Functors used as predicate for less than comparison of counter_array_t.
struct less_symbol_entry_by_samples_nr {
	// Precondition: index < op_nr_counters. Used also as default ctr.
	less_symbol_entry_by_samples_nr(size_t index_ = 0) : index(index_) {}

	bool operator()(const symbol_entry * lhs, const symbol_entry * rhs) const {
		// sorting by vma when samples count are identical is better
		if (lhs->sample.counter[index] != rhs->sample.counter[index])
			return lhs->sample.counter[index] > rhs->sample.counter[index];

		return lhs->sample.vma > rhs->sample.vma;
	}

	size_t index;
};

//---------------------------------------------------------------------------
// Functors used as predicate for filename::linenr less than comparison.
struct less_by_file_loc {
	bool operator()(const sample_entry *lhs, 
			const sample_entry *rhs) const {
		return lhs->file_loc < rhs->file_loc;
	}

	bool operator()(const symbol_entry *lhs,
			const symbol_entry *rhs) const {
		return lhs->sample.file_loc < rhs->sample.file_loc;
	}
};

//---------------------------------------------------------------------------
/// implementation of symbol_container_t
class symbol_container_impl {
 public:
	symbol_container_impl();

	size_t size() const;
	const symbol_entry & operator[](size_t index) const;
	void push_back(const symbol_entry &);
	const symbol_entry * find(string filename, size_t linenr) const;
	const symbol_entry * find_by_vma(bfd_vma vma) const;

	// get a vector of symbols sorted by increased count.
	void get_symbols_by_count(size_t counter, vector<const symbol_entry*>& v) const;
 private:
	void flush_input_symbol(size_t counter) const;
	void build_by_file_loc() const;

	vector<symbol_entry> v;

	typedef multiset<const symbol_entry *, less_symbol_entry_by_samples_nr>
		set_symbol_by_samples_nr;

	typedef set<const symbol_entry *, less_by_file_loc>
		set_symbol_by_file_loc;

	// Carefull : these *MUST* be declared after the vector to ensure
	// a correct life-time.
	// symbol_entry sorted by decreasing samples number.
	// symbol_entry_by_samples_nr[0] is sorted from counter0 etc.
	// This allow easy acccess to (at a samples numbers point of view) :
	//  the nth first symbols.
	//  the nth first symbols that accumulate a certain amount of samples.

	// lazily build when necessary from const function so mutable
	mutable set_symbol_by_samples_nr symbol_entry_by_samples_nr[OP_MAX_COUNTERS];

	mutable set_symbol_by_file_loc symbol_entry_by_file_loc;
};

symbol_container_impl::symbol_container_impl()
{
	// symbol_entry_by_samples_nr has been setup by the default ctr, we
	// need to rebuild it. do not assume that index 0 is correctly built
	for (size_t i = 0 ; i < op_nr_counters ; ++i) {
		less_symbol_entry_by_samples_nr compare(i);
		symbol_entry_by_samples_nr[i] =	set_symbol_by_samples_nr(compare);
	}
}

//---------------------------------------------------------------------------

inline size_t symbol_container_impl::size() const
{
	return v.size();
}

inline const symbol_entry & symbol_container_impl::operator[](size_t index) const
{
	return v[index];
}

inline void symbol_container_impl::push_back(const symbol_entry & symbol)
{
	v.push_back(symbol);
}

const symbol_entry *
symbol_container_impl::find(string filename, size_t linenr) const
{
	build_by_file_loc();

	symbol_entry symbol;
	symbol.sample.file_loc.filename = filename;
	symbol.sample.file_loc.linenr = linenr;

	set_symbol_by_file_loc::const_iterator it =
		symbol_entry_by_file_loc.find(&symbol);

	if (it != symbol_entry_by_file_loc.end())
		return *it;

	return 0;
}

void symbol_container_impl::flush_input_symbol(size_t counter) const
{
	// Update the sets of symbols entries sorted by samples count and the
	// set of symbol entries sorted by file location.
	if (v.size() && symbol_entry_by_samples_nr[counter].empty()) {
		for (size_t i = 0 ; i < v.size() ; ++i)
			symbol_entry_by_samples_nr[counter].insert(&v[i]);
	}
}

void  symbol_container_impl::build_by_file_loc() const
{
	if (v.size() && symbol_entry_by_file_loc.empty()) {
		for (size_t i = 0 ; i < v.size() ; ++i)
			symbol_entry_by_file_loc.insert(&v[i]);
	}
}

const symbol_entry * symbol_container_impl::find_by_vma(bfd_vma vma) const
{
	symbol_entry value;

	value.sample.vma = vma;

	vector<symbol_entry>::const_iterator it =
		lower_bound(v.begin(), v.end(), value, less_sample_entry_by_vma());

	if (it != v.end() && it->sample.vma == vma)
		return &(*it);

	return 0;
}

// get a vector of symbols sorted by increased count.
void symbol_container_impl::get_symbols_by_count(size_t counter, vector<const symbol_entry*> & v) const
{
	if (counter >= op_nr_counters) {
		throw "symbol_container_impl::get_symbols_by_count() : invalid counter number";
	}

	flush_input_symbol(counter);

	v.clear();

	const set_symbol_by_samples_nr & temp = symbol_entry_by_samples_nr[counter];

	set_symbol_by_samples_nr::const_iterator it;
	for (it = temp.begin() ; it != temp.end(); ++it) {
		v.push_back(*it);
	}
}

//---------------------------------------------------------------------------
// Visible user interface implementation, just dispatch to the implementation.

symbol_container_t::symbol_container_t()
	:
	impl(new symbol_container_impl())
{
}

symbol_container_t::~symbol_container_t()
{
	delete impl;
}

size_t symbol_container_t::size() const
{
	return impl->size();
}

const symbol_entry &
symbol_container_t::operator[](size_t index) const
{
	return (*impl)[index];
}

void symbol_container_t::push_back(const symbol_entry & symbol)
{
	impl->push_back(symbol);
}

const symbol_entry *
symbol_container_t::find(string filename, size_t linenr) const
{
	return impl->find(filename, linenr);
}

const symbol_entry * symbol_container_t::find_by_vma(bfd_vma vma) const
{
	return impl->find_by_vma(vma);
}

// get a vector of symbols sorted by increased count.
void symbol_container_t::get_symbols_by_count(size_t counter, vector<const symbol_entry*>& v) const
{
	return impl->get_symbols_by_count(counter, v);
}

//---------------------------------------------------------------------------
/// implementation of sample_container_t
class sample_container_impl {
 public:

	const sample_entry & operator[](size_t index) const;

	size_t size() const;

	bool accumulate_samples(counter_array_t & counter, const string & filename) const;

	const sample_entry * find_by_vma(bfd_vma vma) const;
	bool accumulate_samples(counter_array_t &, const string & filename, size_t linenr) const;
	void push_back(const sample_entry &);
 private:
	void flush_input_counter() const;

	vector<sample_entry> v;

	typedef multiset<const sample_entry *, less_by_file_loc> set_sample_entry_t;

	// Carefull : these *MUST* be declared after the vector to ensure
	// a correct life-time.
	// sample_entry sorted by increasing (filename, linenr).
	// lazily build when necessary from const function so mutable
	mutable set_sample_entry_t samples_by_file_loc;
};

//---------------------------------------------------------------------------
inline const sample_entry & sample_container_impl::operator[](size_t index) const
{
	return v[index];
}

inline size_t sample_container_impl::size() const
{
	return v.size();
}

inline void sample_container_impl::push_back(const sample_entry & sample)
{
	v.push_back(sample);
}

namespace {

inline counter_array_t & add_counts(counter_array_t & arr, const sample_entry * s)
{
	return arr += s->counter;
}

} // namespace anon

bool sample_container_impl::accumulate_samples(counter_array_t & counter,
					       const string & filename) const
{
	flush_input_counter();

	sample_entry lower, upper;

	lower.file_loc.filename = upper.file_loc.filename = filename;
	lower.file_loc.linenr = 0;
	upper.file_loc.linenr = INT_MAX;

	typedef set_sample_entry_t::const_iterator iterator;

	iterator it1 = samples_by_file_loc.lower_bound(&lower);
	iterator it2 = samples_by_file_loc.upper_bound(&upper);

	counter += accumulate(it1, it2, counter, add_counts);

	for (size_t i = 0 ; i < op_nr_counters ; ++i)
		if (counter[i] != 0)
			return true;

	return false;
}

const sample_entry * sample_container_impl::find_by_vma(bfd_vma vma) const
{
	sample_entry value;

	value.vma = vma;

	vector<sample_entry>::const_iterator it =
		lower_bound(v.begin(), v.end(), value, less_sample_entry_by_vma());

	if (it != v.end() && it->vma == vma)
		return &(*it);

	return 0;
}

bool sample_container_impl::accumulate_samples(counter_array_t & counter,
	const string & filename, size_t linenr) const
{
	flush_input_counter();

	sample_entry sample;

	sample.file_loc.filename = filename;
	sample.file_loc.linenr = linenr;

	typedef pair<set_sample_entry_t::const_iterator,
		set_sample_entry_t::const_iterator> p_it_t;

	p_it_t p_it = samples_by_file_loc.equal_range(&sample);

	counter += accumulate(p_it.first, p_it.second, counter, add_counts);

	for (size_t i = 0 ; i < op_nr_counters ; ++i) {
		if (counter[i] != 0)
			return true;
	}

	return false;
}

void sample_container_impl::flush_input_counter() const
{
	if (!v.size() || !samples_by_file_loc.empty())
		return;

	for (size_t i = 0 ; i < v.size() ; ++i) {
		samples_by_file_loc.insert(&v[i]);
	}
}

//---------------------------------------------------------------------------
// Visible user interface implementation, just dispatch to the implementation.

sample_container_t::sample_container_t()
	:
	impl(new sample_container_impl())
{
}

sample_container_t::~sample_container_t()
{
	delete impl;
}

const sample_entry & sample_container_t::operator[](size_t index) const {
	return (*impl)[index];
}

size_t sample_container_t::size() const {
	return impl->size();
}

bool sample_container_t::accumulate_samples(counter_array_t & counter,
					    const string & filename) const
{
	return impl->accumulate_samples(counter, filename);
}

const sample_entry * sample_container_t::find_by_vma(bfd_vma vma) const
{
	return impl->find_by_vma(vma);
}

bool sample_container_t::accumulate_samples(counter_array_t & counter,
					    const string & filename, size_t linenr) const
{
	return impl->accumulate_samples(counter, filename, linenr);
}

void sample_container_t::push_back(const sample_entry & sample)
{
	impl->push_back(sample);
}


//---------------------------------------------------------------------------
// implementation of samples_files_t

samples_files_t::samples_files_t()
{
}

samples_files_t::~samples_files_t()
{
}

// Post condition:
//  the symbols/samples are sorted by increasing vma.
//  the range of sample_entry inside each symbol entry are valid
//  the samples_by_file_loc member var is correctly setup.
void samples_files_t::
add(const opp_samples_files & samples_files, const opp_bfd & abfd,
      bool add_zero_samples_symbols, bool build_samples_by_vma,
      bool add_shared_libs)
{
	do_add(samples_files, abfd, add_zero_samples_symbols,
		 build_samples_by_vma);

	if (!add_shared_libs)
		return;
 
	string const dir = dirname(samples_files.sample_filename);
	string name = basename(samples_files.sample_filename);
 
	list<string> file_list;
 
	get_sample_file_list(file_list, dir, name + "}}}*");

	list<string>::const_iterator it;
	for (it = file_list.begin() ; it != file_list.end(); ++it) {
		string lib_name;
		extract_app_name(*it, lib_name);

		opp_samples_files samples_files(dir + "/" + *it);
		opp_bfd abfd(samples_files.header[samples_files.first_file],
			     samples_files.nr_samples,
			     demangle_filename(lib_name));

		do_add(samples_files, abfd, false, true);
	}
}

// Post condition:
//  the symbols/samples are sorted by increasing vma.
//  the range of sample_entry inside each symbol entry are valid
//  the samples_by_file_loc member var is correctly setup.
void samples_files_t::
do_add(const opp_samples_files & samples_files, const opp_bfd & abfd,
	 bool add_zero_samples_symbols, bool build_samples_by_vma)
{
	string image_name = bfd_get_filename(abfd.ibfd);

	/* kludge to get samples for image w/o symbols */
	if (abfd.syms.size() == 0) {
		u32 start, end;

		cout << "adding artificial symbols for " << image_name << endl;

		start = 0;
		end = start + samples_files.nr_samples;

		symbol_entry symb_entry;
		symb_entry.first = 0;
		symb_entry.name = "?" + image_name;
		symb_entry.sample.vma = start;  // wrong fix this later
		symb_entry.sample.file_loc.linenr = 0;
		symb_entry.sample.file_loc.image_name = image_name;

		samples_files.accumulate_samples(symb_entry.sample.counter,
						 start, end);
		counter += symb_entry.sample.counter;

		/* we can't call add_samples */
		for (u32 pos = start ; pos != end ; ++pos) {
			sample_entry sample;

			if (!samples_files.accumulate_samples(sample.counter, pos))
				continue;

			sample.file_loc.image_name = image_name;
			sample.vma = pos;  // wrong fix this later
			samples.push_back(sample);
		}

		symb_entry.last = samples.size();
		symbols.push_back(symb_entry);
	} /* kludgy block */

	for (size_t i = 0 ; i < abfd.syms.size(); ++i) {
		u32 start, end;
		const char* filename;
		uint linenr;
		symbol_entry symb_entry;

		abfd.get_symbol_range(i, start, end);

		bool found_samples =
		  samples_files.accumulate_samples(symb_entry.sample.counter,
						   start, end);

		if (found_samples == 0 && !add_zero_samples_symbols)
			continue;

		counter += symb_entry.sample.counter;

		symb_entry.name = demangle_symbol(abfd.syms[i]->name);

		if (abfd.get_linenr(i, start, filename, linenr)) {
			symb_entry.sample.file_loc.filename = filename;
			symb_entry.sample.file_loc.linenr = linenr;
		} else {
			symb_entry.sample.file_loc.filename = string();
			symb_entry.sample.file_loc.linenr = 0;
		}

		symb_entry.sample.file_loc.image_name = image_name;

		bfd_vma base_vma = abfd.syms[i]->value + abfd.syms[i]->section->vma;

		symb_entry.sample.vma = abfd.sym_offset(i, start) + base_vma;

		symb_entry.first = samples.size();

		if (build_samples_by_vma)
			add_samples(samples_files, abfd, i, start, end,
				    base_vma, image_name);

		symb_entry.last = samples.size();

		symbols.push_back(symb_entry);
	}
}

void samples_files_t::add_samples(const opp_samples_files& samples_files,
				  const opp_bfd& abfd, size_t sym_index,
				  u32 start, u32 end, bfd_vma base_vma,
				  const string & image_name)
{
	for (u32 pos = start; pos < end ; ++pos) {
		const char * filename;
		sample_entry sample;
		uint linenr;

		if (!samples_files.accumulate_samples(sample.counter, pos))
			continue;

		if (abfd.get_linenr(sym_index, pos, filename, linenr)) {
			sample.file_loc.filename = filename;
			sample.file_loc.linenr = linenr;
		} else {
			sample.file_loc.filename = string();
			sample.file_loc.linenr = 0;
		}

		sample.file_loc.image_name = image_name;

		sample.vma = abfd.sym_offset(sym_index, pos) + base_vma;

		samples.push_back(sample);
	}
}

const symbol_entry* samples_files_t::find_symbol(bfd_vma vma) const
{
	return symbols.find_by_vma(vma);
}

const symbol_entry* samples_files_t::find_symbol(const string & filename,
						size_t linenr) const
{ 
	return symbols.find(filename, linenr);
}

const sample_entry * samples_files_t::find_sample(bfd_vma vma) const
{ 
	return samples.find_by_vma(vma);
}

u32 samples_files_t::samples_count(size_t counter_nr) const
{
	return counter[counter_nr];
}

void samples_files_t::select_symbols(vector<const symbol_entry*> & result,
				     size_t ctr, double threshold,
				     bool until_threshold,
				     bool sort_by_vma) const
{
	vector<const symbol_entry *> v;
	symbols.get_symbols_by_count(ctr , v);

	u32 total_count = samples_count(ctr);

	for (size_t i = 0 ; i < v.size() && threshold >= 0 ; ++i) {
		double percent = do_ratio(v[i]->sample.counter[ctr], 
					  total_count);

		if (until_threshold || percent >= threshold)
			result.push_back(v[i]);

		if (until_threshold)
			threshold -=  percent;
	}

	if (sort_by_vma)
		sort(result.begin(), result.end(), less_sample_entry_by_vma());
}


namespace {

struct filename_by_samples {
	filename_by_samples(string filename_, double percent_)
		: filename(filename_), percent(percent_)
		{}

	bool operator<(const filename_by_samples & lhs) const {
		return percent > lhs.percent;
	}

	string filename;
	// ratio of samples which belongs to this filename.
	double percent;
};

}

void samples_files_t::select_filename(vector<string> & result, size_t ctr,
				      double threshold,
				      bool until_threshold) const
{
	set<string> filename_set;

	// Trying to iterate on symbols to create the set of filenames which
	// contain sample does not work: a symbol can contain samples and this
	// symbol is in a source file that contain zero sample because only
	// inline function in this source file contains samples.
	for (size_t i = 0 ; i < samples.size() ; ++i) {
		filename_set.insert(samples[i].file_loc.filename);
	}

	// Give a sort order on filename for the selected counter.
	vector<filename_by_samples> file_by_samples;

	u32 total_count = samples_count(ctr);

	set<string>::const_iterator it;
	for (it = filename_set.begin() ; it != filename_set.end() ;  ++it) {
		counter_array_t counter;

		samples_count(counter, *it);
			
		double percent = do_ratio(counter[ctr], total_count);

		filename_by_samples f(*it, percent);

		file_by_samples.push_back(f);
	}

	// now sort the file_by_samples entry.
	sort(file_by_samples.begin(), file_by_samples.end());

	for (size_t i = 0 ; i < file_by_samples.size() && threshold >= 0 ; ++i) {
		filename_by_samples & s = file_by_samples[i];

		if (until_threshold || s.percent >= threshold)
			result.push_back(s.filename);

		if (until_threshold)
			threshold -=  s.percent;
	}
}

bool samples_files_t::samples_count(counter_array_t & result,
				    const string & filename) const
{
	return samples.accumulate_samples(result, filename);
}

bool samples_files_t::samples_count(counter_array_t & result,
				    const string & filename, 
				    size_t linenr) const
{
	return samples.accumulate_samples(result, filename, linenr);
}

const sample_entry & samples_files_t::get_samples(size_t idx) const
{
	return samples[idx];

}
