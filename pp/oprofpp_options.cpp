/**
 * @file oprofpp_options.cpp
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author John Levon
 * @author Philippe Elie
 */

#include <fstream>

#include "popt_options.h"

#include "oprofpp_options.h"
#include "opp_symbol.h"
#include "counter_util.h"
#include "cverb.h"

#include <sstream>

using std::string;
using std::vector;
using std::cerr;
using std::endl;
using std::ostream;
using namespace options;

namespace options {
	int counter_mask;
	int sort_by_counter = -1;
	string gprof_file;
	string symbol;
	bool list_symbols;
	bool output_linenr_info;
	bool reverse_sort;
	bool show_shared_libs;
	outsymbflag output_format_flags;
	bool list_all_symbols_details;
	string sample_file;
	string image_file;
	bool demangle;
	vector<string> exclude_symbols;
};

namespace {

bool verbose;
string output_format;
string counter_str;

option options_array[] = {
	option(options::sample_file, "samples-file", 'f', "image sample file", "file"),
	option(options::image_file, "image-file", 'i', "image file", "file"),
	option(options::demangle, "demangle", 'd', "demangle GNU C++ symbol names"),
	option(options::exclude_symbols, "exclude-symbol", 'e', "exclude these comma separated symbols", "symbol_name"),
	option(options::sort_by_counter, "sort", 'C', "which counter to use for sampels sort", "counter nr"),
	option(options::gprof_file, "dump-gprof-file", 'g', "dump gprof format file", "file"),
	option(options::symbol, "list-symbol", 's', "give detailed samples for a symbol", "symbol"),
	option(options::list_symbols, "list-symbols", 'l', "list samples by symbol"),
	option(options::output_linenr_info, "output-linenr-info", 'o', "output filename:linenr info"),
	option(options::reverse_sort, "reverse", 'r', "reverse sort order"),
	option(options::show_shared_libs, "show-shared-libs", 'k', "show details for shared libraries"),
	option(options::list_all_symbols_details, "list-all-symbols-details", 'L', "list samples for all symbols"),
	option(counter_str, "counter", 'c', "which counter to display", "counter number[,counter nr]"),
	option(output_format, "output-format", 't', "choose the output format", "output-format strings"),
	option(verbose, "verbose", 'V', "verbose output"),
};

/**
 * quit_error - quit with error
 * @param err error to show
 */
void quit_error(string const & err)
{
	cerr << err;
	exit(EXIT_FAILURE);
}

} // namespace anon

string const get_options(int argc, char const **argv)
{
	/* non-option file, either a sample or binary image file */
	string arg;

	parse_options(argc, argv, arg);

	counter_mask = parse_counter_mask(counter_str);

	set_verbose(verbose);
 
	if (!list_all_symbols_details && !list_symbols &&
	    gprof_file.empty() && symbol.empty())
		quit_error("oprofpp: no mode specified. What do you want from me ?\n");

	/* check only one major mode specified */
	if ((list_all_symbols_details + list_symbols + !gprof_file.empty() + !symbol.empty()) > 1)
		quit_error("oprofpp: must specify only one output type.\n");

	if (output_linenr_info && !list_all_symbols_details && symbol.empty() && !list_symbols)
		quit_error("oprofpp: cannot list debug info without -L, -l or -s option.\n");

	if (show_shared_libs && (!symbol.empty() || !gprof_file.empty())) {
		quit_error("oprofpp: you cannot specify --show-shared-libs with --dump-gprof-file or --list-symbol output type.\n");
	}

	if (reverse_sort && !list_symbols)
		quit_error("oprofpp: reverse sort can only be used with -l option.\n");

	if (show_shared_libs)
		output_format_flags = static_cast<outsymbflag>(output_format_flags | osf_image_name);
	if (output_linenr_info)
		output_format_flags = static_cast<outsymbflag>(output_format_flags | osf_linenr_info);

	if (output_format.empty()) {
		output_format = "hvspn";
	} else {
		if (!list_symbols && !list_all_symbols_details && symbol.empty()) {
			quit_error("oprofpp: --output-format can be used only without --list-symbols or --list-all-symbols-details.\n");
		}
	}

	if (list_symbols || list_all_symbols_details || !symbol.empty()) {
		outsymbflag fl =
			output_symbol::ParseOutputOption(output_format);

		if (fl == osf_none || (fl & ~(osf_header|osf_details)) == 0) {
			std::cerr << "oprofpp: invalid --output-format flags.\n";
			output_symbol::ShowHelp();
			exit(EXIT_FAILURE);
		}

		output_format_flags = static_cast<outsymbflag>(output_format_flags | fl);
	}

	return arg;
}
