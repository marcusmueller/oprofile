/**
 * @file op_time_options.h
 * Options for summary tool
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author Philippe Elie
 * @author John Levon
 */

#ifndef OP_TIME_OPTIONS_H
#define OP_TIME_OPTIONS_H

#include <string>
#include <vector>
#include <map>

#include "outsymbflag.h"
#include "derive_files.h"

namespace options {
	/// session name
	extern std::string session;
	/// samples directory
	extern std::string samples_dir;
	/// counter to use
	extern int counter;
	/// output format to use
	extern outsymbflag output_format_flags;
	/// is output_format specified through command line
	extern bool output_format_specified;
	/// which symbols to exclude
	extern std::vector<std::string> exclude_symbols;
	/// container of alternate filename location
	extern alt_filename_t alternate_filename;
	/// whether to do symbol-based summary
	extern bool list_symbols;
	/// whether to show image name
	extern bool show_image_name;
	/// reverse the sort
	extern bool reverse_sort;
	/// show dependent shared library samples
	extern bool show_shared_libs;
	/// counter to sort by
	extern int sort_by_counter;
	/// whether to demangle
	extern bool demangle;
	/// Contains the list of image name for which we request information
	extern std::vector<std::string> filename_filters;
	/// verbose flag
	extern bool verbose;
}

/**
 * get_options - process command line
 * @param argc program arg count
 * @param argv program arg array
 *
 * Process the arguments, fatally complaining on error.
 */
void get_options(int argc, char const * argv[]);

#endif // OP_TIME_OPTIONS_H
