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

#include "opgprof_options.h"
#include "popt_options.h"
#include "cverb.h"
#include "profile_spec.h"
#include "arrange_profiles.h"
#include "locate_images.h"

using namespace std;

profile_set profiles;

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

	if (nr_classes == 0 && !exclude_dependent) {
		cerr << "No samples files found: profile specification too "
		     << "strict ?" << endl;
		exit(EXIT_FAILURE);
	}

	size_t nr_app_profiles = 0;
	if (nr_classes)
		nr_app_profiles = classes.v[0].profiles.size();

	if (nr_classes == 1 && nr_app_profiles == 1) {
		profiles = *(classes.v[0].profiles.begin());
		// find 2.6 kernel module and check readability
		profiles.image = find_image_path(profiles.image,
			options::extra_found_images);
		return true;
	}

	// come round for another try
	if (exclude_dependent)
		return false;

	if (nr_app_profiles > 1) {
		cerr << "Specify exactly one binary to process." << endl;
		exit(EXIT_FAILURE);
	}

	// FIXME: we can do a lot better in telling the user the
	// *exact* problem based on the profile class templates
	cerr << "Too many unmerged profile specifications." << endl;
	cerr << "use event:xxxx and/or count:yyyyy to restrict "
	     << "samples files considered.\n" << endl;
	exit(EXIT_FAILURE);

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
