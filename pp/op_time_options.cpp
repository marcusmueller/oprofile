/**
 * @file op_time_options.cpp
 * Options for summary tool
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author Philippe Elie
 * @author John Levon
 */

#include "opp_symbol.h"
#include "format_output.h"
#include "op_time_options.h"
#include "counter_util.h"
#include "session.h"

#include "popt_options.h"
#include "file_manip.h"
#include "cverb.h"

#include <list>
#include <fstream>
#include <sstream>

using namespace std;

namespace options {
	bool list_symbols;
	bool show_image_name;
	bool demangle;
	bool demangle_and_shrink;
	vector<string> exclude_symbols;
	bool reverse_sort;
	bool show_shared_libs;
	int sort_by_counter = -1;
	int counter;
	string samples_dir;
	outsymbflag output_format_flags;
	bool output_format_specified;
	alt_filename_t alternate_filename;
	vector<string> filename_filters;
	bool verbose;
}

namespace {

string output_format;
vector<string> path;
/// selected counters (comma-separated)
string counter_str("0");

popt::option options_array[] = {
	popt::option(options::verbose, "verbose", 'V', "verbose output"),
	popt::option(output_format, "output-format", 't', "choose the output format", "output-format strings"),
	popt::option(counter_str, "counter", 'c', "which counter to use", "counter_nr[,counter_nr]"),
	popt::option(options::list_symbols, "list-symbols", 'l', "list samples by symbol"),
	popt::option(options::show_image_name, "show-image-name", 'n', "show the image name from where come symbols"),
	popt::option(path, "path", 'p', "add path for retrieving image recursively", "path_name[,path_name]"),
	popt::option(options::reverse_sort, "reverse", 'r', "reverse sort order"),
	popt::option(options::show_shared_libs, "show-shared-libs", 'k', "show details for shared libraries."),
	popt::option(options::sort_by_counter, "sort", 'C', "which counter to use for sampels sort", "counter nr"),
	popt::option(options::exclude_symbols, "exclude-symbol", 'e', "exclude these comma separated symbols", "symbol_name"),
	popt::option(options::demangle, "demangle", 'd', "demangle GNU C++ symbol names"),
	popt::option(options::demangle_and_shrink, "smart-demangle", 'D', "demangle GNU C++ symbol names then pass them through regular expression to shrink them")
};

} // namespace anon

/**
 * get_options - process command line
 * @param argc program arg count
 * @param argv program arg array
 *
 * Process the arguments, fatally complaining on error.
 */
void get_options(int argc, char const * argv[])
{
	using namespace options;

	ostringstream format_help;
	format_help << endl;
	format_output::show_help(format_help);	
	format_help << "default format is hvspni, e format is added with -k "
		    << "option" << endl;
	popt::parse_options(argc, argv, filename_filters, format_help.str());

	set_verbose(verbose);

	options::samples_dir = handle_session_options();

	if (demangle_and_shrink) {
		demangle = true;
	}

	output_format_specified = true;
	if (output_format.empty()) {
		output_format = "hvspni";
		output_format_specified = false;
	} else {
		if (!list_symbols) {
			cerr << "op_time: --output-format can be used only with --list-symbols." << endl;
			exit(EXIT_FAILURE);
		}
	}

	if (exclude_symbols.size() && !list_symbols) {
		cerr << "op_time: --exclude-symbol can be used only with --list-symbols." << endl;
		exit(EXIT_FAILURE);
	}

	if (list_symbols) {
		outsymbflag fl =
			format_output::parse_format(output_format);

		if (fl == osf_none || (fl & ~(osf_header|osf_details)) == 0 ||
		    (fl & (osf_percent_details | osf_percent_cumulated_details))) {
			cerr << "op_time: invalid --output-format flags.\n";
			format_output::show_help(cerr);
			exit(EXIT_FAILURE);
		}

		output_format_flags = fl;
	}

	if (show_image_name)
		output_format_flags = static_cast<outsymbflag>(output_format_flags | osf_image_name);

	add_to_alternate_filename(alternate_filename, path);

	options::counter = parse_counter_mask(counter_str);

	validate_counter(counter, options::sort_by_counter);
}
