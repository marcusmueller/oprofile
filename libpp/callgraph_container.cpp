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

bool operator==(cg_symbol const & lhs, cg_symbol const & rhs)
{
	less_symbol cmp_symb;
	return !cmp_symb(lhs, rhs) && !cmp_symb(rhs, lhs);
}


bool compare_by_callee_vma(pair<odb_key_t, odb_value_t> const & lhs,
			   pair<odb_key_t, odb_value_t> const & rhs)
{
	return (lhs.first & 0xffffffff) < (rhs.first & 0xffffffff);
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


vector<arc_recorder::map_t::iterator>
arc_recorder::select_leaf(double threshold, count_array_t & totals)
{
	vector<map_t::iterator> result;
	map_t::iterator end = caller_callee.end();
	for (map_t::iterator it = caller_callee.begin(); it != end; ++it) {
		if (it->second)
			continue;
		double percent = op_ratio(it->first.self_counts[0], totals[0]);
		if (percent < threshold) {
			totals -= it->first.self_counts;
			result.push_back(it);
		}
	}
	return result;
}


void arc_recorder::
fixup_callee_counts(double threshold, count_array_t & totals)
{
	double percent = threshold / 100.0;

	// loop iteration number is bounded by biggest callgraph depth.
	while (true) {
		vector<map_t::iterator> leafs = select_leaf(percent, totals);
		if (leafs.empty())
			break;

		for (size_t i = 0; i < leafs.size(); ++i) {
			remove(leafs[i]->first);

			caller_callee.erase(leafs[i]);
		}
	}

	iterator end = caller_callee.end();
	for (iterator it = caller_callee.begin(); it != end; ) {
		pair<iterator, iterator> p_it =
			caller_callee.equal_range(it->first);
		count_array_t counts;
		for (iterator tit = p_it.first; tit != p_it.second; ++tit) {
			counts += tit->first.callee_counts;
		}

		for (iterator tit = p_it.first; tit != p_it.second; ++tit) {
			cg_symbol & symb = const_cast<cg_symbol &>(tit->first);
			symb.callee_counts = counts;
		}
		it = p_it.second;
	}
}


void arc_recorder::remove(cg_symbol const & caller)
{
	map_t::iterator it = caller_callee.begin();
	map_t::iterator end = caller_callee.end();
	for (; it != end; ++it) {
		if (it->second && caller == *it->second) {
			delete it->second;
			it->second = 0;
			pair<map_t::iterator, map_t::iterator>
				p_it = callee_caller.equal_range(caller);
			callee_caller.erase(p_it.first, p_it.second);
		}
	}
}


arc_recorder::iterator arc_recorder::
find_arc(cg_symbol const & caller, cg_symbol const & callee)
{
	pair<iterator, iterator> p_it = caller_callee.equal_range(caller);
	for ( ; p_it.first != p_it.second; ++p_it.first) {
		if (p_it.first->second && *p_it.first->second == callee)
			break;
	}

	return p_it.first == p_it.second ? caller_callee.end() : p_it.first;
}


void arc_recorder::
add_arc(cg_symbol const & caller, cg_symbol const * cg_callee)
{
	if (cg_callee) {
		iterator it = find_arc(caller, *cg_callee);
		if (it != caller_callee.end()) {
			count_array_t & self = const_cast<count_array_t &>(
				it->first.self_counts);
			self += caller.self_counts;
			count_array_t & counts = const_cast<count_array_t &>(
				it->first.callee_counts);
			counts += caller.callee_counts;
			count_array_t & callee = const_cast<count_array_t &>(
				it->second->self_counts);
			callee += cg_callee->self_counts;
			return;
		}
		cg_callee = new cg_symbol(*cg_callee);
	}

	caller_callee.insert(map_t::value_type(caller, cg_callee));
	if (cg_callee) {
		callee_caller.insert(map_t::value_type(*cg_callee,
		       new cg_symbol(caller)));
	}
}


cg_collection arc_recorder::get_arc() const
{
	cg_collection result;

	iterator const end = caller_callee.end();
	for (iterator it = caller_callee.begin(); it != end; ) {
		result.push_back(it->first);
		it = caller_callee.upper_bound(it->first);
	}

	sort(result.begin(), result.end(),
	     compare_cg_symbol_by_callee_count_reverse);

	return result;
}


cg_collection arc_recorder::get_caller(cg_symbol const & symbol) const
{
	cg_collection result;

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


cg_collection arc_recorder::get_callee(cg_symbol const & symbol) const
{
	cg_collection result;

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
   extra_images const & extra, bool debug_info, double threshold,
   bool merge_lib)
{
	/// non callgraph samples container, we record sample at symbol level
	/// not at vma level.
	profile_container symbols(debug_info, false);

	list<inverted_profile>::const_iterator it;
	list<inverted_profile>::const_iterator const end = iprofiles.end();
	for (it = iprofiles.begin(); it != end; ++it) {
		// populate_caller_image take care about empty sample filename
		populate_for_image(symbols, *it, string_filter());
	}

	for (it = iprofiles.begin(); it != end; ++it) {
		for (size_t i = 0; i < it->groups.size(); ++i) {
			populate(it->groups[i], it->image, extra,
				i, symbols, debug_info, merge_lib);
		}
	}

	add_leaf_arc(symbols);

	recorder.fixup_callee_counts(threshold, total_count);
}


void callgraph_container::populate(list<image_set> const & lset,
	string const & app_image, extra_images const & extra, size_t pclass,
	profile_container const & symbols, bool debug_info, bool merge_lib)
{
	list<image_set>::const_iterator lit;
	list<image_set>::const_iterator const lend = lset.end();
	for (lit = lset.begin(); lit != lend; ++lit) {
		list<profile_sample_files>::const_iterator pit;
		list<profile_sample_files>::const_iterator pend
			= lit->files.end();
		for (pit = lit->files.begin(); pit != pend; ++pit) {
			populate(pit->cg_files, app_image, extra, pclass,
				 symbols, debug_info, merge_lib);
		}
	}
}


void callgraph_container::populate(list<string> const & cg_files,
	string const & app_image, extra_images const & extra, size_t pclass,
	profile_container const & symbols, bool debug_info, bool merge_lib)
{
	list<string>::const_iterator it;
	list<string>::const_iterator const end = cg_files.end();
	for (it = cg_files.begin(); it != end; ++it) {
		parsed_filename caller_file = parse_filename(*it);
		string const app_name = caller_file.image;

		image_error error;
		string caller_binary = find_image_path(caller_file.lib_image,
		                                       extra, error);
		if (error != image_ok)
			report_image_error(caller_file.lib_image, error, false);

		cverb << vdebug << "caller binary name: "
		      << caller_binary  << "\n";

		bool bfd_caller_ok = true;
		op_bfd caller_bfd(caller_binary, string_filter(),
				  bfd_caller_ok);
		if (!bfd_caller_ok)
			report_image_error(caller_binary,
					   image_format_failure, false);

		parsed_filename callee_file = parse_filename(*it);

		string callee_binary =
			find_image_path(callee_file.cg_image,
					extra, error);
		if (error != image_ok)
			report_image_error(callee_file.cg_image, error, false);

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
		profile.add_sample_file(*it, 0);
		add(profile, caller_bfd, bfd_caller_ok, callee_bfd,
		    merge_lib ? app_image : app_name, symbols,
		    debug_info, pclass);
	}
}


void callgraph_container::
add(profile_t const & profile, op_bfd const & caller, bool bfd_caller_ok,
   op_bfd const & callee, string const & app_name,
   profile_container const & symbols, bool debug_info, size_t pclass)
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

		symb_caller.size = end - start;
		symb_caller.name = symbol_names.create(caller.syms[i].name());
		symb_caller.image_name = image_names.create(image_name);
		symb_caller.app_name = image_names.create(app_name);
		symb_caller.sample.vma = caller.sym_offset(i, start) +
			caller.syms[i].vma();

		symbol_entry const * self = symbols.find(symb_caller);
		if (self)
			symb_caller.self_counts = self->sample.counts;

		if (debug_info) {
			string filename;
			if (caller.get_linenr(i, start, filename,
			    symb_caller.sample.file_loc.linenr)) {
				symb_caller.sample.file_loc.filename =
					debug_names.create(filename);
			}
		}

		// Our odb_key_t contain (from_eip << 32 | to_eip), the range
		// of key we selected above contain one caller but different
		// callee and due to the ordering callee offsets are not
		// consecutive so we must sort them first.

		typedef vector<pair<odb_key_t, odb_value_t> > data_t;
		data_t data;

		for (; p_it.first != p_it.second; ++p_it.first) {
			data.push_back(make_pair(p_it.first.vma(),
				p_it.first.count()));
		}

		sort(data.begin(), data.end(), compare_by_callee_vma);
		
		data_t::const_iterator dit;
		data_t::const_iterator dend = data.end();
		for (dit = data.begin(); dit != dend; ) {
			// find the callee.
			data_t::const_iterator it = dit;
			bfd_vma callee_vma = (it->first & 0xffffffff) +
				callee_offset;

			cverb << vdebug
			      << "offset caller: " << hex
			      << (it->first >> 32) << " callee: "
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
				bfd_symb_callee->filepos() - callee_offset;
			cverb << vdebug
			      << "upper bound: " << hex << upper_bound
			      << dec << endl;

			data_t::const_iterator dcur = it;
			// Process all arc from this caller to this callee
			u32 caller_callee_count = 0;
			for (; it != dend &&
			     (it->first & 0xffffffff) < upper_bound;
			     ++it) {
				cverb << (vdebug&vlevel1) << hex
				      << "offset: " << (it->first & 0xffffffff)
				      << dec << endl;
				caller_callee_count += it->second;
			}
			// FIXME: very fragile, any innacuracy in caller offset
			// can lead to an abort!
			if (it == dcur) {
				// This is impossible, we need a core dump else
				// we enter in an infinite loop
				cerr << "failure to advance iterator\n";
				abort();
			}

			symb_caller.callee_counts[pclass] = caller_callee_count;

			cverb << vdebug
			      << caller.syms[i].name() << " "
			      << bfd_symb_callee->name() << " "
			      << caller_callee_count << endl;

			cg_symbol symb_callee;

			symb_callee.size = bfd_symb_callee->size();
			symb_callee.name =
				symbol_names.create(bfd_symb_callee->name());
			symb_callee.image_name =
				image_names.create(callee.get_filename());
			symb_callee.app_name = image_names.create(app_name);
			symb_callee.sample.vma = bfd_symb_callee->vma();

			symbol_entry const * self = symbols.find(symb_callee);
			if (self)
				symb_callee.self_counts = self->sample.counts;

			if (debug_info) {
				symbol_index_t index =
					distance(callee.syms.begin(),
						 bfd_symb_callee);
				u32 start, end;
				callee.get_symbol_range(index, start, end);

				string filename;
				if (callee.get_linenr(index, start, filename,
					symb_callee.sample.file_loc.linenr)) {
					symb_callee.sample.file_loc.filename =
						debug_names.create(filename);
				}
			}

			recorder.add_arc(symb_caller, &symb_callee);

			// next callee symbol
			dit = it;
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

	total_count = symbols.samples_count();
}


column_flags callgraph_container::output_hint() const
{
	column_flags output_hints = cf_none;

	// FIXME: costly must we access directly recorder.caller_callee map ?
	cg_collection arcs = recorder.get_arc();

	cg_collection::const_iterator it;
	cg_collection::const_iterator const end = arcs.end();
	for (it = arcs.begin(); it != end; ++it)
		output_hints = it->output_hint(output_hints);

	return output_hints;
}


count_array_t callgraph_container::samples_count() const
{
	return total_count;
}


cg_collection callgraph_container::get_arc() const
{
	return recorder.get_arc();
}


cg_collection callgraph_container::get_callee(cg_symbol const & s) const
{
	return recorder.get_callee(s);
}


cg_collection callgraph_container::get_caller(cg_symbol const & s) const
{
	return recorder.get_caller(s);
}
