/**
 * @file op_time_options.cpp
 * Options for summary tool
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author Philippe Elie <phil_el@wanadoo.fr>
 * @author John Levon <moz@compsoc.man.ac.uk>
 */

#include "oprofpp.h"
#include "opp_symbol.h"
#include "op_time_options.h"
#include "op_config.h"

#include "popt_options.h"
#include "file_manip.h"
#include "verbose_ostream.h"
 
#include <list>
 
using std::string;
using std::vector;
using std::list;
using std::cerr;
 
namespace options {
	string session;
	string counter_str("0");
	string output_format;
	bool list_symbols;
	bool show_image_name;
	bool demangle;
	vector<string> path;
	vector<string> recursive_path;
	vector<string> exclude_symbols;
	bool reverse_sort;
	bool show_shared_libs;
	int sort_by_counter = -1;
	string samples_dir;
	OutSymbFlag output_format_flags;
	alt_filename_t alternate_filename;
}

namespace {

bool verbose;
 
option options_array[] = {
	option(verbose, "verbose", 'V', "verbose output"),
	option(options::session, "session", 's', "session to use", "name"),
	option(options::counter_str, "counter", 'c', "which counter to use", "counter_nr[,counter_nr]"),
	option(options::output_format, "output-format", 't', "choose the output format", "output-format strings"),
	option(options::list_symbols, "list-symbols", 'l', "list samples by symbol"),
	option(options::show_image_name, "show-image-name", 'n', "show the image name from where come symbols"),
	option(options::path, "path", 'p', "add path for retrieving image", "path_name[,path_name]"),
	option(options::recursive_path, "recursive-path", 'P',
		"add path for retrieving image recursively", "path_name[,path_name]"),
	option(options::reverse_sort, "reverse", 'r', "reverse sort order"),
	option(options::show_shared_libs, "show-shared-libs", 'k',
		"show details for shared libs. Only meaningfull if you have profiled with --separate-samples"),
	option(options::sort_by_counter, "sort", 'C', "which counter to use for sampels sort", "counter nr"),
	option(options::exclude_symbols, "exclude-symbol", 'e', "exclude these comma separated symbols", "symbol_name"),
	option(options::demangle, "demangle", 'd', "demangle GNU C++ symbol names")
};

/// associate filename with directory name where filename exist. Filled
/// through the -p/-P option to allow retrieving of image name when samples
/// file name contains an incorrect location for the image such ram disk
/// module at boot time. We need a multimap to warn against ambiguity between
/// mutiple time found image name.
// FIXME... 

/**
 * add_to_alternate_filename -
 * add all file name below path_name, optionnaly recursively, to the
 * the set of alternative filename used to retrieve image name when
 * a samples image name directory is not accurate
 */
void add_to_alternate_filename(vector<string> const & path_names,
			       bool recursive)
{
	vector<string>::const_iterator path;
	for (path = path_names.begin() ; path != path_names.end() ; ++path) {
		list<string> file_list;
		create_file_list(file_list, *path, "*", recursive);
		list<string>::const_iterator it;
		for (it = file_list.begin() ; it != file_list.end() ; ++it) {
			typedef alt_filename_t::value_type value_t;
			if (recursive) {
				value_t value(basename(*it), dirname(*it));
				options::alternate_filename.insert(value);
			} else {
				value_t value(*it, *path);
				options::alternate_filename.insert(value);
			}
		}
	}
}

/**
 * handle_session_options - derive samples directory
 */
void handle_session_options(void)
{
/*
 * This should eventually be shared amongst all programs
 * to take session names.
 */
	if (options::session.empty()) {
		options::samples_dir = OP_SAMPLES_DIR;
		return;
	}

	if (options::session[0] == '/') {
		options::samples_dir = options::session;
		return;
	}

	options::samples_dir = OP_SAMPLES_DIR + options::session;
}

} // namespace anon
 
verbose_ostream cverb(std::cout);
 
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
 
	string file;
	parse_options(argc, argv, file);

	if (!verbose)
		cverb.go_silent();
 
	if (file.length())
		session = file;

	handle_session_options();
 
	if (output_format.empty()) {
		output_format = "hvspni";
	} else {
		if (!list_symbols) {
			quit_error("op_time: --output-format can be used only with --list-symbols.\n");
		}
	}

	if (exclude_symbols.size() && !list_symbols) {
		quit_error("op_time: --exclude-symbol can be used only with --list-symbols.\n");
	}

	if (list_symbols) {
		OutSymbFlag fl =
			OutputSymbol::ParseOutputOption(output_format);

		if (fl == osf_none) {
			cerr << "op_time: invalid --output-format flags.\n";
			OutputSymbol::ShowHelp();
			exit(EXIT_FAILURE);
		}

		output_format_flags = fl;
	}

	if (show_image_name)
		output_format_flags = static_cast<OutSymbFlag>(output_format_flags | osf_image_name);

	add_to_alternate_filename(path, false);

	add_to_alternate_filename(recursive_path, true);
}
