/**
 * @file samples_container.cpp
 * Sample file container
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author Philippe Elie <phil_el@wanadoo.fr>
 * @author John Levon <moz@compsoc.man.ac.uk>
 */

#include <vector>
#include <set>
#include <algorithm>
#include <numeric>
#include <string>
#include <list>

#include <limits.h>

#include "file_manip.h"

//#include "demangle_symbol.h"

using std::string;
using std::cout;
using std::cerr;
using std::endl;
using std::list;
using std::multiset;
using std::set;
using std::accumulate;
using std::vector;
using std::pair;
using std::sort;

#include "samples_container.h"

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

struct equal_symbol_by_name {
	equal_symbol_by_name(string const & name_) : name(name_) {}

	bool operator()(const symbol_entry & entry) const {
		return name == entry.name;
	}

	string name;
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
/// implementation of symbol_container_imp_t
class symbol_container_imp_t {
 public:
	symbol_index_t size() const;
	const symbol_entry & operator[](symbol_index_t index) const;
	void push_back(const symbol_entry &);
	const symbol_entry * find(string filename, size_t linenr) const;
	const symbol_entry * find(string name) const;
	const symbol_entry * find_by_vma(bfd_vma vma) const;

	// get a vector of symbols sorted by increased count.
	void get_symbols_by_count(size_t counter, vector<const symbol_entry*>& v) const;
 private:
	void build_by_file_loc() const;

	// the main container of symbols. multiple symbols with the same
	// name are allowed.
	vector<symbol_entry> symbols;

	// different named symbol at same file location are allowed e.g.
	// template instanciation
	typedef multiset<const symbol_entry *, less_by_file_loc>
		set_symbol_by_file_loc;

	// must be declared after the vector to ensure a correct life-time.

	// symbol_entry sorted by location order lazily build when necessary
	// from const function so mutable
	mutable set_symbol_by_file_loc symbol_entry_by_file_loc;
};

//---------------------------------------------------------------------------

inline symbol_index_t symbol_container_imp_t::size() const
{
	return symbols.size();
}

inline const symbol_entry & symbol_container_imp_t::operator[](symbol_index_t index) const
{
	return symbols[index];
}

inline void symbol_container_imp_t::push_back(const symbol_entry & symbol)
{
	symbols.push_back(symbol);
}

const symbol_entry *
symbol_container_imp_t::find(string filename, size_t linenr) const
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

const symbol_entry *
symbol_container_imp_t::find(string name) const
{
	vector<symbol_entry>::const_iterator it =
		std::find_if(symbols.begin(), symbols.end(), 
			  equal_symbol_by_name(name));

	if (it != symbols.end() && it->name == name)
		return &(*it);

	return 0;
}

void  symbol_container_imp_t::build_by_file_loc() const
{
	if (symbols.size() && symbol_entry_by_file_loc.empty()) {
		for (symbol_index_t i = 0 ; i < symbols.size() ; ++i)
			symbol_entry_by_file_loc.insert(&symbols[i]);
	}
}

const symbol_entry * symbol_container_imp_t::find_by_vma(bfd_vma vma) const
{
	symbol_entry value;

	value.sample.vma = vma;

	vector<symbol_entry>::const_iterator it =
		lower_bound(symbols.begin(), symbols.end(),
			    value, less_sample_entry_by_vma());

	if (it != symbols.end() && it->sample.vma == vma)
		return &(*it);

	return 0;
}

// get a vector of symbols sorted by increased count.
void symbol_container_imp_t::get_symbols_by_count(size_t counter, vector<const symbol_entry*> & v) const
{
	for (symbol_index_t i = 0 ; i < symbols.size() ; ++i)
		v.push_back(&symbols[i]);

	// check if this is necessary, already sanitized by caller ?
	counter = counter == size_t(-1) ? 0 : counter;
	less_symbol_entry_by_samples_nr compare(counter);

	std::stable_sort(v.begin(), v.end(), compare);
}

//---------------------------------------------------------------------------
/// implementation of sample_container_imp_t
class sample_container_imp_t {
 public:

	const sample_entry & operator[](sample_index_t index) const;

	sample_index_t size() const;

	bool accumulate_samples(counter_array_t & counter,
				const string & filename,
				uint max_counters) const;

