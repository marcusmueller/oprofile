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

size_t pp_nr_counters;

namespace {


/**
 * Used to sort parellel vector of data upon one of the vector. Sorting
 * is done by first building a bijection map[i] = i then sort this array.
 * Later all array access are done using the sorted indexer. After sorting
 * the bijection ensure than:
 *  for i in [1, size) 
 *    compare(data[index_mapper[i]], data[index_mapper[i-1]]) == true
 */
typedef vector<size_t> index_mapper_t;


/// storage for a merged file summary
struct summary {
	summary(string const & image_name_, string const & lib_image_)
		: count(0), image_name(image_name_), lib_image(lib_image_) {}
	size_t count;
	string image_name;
	string lib_image;

	bool operator<(summary const & rhs) const {
		return options::reverse_sort
			? count < rhs.count : rhs.count < count;
	}
};


/**
 * Summary of a group. A group is a set of image summaries
 * for one application, i.e. an application image and all
 * dependent images such as libraries.
 */
struct group_summary {
	group_summary(string const & image_name_, string const & lib_image_) 
		: count(0), image_name(image_name_), lib_image(lib_image_) {}
	group_summary() : count(0) {}

	size_t count;
	string image_name;
	string lib_image;
	vector<summary> files;
	index_mapper_t index_mapper;

	summary const & file(size_t index) const {
		return files[index_mapper[index]];
	}

	bool operator<(group_summary const & rhs) const {
		return options::reverse_sort 
			? count < rhs.count : rhs.count < count;
	}

	/// return true if the deps should not be output
	bool should_hide_deps() const;
};


/// all group_summary belonging to a counter
struct event_group_summary {
	event_group_summary() : total_count(0.0) {}

	group_summary const & group(size_t index) const {
		return groups[index_mapper[index]];
	}

	vector<group_summary> groups;
	/// total count of samples for this counter
	double total_count;

