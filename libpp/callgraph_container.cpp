/**
 * @file callgraph_container.cpp
 * Container associating symbols and caller/caller symbols
 *
 * @remark Copyright 2004 OProfile authors
 * @remark Read the file COPYING
 *
 * @author Philippe Elie
 * @author John Levon
 */

#include <cstdlib>

#include <map>
#include <set>
#include <algorithm>
#include <iterator>
#include <string>
#include <iostream>
#include <numeric>

#include "cverb.h"
#include "parse_filename.h"
#include "callgraph_container.h"
#include "arrange_profiles.h" 
#include "populate.h"
#include "string_filter.h"
#include "op_bfd.h"
#include "op_sample_file.h"
#include "locate_images.h"

using namespace std;

namespace {

/// this comparator is used to partition cg filename by identical caller
/// allowing to avoid (partially) some redundant bfd open
/// \sa callgrap_container::populate()
struct compare_cg_filename {
	bool operator()(string const & lhs, string const & rhs) const;
};


bool
compare_cg_filename::operator()(string const & lhs, string const & rhs) const
{
	parsed_filename plhs = parse_filename(lhs);
	parsed_filename prhs = parse_filename(rhs);

	return plhs.lib_image < prhs.lib_image;
}

/**
 * We need 2 comparators for callgraph, the arcs are sorted by callee_count,
 * the callees too and the callers by callee_counts in reversed order like:
 *
 *	caller_with_few_callee_samples
 *	caller_with_many_callee_samples
 * function_with_many_samples
 *	callee_with_many_callee_samples
 *	callee_with_few_callee_samples
 */

bool
compare_cg_symbol_by_callee_count(cg_symbol const & lhs, cg_symbol const & rhs)
{
	return lhs.callee_counts[0] < rhs.callee_counts[0];
}

bool
compare_cg_symbol_by_callee_count_reverse(cg_symbol const & lhs,
                                          cg_symbol const & rhs)
{
	return rhs.callee_counts[0] < lhs.callee_counts[0];
}

} // anonymous namespace


arc_recorder::~arc_recorder()
{
	map_t::iterator end = caller_callee.end();
	for (map_t::iterator it = caller_callee.begin(); it != end; ++it) {
		delete it->second;
	}

	end = callee_caller.end();
	for (iterator it = callee_caller.begin(); it != end; ++it) {
		delete it->second;
	}
}


void arc_recorder::fixup_callee_counts()
{
	// FIXME: can be optimized easily
	iterator end = caller_callee.end();
	for (iterator it = caller_callee.begin(); it != end; ++it) {
		pair<iterator, iterator> p_it =
			caller_callee.equal_range(it->first);
		count_array_t counts;
		for (; p_it.first != p_it.second; ++p_it.first) {
			counts += p_it.first->first.sample.counts;
		}
		cg_symbol & symbol = const_cast<cg_symbol &>(it->first);
		symbol.callee_counts = counts;
	}
}


void arc_recorder::add_arc(cg_symbol const & caller, cg_symbol const * callee)
{
	if (callee)
		callee = new cg_symbol(*callee);
	caller_callee.insert(map_t::value_type(caller, callee));
	if (callee) {
		callee_caller.insert(map_t::value_type(*callee,
		       new cg_symbol(caller)));
	}
}


vector<cg_symbol> arc_recorder::get_arc() const
{
	vector<cg_symbol> result;

	iterator const end = caller_callee.end();
	for (iterator it = caller_callee.begin(); it != end; ) {
		result.push_back(it->first);
		it = caller_callee.upper_bound(it->first);
	}

	sort(result.begin(), result.end(),
	     compare_cg_symbol_by_callee_count_reverse);

	return result;
}


vector<cg_symbol> arc_recorder::
get_callee(cg_symbol const & symbol) const
{
	vector<cg_symbol> result;

	pair<iterator, iterator> p_it = callee_caller.equal_range(symbol);
	for (; p_it.first != p_it.second; ++p_it.first) {
		if (p_it.first->second) {
			// the reverse map doesn't contain correct information
			// about callee_counts, retrieve them.
			cg_symbol symbol = *p_it.first->second;
			cg_symbol const * symb = find_caller(symbol);
			if (symb)
				symbol.callee_counts = symb->callee_counts;
			result.push_back(symbol);
		}
	}

	sort(result.begin(), result.end(), compare_cg_symbol_by_callee_count);

	return result;
}


vector<cg_symbol> arc_recorder::get_caller(cg_symbol const & symbol) const
{
	vector<cg_symbol> result;

	pair<iterator, iterator> p_it = caller_callee.equal_range(symbol);
	for (; p_it.first != p_it.second; ++p_it.first) {
		if (p_it.first->second) {
			// the direct map second member doesn't contain correct
			// information about callee_counts, retrieve them.
			cg_symbol symbol = *p_it.first->second;
			cg_symbol const * symb = find_caller(symbol);
			if (symb)
				symbol.callee_counts = symb->callee_counts;
			result.push_back(symbol);
		}
	}

	sort(result.begin(), result.end(),
	     compare_cg_symbol_by_callee_count_reverse);

	return result;
}


