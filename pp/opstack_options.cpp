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
	demangle_type demangle = dmt_normal;
	bool exclude_dependent;
	merge_option merge_by;
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

	if (classes.v.size() > 1) {
		cerr << "error: too many profiles classes, you must restrict"
			" samples specification\n";
		exit(EXIT_FAILURE);
	}
}
