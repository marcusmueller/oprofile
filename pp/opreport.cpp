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
#include <numeric>

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

static size_t nr_groups;

/// storage for a merged file summary
struct summary {
	count_array_t counts;
	string image;
	string lib_image;

	bool operator<(summary const & rhs) const {
		return options::reverse_sort
		    ? counts[0] < rhs.counts[0] : rhs.counts[0] < counts[0];
	}
};


/**
 * Summary of an application: a set of image summaries
 * for one application, i.e. an application image and all
 * dependent images such as libraries.
 */
struct app_summary {
	count_array_t counts;
	string image;
	string lib_image;
	vector<summary> files;

	// return the number of samples added
	size_t add_samples(split_sample_filename const &, size_t count_group);

	bool operator<(app_summary const & rhs) const {
		return options::reverse_sort 
		    ? counts[0] < rhs.counts[0] : rhs.counts[0] < counts[0];
	}

	/// return true if the deps should not be output
	bool should_hide_deps() const;
};


size_t app_summary::
add_samples(split_sample_filename const & split, size_t count_group)
{
	// FIXME: linear search inefficient ?
	vector<summary>::iterator it = files.begin();
	vector<summary>::iterator const end = files.end();
	for (; it != end; ++it) {
		if (it->image == split.image && 
		    it->lib_image == split.lib_image)
			break;
	}

	size_t nr_samples = profile_t::sample_count(split.sample_filename);

	if (it == end) {
		summary summary;
		summary.image = split.image;
		summary.lib_image = split.lib_image;
		summary.counts[count_group] = nr_samples;
		counts[count_group] += nr_samples;
		files.push_back(summary);
	} else {
		// assert it->counts[count_group] == 0
		it->counts[count_group] = nr_samples;
		counts[count_group] += nr_samples;
	}

	return nr_samples;
}


bool app_summary::should_hide_deps() const
{
	if (files.size() == 0)
		return true;

	// can this happen ?
	if (counts.zero())
		return true;

	string image_name = image;
	if (options::merge_by.lib && !lib_image.empty())
		image_name = lib_image;

	summary const & first = files[0];
	string const & dep_image = first.lib_image.empty()
		? first.image : first.lib_image;

	bool hidedep = options::exclude_dependent;
	hidedep |= options::merge_by.lib;

	// If we're only going to show the main image again,
	// and it's the same image (can be different when
	// it's a library and there's no samples for the main
	// application image), then don't show it
	hidedep |= files.size() == 1 && dep_image == image;
	return hidedep;
}


/// All image summaries to be output are contained here.
struct summary_container {
	summary_container(vector<partition_files> const & sample_files);
	/// all app summaries
	vector<app_summary> apps;
	/// total count of samples for all summaries
	count_array_t total_counts;
};


summary_container::
summary_container(vector<partition_files> const & sample_files)
{
	// second member is the partition file index i.e. the events/counts
	// identifier
	typedef pair<split_sample_filename const *, size_t> value_t;
	typedef multimap<string, value_t> map_t;
	map_t sample_filenames;

	for (size_t i = 0; i < sample_files.size(); ++i) {
		partition_files const & partition = sample_files[i];
		for (size_t j = 0; j < partition.nr_set(); ++j) {
			partition_files::filename_set const & files =
				partition.set(j);
			partition_files::filename_set::const_iterator it;
			for (it = files.begin(); it != files.end(); ++it) {
				value_t value(&*it, i);
				map_t::value_type val(it->image, value);
				sample_filenames.insert(val);
			}
		}
	}

	map_t::const_iterator it = sample_filenames.begin();
	for (; it != sample_filenames.end(); ) {
		split_sample_filename const * cur = it->second.first;

		app_summary app;
		app.image = cur->image;
		app.lib_image = cur->lib_image;

		pair<map_t::const_iterator, map_t::const_iterator> p_it =
			sample_filenames.equal_range(it->first);
		for (it = p_it.first; it != p_it.second; ++it) {
			size_t nr_samples = app.add_samples(
				*it->second.first, it->second.second);
			total_counts[it->second.second] += nr_samples;
		}

		stable_sort(app.files.begin(), app.files.end());

		apps.push_back(app);
	}

	stable_sort(apps.begin(), apps.end());
}


