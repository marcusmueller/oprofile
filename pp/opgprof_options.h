/**
 * @file opgprof_options.h
 * Options for opgprof tool
 *
 * @remark Copyright 2003 OProfile authors
 * @remark Read the file COPYING
 *
 * @author John Levon
 * @author Philippe Elie
 */

#ifndef OPGPROF_OPTIONS_H
#define OPGPROF_OPTIONS_H

#include <string>

#include "utility.h"
#include "common_option.h"

namespace options {
	extern std::string gmon_filename;
}

class inverted_profile;

/// a set of sample filenames to handle.
extern inverted_profile image_profile;

/**
 * handle_options - process command line
 * @param non_options vector of non options string
 *
 * Process the arguments, fatally complaining on error.
 */
void handle_options(std::vector<std::string> const & non_options);

#endif // OPGPROF_OPTIONS_H