	index_mapper_t index_mapper;
};


void output_header()
{
	if (!options::show_header)
		return;

	bool first_output = true;
	for (vector<partition_files const *>::size_type i = 0;
	     i < sample_file_partition.size(); ++i) {
		if (sample_file_partition[i].nr_set()) {
			partition_files::filename_set const & file_set =
				sample_file_partition[i].set(0);
			opd_header header =
				read_header(file_set.begin()->sample_filename);

			if (first_output) {
				output_cpu_info(cout, header);
				first_output = false;
			}
			cout << header;
		}
	}
}


string get_filename(string const & filename)
{
	return options::long_filenames ? filename : basename(filename);
}


/// Output a count and a percentage
void output_counter(double total_count, size_t count)
{
	cout << setw(9) << count << " ";
	double ratio = op_ratio(count, total_count);
	cout << format_double(ratio * 100, percent_int_width,
			      percent_fract_width) << " ";
}


bool group_summary::should_hide_deps() const
{
	if (files.size() == 0)
		return true;

	if (count == 0)
		return true;

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


void
output_deps(vector<event_group_summary> const & summaries,
	    vector<group_summary> const & event_group)
{
	bool should_hide_deps = true;
	for (size_t i = 0 ; i < event_group.size(); ++i) {
		if (!event_group[i].should_hide_deps())
			should_hide_deps = false;
	}

	if (should_hide_deps)
		return;

	for (size_t j = 0 ; j < event_group[0].files.size(); ++j) {
		cout << "\t";
		for (size_t i = 0; i < event_group.size(); ++i) {
			group_summary const & group = event_group[i];
			summary const & file = group.file(j);

			double tot_count = options::global_percent
				? summaries[i].total_count : group.count;

			output_counter(tot_count, file.count);
		}

		summary const & file = event_group[0].file(j);
		if (file.lib_image.empty())
			cout << " " << get_filename(file.image_name);
		else
			cout << " " << get_filename(file.lib_image);
		cout << '\n';
	}
}


/**
 * Generate summaries for each of the profiles in this
 * partition set.
 */
group_summary summarize(partition_files::filename_set const & files)
{
	group_summary group(files.begin()->image, files.begin()->lib_image);

	partition_files::filename_set::const_iterator it;
	for (it = files.begin(); it != files.end(); ++it) {
		summary dep_summary(it->image, it->lib_image);

		dep_summary.count =
			profile_t::sample_count(it->sample_filename);

		group.count += dep_summary.count;
		group.files.push_back(dep_summary);
	}

	return group;
}


/// comparator used to sort a vector<summary> through a mapping index
class build_summary_index {
public:
	build_summary_index(vector<summary> const & files_)
		: files(files_) {}
	bool operator()(size_t lhs, size_t rhs) const {
		return files[lhs] < files[rhs];
	}
private:
	vector<summary> const & files;
};


/**
 * create the index to sorted group_summary, summaries[0] is used to create
 * the sort order. Data themself remains at fixed address, we just fill an
 * index mapper.
 */
index_mapper_t create_index_mapper(vector<group_summary> const & summaries)
{
	index_mapper_t result(summaries[0].files.size());

	for (size_t i = 0; i < result.size(); ++i) {
		result[i] = i;
	}

	sort(result.begin(), result.end(), 
	     build_summary_index(summaries[0].files));

	return result;
}


/**
 * build each group_summary::vector<summary> in synched way
 * populating a vector<group_summary> w/o any gap between each entry
 * i.e post condition are:
 * for i in [1 result.size)
 *       result[i].groups.size() == result[i-1].groups.size()
 * for i in [1 result.size)
 *       result[i].groups.image_name == result[i-1].groups.image_name
 *
 * ascii art for 3 events:
 *
 * sample_file_partition array:
 * -- AA -------  BC --- CD --- EF -----------
 * -------- BA --------- CD ------------------
 * -------------- BC ----------------- FG ----
 *
 * result:
 * -- AA -- BA -- BC --- CD --- EF --- FG ----
 * -- AA -- BA -- BC --- CD --- EF --- FG ----
 * -- AA -- BA -- BC --- CD --- EF --- FG ----
 *
 * the added items contain zero samples.
 *
 * @internal a multimap<name, <data, vector entry index>> is used to partition
 * the input data (coming from all counter). Then we iterate over equivalence
 * class, each class contain item for an unique name and associated info
 * contains the counter nr for this data.
 */
vector<group_summary>
populate_summaries(vector<event_group_summary> const & unfilled, size_t index)
{
	typedef pair<summary const *, int> value_t;
	typedef multimap<string, value_t> map_t;
	map_t map;

	// Partition the files set at index.
	for (size_t i = 0; i < unfilled.size(); ++i) {
		vector<summary> const & groups = unfilled[i].group(index).files;
		for (size_t j = 0; j < groups.size(); ++j) {
			string image = groups[j].image_name;
			if (!groups[j].lib_image.empty())
				image = groups[j].lib_image;
			value_t value(&groups[j], i);
			map.insert(map_t::value_type(image, value));
		}
	}

	vector<group_summary> result(unfilled.size());

	for (size_t i = 0; i < unfilled.size(); ++i) {
		group_summary const & group = unfilled[i].group(index);
		result[i].count = group.count;
		result[i].image_name = group.image_name;
		result[i].lib_image = group.lib_image;
	}

	// for each equivalance class.
	for (map_t::const_iterator it = map.begin(); it != map.end(); ) {
		// Populate entries summary with empty summary, it->second
		// is a representant of the current equivalence class.
		for (size_t i = 0 ; i < unfilled.size() ; ++i) {
			string image_name = it->second.first->image_name;
			string lib_image  = it->second.first->lib_image;
			summary summary(image_name, lib_image);

			result[i].files.push_back(summary);
		}

		// Overwrite empty summary create above by the existing one.
		pair<map_t::const_iterator, map_t::const_iterator> p_it =
			p_it = map.equal_range(it->first);
		for (; it != p_it.second; ++it) {
			result[it->second.second].files.back() =
				*it->second.first;
		}
	}

	index_mapper_t index_mapper = create_index_mapper(result);
	for (size_t i = 0; i < result.size(); ++i)
		result[i].index_mapper = index_mapper;

	return result;
}


/**
 * Display all the given summary information
 */
void output_summaries(vector<event_group_summary> const & summaries)
{
	for (size_t i = 0 ; i < summaries[0].groups.size(); ++i) {
		group_summary const & group = summaries[0].group(i);

		if ((group.count * 100.0) / summaries[0].total_count <
		    options::threshold) {
			continue;
		}

		for (size_t j = 0; j < summaries.size(); ++j) {
			group_summary const & group = summaries[j].group(i);
			output_counter(summaries[j].total_count, group.count);
		}

		string image = group.image_name;
		if (options::merge_by.lib && !group.lib_image.empty())
			image = group.lib_image;

		cout << get_filename(image) << '\n';

		vector<group_summary> filled_summaries =
			populate_summaries(summaries, i);

		output_deps(summaries, filled_summaries);
	}
}


/// comparator used to sort a vector<group_summary> through a mapping index
class build_group_summary_index {
public:
	build_group_summary_index(vector<group_summary> const & group_)
		: group(group_) {}
	bool operator()(size_t lhs, size_t rhs) const {
		return group[lhs] < group[rhs];
	}
private:
	vector<group_summary> const & group;
};


/**
 * create the index to sorted group_summary, summaries[0] is used to create
 * the sort order. Data remains at fixed address, we just fill an index mapper
 */
index_mapper_t
create_index_mapper(vector<event_group_summary> const & summaries)
{
	index_mapper_t result(summaries[0].groups.size());

	// no stl-ish way to generate [0 - size()) range
	for (size_t i = 0; i < result.size(); ++i) {
		result[i] = i;
	}

	sort(result.begin(), result.end(),
	     build_group_summary_index(summaries[0].groups));

	return result;
}


/**
 * build each event_group_summary::vector<group_summary> in synched way
 * populating a vector<event_group_summary> w/o any gap between each entry
 * i.e post condition are:
 * for i in [1 result.size)
 *       result[i].groups.size() == result[i-1].groups.size()
 * for i in [1 result.size)
 *       result[i].groups.image_name == result[i-1].groups.image_name
 *
 * ascii art for 3 events:
 *
 * sample_file_partition array:
 * -- AA -------  BC --- CD --- EF -----------
 * -------- BA --------- CD ------------------
 * -------------- BC ----------------- FG ----
 *
 * result:
 * -- AA -- BA -- BC --- CD --- EF --- FG ----
 * -- AA -- BA -- BC --- CD --- EF --- FG ----
 * -- AA -- BA -- BC --- CD --- EF --- FG ----
 *
 * the added items contain zero samples.
 *
 * @internal a multimap<name, <data, vector entry index>> is used to partition
 * the input data (coming from all counter). Then we iterate over equivalence
 * class, each class contain item for an unique name and associated info
 * contains the counter nr for this data.
 */
vector<event_group_summary> populate_group_summaries()
{
	typedef pair<group_summary, int> value_t;
	typedef multimap<string, value_t> map_t;
	map_t map;

	size_t const nr_events = sample_file_partition.size();

	vector<event_group_summary> result(nr_events);

	// partition the files set.
	for (size_t event = 0; event < nr_events; ++event) {
		partition_files const & files = sample_file_partition[event];

		for (size_t j = 0; j < files.nr_set(); ++j) {
			partition_files::filename_set const & file_set
				= files.set(j);

			group_summary group(summarize(file_set));
			result[event].total_count += group.count;

			string image = group.image_name;
			if (options::merge_by.lib && !group.lib_image.empty())
				image = group.lib_image;

			value_t value(group, event);
			map.insert(map_t::value_type(image, value));
		}
	}

	// for each equivalence class
	for (map_t::const_iterator it = map.begin(); it != map.end(); ) {
		// populate entries with empty group_summary, it->second is a
		// representant of the current equivalence class
		for (size_t i = 0 ; i < nr_events; ++i) {
			string image_name = it->second.first.image_name;
			string lib_image  = it->second.first.lib_image;
			group_summary group(image_name, lib_image);

			result[i].groups.push_back(group);
		}

		// overwrite the empty group_summary create above by existing
		// group_summary
		pair<map_t::const_iterator, map_t::const_iterator> p_it =
			map.equal_range(it->first);
		for ( ; it != p_it.second; ++it) {
			result[it->second.second].groups.back() =
				it->second.first;
		}
	}

	index_mapper_t index_mapper = create_index_mapper(result);
	for (size_t i = 0; i < nr_events; ++i) {
		result[i].index_mapper = index_mapper;
	}

	return result;
}


/**
 * Load the given samples container with the profile data from
 * the files container, merging as appropriate.
 */
void populate_profiles(partition_files const & files, profile_container & samples, size_t counter)
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
	
			samples.add(profile, abfd, app_name, counter);
		} else {
			for (; it != p_it.second; ++it) {
				string app_name = it->second.image;

				profile_t profile;
				profile.add_sample_file(
					it->second.sample_filename,
					abfd.get_start_offset());

				check_mtime(abfd.get_filename(),
					    profile.get_header());
	
				samples.add(profile, abfd, app_name, counter);
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

	pp_nr_counters = sample_file_partition.size();

	output_header();

	if (!options::symbols) {
		vector<event_group_summary> filled_summaries =
			populate_group_summaries();

		output_summaries(filled_summaries);
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
