/**
 * @file opstack_options.h
 * Options for opstack tool
 *
 * @remark Copyright 2004 OProfile authors
 * @remark Read the file COPYING
 *
 * @author John Levon
 * @author Philippe Elie
 */

#ifndef OPSTACK_OPTIONS_H
#define OPSTACK_OPTIONS_H

#include "common_option.h"
#include "symbol_sort.h"

class profile_classes;

/// All the chosen sample files.
extern profile_classes classes;

namespace options {
	extern std::string archive_path;
	extern demangle_type demangle;
	extern merge_option merge_by;
	extern sort_options sort_by;
	extern bool reverse_sort;
	extern bool long_filenames;
	extern bool show_header;
	extern bool show_address;
	extern bool debug_info;
	extern bool accumulated;
}

/**
 * handle_options - process command line
 * @param non_options vector of non options string
 *
 * Process the arguments, fatally complaining on error.
 */
void handle_options(std::vector<std::string> const & non_options);

#endif // OPSTACK_OPTIONS_H
