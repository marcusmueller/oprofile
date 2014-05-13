/**
 * @file opannotate_options.cpp
 * Options for opannotate tool
 *
 * @remark Copyright 2003 OProfile authors
 * @remark Read the file COPYING
 *
 * @author John Levon
 * @author Philippe Elie
 */

#include <cstdlib>

#include <vector>
#include <list>
#include <iterator>
#include <iostream>
#include <cstdlib>

#include "op_config.h"
#include "profile_spec.h"
#include "arrange_profiles.h"
#include "op_exception.h"
#include "opannotate_options.h"
#include "popt_options.h"
#include "cverb.h"

using namespace std;

profile_classes classes;

namespace options {
	demangle_type demangle = dmt_normal;
	string output_dir;
	vector<string> search_dirs;
	vector<string> base_dirs;
	merge_option merge_by;
	path_filter file_filter;
	string_filter symbol_filter;
	bool source;
	bool assembly;
	vector<string> objdump_params;
	bool exclude_dependent;
}


namespace {

string include_symbols;
string exclude_symbols;
string include_file;
string exclude_file;
string demangle_option = "normal";
vector<string> mergespec;

popt::option options_array[] = {
	popt::option(demangle_option, "demangle", 'D',
		     "demangle GNU C++ symbol names (default normal)",
	             "none|normal|smart"),
	popt::option(options::output_dir, "output-dir", 'o',
		     "output directory", "directory name"),
	popt::option(options::search_dirs, "search-dirs", 'd',
	             "directories to look for source files", "comma-separated paths"),
	popt::option(options::base_dirs, "base-dirs", 'b',
	             "source file prefixes to strip", "comma-separated paths"),
	popt::option(include_file, "include-file", '\0',
		     "include these comma separated filename", "filenames"),
	popt::option(exclude_file, "exclude-file", '\0',
		     "exclude these comma separated filename", "filenames"),
	popt::option(include_symbols, "include-symbols", 'i',
		     "include these comma separated symbols", "symbols"),
	popt::option(exclude_symbols, "exclude-symbols", 'e',
		     "exclude these comma separated symbols", "symbols"),
	popt::option(options::objdump_params, "objdump-params", '\0',
		     "additional params to pass to objdump", "parameters"),
	popt::option(options::exclude_dependent, "exclude-dependent", 'x',
		     "exclude libs, kernel, and module samples for applications"),
	popt::option(mergespec, "merge", 'm',
		     "comma separated list", "cpu,tid,tgid,unitmask,all"),
	popt::option(options::source, "source", 's', "output source"),
	popt::option(options::assembly, "assembly", 'a', "output assembly"),
	popt::option(options::threshold_opt, "threshold", 't',
		     "minimum percentage needed to produce output",
		     "percent"),
};

}  // anonymous namespace


void handle_options(options::spec const & spec)
{
	using namespace options;
	vector<string> tmp_objdump_parms;

	/* When passing a quoted string of options from opannotate for the
	 * objdump command, objdump_parms consists of a single string.  Need
	 * to break the string into a series of individual options otherwise
	 * the call to exec_comand fails when it sees the space between the
	 * options.
	 */
	for (unsigned int i = 0; i < objdump_params.size(); i++) {
		string s;
		s = objdump_params[i];
		stringstream ss(s);
		istream_iterator<string> begin(ss);
		istream_iterator<string> end;
		vector<string> vstrings(begin, end);

		for (unsigned int j = 0; j < vstrings.size(); j++)
			tmp_objdump_parms.push_back(vstrings[j]);
	}

	// update objdump_parms.
	objdump_params.assign(tmp_objdump_parms.begin(), tmp_objdump_parms.end());

	if (spec.first.size()) {
		cerr << "differential profiles not allowed" << endl;
		exit(EXIT_FAILURE);
	}

	if (!assembly && !source) {
		cerr <<	"you must specify at least --source or --assembly\n";
		exit(EXIT_FAILURE);
	}

	if (!objdump_params.empty() && !assembly) {
		cerr << "--objdump-params is meaningless without --assembly\n";
		exit(EXIT_FAILURE);
	}

	if (search_dirs.empty() && !base_dirs.empty()) {
		cerr << "--base-dirs is useless unless you specify an "
			"alternative source location with --search-dirs"
		     << endl;
		exit(EXIT_FAILURE);
	}

	if (assembly && !output_dir.empty()) {
		cerr << "--output-dir is meaningless with --assembly" << endl;
		exit(EXIT_FAILURE);
	}

	if (assembly && (!include_file.empty() || !exclude_file.empty())) {
		cerr << "--exclude[include]-file options not supported with --assembly" << endl;
		cerr << "Please see the opannotate man page." << endl;
		exit(EXIT_FAILURE);
	}

	options::symbol_filter = string_filter(include_symbols, exclude_symbols);

	options::file_filter = path_filter(include_file, exclude_file);

	profile_spec const pspec =
		profile_spec::create(spec.common, options::image_path,
				     options::root_path);

	if (!was_session_dir_supplied())
		cerr << "Using " << op_samples_dir << " for session-dir" << endl;

	list<string> sample_files = pspec.generate_file_list(exclude_dependent, true);

	cverb << vsfile << "Archive: " << pspec.get_archive_path() << endl;

	cverb << vsfile << "Matched sample files: " << sample_files.size()
	      << endl;
	copy(sample_files.begin(), sample_files.end(),
	     ostream_iterator<string>(cverb << vsfile, "\n"));

	demangle = handle_demangle_option(demangle_option);

	// we always merge but this have no effect on output since at source
	// or assembly point of view the result will be merged anyway
	merge_by = handle_merge_option(mergespec, false, exclude_dependent);

	classes = arrange_profiles(sample_files, merge_by,
				   pspec.extra_found_images);

	cverb << vsfile << "profile_classes:\n" << classes << endl;

	if (classes.v.empty()) {
		cerr << "error: no sample files found: profile specification "
		     "too strict ?" << endl;
		exit(EXIT_FAILURE);
	}
}
