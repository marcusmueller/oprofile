/**
 * @file op_time.cpp
 * Give system summaries
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author Philippe Elie
 * @author John Levon
 */

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

#include <string>
#include <list>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <algorithm>
#include <map>
#include <vector>

#include "op_exception.h"
#include "version.h"
#include "op_libiberty.h"
#include "op_mangling.h"
#include "op_time_options.h"

#include "profile_container.h"
#include "profile.h"
#include "format_output.h"

#include "file_manip.h"
#include "string_manip.h"

using namespace std;

/* TODO: if we have a quick read samples files format we can handle a great
 * part of complexity here by using profile_container_t to handle straight
 * op_time. Just create an artificial symbol that cover the whole samples
 * files with the name of the application this allow to remove image_name
 * and sorted_map_t class and all related  stuff and to use output_symbol to
 * make the report
 */

/// image_name - class to store name for a samples file
struct image_name
{
	image_name(string const & samplefile_name);

	/// total number of samples for this samples file, this is a place
	/// holder to avoid separate data struct which associate image_name
	/// with a sample count.
	counter_array_t count;
	/// complete name of the samples file (without leading dir)
	string samplefile_name;
	/// application name which belong this sample file, == name if the
	/// file belong to an application or if the user have obtained
	/// this file without --separate-samples
	string app_name;
	/// image name which belong this sample file, empty if the file belong
	/// to an application or if the user have obtained this file without
	/// --separate-samples
	string lib_name;
};


/// comparator for sorted_map_t
struct sort_by_counter_t {
	sort_by_counter_t(size_t index_) : index(index_) {}

	bool operator()(counter_array_t const & lhs,
			counter_array_t const & rhs) const {
		return lhs[index] < rhs[index];
	}

	size_t index;
};

typedef multimap<string, image_name> map_t;
typedef pair<map_t::iterator, map_t::iterator> pair_it_t;
typedef multimap<counter_array_t, map_t::const_iterator, sort_by_counter_t> sorted_map_t;


/**
 * image_name - ctor from a sample file name
 */
image_name::image_name(string const & samplefile_name_)
	:
	samplefile_name(samplefile_name_)
{
	app_name = extract_app_name(samplefile_name, lib_name);
}

/**
 * filter_image_name - check if image name match one of the filter given
 * on comand line.
 */
static bool filter_image_name(string const & image_name)
{
	if (options::filename_filters.empty())
		return true;

	string temp(image_name);
	replace(temp.begin(), temp.end(), '}', '/');

	return find(options::filename_filters.begin(),
		    options::filename_filters.end(),
		    temp) != options::filename_filters.end();
}

/**
 * sort_file_list_by_name - insert in result a file list sorted by app name
 * @param result where to put result
 * @param file_list a list of string which must be insert in result
 *
 * for each filename try to extract if possible the app name and
 * use it as a key to insert the filename in result
 *  filename are on the form
 *   }usr}sbin}syslogd}}}lib}libc-2.1.2.so (shared lib
 *     which belong to app /usr/sbin/syslogd)
 *  or
 *   }bin}bash (application)
 *
 * The sort order used is likely to get in result file name as:
 * app_name_A, all samples filenames from shared libs which belongs
 *  to app_name_A,
 * app_name_B, etc.
 *
 * This sort is used later to find samples filename which belong
 * to the same application. Note than the sort is correct even if
 * the input file list contains only app_name (i.e. samples file
 * obtained w/o --separate-samples) in this case shared libs are
 * treated as application
 */
static void sort_file_list_by_name(map_t & result,
				   list<string> const & file_list)
{
	list<string>::const_iterator it;
	for (it = file_list.begin() ; it != file_list.end() ; ++it) {
		// the file list is created with /base_dir/* pattern but with
		// the lazilly samples creation it is perfectly correct for one
		// samples file belonging to counter 0 exist and not exist for
		// counter 1, so we must filter them.
		int i;
		for (i = 0 ; i < OP_MAX_COUNTERS ; ++i) {
			if ((options::counter & (1 << i)) != 0) {
				string filename =
					sample_filename(options::samples_dir,
							*it, i);
				if (op_file_readable(filename)) {
					break;
				}
			}
		}

		if (i < OP_MAX_COUNTERS) {
			image_name image(*it);
			if (filter_image_name(image.app_name)) {
				map_t::value_type value(image.app_name, image);
				result.insert(value);
			}
		}
	}
}

