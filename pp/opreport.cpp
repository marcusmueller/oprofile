/**
 * @file opreport.cpp
 * Implement opreport utility
 *
 * @remark Copyright 2003 OProfile authors
 * @remark Read the file COPYING
 *
 * @author John Levon
 * @author Philippe Elie
 */

#include <iostream>
#include <iomanip>
#include <vector>
#include <algorithm>
#include <sstream>

#include "op_header.h"
#include "string_manip.h"
#include "file_manip.h"
#include "opreport_options.h"
#include "profile.h"
#include "partition_files.h"
#include "profile_container.h"
#include "symbol_sort.h"
#include "format_output.h"

using namespace std;

namespace {


void output_header(partition_files const & files)
{
	if (files.nr_set()) {
		partition_files::filename_set const & file_set = files.set(0);
		opd_header header =
			read_header(file_set.begin()->sample_filename);
		cout << header;
	}
}


string get_filename(string const & filename)
{
	return options::long_filenames ? filename : basename(filename);
}


/// Output a count and a percentage
void output_counter(double total_count, size_t count)
{
	// FIXME: left or right, op_time was using left
	// left io manipulator doesn't exist in 2.95
//	cout.setf(ios::left, ios::adjustfield);
	cout << setw(9) << count << " ";
	double ratio = op_ratio(count, total_count);
	cout << format_double(ratio * 100, percent_int_width,
			      percent_fract_width) << " ";
}


/// storage for a merged file summary
struct summary {
	summary() : count(0) {}
	size_t count;
	string image_name;
	string lib_image;

	struct compare {
		bool operator()(summary const & lhs,
		                summary const & rhs) const {
			return options::reverse_sort 
				? lhs.count < rhs.count
				: rhs.count < lhs.count;
		}
	};
};


/**
 * Summary of a group. A group is a set of image summaries
 * for one application, i.e. an application image and all
 * dependent images such as libraries.
 */
struct group_summary {
	group_summary() : count(0) {}
	size_t count;
	string image_name;
	string lib_image;
	vector<summary> files;

	struct compare {
		bool operator()(group_summary const & lhs,
		                group_summary const & rhs) const {
			return options::reverse_sort 
				? lhs.count < rhs.count
				: rhs.count < lhs.count;
		}
	};

