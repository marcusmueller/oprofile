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
#include <string>

#include <limits.h>

using namespace std;

#include "opf_filter.h"

//---------------------------------------------------------------------------
// Functor used as predicate for vma comparison.
struct less_sample_entry_by_vma {
	bool operator()(const sample_entry & lsh, const sample_entry & rsh) const {
		return lsh.vma < rsh.vma;
	}
};

//---------------------------------------------------------------------------
// Functor used as predicate for less than comparison of counter_array_t. There is no
//  multi-sort choice, ie the the counter number used is statically set at ctr time.
struct less_sample_entry_by_samples_nr {
	// Precondition: index < max_counter_number. Used also as default ctr.
	less_sample_entry_by_samples_nr(size_t index_ = 0) : index(index_) {}

	bool operator()(const sample_entry * lhs, const sample_entry * rhs) const {
		return lhs->counter[index] < rhs->counter[index];
	}

	size_t index;
};

//---------------------------------------------------------------------------
// Functor used as predicate for filename::linenr equality comparison.
struct equal_by_file_loc {
	equal_by_file_loc(const file_location & file_loc_)
		: file_loc(file_loc_) {}

	bool operator()(const file_location & lsh) const {
		return file_loc.linenr == lsh.linenr && 
			file_loc.filename == lsh.filename;
	}

	file_location file_loc;
};

//---------------------------------------------------------------------------
// Functor used as predicate for filename::linenr less than comparison.
struct less_by_file_loc {
	bool operator()(const file_location * lhs,
			const file_location * rhs) const {
		return lhs->filename < rhs->filename ||
			(lhs->filename == rhs->filename && lhs->linenr < rhs->linenr);
	}
};

