/**
 * @file op_time_options.h
 * Options for summary tool
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author Philippe Elie <phil_el@wanadoo.fr>
 * @author John Levon <moz@compsoc.man.ac.uk>
 */

#ifndef OP_TIME_OPTIONS_H
#define OP_TIME_OPTIONS_H
 
#include <string>
#include <vector>
#include <map>

#include "outsymbflag.h"
 
typedef std::multimap<std::string, std::string> alt_filename_t;
 
namespace options {
	extern std::string session;
	extern std::string counter_str;
	extern std::string output_format;
	extern bool list_symbols;
	extern bool show_image_name;
	extern std::vector<std::string> path;
	extern std::vector<std::string> recursive_path;
	extern bool reverse_sort;
	extern bool show_shared_libs;
	extern int sort_by_counter;
	extern std::string samples_dir;
	extern int counter;
	extern OutSymbFlag output_format_flags;
	extern bool demangle;
	extern std::vector<std::string> exclude_symbols;
	extern alt_filename_t alternate_filename;
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
