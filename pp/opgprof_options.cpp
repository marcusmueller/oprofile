/**
 * @file opgprof_options.cpp
 * Options for opgprof tool
 *
 * @remark Copyright 2003 OProfile authors
 * @remark Read the file COPYING
 *
 * @author John Levon
 * @author Philippe Elie
 */

#include <vector>
#include <list>
#include <iterator>
#include <iostream>

#include "opgprof_options.h"
#include "popt_options.h"
#include "cverb.h"
#include "profile_spec.h"
#include "arrange_profiles.h"

using namespace std;

inverted_profile image_profile;

namespace options {
	string gmon_filename = "gmon.out";

	// Ugly, for build only
	bool demangle;
	bool smart_demangle;
}


namespace {

popt::option options_array[] = {
	popt::option(options::gmon_filename, "output-filename", 'o',
	             "output filename, defaults to gmon.out if not specified",
	             "filename"),
};


bool try_merge_profiles(profile_spec const & spec, bool exclude_dependent)
{
	list<string> sample_files = spec.generate_file_list(exclude_dependent);

	cverb << "Matched sample files: " << sample_files.size() << endl;
	copy(sample_files.begin(), sample_files.end(),
	     ostream_iterator<string>(cverb, "\n"));

	// opgprof merge all by default
	merge_option merge_by;
	merge_by.cpu = true;
	merge_by.lib = true;
	merge_by.tid = true;
	merge_by.tgid = true;
	merge_by.unitmask = true;

	profile_classes classes
		= arrange_profiles(sample_files, merge_by);

	size_t nr_classes = classes.v.size();

	list<inverted_profile> iprofiles
		= invert_profiles(classes, options::extra_found_images);

	if (nr_classes == 1 && iprofiles.size() == 1) {
		image_profile = *(iprofiles.begin());
		return true;
	}

	// come round for another try
	if (exclude_dependent)
		return false;

	if (iprofiles.empty()) {
		cerr << "error: no sample files found: profile specification "
		     "too strict ?" << endl;
		exit(EXIT_FAILURE);
	}

	if (nr_classes > 1 || iprofiles.size() > 1) {
		cerr << "error: specify exactly one binary to process "
		     "and give an event: or count: specification if necessary"
		     << endl;
		exit(EXIT_FAILURE);
	}

	return false;
}

}  // anonymous namespace


void handle_options(vector<string> const & non_options)
{
	cverb << "output filename: " << options::gmon_filename << endl;

	profile_spec const spec =
		profile_spec::create(non_options, options::extra_found_images);

	// we do a first try with exclude-dependent if it fails we include
	// dependent. First try should catch "opgrof /usr/bin/make" whilst
	// the second catch "opgprof /lib/libc-2.2.5.so"
	if (!try_merge_profiles(spec, true)) {
		try_merge_profiles(spec, false);
	}
}
