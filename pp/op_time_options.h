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

/// the type used to store alternative location of binary image. We need a
/// multimap to warn against ambiguity between mutiple time found image name.
/// \sa options::alternate_filename
typedef std::multimap<std::string, std::string> alt_filename_t;

namespace options {
	/// session name
	extern std::string session;
	/// samples directory
	extern std::string samples_dir;
	/// counter to use
	extern int counter;
	/// output format to use
	extern outsymbflag output_format_flags;
	/// which symbols to exclude
	extern std::vector<std::string> exclude_symbols;
	/// filled through the --path or --recursive-path allowing to specify
	/// alternate location for binary image, this occur when when samples
        /// filename contains an incorrect location for binary image name such
	/// ram disk module at boot time
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
	extern std::vector<string> filename_filters;
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
