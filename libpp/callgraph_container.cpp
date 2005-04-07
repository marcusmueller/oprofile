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

#include "callgraph_container.h"
#include "cverb.h"
#include "parse_filename.h"
#include "profile_container.h"
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

/*
 * We need 2 comparators for callgraph to get the desired output:
 *
 *	caller_with_few_samples
 *	caller_with_many_samples
 * function_with_many_samples
 *	callee_with_many_samples
 *	callee_with_few_samples
 */

bool
compare_arc_count(symbol_entry const & lhs, symbol_entry const & rhs)
{
	return lhs.sample.counts[0] < rhs.sample.counts[0];
}


bool
compare_arc_count_reverse(symbol_entry const & lhs, symbol_entry const & rhs)
{
	return rhs.sample.counts[0] < lhs.sample.counts[0];
}


} // anonymous namespace


void arc_recorder::
add(symbol_entry const & caller, symbol_entry const * callee,
    count_array_t const & arc_count)
{
	cg_data & data = sym_map[caller];

	// If we have a callee, add it to the caller's list, then
	// add the caller to the callee's list.
	if (callee) {
		data.callees[*callee] += arc_count;

		cg_data & callee_data = sym_map[*callee];

		callee_data.callers[caller] += arc_count;
	}
}


void arc_recorder::process_children(cg_symbol & sym, double threshold)
{
	// generate the synthetic self entry for the symbol
	symbol_entry self = sym;

	self.name = symbol_names.create(symbol_names.demangle(self.name)
	                                + " [self]");

	sym.total_callee_count += self.sample.counts;
	sym.callees.push_back(self);

	sort(sym.callers.begin(), sym.callers.end(), compare_arc_count);
	sort(sym.callees.begin(), sym.callees.end(), compare_arc_count_reverse);

	// FIXME: this relies on sort always being sample count

	cg_symbol::children::iterator cit = sym.callers.begin();
	cg_symbol::children::iterator cend = sym.callers.end();

	while (cit != cend && op_ratio(cit->sample.counts[0],
	       sym.total_caller_count[0]) < threshold)
		++cit;

	if (cit != cend)
		sym.callers.erase(sym.callers.begin(), cit);

	cit = sym.callees.begin();
	cend = sym.callees.end();

	while (cit != cend && op_ratio(cit->sample.counts[0],
	       sym.total_callee_count[0]) >= threshold)
		++cit;

	if (cit != cend)
		sym.callees.erase(cit, sym.callees.end());
}


void arc_recorder::
process(count_array_t total, double threshold,
        string_filter const & sym_filter)
{
	map_t::const_iterator it;
	map_t::const_iterator end = sym_map.end();

	for (it = sym_map.begin(); it != end; ++it) {
		cg_symbol sym((*it).first);
		cg_data const & data = (*it).second;

		// threshold out the main symbol if needed
		if (op_ratio(sym.sample.counts[0], total[0]) < threshold)
			continue;

		// FIXME: slow?
		if (!sym_filter.match(symbol_names.demangle(sym.name)))
			continue;

		cg_data::children::const_iterator cit;
		cg_data::children::const_iterator cend = data.callers.end();

		for (cit = data.callers.begin(); cit != cend; ++cit) {
			symbol_entry csym = cit->first;
			csym.sample.counts = cit->second;
			sym.callers.push_back(csym);
			sym.total_caller_count += cit->second;
		}

		cend = data.callees.end();

		for (cit = data.callees.begin(); cit != cend; ++cit) {
			symbol_entry csym = cit->first;
			csym.sample.counts = cit->second;
			sym.callees.push_back(csym);
			sym.total_callee_count += cit->second;
		}

		process_children(sym, threshold);

		cg_syms.push_back(sym);
	}
}


cg_collection arc_recorder::get_symbols() const
{
	return cg_syms;
}


void callgraph_container::populate(string const & archive_path, 
   list<inverted_profile> const & iprofiles,
   extra_images const & extra, bool debug_info, double threshold,
   bool merge_lib, string_filter const & sym_filter)
{
	// non callgraph samples container, we record sample at symbol level
	// not at vma level.
	profile_container pc(debug_info, false);

	list<inverted_profile>::const_iterator it;
	list<inverted_profile>::const_iterator const end = iprofiles.end();
	for (it = iprofiles.begin(); it != end; ++it) {
		// populate_caller_image take care about empty sample filename
		populate_for_image(archive_path, pc, *it, sym_filter, 0);
	}

	add_symbols(pc);

	total_count = pc.samples_count();

	for (it = iprofiles.begin(); it != end; ++it) {
		for (size_t i = 0; i < it->groups.size(); ++i) {
			populate(archive_path, it->groups[i], it->image, extra,
				i, pc, debug_info, merge_lib);
		}
	}

	recorder.process(total_count, threshold / 100.0, sym_filter);
}


