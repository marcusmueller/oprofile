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
#include "file_manip.h"
#include "cverb.h"

using namespace std;

profile_classes classes;

namespace options {
	bool demangle = true;
	bool smart_demangle;
	bool symbols;
	bool debug_info;
	bool details;
	bool exclude_dependent;
	string_filter symbol_filter;
	sort_options sort_by;
	merge_option merge_by;
	bool show_header = true;
	bool long_filenames;
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

popt::option options_array[] = {
	popt::option(options::demangle, "demangle", '\0',
		     "demangle GNU C++ symbol names (default on)"),
	popt::option(options::demangle, "no-demangle", '\0',
		     "don't demangle GNU C++ symbol names"),
	popt::option(options::smart_demangle, "smart-demangle", 'D',
		     "demangle GNU C++ symbol names and shrink them"),
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
		     "comma separated list", "cpu,pid,lib"),
	popt::option(options::show_header, "no-header", 'n',
		     "remove all header from output"),
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
void handle_merge_option()
{
	vector<string>::const_iterator cit = mergespec.begin();
	vector<string>::const_iterator end = mergespec.end();

	for (; cit != end; ++cit) {
		if (*cit == "cpu") {
			options::merge_by.cpu = true;
		} else if (*cit == "tid") {
			options::merge_by.tid = true;
		} else if (*cit == "tgid") {
			// PP:5.21 tgid merge imply tid merging.
			options::merge_by.tgid = true;
			options::merge_by.tid = true;
		} else if (*cit == "lib") {
			options::merge_by.lib = true;
		} else if (*cit == "unitmask") {
			options::merge_by.unitmask = true;
		} else if (*cit == "all") {
			options::merge_by.cpu = true;
			options::merge_by.lib = true;
			options::merge_by.tid = true;
			options::merge_by.tgid = true;
			options::merge_by.unitmask = true;
		} else {
			cerr << "unknown merge option: " << *cit << endl;
			exit(EXIT_FAILURE);
		}
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

	if (exclude_dependent) {
		if (merge_by.lib) {
			cerr << "--merge=lib is meaningless "
			     << "with --exclude-dependent" << endl;
			do_exit = true;
		}
	}

	if (do_exit)
		exit(EXIT_FAILURE);
}

} // namespace anon


void handle_options(vector<string> const & non_options)
{
	using namespace options;

	if (details)
		symbols = true;

	handle_sort_option();
	handle_merge_option();
	handle_output_file();
	check_options();

	symbol_filter = string_filter(include_symbols, exclude_symbols);

	profile_spec const spec =
		profile_spec::create(non_options, extra_found_images);

	list<string> sample_files = spec.generate_file_list(exclude_dependent);

	cverb << "Matched sample files: " << sample_files.size() << endl;
	copy(sample_files.begin(), sample_files.end(),
	     ostream_iterator<string>(cverb, "\n"));

	classes = arrange_profiles(sample_files, merge_by);

	if (classes.v.empty()) {
		cerr << "No samples files found: profile specification too "
		     << "strict ?" << endl;
		exit(EXIT_FAILURE);
	}
}
