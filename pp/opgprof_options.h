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

class profile_set;

namespace options {
	extern std::string gmon_filename;
}

/// a set of sample filenames to handle.
extern profile_set profiles;

/**
 * handle_options - process command line
 * @param non_options vector of non options string
 *
 * Process the arguments, fatally complaining on error.
 */
void handle_options(std::vector<std::string> const & non_options);

#endif // OPGPROF_OPTIONS_H
