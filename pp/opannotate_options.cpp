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

#include <vector>
#include <list>
#include <iterator>

#include "profile_spec.h"
#include "partition_files.h"
#include "op_exception.h"
#include "opannotate_options.h"
#include "popt_options.h"
#include "cverb.h"

using namespace std;

scoped_ptr<partition_files> sample_file_partition;

namespace options {
	bool demangle = true;
	bool smart_demangle;
	string output_dir;
	vector<string> search_dirs;
	vector<string> base_dirs;
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

popt::option options_array[] = {
	popt::option(options::demangle, "demangle", '\0',
		     "demangle GNU C++ symbol names (default on)"),
	popt::option(options::demangle, "no-demangle", '\0',
		     "don't demangle GNU C++ symbol names"),
	popt::option(options::smart_demangle, "smart-demangle", 'D',
		     "demangle GNU C++ symbol names and shrink them"),
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
		     "additionnal params to pass to objdump", "parameters"),
	popt::option(options::exclude_dependent, "exclude-dependent", 'x',
		     "exclude libs, kernel, and module samples for applications"),
	popt::option(options::source, "source", 's', "output source"),
	popt::option(options::assembly, "assembly", 'a', "output assembly"),
};

}  // anonymous namespace


void handle_options(vector<string> const & non_options)
{
	using namespace options;

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

	options::symbol_filter = string_filter(include_symbols, exclude_symbols);

	options::file_filter = path_filter(include_file, exclude_file);

	profile_spec const spec =
		profile_spec::create(non_options, options::extra_found_images);

	list<string> sample_files = spec.generate_file_list(exclude_dependent);

	cverb << "Matched sample files: " << sample_files.size() << endl;
	copy(sample_files.begin(), sample_files.end(),
	     ostream_iterator<string>(cverb, "\n"));

	vector<unmergeable_profile>
		unmerged_profile = merge_profile(sample_files);

	cverb << "Unmergeable profile specification:\n";
	copy(unmerged_profile.begin(), unmerged_profile.end(),
	     ostream_iterator<unmergeable_profile>(cverb, "\n"));

	if (unmerged_profile.empty()) {
		cerr << "No samples files found: profile specification too "
		     << "strict ?" << endl;
		exit(EXIT_FAILURE);
	}

	if (unmerged_profile.size() > 1) {
		// quick and dirty check for now
		cerr << "Can't handle multiple counter!" << endl;
		cerr << "use event:xxxx and/or count:yyyyy to restrict "
		     << "samples files set considered\n" << endl;
		exit(EXIT_FAILURE);
	}

	// we always merge but this have no effect on output since at source
	// or assembly point of view the result be merged anyway
	merge_option merge_by;
	merge_by.cpu = true;
	merge_by.lib = true;
	merge_by.tid = true;
	merge_by.tgid = true;
	merge_by.unitmask = true;

	sample_file_partition.reset(
		new partition_files(sample_files, merge_by));
}
