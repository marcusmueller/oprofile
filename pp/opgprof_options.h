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

class partition_files;

namespace options {
	extern std::string gmon_filename;
}

/**
 * a partition of sample filename to treat, each sub-list is a list of
 * sample to merge. filled by handle_options()
 */
extern scoped_ptr<partition_files> sample_file_partition;

/**
 * handle_options - process command line
 * @param non_options vector of non options string
 *
 * Process the arguments, fatally complaining on error.
 */
void handle_options(std::vector<std::string> const & non_options);

#endif // OPGPROF_OPTIONS_H
