/**
 * @file samples_container.cpp
 * Sample file container
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author Philippe Elie
 * @author John Levon
 */

#include <set>
#include <vector>
#include <string>
#include <iostream>
#include <algorithm>

#include "symbol_functors.h"
#include "samples_container.h"
#include "samples_file.h"
#include "sample_container_imp.h"
#include "symbol_container_imp.h"

using namespace std;

namespace {

// FIXME: op_ratio.h in libutil ??
inline double do_ratio(size_t counter, size_t total)
{
	return total == 0 ? 1.0 : ((double)counter / total);
}

struct filename_by_samples {
	filename_by_samples(string filename_, double percent_)
		: filename(filename_), percent(percent_)
		{}

	bool operator<(filename_by_samples const & lhs) const {
		if (percent != lhs.percent)
			return percent > lhs.percent;
		return filename > lhs.filename;
	}

	string filename;
	// ratio of samples which belongs to this filename.
	double percent;
};

}

samples_container_t::samples_container_t(bool add_zero_samples_symbols_,
					 outsymbflag flags_,
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
}
 
// Post condition:
//  the symbols/samples are sorted by increasing vma.
//  the range of sample_entry inside each symbol entry are valid
//  the samples_by_file_loc member var is correctly setup.
void samples_container_t::
add(opp_samples_files const & samples_files, op_bfd const & abfd,
    string const & symbol_name)
{
	// paranoid checking
	if (nr_counters != static_cast<uint>(-1) &&
	    nr_counters != samples_files.nr_counters) {
		cerr << "Fatal: samples_container_t::do_add(): mismatch"
		     << "between nr_counters and samples_files.nr_counters\n";
		exit(EXIT_FAILURE);
	}

	nr_counters = samples_files.nr_counters;

	string const image_name = abfd.get_filename();
	bool const need_linenr = (flags & (osf_linenr_info | osf_short_linenr_info));
	bool const need_details = (flags & osf_details);

	for (symbol_index_t i = 0 ; i < abfd.syms.size(); ++i) {

		if (!symbol_name.empty() && abfd.syms[i].name() != symbol_name)
			continue;

		u32 start, end;
		string filename;
		uint linenr;
		symbol_entry symb_entry;

		abfd.get_symbol_range(i, start, end);

		bool const found_samples =
			samples_files.accumulate_samples(symb_entry.sample.counter,
				start, end);

		if (found_samples == 0 && !add_zero_samples_symbols)
			continue;

		counter += symb_entry.sample.counter;

		symb_entry.name = abfd.syms[i].name();

		if (need_linenr && abfd.get_linenr(i, start, filename, linenr)) {
			symb_entry.sample.file_loc.filename = filename;
			symb_entry.sample.file_loc.linenr = linenr;
		} else {
			symb_entry.sample.file_loc.linenr = 0;
		}

		symb_entry.sample.file_loc.image_name = image_name;

		bfd_vma base_vma = abfd.syms[i].vma();

		symb_entry.sample.vma = abfd.sym_offset(i, start) + base_vma;

		symb_entry.first = samples->size();

		if (need_details) {
			add_samples(samples_files, abfd, i, start, end,
				    base_vma, image_name);
		}

		symb_entry.last = samples->size();

		symbols->push_back(symb_entry);
	}
}