void output_header()
{
	if (!options::show_header)
		return;

	bool first_output = true;

	for (vector<partition_files const *>::size_type i = 0;
	     i < sample_file_partition.size(); ++i) {

		if (!sample_file_partition[i].nr_set())
			continue;

		partition_files::filename_set const & file_set =
			sample_file_partition[i].set(0);
		opd_header const & header =
			read_header(file_set.begin()->sample_filename);

		if (first_output) {
			output_cpu_info(cout, header);
			first_output = false;
		}

		cout << header;
	}
}


string get_filename(string const & filename)
{
	return options::long_filenames ? filename : basename(filename);
}


/// Output a count and a percentage
void output_count(double total_count, size_t count)
{
	cout << setw(9) << count << " ";
	double ratio = op_ratio(count, total_count);
	cout << format_double(ratio * 100, percent_int_width,
			      percent_fract_width) << " ";
}


void
output_deps(summary_container const & summaries,
	    app_summary const & app)
{
	for (size_t j = 0 ; j < app.files.size(); ++j) {
		cout << "\t";
		summary const & file = app.files[j];
		for (size_t i = 0; i < nr_groups; ++i) {
			double tot_count = options::global_percent
				? summaries.total_counts[i] : app.counts[i];

			output_count(tot_count, file.counts[i]);
		}

		if (file.lib_image.empty())
			cout << " " << get_filename(file.image);
		else
			cout << " " << get_filename(file.lib_image);
		cout << '\n';
	}
}


/**
 * Display all the given summary information
 */
void output_summaries(summary_container const & summaries)
{
	for (size_t i = 0; i < summaries.apps.size(); ++i) {
		app_summary const & app = summaries.apps[i];

		if ((app.counts[0] * 100.0) / summaries.total_counts[0]
		    < options::threshold) {
			continue;
		}

		for (size_t j = 0; j < nr_groups; ++j) {
			output_count(summaries.total_counts[j],
			             app.counts[j]);
		}

		cout << get_filename(app.image) << '\n';

		if (!app.should_hide_deps())
			output_deps(summaries, app);
	}
}


/**
 * Load the given samples container with the profile data from
 * the files container, merging as appropriate.
 */
void populate_profiles(partition_files const & files,
                       profile_container & samples, size_t count_group)
{
	image_set images = sort_by_image(files, options::extra_found_images);

	image_set::const_iterator it;
	for (it = images.begin(); it != images.end(); ) {
		pair<image_set::const_iterator, image_set::const_iterator>
			p_it = images.equal_range(it->first);

		op_bfd abfd(it->first, options::symbol_filter);

		// we can optimize by cumulating samples to this binary in
		// a profile_t only if we merge by lib since for non merging
		// case application name change and must be recorded
		if (options::merge_by.lib) {
			string app_name = p_it.first->first;

			profile_t profile;

			for (;  it != p_it.second; ++it) {
				profile.add_sample_file(
					it->second.sample_filename,
					abfd.get_start_offset());
			}

			check_mtime(abfd.get_filename(), profile.get_header());
	
			samples.add(profile, abfd, app_name, count_group);
		} else {
			for (; it != p_it.second; ++it) {
				string app_name = it->second.image;

				profile_t profile;
				profile.add_sample_file(
					it->second.sample_filename,
					abfd.get_start_offset());

				check_mtime(abfd.get_filename(),
					    profile.get_header());
	
				samples.add(profile, abfd,
				            app_name, count_group);
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

	out.set_nr_groups(nr_groups);

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

	output_header();

	nr_groups = sample_file_partition.size();

	if (!options::symbols) {
		summary_container summaries(sample_file_partition);
		output_summaries(summaries);
		return 0;
	}

	profile_container samples(false,
		options::debug_info, options::details);

	for (size_t i = 0; i < sample_file_partition.size(); ++i)
		populate_profiles(sample_file_partition[i], samples, i);

	output_symbols(samples);
	return 0;
}

}  // anonymous namespace


int main(int argc, char const * argv[])
{
	cout.tie(0);
	return run_pp_tool(argc, argv, opreport);
}