cg_symbol const * arc_recorder::find_caller(cg_symbol const & symbol) const
{
	map_t::const_iterator it = caller_callee.find(symbol);
	return it == caller_callee.end() ? 0 : &it->first;
}


void callgraph_container::populate(list<inverted_profile> const & iprofiles,
   extra_images const & extra)
{
	/// Normal (i.e non callgraph) samples container, we record sample
	/// at symbol level, not at vma level.
	profile_container symbols(false, false);

	list<inverted_profile>::const_iterator it;
	list<inverted_profile>::const_iterator const end = iprofiles.end();
	for (it = iprofiles.begin(); it != end; ++it) {
		// populate_caller_image is careful about empty sample filename
		populate_for_image(symbols, *it, string_filter());
	}

	// partition identical lib_image (e.g identical caller) to avoid
	// some redundant bfd_open but it's difficult to avoid more.
	// FIXME: must we try harder to avoid bfd open ?
	typedef multiset<string, compare_cg_filename> cg_fileset;
	cg_fileset fset;

	for (it = iprofiles.begin(); it != end; ++it) {
		for (size_t i = 0; i < it->groups.size(); ++i) {
			list<image_set>::const_iterator lit;
			list<image_set>::const_iterator const lend
				= it->groups[i].end();
			for (lit = it->groups[i].begin(); lit != lend; ++lit) {
				list<profile_sample_files>::const_iterator pit
					= lit->files.begin();
				list<profile_sample_files>::const_iterator pend
					= lit->files.end();
				for (; pit != pend; ++pit) {
					copy(pit->cg_files.begin(),
					   pit->cg_files.end(),
					   inserter(fset, fset.begin()));
				}
			}
		}
	}

	// now iterate over our partition, equivalence class get the same bfd,
	// the caller.
	cg_fileset::const_iterator cit;
	for (cit = fset.begin(); cit != fset.end(); ) {
		parsed_filename caller_file = parse_filename(*cit);
		string const app_name = caller_file.image;

		image_error error;
		string caller_binary = find_image_path(caller_file.lib_image,
		                                       extra, error);
		if (error != image_ok)
			report_image_error(caller_binary, error, false);

		cverb << vdebug << "caller binary name: "
		      << caller_binary  << "\n";

		bool bfd_caller_ok = true;
		op_bfd caller_bfd(caller_binary, string_filter(),
				  bfd_caller_ok);
		if (!bfd_caller_ok)
			report_image_error(caller_binary,
					   image_format_failure, false);

		cg_fileset::const_iterator last;
		for (last = fset.upper_bound(*cit); cit != last; ++cit) {
			parsed_filename callee_file = parse_filename(*cit);

			string callee_binary =
				find_image_path(callee_file.cg_image,
		                                       extra, error);
			if (error != image_ok)
				report_image_error(callee_binary, error, false);

			cverb << vdebug << "cg binary callee name: "
			      << callee_binary << endl;

			bool bfd_callee_ok = true;
			op_bfd callee_bfd(callee_binary, string_filter(),
					  bfd_callee_ok);
			if (!bfd_callee_ok)
				report_image_error(callee_binary,
				   image_format_failure, false);

			profile_t profile;
			// We can't use start_offset support in profile_t, give
			// it a zero offset and we will fix that in add()
			profile.add_sample_file(*cit, 0);
			add(profile, caller_bfd, bfd_caller_ok, callee_bfd,
			    app_name, symbols);
		}
	}

	add_leaf_arc(symbols);

	recorder.fixup_callee_counts();
}


column_flags callgraph_container::output_hint() const
{
	column_flags output_hints = cf_none;

	// FIXME: costly must we access directly recorder.caller_callee map ?
	vector<cg_symbol> arcs = recorder.get_arc();

	vector<cg_symbol>::const_iterator it;
	vector<cg_symbol>::const_iterator const end = arcs.end();
	for (it = arcs.begin(); it != end; ++it)
		output_hints = it->output_hint(output_hints);

	return output_hints;
}


