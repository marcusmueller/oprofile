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
	std::string archive_path;
	demangle_type demangle = dmt_normal;
	bool exclude_dependent;
	merge_option merge_by;
	bool long_filenames;
	bool show_header = true;
	bool show_address;
	bool debug_info;
	bool accumulated;
}


namespace {

vector<string> mergespec;
string demangle_option = "normal";

popt::option options_array[] = {
	popt::option(demangle_option, "demangle", '\0',
		     "demangle GNU C++ symbol names (default normal)",
	             "none|normal|smart"),
	popt::option(options::exclude_dependent, "exclude-dependent", 'x',
		     "exclude libs, kernel, and module samples for applications"),
	popt::option(mergespec, "merge", 'm',
		     "comma separated list", "cpu,lib,tid,tgid,unitmask,all"),
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
};

}  // anonymous namespace


void handle_options(vector<string> const & non_options)
{
	using namespace options;

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