// A predicate which return true if the iterator range is sorted.
template <class Iterator, class Compare>
bool range_iterator_sorted_p(Iterator first, Iterator last, const Compare & compare) {
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
// Implementation.

class symbol_container_impl {
 public:
	symbol_container_impl();

	size_t size() const;
	const symbol_entry & operator[](size_t index) const;
	      symbol_entry & operator[](size_t index);
	void push_back(const symbol_entry &);
	const symbol_entry * find(string filename, size_t linenr) const;
	void flush_input_symbol();
	const symbol_entry * find_by_vma(unsigned long vma) const;
 private:
	std::vector<symbol_entry> v;

	// Carefull : these *MUST* be declared after the vector to ensure
	// a correct life-time.
	// symbol_entry sorted by decreasing samples number.
	// symbol_entry_by_samples_nr[0] is sorted from counter0 etc.
	// This allow easy acccess to (at a samples numbers point of view) :
	//  the nth first symbols.
	//  the nth first symbols that cumulate a certain amount of samples.
	std::multiset<const symbol_entry *, less_sample_entry_by_samples_nr>
		symbol_entry_by_samples_nr[max_counter_number];
};

symbol_container_impl::symbol_container_impl() {
	// symbol_entry_by_samples_nr has been setup by the default ctr, we need to
	// rebuild it but index 0 is already correct because it correspond to the
	// default comparator ctr so start at index 1.
	for (size_t i = 1 ; i < max_counter_number ; ++i) {
		less_sample_entry_by_samples_nr compare(i);
		symbol_entry_by_samples_nr[i] =
			multiset<const symbol_entry *, less_sample_entry_by_samples_nr>(compare);
	}
}

//---------------------------------------------------------------------------

size_t symbol_container_impl::size() const {
	return v.size();
}

const symbol_entry & symbol_container_impl::operator[](size_t index) const {
	return v[index];
}

symbol_entry & symbol_container_impl::operator[](size_t index) {
	return v[index];
}

void symbol_container_impl::push_back(const symbol_entry & symbol) {
	v.push_back(symbol);
}

const symbol_entry * 
symbol_container_impl::find(string filename, size_t linenr) const {
	//  TODO : this is a linear search (number of symbols), if should be
	// replaced by a log(number of symbols) complexity search. 
	//  (really an increase of speed ? )
	file_location file_loc;
	file_loc.filename = filename;
	file_loc.linenr = linenr;

	equal_by_file_loc compare_by(file_loc);

	vector<symbol_entry>::const_iterator it = 
		find_if(v.begin(), v.end(), compare_by);

	if (it != v.end()) {
		return &(*it);
	}

	return 0;
}

void symbol_container_impl::flush_input_symbol() {
	// Update the set of symbol entry sorted by samples count.
	for (size_t i = 0 ; i < v.size() ; ++i) {
		for (size_t counter = 0 ; counter < max_counter_number ; ++counter)
			symbol_entry_by_samples_nr[counter].insert(&v[i]);
	}

	if (range_iterator_sorted_p(v.begin(), v.end(), 
				    less_sample_entry_by_vma()) == false) {
		cerr << "post condition fail : symbol_vector not "
		     << "sorted by increased vma" << endl;

		exit(1);
	}
}

const symbol_entry * symbol_container_impl::find_by_vma(unsigned long vma) const {

	symbol_entry value;

	value.vma = vma;

	vector<symbol_entry>::const_iterator it =
		lower_bound(v.begin(), v.end(), value, less_sample_entry_by_vma());

	if (it != v.end() && it->vma == vma)
		return &(*it);

	return 0;
}

//---------------------------------------------------------------------------
// Visible user interface implementation, just dispatch to the implementation.

symbol_container_t::symbol_container_t() 
	: 
	impl(new symbol_container_impl()) 
{
}

symbol_container_t::~symbol_container_t() {
	delete impl;
}

size_t symbol_container_t::size() const {
	return impl->size();
}

const symbol_entry &
symbol_container_t::operator[](size_t index) const {
	return (*impl)[index];
}

symbol_entry &
symbol_container_t::operator[](size_t index) {
	return (*impl)[index];
}

void symbol_container_t::push_back(const symbol_entry & symbol) {
	impl->push_back(symbol);
}

const symbol_entry * 
symbol_container_t::find(string filename, size_t linenr) const {
	return impl->find(filename, linenr);
}

void symbol_container_t::flush_input_symbol() {
	impl->flush_input_symbol();
}

const symbol_entry * symbol_container_t::find_by_vma(unsigned long vma) const {
	return impl->find_by_vma(vma);
}

//---------------------------------------------------------------------------
// Implementation.
class sample_container_impl {
 public:

	const sample_entry & operator[](size_t index) const;

	size_t size() const;

	bool cumulate_samples_for_file(counter_array_t & counter, const string & filename) const;

	const sample_entry * find_by_vma(unsigned long vma) const;
	bool cumulate_samples(counter_array_t &, const string & filename, size_t linenr) const;
	void flush_input_counter();
	void push_back(const sample_entry &);
 private:
	vector<sample_entry> v;

	// Carefull : these *MUST* be declared after the vector to ensure
	// a correct life-time.
	// sample_entry sorted by increasing (filename, linenr).
	multiset<const sample_entry *, less_by_file_loc> samples_by_file_loc;
};

//---------------------------------------------------------------------------
const sample_entry & sample_container_impl::operator[](size_t index) const {
	return v[index];
}

size_t sample_container_impl::size() const {
	return v.size();
}


bool sample_container_impl::cumulate_samples_for_file(counter_array_t & counter, 
						      const string & filename) const {
	sample_entry lower, upper;

	lower.filename = upper.filename = filename;
	lower.linenr = 0;
	upper.linenr = INT_MAX;

	typedef multiset<const sample_entry *, less_by_file_loc>
		set_sample_entry_t;
	typedef set_sample_entry_t::const_iterator iterator;

	iterator it1 = samples_by_file_loc.lower_bound(&lower);
	iterator it2 = samples_by_file_loc.upper_bound(&upper);

	for ( ; it1 != it2 ; ++it1) 	{
		counter += (*it1)->counter;
	}

	for (size_t i = 0 ; i < max_counter_number ; ++i)
		if (counter[i] != 0)
			return true;

	return false;
}

const sample_entry * sample_container_impl::find_by_vma(unsigned long vma) const {

	sample_entry value;

	value.vma = vma;

	vector<sample_entry>::const_iterator it =
		lower_bound(v.begin(), v.end(), value, less_sample_entry_by_vma());

	if (it != v.end() && it->vma == vma) {
		return &(*it);
	}

	return 0;
}

bool sample_container_impl::cumulate_samples(counter_array_t & counter,
					     const string & filename, size_t linenr) const {

	sample_entry sample;

	sample.filename = filename;
	sample.linenr = linenr;

	typedef multiset<const sample_entry *, less_by_file_loc>
		set_sample_entry_t;
	typedef pair<set_sample_entry_t::const_iterator,
		set_sample_entry_t::const_iterator> p_it_t;

	p_it_t p_it = samples_by_file_loc.equal_range(&sample);

	if (p_it.first != p_it.second) {
		set_sample_entry_t::const_iterator it;
		for (it = p_it.first ; it != p_it.second ; ++it) {
			counter += (*it)->counter;
		}

		return true;
	}

	return false;
}

void sample_container_impl::flush_input_counter() {

	for (size_t i = 0 ; i < v.size() ; ++i)
		samples_by_file_loc.insert(&v[i]);

	if (range_iterator_sorted_p(v.begin(), v.end(), 
				    less_sample_entry_by_vma()) == false) {
		cerr << "post condition fail : counter_vector not "
		     << "sorted by increased vma" << endl;

		exit(1);
	}
}

void sample_container_impl::push_back(const sample_entry & sample) {
	v.push_back(sample);
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

bool sample_container_t::cumulate_samples_for_file(counter_array_t & counter, 
						   const string & filename) const {
	return impl->cumulate_samples_for_file(counter, filename);
}

const sample_entry * sample_container_t::find_by_vma(unsigned long vma) const {
	return impl->find_by_vma(vma);
}

bool sample_container_t::cumulate_samples(counter_array_t & counter,
					  const string & filename, size_t linenr) const {
	return impl->cumulate_samples(counter, filename, linenr);
}

void sample_container_t::flush_input_counter() {
	impl->flush_input_counter();
}

void sample_container_t::push_back(const sample_entry & sample) {
	impl->push_back(sample);
}
