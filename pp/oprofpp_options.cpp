/**
 * @file oprofpp_options.cpp
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author John Levon <moz@compsoc.man.ac.uk>
 * @author Philippe Elie <phil_el@wanadoo.fr>
 */
 
#include "oprofpp_options.h"
#include "popt_options.h"
#include "oprofpp.h"
#include "opp_symbol.h"
 
#include "verbose_ostream.h"
 
using std::string;
using std::vector;
using std::ostream;
using namespace options;
 
namespace options {
	string ctr_str;
	int sort_by_counter = -1;
	string gproffile;
	string symbol;
	bool list_symbols;
	bool output_linenr_info;
	bool reverse_sort;
	bool show_shared_libs;
	OutSymbFlag output_format_flags;
	bool list_all_symbols_details;
	string samplefile;
	string imagefile;
	bool demangle;
	vector<string> exclude_symbols;
};
 
namespace {
 
bool verbose;

string output_format;
 
option options_array[] = {
	option(options::samplefile, "samples-file", 'f', "image sample file", "file"),
	option(options::imagefile, "image-file", 'i', "image file", "file"),
	option(options::demangle, "demangle", 'd', "demangle GNU C++ symbol names"),
	option(options::exclude_symbols, "exclude-symbol", 'e', "exclude these comma separated symbols", "symbol_name"),
	option(options::ctr_str, "counter", 'c', "which counter to display", "counter number[,counter nr]"),
	option(options::sort_by_counter, "sort", 'C', "which counter to use for sampels sort", "counter nr"),
	option(options::gproffile, "dump-gprof-file", 'g', "dump gprof format file", "file"),
	option(options::symbol, "list-symbol", 's', "give detailed samples for a symbol", "symbol"),
	option(options::list_symbols, "list-symbols", 'l', "list samples by symbol"),
	option(options::output_linenr_info, "output-linenr-info", 'o', "output filename:linenr info"),
	option(options::reverse_sort, "reverse", 'r', "reverse sort order"),
	option(options::show_shared_libs, "show-shared-libs", 'k', "show details for shared libraries"),
	option(options::list_all_symbols_details, "list-all-symbols-details", 'L', "list samples for all symbols"),
	option(output_format, "output-format", 't', "choose the output format", "output-format strings"),
	option(verbose, "verbose", 'V', "verbose output"),
};
 
} // namespace anon
 
verbose_ostream cverb(verbose);
 
string const get_options(int argc, char const **argv)
{
	/* non-option file, either a sample or binary image file */
	string arg;
	
	parse_options(argc, argv, arg);

	if (!list_all_symbols_details && !list_symbols && 
	    gproffile.empty() && symbol.empty())
		quit_error("oprofpp: no mode specified. What do you want from me ?\n");

	/* check only one major mode specified */
	if ((list_all_symbols_details + list_symbols + !gproffile.empty() + !symbol.empty()) > 1)
		quit_error("oprofpp: must specify only one output type.\n");

	if (output_linenr_info && !list_all_symbols_details && symbol.empty() && !list_symbols)
		quit_error("oprofpp: cannot list debug info without -L, -l or -s option.\n");

	if (show_shared_libs && (!symbol.empty() || !gproffile.empty())) {
		quit_error("oprofpp: you cannot specify --show-shared-libs with --dump-gprof-file or --list-symbol output type.\n");
	}
 
	if (reverse_sort && !list_symbols)
		quit_error("oprofpp: reverse sort can only be used with -l option.\n");
 
	if (show_shared_libs)
		output_format_flags = static_cast<OutSymbFlag>(output_format_flags | osf_image_name);
	if (output_linenr_info)
		output_format_flags = static_cast<OutSymbFlag>(output_format_flags | osf_linenr_info);

	if (output_format.empty()) {
		output_format = "hvspn";
	} else {
		if (!list_symbols && !list_all_symbols_details && symbol.empty()) {
			quit_error("oprofpp: --output-format can be used only without --list-symbols or --list-all-symbols-details.\n");
		}
	}

	if (list_symbols || list_all_symbols_details || !symbol.empty()) {
		OutSymbFlag fl =
			OutputSymbol::ParseOutputOption(output_format);

		if (fl == osf_none) {
			cerr << "oprofpp: invalid --output-format flags.\n";
			OutputSymbol::ShowHelp();
			exit(EXIT_FAILURE);
		}

		output_format_flags = static_cast<OutSymbFlag>(output_format_flags | fl);
	}

	return arg;
}
