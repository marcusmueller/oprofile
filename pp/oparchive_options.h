/**
 * @file oparchive_options.h
 * Options for oparchive tool
 *
 * @remark Copyright 2003 OProfile authors
 * @remark Read the file COPYING
 *
 * @author Will Cohen
 * @author Philippe Elie
 */

#ifndef OPARCHIVE_OPTIONS_H
#define OPARCHIVE_OPTIONS_H

#include "common_option.h"

class profile_classes;
class merge_option;

namespace options {
	extern std::string archive_path;
	extern bool exclude_dependent;
	extern merge_option merge_by;
	extern std::string outdirectory;
}

/// All the chosen sample files.
extern profile_classes classes;
extern std::list<std::string> sample_files;

/**
 * handle_options - process command line
 * @param non_options vector of non options string
 *
 * Process the arguments, fatally complaining on error.
 */
void handle_options(std::vector<std::string> const & non_options);

#endif // OPARCHIVE_OPTIONS_H