	/// return true if the deps should not be output
	bool should_hide_deps() const;
	/// output this summary
	void output(double total) const;
	/// show an image summary for the dependent images
	void output_deps(double total) const;
};


void group_summary::output_deps(double total) const
{
	if (should_hide_deps())
		return;

	vector<summary>::const_iterator cit = files.begin();
	vector<summary>::const_iterator const end = files.end();

	for (; cit != end; ++cit) {
		cout << "\t";
		double tot_count = options::global_percent 
			? total : count;
		output_counter(tot_count, cit->count);

		if (cit->lib_image.empty())
			cout << " " << get_filename(cit->image_name);
		else
			cout << " " << get_filename(cit->lib_image);
		cout << endl;
	}
}


bool group_summary::should_hide_deps() const
{
	string image = image_name;
	if (options::merge_by.lib && !lib_image.empty())
		image = lib_image;

	summary const & first = files[0];
	string const & dep_image = first.lib_image.empty()
		? first.image_name : first.lib_image;

	bool hidedep = options::exclude_dependent;
	hidedep |= options::merge_by.lib;

	// If we're only going to show the main image again,
	// and it's the same image (can be different when
	// it's a library and there's no samples for the main
	// application image), then don't show it
	hidedep |= files.size() == 1 && dep_image == image;
	return hidedep;
}


void group_summary::output(double total) const
{
	output_counter(total, count);

	string image = image_name;
	if (options::merge_by.lib && !lib_image.empty())
		image = lib_image;

	cout << get_filename(image) << endl;

	output_deps(total);
}


/**
 * Generate summaries for each of the profiles in this
 * partition set.
 */
group_summary summarize(partition_files::filename_set const & files)
{
	group_summary group;

	group.image_name = files.begin()->image;
	group.lib_image = files.begin()->lib_image;

	partition_files::filename_set::const_iterator it;
	for (it = files.begin(); it != files.end(); ++it) {
		profile_t samples;
		// THere's another FIXME on this elsewhere. Note
		// that the use of profile_t for this means we
		// pass in an offset of "0" instead of the real
		// abfd offset. This is perhaps a bit dubious...
		samples.add_sample_file(it->sample_filename, 0);

		summary dep_summary;
		dep_summary.image_name = it->image;
		dep_summary.lib_image  = it->lib_image;
		dep_summary.count = samples.accumulate_samples(0, ~0);

		group.count += dep_summary.count;
		group.files.push_back(dep_summary);
	}

	sort(group.files.begin(), group.files.end(),
	     summary::compare());

	return group;
}


/**
 * Create summary data for each of the given files
 */
double populate_summaries(partition_files const & files,
                          vector<group_summary> & summaries)
{
	double total_count = 0;

	for (size_t i = 0 ; i < files.nr_set(); ++i) {
		partition_files::filename_set const & file_set = files.set(i);

		summaries.push_back(summarize(file_set));

		total_count += summaries.back().count;
	}

	sort(summaries.begin(), summaries.end(),
	     group_summary::compare());

	return total_count;
}


/**
 * Display all the given summary information
 */
void
output_summaries(vector<group_summary> const & summaries, double total_count)
{
	vector<group_summary>::const_iterator it = summaries.begin();
	vector<group_summary>::const_iterator end = summaries.end();

	for (; it != end; ++it) {
		if ((it->count * 100.0) / total_count < options::threshold)
			continue;

		it->output(total_count);
	}
}


/**
 * Load the given samples container with the profile data from
 * the files container, merging as appropriate.
 */
void populate_profiles(partition_files const & files, profile_container & samples)
{
	image_set images = sort_by_image(files, options::extra_found_images);

	image_set::const_iterator it;
	for (it = images.begin(); it != images.end(); ) {
		pair<image_set::const_iterator, image_set::const_iterator>
			p_it = images.equal_range(it->first);

		if (p_it.first == p_it.second)
			continue;

		op_bfd abfd(p_it.first->first, options::symbol_filter);

		// we can optimize by cumulating samples to this binary in
		// a profile_t only if we merge by lib since for non merging
		// case application name change and must be recorded
		if (options::merge_by.lib) {
			string app_name = p_it.first->first;

			profile_t profile;

			for (it = p_it.first;  it != p_it.second; ++it) {
				profile.add_sample_file(
					it->second.sample_filename,
					abfd.get_start_offset());
			}

			check_mtime(abfd.get_filename(), profile.get_header());
	
			samples.add(profile, abfd, app_name);
		} else {
			for (it = p_it.first;  it != p_it.second; ++it) {
				string app_name = it->second.image;

				profile_t profile;
				profile.add_sample_file(
					it->second.sample_filename,
					abfd.get_start_offset());

				check_mtime(abfd.get_filename(),
					    profile.get_header());
	
				samples.add(profile, abfd, app_name);
			}
		}
	}
}


format_flags const get_format_flags(column_flags const & cf)
{
	format_flags flags(ff_none);
	flags = format_flags(flags | ff_vma | ff_nr_samples);
	flags = format_flags(flags | ff_percent | ff_symb_name);

	if (cf & cf_multiple_apps)
		flags = format_flags(flags | ff_app_name);
	if (options::debug_info)
		flags = format_flags(flags | ff_linenr_info);

	if (options::accumulated) {
		flags = format_flags(flags | ff_nr_samples_cumulated);
		flags = format_flags(flags | ff_percent_cumulated);
	}

	if (cf & cf_image_name)
		flags = format_flags(flags | ff_image_name);

	return flags;
}


void output_symbols(profile_container const & samples)
{
	profile_container::symbol_choice choice;
	choice.threshold = options::threshold;
	symbol_collection symbols = samples.select_symbols(choice);
	options::sort_by.sort(symbols, options::reverse_sort,
	                      options::long_filenames);

	format_output::formatter out(samples);

	if (options::details)
		out.show_details();
	if (options::long_filenames)
		out.show_long_filenames();
	if (!options::show_header)
		out.hide_header();
	if (choice.hints & cf_64bit_vma)
		out.vma_format_64bit();

	out.add_format(get_format_flags(choice.hints));
	out.output(cout, symbols);
}


int opreport(vector<string> const & non_options)
{
	handle_options(non_options);

	output_header(*sample_file_partition);

	if (!options::symbols) {
		vector<group_summary> summaries;
		double const total =
			populate_summaries(*sample_file_partition, summaries);
		output_summaries(summaries, total);
		return 0;
	}

	profile_container samples(false,
		options::debug_info, options::details);
	populate_profiles(*sample_file_partition, samples);
	output_symbols(samples);
	return 0;
}

}  // anonymous namespace


int main(int argc, char const * argv[])
{
	return run_pp_tool(argc, argv, opreport);
}