void samples_container_t::add_samples(opp_samples_files const & samples_files,
				      op_bfd const & abfd,
				      symbol_index_t sym_index,
				      u32 start, u32 end, bfd_vma base_vma,
				      string const & image_name)
{
	bool const need_linenr = (flags & (osf_linenr_info | osf_short_linenr_info));

	for (u32 pos = start; pos < end ; ++pos) {
		string filename;
		sample_entry sample;
		uint linenr;

		if (!samples_files.accumulate_samples(sample.counter, pos))
			continue;

		if (need_linenr && sym_index != size_t(-1) &&
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

samples_container_t::symbol_collection const
samples_container_t::select_symbols(size_t ctr, double threshold,
				    bool until_threshold,
				    bool sort_by_vma) const
{
	symbol_collection v;
	symbol_collection result;

	symbols->get_symbols_by_count(ctr, v);

	u32 const total_count = samples_count(ctr);

	symbol_collection::const_iterator it = v.begin();
	symbol_collection::const_iterator const end = v.end();
	for (; it < end && threshold >= 0; ++it) {
		double const percent =
			do_ratio((*it)->sample.counter[ctr],
			total_count);

		if (until_threshold || percent >= threshold)
			result.push_back((*it));

		if (until_threshold)
			threshold -= percent;
	}

	if (sort_by_vma) {
		sort(result.begin(), result.end(),
			less_sample_entry_by_vma());
	}

	return result;
}


vector<string> const samples_container_t::select_filename(
	size_t ctr, double threshold,
	bool until_threshold) const
{
	vector<string> result;
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

	set<string>::const_iterator it = filename_set.begin();
	set<string>::const_iterator const end = filename_set.end();
	for (; it != end; ++it) {
		counter_array_t counter;

		samples_count(counter, *it);

		filename_by_samples f(*it, do_ratio(counter[ctr], total_count));

		file_by_samples.push_back(f);
	}

	// now sort the file_by_samples entry.
	sort(file_by_samples.begin(), file_by_samples.end());

	vector<filename_by_samples>::const_iterator cit = file_by_samples.begin();
	vector<filename_by_samples>::const_iterator const cend = file_by_samples.end();
	for (; cit != cend && threshold >= 0; ++cit) {
		filename_by_samples const & s = *cit;

		if (until_threshold || s.percent >= threshold)
			result.push_back(s.filename);

		if (until_threshold)
			threshold -=  s.percent;
	}

	return result;
}

// Rest here are delegated to our private implementation.

symbol_entry const * samples_container_t::find_symbol(bfd_vma vma) const
{
	return symbols->find_by_vma(vma);
}

symbol_entry const * samples_container_t::find_symbol(string const & filename,
						     size_t linenr) const
{
	return symbols->find(filename, linenr);
}

symbol_entry const * samples_container_t::find_symbol(string const & name) const {
	return symbols->find(name);
}

sample_entry const * samples_container_t::find_sample(bfd_vma vma) const
{
	return samples->find_by_vma(vma);
}

u32 samples_container_t::samples_count(size_t counter_nr) const
{
	return counter[counter_nr];
}

bool samples_container_t::samples_count(counter_array_t & result,
					string const & filename) const
{
	return samples->accumulate_samples(result, filename,
					   get_nr_counters());
}

bool samples_container_t::samples_count(counter_array_t & result,
				    string const & filename,
				    size_t linenr) const
{
	return samples->accumulate_samples(result,
		filename, linenr, get_nr_counters());
}

sample_entry const & samples_container_t::get_samples(sample_index_t idx) const
{
	return (*samples)[idx];
}

uint samples_container_t::get_nr_counters() const
{
	if (nr_counters != static_cast<uint>(-1))
		return nr_counters;

	cerr << "Fatal: samples_container_t::get_nr_counters() attempt to\n"
	     << "access a samples container w/o any samples files. Please\n"
	     << "report this bug to <oprofile-list@lists.sourceforge.net>\n";
	exit(EXIT_FAILURE);
}

bool add_samples(samples_container_t& samples, string sample_filename,
		 size_t counter_mask, string image_name,
		 vector<string> const& excluded_symbols,
		 string symbol)
{
	opp_samples_files samples_files(sample_filename, counter_mask);

	op_bfd abfd(image_name, excluded_symbols);

	samples_files.check_mtime(image_name);
	samples_files.set_start_offset(abfd.get_start_offset());
	
	samples.add(samples_files, abfd, symbol);

	return abfd.have_debug_info();
}