	const sample_entry * find_by_vma(bfd_vma vma) const;
	bool accumulate_samples(counter_array_t &, const string & filename,
				size_t linenr, uint max_counters) const;
	void push_back(const sample_entry &);
 private:
	void flush_input_counter() const;

	vector<sample_entry> samples;

	typedef multiset<const sample_entry *, less_by_file_loc> set_sample_entry_t;

	// must be declared after the vector to ensure a correct life-time.
	// sample_entry sorted by increasing (filename, linenr).
	// lazily build when necessary from const function so mutable
	mutable set_sample_entry_t samples_by_file_loc;
};

//---------------------------------------------------------------------------

inline const sample_entry & sample_container_imp_t::operator[](sample_index_t index) const
{
	return samples[index];
}

inline sample_index_t sample_container_imp_t::size() const
{
	return samples.size();
}

inline void sample_container_imp_t::push_back(const sample_entry & sample)
{
	samples.push_back(sample);
}

namespace {

inline counter_array_t & add_counts(counter_array_t & arr, const sample_entry * s)
{
	return arr += s->counter;
}

} // namespace anon

bool sample_container_imp_t::accumulate_samples(counter_array_t & counter,
						const string & filename,
						uint max_counters) const
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

	for (size_t i = 0 ; i < max_counters ; ++i)
		if (counter[i] != 0)
			return true;

	return false;
}

const sample_entry * sample_container_imp_t::find_by_vma(bfd_vma vma) const
{
	sample_entry value;

	value.vma = vma;

	vector<sample_entry>::const_iterator it =
		lower_bound(samples.begin(), samples.end(), value,
			    less_sample_entry_by_vma());

	if (it != samples.end() && it->vma == vma)
		return &(*it);

	return 0;
}

bool sample_container_imp_t::accumulate_samples(counter_array_t & counter,
	const string & filename, size_t linenr, uint max_counters) const
{
	flush_input_counter();

	sample_entry sample;

	sample.file_loc.filename = filename;
	sample.file_loc.linenr = linenr;

	typedef pair<set_sample_entry_t::const_iterator,
		set_sample_entry_t::const_iterator> p_it_t;

	p_it_t p_it = samples_by_file_loc.equal_range(&sample);

	counter += accumulate(p_it.first, p_it.second, counter, add_counts);

	for (size_t i = 0 ; i < max_counters ; ++i) {
		if (counter[i] != 0)
			return true;
	}

	return false;
}

void sample_container_imp_t::flush_input_counter() const
{
	if (!samples.size() || !samples_by_file_loc.empty())
		return;

	for (sample_index_t i = 0 ; i < samples.size() ; ++i) {
		samples_by_file_loc.insert(&samples[i]);
	}
}

//---------------------------------------------------------------------------
// implementation of samples_container_t

samples_container_t::samples_container_t(bool add_zero_samples_symbols_,
					 OutSymbFlag flags_,
					 int counter_mask_)
	:
	symbols(new symbol_container_imp_t),
	samples(new sample_container_imp_t),
	nr_counters(static_cast<uint>(-1)),
	add_zero_samples_symbols(add_zero_samples_symbols_),
	flags(flags_),
	counter_mask(counter_mask_)
{
}

samples_container_t::~samples_container_t()
{
	delete symbols;
	delete samples;
}