void callgraph_container::populate(string const & archive_path,
	list<image_set> const & lset,
	string const & app_image, extra_images const & extra, size_t pclass,
	profile_container const & pc, bool debug_info, bool merge_lib)
{
	list<image_set>::const_iterator lit;
	list<image_set>::const_iterator const lend = lset.end();
	for (lit = lset.begin(); lit != lend; ++lit) {
		list<profile_sample_files>::const_iterator pit;
		list<profile_sample_files>::const_iterator pend
			= lit->files.end();
		for (pit = lit->files.begin(); pit != pend; ++pit) {
			populate(archive_path, pit->cg_files, app_image,
				 extra, pclass, pc, debug_info, merge_lib);
		}
	}
}


void callgraph_container::populate(string const & archive_path,
	list<string> const & cg_files,
	string const & app_image, extra_images const & extra, size_t pclass,
	profile_container const & pc, bool debug_info, bool merge_lib)
{
	list<string>::const_iterator it;
	list<string>::const_iterator const end = cg_files.end();
	for (it = cg_files.begin(); it != end; ++it) {
		cverb << vdebug << "samples file : " << *it << endl;

		parsed_filename caller_file = parse_filename(*it);
		string const app_name = caller_file.image;

		image_error error;
		string caller_binary =
			find_image_path(archive_path, caller_file.lib_image,
					extra, error);

		if (error != image_ok)
			report_image_error(archive_path + caller_file.lib_image,
					   error, false);

		cverb << vdebug << "caller binary name: "
		      << caller_binary  << "\n";

		bool caller_bfd_ok = true;
		op_bfd caller_bfd(archive_path, caller_binary,
				  string_filter(), caller_bfd_ok);
		if (!caller_bfd_ok)
			report_image_error(caller_binary,
			                   image_format_failure, false);

		parsed_filename callee_file = parse_filename(*it);

		string callee_binary =
			find_image_path(archive_path, callee_file.cg_image,
			                extra, error);
		if (error != image_ok)
			report_image_error(callee_file.cg_image, error, false);

		cverb << vdebug << "cg binary callee name: "
		      << callee_binary << endl;

		bool callee_bfd_ok = true;
		op_bfd callee_bfd(archive_path, callee_binary,
				  string_filter(), callee_bfd_ok);
		if (!callee_bfd_ok)
			report_image_error(callee_binary,
		                           image_format_failure, false);

		profile_t profile;
		// We can't use start_offset support in profile_t, give
		// it a zero offset and we will fix that in add()
		profile.add_sample_file(*it, 0);
		add(profile, caller_bfd, caller_bfd_ok, callee_bfd,
		    merge_lib ? app_image : app_name, pc,
		    debug_info, pclass);
	}
}


