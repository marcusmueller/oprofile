/**
 * @file op_to_source_options.cpp
 * Annotated source output command line options
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author Philippe Elie
 * @author John Levon
 */

#include "op_to_source_options.h"
#include "popt_options.h"

#include <fstream>
#include <iostream>

#include "cverb.h"

using std::vector;
using std::string;
using std::cerr;
using std::endl;

namespace options {
	int with_more_than_samples;
	int until_more_than_samples;
	int sort_by_counter = -1;
	string source_dir;
	string output_dir;
	string output_filter;
	string no_output_filter;
	bool assembly;
	bool source_with_assembly;
	string sample_file;
	string image_file;
	bool demangle;
	vector<string> exclude_symbols;
	string objdump_params;
};

namespace {

bool verbose;

option options_array[] = {
	option(verbose, "verbose", 'V', "verbose output"),
	option(options::sample_file, "samples-file", 'f', "image sample file", "file"),
	option(options::image_file, "image-file", 'i', "image file", "file"),
	option(options::demangle, "demangle", 'd', "demangle GNU C++ symbol names"),
	option(options::with_more_than_samples, "with-more-than-samples", 'w',
		"show all source file if the percent of samples in this file is more than argument", "[0-100]"),
	option(options::until_more_than_samples, "until-more-than-samples", 'm',
		"show all source files until the percent of samples specified is reached", "[0-100]"),
	option(options::sort_by_counter, "sort-by-counter", 'c', "sort by counter", "counter nr"),
	option(options::source_dir, "source-dir", '\0', "source directory", "directory name"),
	option(options::output_dir, "output-dir", '\0', "output directory", "directory name"),
	option(options::output_filter, "output", '\0', "output filename filter", "filter string"),
	option(options::no_output_filter, "no-output", '\0', "no output filename filter", "filter string"),
	option(options::assembly, "assembly", 'a', "output assembly code"),
	option(options::source_with_assembly, "source-with-assembly", 's', "output assembly code mixed with source"),
	option(options::exclude_symbols, "exclude-symbol", 'e', "exclude these comma separated symbols", "symbol_name"),
	option(options::objdump_params, "objdump-params", 'o', "additional parameters to pass to objdump", "objdump argument(s)")
};

}

/**
 * get_options - process command line
 * @param argc program arg count
 * @param argv program arg array
 *
 * Process the arguments, fatally complaining on error.
 */
string const get_options(int argc, char const * argv[])
{
	string arg;

	parse_options(argc, argv, arg);

	set_verbose(verbose);

	if (options::with_more_than_samples
		&& options::until_more_than_samples) {
		cerr << "op_to_source: --with-more-than-samples and "
			<< "--until-more-than-samples can not be "
			<< "specified together" << endl;
		exit(EXIT_FAILURE);
	}

	if (options::output_filter.empty())
		options::output_filter = "*";

	return arg;
}
