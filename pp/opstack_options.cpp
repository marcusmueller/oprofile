/**
 * @file opstack_options.cpp
 * Options for opstack tool
 *
 * @remark Copyright 2002, 2003 OProfile authors
 * @remark Read the file COPYING
 *
 * @author Philippe Elie
 */

#include <cstdlib>
#include <iterator>
#include <iostream>

#include "cverb.h"
#include "profile_spec.h"
#include "arrange_profiles.h"
#include "opstack_options.h"
#include "popt_options.h"

using namespace std;

profile_classes classes;

namespace options {
	string archive_path;
	demangle_type demangle = dmt_normal;
	bool exclude_dependent;
	merge_option merge_by;
	sort_options sort_by;
	bool reverse_sort;
	bool long_filenames;
	bool show_header = true;
	bool show_address;
	bool debug_info;
	bool accumulated;
}


namespace {

vector<string> mergespec;
vector<string> sort;
string demangle_option = "normal";

popt::option options_array[] = {
	popt::option(demangle_option, "demangle", '\0',
		     "demangle GNU C++ symbol names (default normal)",
	             "none|normal|smart"),
	popt::option(options::exclude_dependent, "exclude-dependent", 'x',
		     "exclude libs, kernel, and module samples for applications"),
	popt::option(sort, "sort", 's',
		     "sort by", "sample,image,app-name,symbol,debug,vma"),
	popt::option(mergespec, "merge", 'm',
		     "comma separated list", "cpu,lib,tid,tgid,unitmask,all"),
	popt::option(options::reverse_sort, "reverse-sort", 'r',
		     "use reverse sort"),
	popt::option(options::long_filenames, "long-filenames", 'f',
		     "show the full path of filenames"),
	popt::option(options::show_header, "no-header", 'n',
		     "remove all headers from output"),
	popt::option(options::show_address, "show-address", 'w',
	             "show VMA address of each symbol"),
	popt::option(options::debug_info, "debug-info", 'g',
		     "add source file and line number to output"),
	popt::option(options::accumulated, "accumulated", 'c',
		     "percentage field show accumulated count"),
	popt::option(options::threshold_opt, "threshold", 't',
		     "minimum percentage needed to produce output",
		     "percent"),
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


}  // anonymous namespace


void handle_options(vector<string> const & non_options)
{
	using namespace options;

	handle_sort_option();
	merge_by = handle_merge_option(mergespec, true, exclude_dependent);
	demangle = handle_demangle_option(demangle_option);

	profile_spec const spec =
		profile_spec::create(non_options, extra_found_images);

	list<string> sample_files =
		spec.generate_file_list(exclude_dependent, false);

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
