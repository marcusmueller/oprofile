/**
 * @file opreport_options.cpp
 * Options for opreport tool
 *
 * @remark Copyright 2003 OProfile authors
 * @remark Read the file COPYING
 *
 * @author John Levon
 * @author Philippe Elie
 */

#include <vector>
#include <list>
#include <iostream>
#include <algorithm>
#include <iterator>
#include <fstream>

#include "profile_spec.h"
#include "arrange_profiles.h"
#include "opreport_options.h"
#include "popt_options.h"
#include "string_filter.h"
#include "file_manip.h"
#include "cverb.h"

using namespace std;

profile_classes classes;

namespace options {
	string archive_path;
	demangle_type demangle = dmt_normal;
	bool symbols;
	bool debug_info;
	bool details;
	bool exclude_dependent;
	string_filter symbol_filter;
	sort_options sort_by;
	merge_option merge_by;
	bool show_header = true;
	bool long_filenames;
	bool show_address;
	bool accumulated;
	bool reverse_sort;
	bool global_percent;
}


namespace {

string outfile;
vector<string> mergespec;
vector<string> sort;
vector<string> exclude_symbols;
vector<string> include_symbols;
string demangle_option = "normal";

popt::option options_array[] = {
	popt::option(demangle_option, "demangle", '\0',
		     "demangle GNU C++ symbol names (default normal)",
	             "none|normal|smart"),
	popt::option(outfile, "output-file", 'o',
	             "output to the given filename", "file"),
	// PP:5
	popt::option(options::symbols, "symbols", 'l',
		     "list all symbols"),
	popt::option(options::debug_info, "debug-info", 'g',
		     "add source file and line number to output"),
	popt::option(options::details, "details", 'd',
		     "output detailed samples for each symbol"),
	popt::option(options::exclude_dependent, "exclude-dependent", 'x',
		     "exclude libs, kernel, and module samples for applications"),
	popt::option(sort, "sort", 's',
		     "sort by", "sample,image,app-name,symbol,debug,vma"),
	popt::option(exclude_symbols, "exclude-symbols", 'e',
		     "exclude these comma separated symbols", "symbols"),
	popt::option(include_symbols, "include-symbols", 'i',
		     "include these comma separated symbols", "symbols"),
	popt::option(mergespec, "merge", 'm',
		     "comma separated list", "cpu,lib,tid,tgid,unitmask,all"),
	popt::option(options::show_header, "no-header", 'n',
		     "remove all headers from output"),
	popt::option(options::show_address, "show-address", 'w',
	             "show VMA address of each symbol"),
	popt::option(options::long_filenames, "long-filenames", 'f',
		     "show the full path of filenames"),
	popt::option(options::accumulated, "accumulated", 'c',
		     "percentage field show accumulated count"),
	popt::option(options::reverse_sort, "reverse-sort", 'r',
		     "use reverse sort"),
	popt::option(options::global_percent, "global-percent", '\0',
		     "percentage are not relative to symbol count or image "
		     "count but total sample count"),
};


// FIXME: separate file if reused
void handle_sort_option()
{
	if (sort.empty()) {
		// PP:5.14 sort default to sample
		sort.push_back("sample");
	}

	vector<string>::const_iterator cit = sort.begin();
	vector<string>::const_iterator end = sort.end();

	for (; cit != end; ++cit) {
		options::sort_by.add_sort_option(*cit);
	}
}


// FIXME: separate file if reused
void handle_output_file()
{
	if (outfile.empty())
		return;

	static ofstream os(outfile.c_str());
	if (!os) {
		cerr << "Couldn't open \"" << outfile
		     << "\" for writing." << endl;
		exit(EXIT_FAILURE);
	}

	cout.rdbuf(os.rdbuf());
}

/**
 * check incompatible or meaningless options
 *
 */
void check_options()
{
	using namespace options;

	bool do_exit = false;

	if (!symbols) {
		if (show_address) {
			cerr << "--show-address is meaningless "
				"without --symbols" << endl;
			do_exit = true;
		}

		if (debug_info || accumulated) {
			cerr << "--debug-info and --accumulated are "
			     << "meaningless without --symbols" << endl;
			do_exit = true;
		}

		if (!exclude_symbols.empty() || !include_symbols.empty()) {
			cerr << "--exclude-symbols and --include-symbols are "
			     << "meaningless without --symbols" << endl;
			do_exit = true;
		}

		if (find(sort_by.options.begin(), sort_by.options.end(), 
			 sort_options::vma) != sort_by.options.end()) {
			cerr << "--sort=vma is "
			     << "meaningless without --symbols" << endl;
			do_exit = true;
		}
	}

	if (global_percent && symbols && !details) {
		cerr << "--global-percent is meaningless "
		     << "with --symbols and without --details" << endl;
		do_exit = true;
	}

	if (do_exit)
		exit(EXIT_FAILURE);
}

} // namespace anon


void handle_options(vector<string> const & non_options)
{
	using namespace options;

	if (details) {
		symbols = true;
		show_address = true;
	}

	handle_sort_option();
	merge_by = handle_merge_option(mergespec, true, exclude_dependent);
	handle_output_file();
	demangle = handle_demangle_option(demangle_option);
	check_options();

	symbol_filter = string_filter(include_symbols, exclude_symbols);

	profile_spec const spec =
		profile_spec::create(non_options, extra_found_images);

	list<string> sample_files = spec.generate_file_list(exclude_dependent, true);

	archive_path = spec.get_archive_path();
	cverb << vsfile << "Archive: " << archive_path << endl;

	cverb << vsfile << "Matched sample files: " << sample_files.size()
	      << endl;
	copy(sample_files.begin(), sample_files.end(),
	     ostream_iterator<string>(cverb << vsfile, "\n"));

	classes = arrange_profiles(sample_files, merge_by);

	cverb << vsfile << "profile_classes:\n" << classes << endl;

	if (classes.v.empty()) {
		cerr << "error: no sample files found: profile specification "
		     "too strict ?" << endl;
		exit(EXIT_FAILURE);
	}
}
