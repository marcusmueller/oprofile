/* COPYRIGHT (C) 2001 Philippe Elie
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59 Temple
 * Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * first written by P.Elie, many cleanup by John Levon
 */

#include <popt.h>
#include <stdlib.h>

#include <string>
#include <list>
#include <iostream>
#include <sstream>
#include <fstream>
#include <algorithm>
#include <map>

#include "oprofpp.h"

#include "../util/file_manip.h"
#include "../util/op_popt.h"

using std::string;
using std::list;
using std::cerr;
using std::cout;
using std::endl;
using std::ostringstream;
using std::ifstream;
using std::multimap;
using std::pair;

// TODO header file
/// image_name - class to store name for a samples file
struct image_name
{
	image_name(const string& samplefile_name_, const string& app_name_,
		   const string& lib_name_, u32 = 0);

	image_name(const string& samplefile_name, u32 count = 0);

	/// total number of samples for this samples file, this is a place
	/// holder to avoid separate data struct which associate image_name
	/// with a sample count.
	u32 count;
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

typedef multimap<string, image_name> map_t;
typedef pair<map_t::iterator, map_t::iterator> pair_it_t;
typedef multimap<u32, map_t::const_iterator> sorted_map_t;

static int showvers;
static int reverse_sort;
static int show_shared_libs;
static const char * base_dir = "/var/opd/samples";

static struct poptOption options[] = {
	{ "verbose", 'V', POPT_ARG_NONE, &verbose, 0, "verbose output", NULL, },
	{ "use-counter", 'c', POPT_ARG_INT, &ctr, 0,
	  "use counter", "counter nr", },
	{ "show-shared-libs", 'k', POPT_ARG_NONE, &show_shared_libs, 0,
	  "show details for shared libs. Only meaningfull if you have profiled with --separate-samples", NULL, },
	{ "reverse", 'r', POPT_ARG_NONE, &reverse_sort, 0,
	  "reverse sort order", NULL, },
	{ "version", 'v', POPT_ARG_NONE, &showvers, 0, "show version", NULL, },
	POPT_AUTOHELP
	{ NULL, 0, 0, NULL, 0, NULL, NULL, },
};

/**
 * get_options - process command line
 * @argc: program arg count
 * @argv: program arg array
 *
 * Process the arguments, fatally complaining on error.
 */
static void get_options(int argc, char const * argv[])
{
	poptContext optcon;
	char const * file;

	optcon = opd_poptGetContext(NULL, argc, argv, options, 0);

	if (showvers) {
		printf("%s: " VERSION_STRING " compiled on "
		       __DATE__ " " __TIME__ "\n", argv[0]);
		exit(EXIT_SUCCESS);
	}

	// non-option file, must be valid directory name
	file = poptGetArg(optcon);
	if (file)
		base_dir = file;

	if (ctr == -1)
		ctr = 0;

	poptFreeContext(optcon);
}

/**
 * image_name - ctor from an already parsed name
 */
image_name::image_name(const string& samplefile_name_, const string& app_name_,
		       const string& lib_name_, u32 count_ = 0)
	: 
	count(count_),
	samplefile_name(samplefile_name_),
	app_name(app_name_),
	lib_name(lib_name_)
{
}

/**
 * image_name - ctor from a sample file name
 */
image_name::image_name(const string& samplefile_name, u32 count_ = 0)
	:
	count(count_),
	samplefile_name(samplefile_name)
{
	app_name = extract_app_name(samplefile_name, lib_name);
}

/**
 * samples_file_exist - test for a samples file existence
 * @filename: the base samples filename
 *
 * return true if @filename#ctr exists
 */
static bool samples_file_exist(const std::string & filename)
{
	std::ostringstream s;

	s << filename << "#" << ctr;

	ifstream in(s.str().c_str());

	return in;
}

/**
 * sort_file_list - insert in result a file list sorted by app name
 * @result: where to put result
 * @file_list: a list of string which must be insert in @result
 *
 * for each filename try to extract if possible the app name and
 * use it as a key to insert the filename in @result
 *  filename are on the form
 *   }usr}sbin}syslogd}}}lib}libc-2.1.2.so (shared lib
 *     which belong to app /usr/sbin/syslogd)
 *  or
 *   }bin}bash (application)
 * This sorting is used later to find samples filename which belong
 * to the same application.
 */
static void sort_file_list(map_t & result,
			   const list<string> & file_list)
{
	list<string>::const_iterator it;
	for (it = file_list.begin() ; it != file_list.end() ; ++it) {
		image_name image(*it);

		result.insert(map_t::value_type(image.app_name, image));
	}
}

/**
 * out_filename - display a filename and it associated ratio of samples
 */
static void out_filename(const string& app_name, size_t app_count,
			 u32 count, double total_count)
{
	cout << demangle_filename(app_name) << " " << count  << " ";

	if (total_count > 1) {
		cout << (count / total_count) * 100 << "%";
	} else {
		cout << "0%";
	}

	if (app_count != size_t(-1) && (app_count!=0))
		cout << " (" << (count / double(app_count)) * 100 << "%)";

	cout << endl;
}

/**
 * output_image_details - output the samples ratio for some images
 * @first: the start iterator
 * @last: the end iterator
 * @total_count: the total samples count
 *
 * iterator parameters are intended to be of type @map_t::iterator or
 * @map_t::reverse_iterator
 */
template <class Iterator>
static void output_image_details(Iterator first, Iterator last, u32 app_count,
				 double total_count)
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
 * build_sorted_map_by_count - insert element in a @sorted_map_t
 * @first: the start iterator
 * @last: the end iterator
 * @total_count: the total samples count
 *
 */
void build_sorted_map_by_count(sorted_map_t & sorted_map, pair_it_t p_it)
{
	map_t::iterator it;
	for (it = p_it.first ; it != p_it.second ; ++it) {
		sorted_map_t::value_type value(it->second.count, it);

		sorted_map.insert(value);
	}
}

/**
 * treat_file_list - open each samples file to cumulate samples count
 * and display a sorted list of filename and samples ratio
 * @files: the file list to treat.
 */
static void treat_file_list(map_t& files)
{
	double total_count = 0;

	/* 1st pass: accumulate for each image_name the samples count and
	 * update the total_count of samples */

	map_t::const_iterator it;
	for (it = files.begin(); it != files.end() ; ) {
		pair_it_t p_it = files.equal_range(it->first);

		// the range [p_it.first, p_it.second[ belongs to application
		// it.first->first
		for ( ; p_it.first != p_it.second ; ++p_it.first) {
			std::string filename(string(base_dir) + "/" +
				     p_it.first->second.samplefile_name);

			if (samples_file_exist(filename) == false)
				continue;

			opp_samples_files samples(filename);

			u32 count = 0;
			if (samples.is_open(ctr)) {
				const opd_fentry * begin;
				const opd_fentry * end;
				begin = samples.samples[ctr];
				end = begin + samples.nr_samples;
				for ( ; begin != end ; ++begin)
					count += begin->count;
			}

			p_it.first->second.count = count;
			total_count += count;
		}

		it = p_it.second;
	}

	if (total_count == 0.0) {
		cerr << "no samples files found\n";
		return;	// Would exit(EXIT_FAILURE); perhaps
	}

	/* 2nd pass: insert the count of samples for each application and
	 * associate with an iterator to a image name e.g. insert one
	 * item for each application */

	sorted_map_t sorted_map;
	for (it = files.begin(); it != files.end() ; ) {
		pair_it_t p_it = files.equal_range(it->first);
		u32 count = 0;
		for ( ; p_it.first != p_it.second ; ++p_it.first) {
			count += p_it.first->second.count;
		}

		sorted_map.insert(sorted_map_t::value_type(count, it));

		it = p_it.second;
	}

	/* 3rd pass: we can output the result, during the output we optionnaly
	 * build the set of image_file which belongs to one application and
	 * display these results too */

	/* this if else are only different by the type of iterator used, we
	 * can't easily templatize the block on iterator type because we use
	 * in one case begin()/end() and in another rbegin()/rend(), it's
	 * worthwhile to try to factorize these two piece of code */
	if (reverse_sort) {
		sorted_map_t::reverse_iterator s_it = sorted_map.rbegin();
		for ( ; s_it != sorted_map.rend(); ++s_it) {
			map_t::const_iterator it = s_it->second;

			out_filename(it->first, size_t(-1), s_it->first,
				     total_count);

			if (show_shared_libs) {
				pair_it_t p_it = files.equal_range(it->first);
				sorted_map_t temp_map;

				build_sorted_map_by_count(temp_map, p_it);

				output_image_details(temp_map.rbegin(),
						     temp_map.rend(),
						     s_it->first,
						     total_count);
			}
		}
	} else {
		sorted_map_t::iterator s_it = sorted_map.begin();
		for ( ; s_it != sorted_map.end() ; ++s_it) {
			map_t::const_iterator it = s_it->second;

			out_filename(it->first, size_t(-1), s_it->first,
				     total_count);

			if (show_shared_libs) {
				pair_it_t p_it = files.equal_range(it->first);
				sorted_map_t temp_map;

				build_sorted_map_by_count(temp_map, p_it);

				output_image_details(temp_map.begin(),
						     temp_map.end(),
						     s_it->first,
						     total_count);
			}
		}
	}
}

/**
 * yet another main ...
 */
int main(int argc, char const * argv[])
{
	get_options(argc, argv);

	list<string> file_list;
	get_sample_file_list(file_list, base_dir, "*");

	map_t file_map;
	sort_file_list(file_map, file_list);

	treat_file_list(file_map);

	return 0;
}