void callgraph_container::
add(profile_t const & profile, op_bfd const & caller, bool bfd_caller_ok,
   op_bfd const & callee, string const & app_name,
   profile_container const & symbols)
{
	string const image_name = caller.get_filename();

	// We must handle start_offset, this offset can be different for the
	// caller and the callee: kernel sample traversing the syscall barrier.
	u32 caller_start_offset = 0;
	if (profile.get_header().is_kernel) {
		// We can't use kernel sample file w/o the binary else we will
		// use it with a zero offset, the code below will abort because
		// we will get incorrect callee sub-range and out of range
		// callee vma. FIXME
		if (!bfd_caller_ok)
			// We already warned.
			return;
		caller_start_offset = caller.get_start_offset();
	}

	u32 callee_offset = 0;
	if (profile.get_header().cg_to_is_kernel)
		callee_offset = callee.get_start_offset();

	cverb << vdebug << hex
	      << "bfd_caller_start_offset: " << caller_start_offset << endl
	      << "bfd_callee_start_offset: " << callee_offset << dec << endl;

	for (symbol_index_t i = 0; i < caller.syms.size(); ++i) {
		u32 start, end;
		caller.get_symbol_range(i, start, end);

		profile_t::iterator_pair p_it = profile.samples_range(
			odb_key_t(start - caller_start_offset) << 32,
			odb_key_t(end - caller_start_offset) << 32);

		cg_symbol symb_caller;
		symb_caller.sample.counts[0] = 0;
		symb_caller.size = end - start;
		symb_caller.name = symbol_names.create(caller.syms[i].name());
		symb_caller.sample.file_loc.linenr = 0;
		symb_caller.image_name = image_names.create(image_name);
		symb_caller.app_name = image_names.create(app_name);
		symb_caller.sample.vma = caller.sym_offset(i, start) +
			caller.syms[i].vma();

		symbol_entry const * self = symbols.find(symb_caller);
		if (self)
			symb_caller.self_counts = self->sample.counts;

		// Our odb_key_t contain (from_eip << 32 | to_eip), the range
		// of key we selected above can contain different callee but
		// due to the ordering this callee are consecutive so we
		// iterate over the range, then iterate over the sub-range
		// for each distinct callee symbol.

		while (p_it.first != p_it.second) {
			// find the callee.
			profile_t::const_iterator it = p_it.first;
			bfd_vma callee_vma = (it.vma() & 0xffffffff) +
				callee_offset;

			cverb << vdebug
			      << "offset caller: " << hex
			      << (p_it.first.vma() >> 32) << " callee: "
			      << callee_vma << dec << endl;

			op_bfd_symbol symb(callee_vma, 0, string());
			vector<op_bfd_symbol>::const_iterator bfd_symb_callee =
				upper_bound(callee.syms.begin(),
					callee.syms.end(), symb);
			if (bfd_symb_callee == callee.syms.end())
				cverb << vdebug << "reaching end of symbol\n";
			// ugly but upper_bound() work in this way.
			if (bfd_symb_callee != callee.syms.begin())
				--bfd_symb_callee;
			if (bfd_symb_callee == callee.syms.end()) {
				// FIXME: not clear if we need to abort,
				// recover or an exception, for now I need a
				// a core dump
				cerr << "Unable to retrieve callee symbol\n";
				abort();
			}

			u32 upper_bound = bfd_symb_callee->size() +
				bfd_symb_callee->filepos();
			cverb << vdebug
			      << "upper bound: " << hex << upper_bound
			      << dec << endl;

			// Process all arc from this caller to this callee
			u32 caller_callee_count = 0;
			for (; it != p_it.second && 
			 (it.vma() & 0xffffffff) + callee_offset < upper_bound;
			     ++it) {
				caller_callee_count += it.count();
			}
			// FIXME: very fragile, any innacuracy in caller offset
			// can lead to an abort!
			if (it == p_it.first) {
				// This is impossible, we need a core dump else
				// we enter in an infinite loop
				cerr << "failure to advance iterator\n";
				abort();
			}

			symb_caller.sample.counts[0] = caller_callee_count;

			cverb << vdebug
			      << caller.syms[i].name() << " "
			      << bfd_symb_callee->name()   << " "
			      << caller_callee_count << endl;

			cg_symbol symb_callee;
			symb_callee.sample.counts[0] = 0;
			symb_callee.size = bfd_symb_callee->size();
			symb_callee.name =
				symbol_names.create(bfd_symb_callee->name());
			symb_callee.sample.file_loc.linenr = 0;
			symb_callee.image_name =
				image_names.create(callee.get_filename());
			symb_callee.app_name = image_names.create(app_name);
			symb_callee.sample.vma = bfd_symb_callee->vma();

			symbol_entry const * self = symbols.find(symb_callee);
			if (self)
				symb_callee.self_counts = self->sample.counts;

			recorder.add_arc(symb_caller, &symb_callee);

			// next callee symbol
			p_it.first = it;
		}
	}
}


void callgraph_container::add_leaf_arc(profile_container const & symbols)
{
	symbol_container::symbols_t::iterator it;
	symbol_container::symbols_t::iterator const end = symbols.end_symbol();
	for (it = symbols.begin_symbol(); it != end; ++it) {
		cg_symbol symbol(*it);
		symbol.self_counts = it->sample.counts;
		recorder.add_arc(symbol, 0);
	}
}


vector<cg_symbol> callgraph_container::get_arc() const
{
	return recorder.get_arc();
}


vector<cg_symbol> callgraph_container::get_callee(cg_symbol const & s) const
{
	return recorder.get_callee(s);
}


vector<cg_symbol> callgraph_container::get_caller(cg_symbol const & s) const
{
	return recorder.get_caller(s);
}