/**
 * out_filename - display a filename and it associated ratio of samples
 */
static void out_filename(string const & app_name,
			 counter_array_t const & app_count,
			 counter_array_t const & count,
			 double total_count[OP_MAX_COUNTERS])
{
	for (size_t i = 0 ; i < OP_MAX_COUNTERS; ++i) {
		if ((options::counter & (1 << i)) != 0) {
			// feel to rewrite with cout and its formated output
#if 1
			printf("%-9d", count[i]);
			double ratio = op_ratio(count[i], total_count[i]);

			if (ratio < 10.00 / 100.0)
				printf(" ");
			printf(" %2.4f", ratio * 100);

			ratio = op_ratio(count[i], app_count[i]);

			if (ratio < 10.00 / 100.0)
				printf(" ");
			printf(" %2.4f", ratio * 100);
#else
			cout << count[i] << " ";

			if (total_count[i] > 1) {
				double ratio = count[i] / total_count[i];
				cout << ratio * 100 << "%";
			} else {
				cout << "0%";
			}

			if (app_count[i] != 0) {
				double ratio = count[i] / double(app_count[i]);
				cout << " (" << ratio * 100 << "%)";
			}
#endif
			cout << " ";
		}
	}

	cout << demangle_filename(app_name);
	cout << endl;
}

/**
 * output_image_samples_count - output the samples ratio for some images
 * @param first the start iterator
 * @param last the end iterator
 * @param total_count the total samples count
 *
 * iterator parameters are intended to be of type map_t::iterator or
 * map_t::reverse_iterator
 */
template <class Iterator>
static void output_image_samples_count(Iterator first, Iterator last,
				       counter_array_t const & app_count,
				       double total_count[OP_MAX_COUNTERS])
{
	for (Iterator it = first ; it != last ; ++it) {
		string name = it->second->second.lib_name;
		if (name.length() == 0)
			name = it->second->second.samplefile_name;

		cout << "  ";
		out_filename(name, app_count, it->first, total_count);
	}
}

/**
 * build_sorted_map_by_count - insert element in a sorted_map_t
 * @param first the start iterator
 * @param last the end iterator
 * @param total_count the total samples count
 *
 */
static void build_sorted_map_by_count(sorted_map_t & sorted_map, pair_it_t p_it)
{
	map_t::iterator it;
	for (it = p_it.first ; it != p_it.second ; ++it) {
		sorted_map_t::value_type value(it->second.count, it);

		sorted_map.insert(value);
	}
}

/**
 * output_files_count - open each samples file to cumulate samples count
 * and display a sorted list of filename and samples ratio
 * @param files the file list to treat.
 *
 * print the whole cumulated count for all selected filename (currently
 * the whole base_dir directory), does not show any details about symbols
 */
