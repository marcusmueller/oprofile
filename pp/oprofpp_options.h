/**
 * @file oprofpp_options.h
 * Command-line options
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author John Levon
 * @author Philippe Elie
 */

#ifndef OPROFPP_OPTIONS_H
#define OPROFPP_OPTIONS_H

#include <string>
#include <vector>

#include "outsymbflag.h"

namespace options {
	/// sample file to work on
	extern std::string sample_file;
	/// image file to work on
	extern std::string image_file;
	/// gprof file to output to
	extern std::string gprof_file;
	/// symbol to show in detail
	extern std::string symbol;
	/// mask of counters to show
	extern int counter_mask;
	/// counters to sort by
	extern int sort_by_counter;
	/// show symbol summary
	extern bool list_symbols;
	/// show all symbol's details
	extern bool list_all_symbols_details;
	/// show debug info
	extern bool output_linenr_info;
	/// reverse sort
	extern bool reverse_sort;
	/// show shared library symbols
	extern bool show_shared_libs;
	/// demangle symbols
	extern bool demangle;
	/// what format to output
	extern outsymbflag output_format_flags;
	/// symbols to exclude
	extern std::vector<std::string> exclude_symbols;
};

/**
 * get_options - process command line
 * @param argc program arg count
 * @param argv program arg array
 * @return an additional argument
 *
 * Process the arguments, fatally complaining on
 * error. This fills the values in the above options
 * namespace.
 */
std::string const get_options(int argc, char const **argv);

#endif // OPROFPP_OPTIONS_H