void callgraph_container::
add(profile_t const & profile, op_bfd const & caller_bfd, bool caller_bfd_ok,
   op_bfd const & callee_bfd, string const & app_name,
   profile_container const & pc, bool debug_info, size_t pclass)
{
	string const image_name = caller_bfd.get_filename();

	// We must handle start_offset, this offset can be different for the
	// caller and the callee: kernel sample traversing the syscall barrier.
	u32 caller_start_offset = 0;
	if (profile.get_header().is_kernel) {
		// We can't use kernel sample file w/o the binary else we will
		// use it with a zero offset, the code below will abort because
		// we will get incorrect callee sub-range and out of range
		// callee vma. FIXME
		if (!caller_bfd_ok) {
			// We already warned.
			return;
		}
		caller_start_offset = caller_bfd.get_start_offset();
	}

	u32 callee_offset = 0;
	if (profile.get_header().cg_to_is_kernel)
		callee_offset = callee_bfd.get_start_offset();

	cverb << vdebug << hex
	      << "caller_bfd_start_offset: " << caller_start_offset << endl
	      << "callee_bfd_start_offset: " << callee_offset << dec << endl;

	image_name_id image_id = image_names.create(image_name);
	image_name_id callee_image_id = image_names.create(callee_bfd.get_filename());
	image_name_id app_id = image_names.create(app_name);

	for (symbol_index_t i = 0; i < caller_bfd.syms.size(); ++i) {
		unsigned long start, end;
		caller_bfd.get_symbol_range(i, start, end);

		profile_t::iterator_pair p_it = profile.samples_range(
			odb_key_t(start - caller_start_offset) << 32,
			odb_key_t(end - caller_start_offset) << 32);

		symbol_entry caller;

		caller.size = end - start;
		caller.name = symbol_names.create(caller_bfd.syms[i].name());
		caller.image_name = image_id;
		caller.app_name = app_id;
		caller.sample.vma = caller_bfd.sym_offset(i, start) +
			caller_bfd.syms[i].vma();

		symbol_entry const * self = pc.find(caller);
		if (self)
			caller.sample.counts = self->sample.counts;

		if (debug_info) {
			string filename;
			if (caller_bfd.get_linenr(i, start, filename,
			    caller.sample.file_loc.linenr)) {
				caller.sample.file_loc.filename =
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
				upper_bound(callee_bfd.syms.begin(),
					callee_bfd.syms.end(), symb);

			if (bfd_symb_callee == callee_bfd.syms.end())
				cverb << vdebug << "reaching end of symbol\n";
			// ugly but upper_bound() work in this way.
			if (bfd_symb_callee != callee_bfd.syms.begin())
				--bfd_symb_callee;
			if (bfd_symb_callee == callee_bfd.syms.end()) {
				// FIXME: not clear if we need to abort,
				// recover or an exception, for now I need a
				// a core dump
				cerr << "Unable to retrieve callee symbol\n";
				abort();
			}

			symbol_entry callee;

			callee.size = bfd_symb_callee->size();
			callee.name =
				symbol_names.create(bfd_symb_callee->name());
			callee.image_name = callee_image_id;
			callee.app_name = app_id;
			callee.sample.vma = bfd_symb_callee->vma();

			self = pc.find(callee);
			if (self)
				callee.sample.counts = self->sample.counts;

			u32 upper_bound = bfd_symb_callee->size() +
				bfd_symb_callee->filepos() - callee_offset;
			cverb << vdebug
			      << "upper bound: " << hex << upper_bound
			      << dec << endl;

			data_t::const_iterator dcur = it;

			// Process all arc from this caller to this callee

			count_array_t arc_count;

			for (; it != dend &&
			     (it->first & 0xffffffff) < upper_bound;
			     ++it) {
				cverb << (vdebug & vlevel1) << hex
				      << "offset: " << (it->first & 0xffffffff)
				      << dec << endl;
				arc_count[pclass] += it->second;
			}
			// FIXME: very fragile, any inaccuracy in caller offset
			// can lead to an abort!
			if (it == dcur) {
				// This is impossible, we need a core dump else
				// we enter in an infinite loop
				cerr << "failure to advance iterator\n";
				abort();
			}

			cverb << vdebug
			      << caller_bfd.syms[i].name() << " "
			      << bfd_symb_callee->name() << " "
			      << arc_count[pclass] << endl;

			if (debug_info) {
				symbol_index_t index =
					distance(callee_bfd.syms.begin(),
						 bfd_symb_callee);
				unsigned long start, end;
				callee_bfd.get_symbol_range(index, start, end);

				string filename;
				if (callee_bfd.get_linenr(index, start, filename,
					callee.sample.file_loc.linenr)) {
					callee.sample.file_loc.filename =
						debug_names.create(filename);
				}
			}

			recorder.add(caller, &callee, arc_count);

			// next callee symbol
			dit = it;
		}
	}
}


void callgraph_container::add_symbols(profile_container const & pc)
{
	symbol_container::symbols_t::iterator it;
	symbol_container::symbols_t::iterator const end = pc.end_symbol();

	for (it = pc.begin_symbol(); it != end; ++it)
		recorder.add(*it, 0, count_array_t());
}


column_flags callgraph_container::output_hint() const
{
	column_flags output_hints = cf_none;

	// FIXME: costly: must we access directly recorder map ?
	cg_collection syms = recorder.get_symbols();

	cg_collection::const_iterator it;
	cg_collection::const_iterator const end = syms.end();
	for (it = syms.begin(); it != end; ++it)
		output_hints = it->output_hint(output_hints);

	return output_hints;
}


count_array_t callgraph_container::samples_count() const
{
	return total_count;
}


cg_collection callgraph_container::get_symbols() const
{
	return recorder.get_symbols();
}