static void output_files_count(map_t & files)
{
	double total_count[OP_MAX_COUNTERS] = { 0.0 };

	/* 1st pass: accumulate for each image_name the samples count and
	 * update the total_count of samples */

	map_t::const_iterator it;
	for (it = files.begin(); it != files.end() ; ) {
		pair_it_t p_it = files.equal_range(it->first);

		// the range [p_it.first, p_it.second) belongs to application
		// it.first->first
		for ( ; p_it.first != p_it.second ; ++p_it.first) {
			for (int i = 0 ; i < OP_MAX_COUNTERS ; ++i) {
				string filename = sample_filename(
					options::samples_dir,
					p_it.first->second.samplefile_name, i);
				if (!op_file_readable(filename))
					continue;

				counter_profile_t samples(filename);

				u32 count = samples.count(0, ~0);

				p_it.first->second.count[i] = count;
				total_count[i] += count;
			}
		}

		it = p_it.second;
	}

	bool empty = true;
	for (int i = 0 ; i < OP_MAX_COUNTERS; ++i) {
		if (total_count[i] != 0.0) {
			empty = false;
		}
	}

	// already checked by caller but we must recheck it now, we can have
	// samples files in samples dir but all of them are for a different
	// counter than the selected once
	if (empty) {
		cerr << "no samples files found for the slected counter(s) (try running opcontrol --dump ?)\n";
		return;	// Would exit(EXIT_FAILURE); perhaps
	}

	/* 2nd pass: insert the count of samples for each application and
	 * associate with an iterator to a image name e.g. insert one
	 * item for each application */

	sort_by_counter_t compare(options::sort_by_counter);
	sorted_map_t sorted_map(compare);
	for (it = files.begin(); it != files.end() ; ) {
		pair_it_t p_it = files.equal_range(it->first);
		counter_array_t count;
		for ( ; p_it.first != p_it.second ; ++p_it.first) {
			count += p_it.first->second.count;
		}

		sorted_map.insert(sorted_map_t::value_type(count, it));

		it = p_it.second;
	}

	/* 3rd pass: we can output the result, during the output we optionally
	 * build the set of image_name which belongs to one application and
	 * display these results too */

	/* this if else are only different by the type of iterator used, we
	 * can't easily templatize the block on iterator type because we use
	 * in one case begin()/end() and in another rbegin()/rend(), it's
	 * worthwhile to try to factorize these two piece of code */
	if (options::reverse_sort) {
		sorted_map_t::reverse_iterator s_it = sorted_map.rbegin();
		for ( ; s_it != sorted_map.rend(); ++s_it) {
			map_t::const_iterator it = s_it->second;
			counter_array_t temp;

			out_filename(it->first, temp, s_it->first,
				     total_count);

			if (options::show_shared_libs) {
				pair_it_t p_it = files.equal_range(it->first);
				sort_by_counter_t compare(options::sort_by_counter);
				sorted_map_t temp_map(compare);

				build_sorted_map_by_count(temp_map, p_it);

				output_image_samples_count(temp_map.rbegin(),
							   temp_map.rend(),
							   s_it->first,
							   total_count);
			}
		}
	} else {
		sorted_map_t::iterator s_it = sorted_map.begin();
		for ( ; s_it != sorted_map.end() ; ++s_it) {
			map_t::const_iterator it = s_it->second;

			counter_array_t temp;
			out_filename(it->first, temp, s_it->first,
				     total_count);

			if (options::show_shared_libs) {
				pair_it_t p_it = files.equal_range(it->first);
				sort_by_counter_t compare(options::sort_by_counter);
				sorted_map_t temp_map(compare);

				build_sorted_map_by_count(temp_map, p_it);

				output_image_samples_count(temp_map.begin(),
							   temp_map.end(),
							   s_it->first,
							   total_count);
			}
		}
	}
}


/**
 * check_image_name - check than image_name belonging to samples_filename
 * exist. If not it try to retrieve it through the alternate_filename
 * location.
 */
static string check_image_name(string const & image_name,
			       string const & samples_filename)
{
	if (op_file_readable(image_name))
		return image_name;

	if (errno == EACCES) {
		static bool first_warn = true;
		if (first_warn) {
			cerr << "you have not read access to some binary image"
			     << ", all\nof this file(s) will be ignored in"
			     << " statistics\n";
			first_warn = false;
		}
		cerr << "access denied for : " << image_name << endl;

		return string(); 
	}

	typedef alt_filename_t::const_iterator it_t;
	pair<it_t, it_t> p_it =
		options::alternate_filename.equal_range(basename(image_name));

	if (p_it.first == p_it.second) {

		static bool first_warn = true;
		if (first_warn) {
			cerr << "I can't locate some binary image file, all\n"
			     << "of this file(s) will be ignored in statistics"
			     << endl
			     << "Have you provided the right -p/-P option ?"
			     << endl;
			first_warn = false;
		}

		cerr << "warning: can't locate image file for samples files : "
		     << samples_filename << endl;

		return string();
	}

	if (distance(p_it.first, p_it.second) != 1) {
		cerr << "the image name for samples files : "
		     << samples_filename << " is ambiguous\n"
		     << "so this file file will be ignored" << endl;
		return string();
	}

	return p_it.first->second + '/' + p_it.first->first;
}

