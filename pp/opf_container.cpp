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

using std::string;
using std::cerr;
using std::endl;
using std::multiset;
using std::set;
using std::accumulate;
using std::vector;
using std::pair;

#include "opf_filter.h"

extern uint op_nr_counters;

//---------------------------------------------------------------------------
// Functors used as predicate for vma comparison.
struct less_sample_entry_by_vma {
	bool operator()(const sample_entry & lhs, const sample_entry & rhs) const {
		return lhs.vma < rhs.vma;
	}

	bool operator()(const symbol_entry & lhs, const symbol_entry & rhs) const {
		return (*this)(lhs.sample, rhs.sample);
	}
};

//---------------------------------------------------------------------------
// Functors used as predicate for less than comparison of counter_array_t. There is no
//  multi-sort choice, ie the the counter number used is statically set at ctr time.
struct less_symbol_entry_by_samples_nr {
	// Precondition: index < op_nr_counters. Used also as default ctr.
	less_symbol_entry_by_samples_nr(size_t index_ = 0) : index(index_) {}

	bool operator()(const symbol_entry * lhs, const symbol_entry * rhs) const {
		return lhs->sample.counter[index] > rhs->sample.counter[index];
	}

	size_t index;
};

//---------------------------------------------------------------------------
// Functors used as predicate for filename::linenr less than comparison.
struct less_by_file_loc {
	bool operator()(const file_location &lhs,
			const file_location &rhs) const {
		return lhs.filename < rhs.filename ||
			(lhs.filename == rhs.filename && lhs.linenr < rhs.linenr);
	}

	bool operator()(const sample_entry *lhs,
			const sample_entry *rhs) const {
		return (*this)(lhs->file_loc, rhs->file_loc);
	}

	bool operator()(const symbol_entry *lhs,
			const symbol_entry *rhs) const {
		return (*this)(&lhs->sample, &rhs->sample);
	}
};

// A predicate which return true if the iterator range is sorted.
template <class Iterator, class Compare>
bool range_iterator_sorted_p(Iterator first, Iterator last, const Compare & compare)
{
	if (distance(first, last) > 1) {
  		for (++first ; first != last ; ++first) {
			Iterator temp = first;
			--temp;
			if (!compare(*temp, *first)) {
				return false;
			}
		}
	}

	return true;
}


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
		if (range_iterator_sorted_p(v.begin(), v.end(),
				    less_sample_entry_by_vma()) == false) {
			cerr << "opf_filter: post condition fail : "
			     << "symbol_vector not sorted by increased vma"
			     << endl;

			exit(EXIT_FAILURE);
		}

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

	bool accumulate_samples_for_file(counter_array_t & counter, const string & filename) const;

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

counter_array_t & add_counts(counter_array_t & arr, const sample_entry * s)
{
	return arr += s->counter;
}

} // namespace anon

bool sample_container_impl::accumulate_samples_for_file(counter_array_t & counter,
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

	counter += std::accumulate(p_it.first, p_it.second, counter, add_counts);

	for (size_t i = 0 ; i < op_nr_counters ; ++i)
		if (counter[i] != 0)
			return true;

	return false;
}

void sample_container_impl::flush_input_counter() const
{
	if (v.size() && samples_by_file_loc.empty()) {
		for (size_t i = 0 ; i < v.size() ; ++i)
			samples_by_file_loc.insert(&v[i]);

		if (range_iterator_sorted_p(v.begin(), v.end(),
					less_sample_entry_by_vma()) == false) {
			cerr << "opf_filter: post condition fail : "
			     << "counter_vector not sorted by increased vma"
			     << endl;

			exit(EXIT_FAILURE);
		}
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

bool sample_container_t::accumulate_samples_for_file(counter_array_t & counter,
						     const string & filename) const
{
	return impl->accumulate_samples_for_file(counter, filename);
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