// Post condition:
//  the symbols/samples are sorted by increasing vma.
//  the range of sample_entry inside each symbol entry are valid
//  the samples_by_file_loc member var is correctly setup.
void samples_container_t::
add(const opp_samples_files & samples_files, const op_bfd & abfd)
{
	// paranoiad checking
	if (nr_counters != static_cast<uint>(-1) && 
	    nr_counters != samples_files.nr_counters) {
		cerr << "Fatal: samples_container_t::do_add(): mismatch"
		     << "between nr_counters and samples_files.nr_counters\n";
		exit(EXIT_FAILURE);
	}
	nr_counters = samples_files.nr_counters;

	string image_name = bfd_get_filename(abfd.ibfd);

	for (symbol_index_t i = 0 ; i < abfd.syms.size(); ++i) {
		u32 start, end;
		char const * filename;
		uint linenr;
		symbol_entry symb_entry;

		abfd.get_symbol_range(i, start, end);

		bool found_samples =
		  samples_files.accumulate_samples(symb_entry.sample.counter,
						   start, end);

		if (found_samples == 0 && !add_zero_samples_symbols)
			continue;

		counter += symb_entry.sample.counter;

		// FIXME - kill char * !!!
//		char const * symname = abfd.syms[i].name();
//		symb_entry.name = symname ? demangle_symbol(symname) : "";
		symb_entry.name = abfd.syms[i].name();

		if ((flags & (osf_linenr_info | osf_short_linenr_info)) != 0 &&
		    abfd.get_linenr(i, start, filename, linenr)) {
			symb_entry.sample.file_loc.filename = filename;
			symb_entry.sample.file_loc.linenr = linenr;
		} else {
			symb_entry.sample.file_loc.linenr = 0;
		}

		symb_entry.sample.file_loc.image_name = image_name;

		bfd_vma base_vma = abfd.syms[i].vma();

		symb_entry.sample.vma = abfd.sym_offset(i, start) + base_vma;

		symb_entry.first = samples->size();

		if (flags & osf_details)
			add_samples(samples_files, abfd, i, start, end,
				    base_vma, image_name);

		symb_entry.last = samples->size();

		symbols->push_back(symb_entry);
	}
}

void samples_container_t::add_samples(const opp_samples_files& samples_files,
				      const op_bfd& abfd,
				      symbol_index_t sym_index,
				      u32 start, u32 end, bfd_vma base_vma,
				      const std::string & image_name)
{
	for (u32 pos = start; pos < end ; ++pos) {
		char const * filename;
		sample_entry sample;
		uint linenr;

		if (!samples_files.accumulate_samples(sample.counter, pos))
			continue;

		if ((flags & (osf_linenr_info | osf_short_linenr_info)) != 0 &&
		    sym_index != size_t(-1) &&
		    abfd.get_linenr(sym_index, pos, filename, linenr)) {
			sample.file_loc.filename = filename;
			sample.file_loc.linenr = linenr;
		} else {
			sample.file_loc.linenr = 0;
		}

		sample.file_loc.image_name = image_name;

		sample.vma = (sym_index != nil_symbol_index)
			? abfd.sym_offset(sym_index, pos) + base_vma 
			: pos;

		samples->push_back(sample);
	}
}

const symbol_entry* samples_container_t::find_symbol(bfd_vma vma) const
{
	return symbols->find_by_vma(vma);
}

const symbol_entry* samples_container_t::find_symbol(const std::string & filename,
						     size_t linenr) const
{
	return symbols->find(filename, linenr);
}

const symbol_entry* samples_container_t::find_symbol(std::string const & name) const {
	return symbols->find(name);
}

const sample_entry * samples_container_t::find_sample(bfd_vma vma) const
{ 
	return samples->find_by_vma(vma);
}

u32 samples_container_t::samples_count(size_t counter_nr) const
{
	return counter[counter_nr];
}

void samples_container_t::select_symbols(std::vector<const symbol_entry*> & result,
				     size_t ctr, double threshold,
				     bool until_threshold,
				     bool sort_by_vma) const
{
	vector<const symbol_entry *> v;

	symbols->get_symbols_by_count(ctr , v);

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
		if (percent != lhs.percent)
			return percent > lhs.percent;
		return filename > lhs.filename;
	}

	string filename;
	// ratio of samples which belongs to this filename.
	double percent;
};

}

void samples_container_t::select_filename(std::vector<std::string> & result,
				      size_t ctr, double threshold,
				      bool until_threshold) const
{
	set<string> filename_set;

	// Trying to iterate on symbols to create the set of filenames which
	// contain sample does not work: a symbol can contain samples and this
	// symbol is in a source file that contain zero sample because only
	// inline function in this source file contains samples.
	for (size_t i = 0 ; i < samples->size() ; ++i) {
		filename_set.insert((*samples)[i].file_loc.filename);
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

bool samples_container_t::samples_count(counter_array_t & result,
				    const std::string & filename) const
{
	return samples->accumulate_samples(result, filename, nr_counters);
}

bool samples_container_t::samples_count(counter_array_t & result,
				    const std::string & filename, 
				    size_t linenr) const
{
	return samples->accumulate_samples(result, filename, linenr,
					  nr_counters);
}

const sample_entry & samples_container_t::get_samples(sample_index_t idx) const
{
	return (*samples)[idx];

}