/**
 * output_symbols_count - open each samples file to cumulate samples count
 * and display a sorted list of symbols and samples ratio
 * @param files the file list to treat.
 * @param counter on which counter to work
 *
 * print the whole cumulated count for all symbols in selected filename
 * (currently the whole base_dir directory)
 */
static void output_symbols_count(map_t& files, int counter)
{
	profile_container_t samples(false, options::output_format_flags, counter);

	map_t::iterator it_f;
	for (it_f = files.begin() ; it_f != files.end() ; ++it_f) {

		string filename = it_f->second.samplefile_name;
		string samples_filename = string(options::samples_dir) + "/" + filename;

		string lib_name;
		string image_name = extract_app_name(filename, lib_name);
		string app_name = demangle_filename(image_name);

		// if the samples file belongs to a shared lib we need to get
		// the right binary name
		if (lib_name.length())
			image_name = lib_name;

		image_name = demangle_filename(image_name);

		// if the image files does not exist try to retrieve it
		image_name = check_image_name(image_name, samples_filename);

		// check_image_name have already warned the user if something
		// feel bad.
		if (op_file_readable(image_name)) {
			add_samples(samples, samples_filename, counter,
				    image_name, app_name,
				    options::exclude_symbols);
		}
	}

	// select the symbols
	vector<symbol_entry const *> symbols =
		samples.select_symbols(options::sort_by_counter, 0.0, false);

	// check if the profiling session separate samples for shared libs
	// if yes change the output format to add owning app name unless
	// user specify explicitely through command line the output format.
	if (options::output_format_specified == false) {
		for (size_t i = 0 ; i < symbols.size() ; ++i) {
			if (symbols[i]->sample.file_loc.app_name !=
			    symbols[i]->sample.file_loc.image_name) {
				options::output_format_flags = static_cast<outsymbflag>(options::output_format_flags | osf_app_name);
				break;
			}
		}
	}

	bool need_vma64 = vma64_p(symbols.begin(), symbols.end());

	format_output::formatter out(samples, counter);
	out.add_format(options::output_format_flags);
	out.output(cout, symbols, !options::reverse_sort, need_vma64);
}

static int do_it(int argc, char const * argv[])
{
	get_options(argc, argv);

	if (options::list_symbols && options::show_shared_libs) {
		cerr << "You can't specify --show-shared-libs and "
			"--list-symbols together." << endl;
		exit(EXIT_FAILURE);
	}

	/* TODO: allow as op_merge to specify explicitly name of samples
	 * files rather getting the whole directory. Code in op_merge can
	 * be probably re-used */
	list<string> file_list;
	get_sample_file_list(file_list, options::samples_dir, "*#*");

	map_t file_map;
	sort_file_list_by_name(file_map, file_list);

	if (file_map.empty()) {
		cerr << "no samples files found (try running opcontrol --dump ?)\n";
		exit(EXIT_FAILURE);
	}

	if (options::list_symbols) {
		output_symbols_count(file_map, options::counter);
	} else {
		output_files_count(file_map);
	}

	return 0;
}

int main(int argc, char const * argv[])
{
	// FIXME : same piece of code in all pp tools, we must add
	// do_it(ptr_to_function_to_exec, argc, argv); and share this code ?
	try {
		return do_it(argc, argv);
	}
	catch (op_runtime_error const & e) {
		cerr << "op_runtime_error:" << e.what() << endl;
		return 1;
	}
	catch (op_fatal_error const & e) {
		cerr << "op_fatal_error:" << e.what() << endl;
	}
	catch (op_exception const & e) {
		cerr << "op_exception:" << e.what() << endl;
	}
	catch (exception const & e) {
		cerr << "exception:" << e.what() << endl;
	}
	catch (...) {
		cerr << "unknown exception" << endl;
	}

	return EXIT_SUCCESS;
}
