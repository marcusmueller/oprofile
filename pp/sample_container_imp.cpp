/**
 * @file sample_container_imp.cpp
 * Internal container for samples
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author Philippe Elie <phil_el@wanadoo.fr>
 * @author John Levon <moz@compsoc.man.ac.uk>
 */

#include <set>
#include <numeric>
#include <algorithm>
#include <vector>
 
#include "opp_symbol.h"
#include "symbol_functors.h"
#include "sample_container_imp.h"

using std::string;
using std::vector;
using std::accumulate;
using std::pair;
 
namespace {

inline counter_array_t & add_counts(counter_array_t & arr, sample_entry const * s)
{
	return arr += s->counter;
}

} // namespace anon

sample_entry const & sample_container_imp_t::operator[](sample_index_t index) const
{
	return samples[index];
}

sample_index_t sample_container_imp_t::size() const
{
	return samples.size();
}

void sample_container_imp_t::push_back(sample_entry const & sample)
{
	samples.push_back(sample);
}

bool sample_container_imp_t::accumulate_samples(counter_array_t & counter,
						string const & filename,
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

	for (size_t i = 0 ; i < max_counters ; ++i) {
		if (counter[i] != 0)
			return true;
	}

	return false;
}

sample_entry const * sample_container_imp_t::find_by_vma(bfd_vma vma) const
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
	string const & filename, size_t linenr, uint max_counters) const
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
