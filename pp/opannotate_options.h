/**
 * @file opannotate_options.h
 * Options for opannotate tool
 *
 * @remark Copyright 2003 OProfile authors
 * @remark Read the file COPYING
 *
 * @author John Levon
 * @author Philippe Elie
 */

#ifndef OPANNOTATE_OPTIONS_H
#define OPANNOTATE_OPTIONS_H

#include <string>
#include <vector>

#include "utility.h"
#include "common_option.h"
#include "string_filter.h"
#include "path_filter.h"

class partition_files;

namespace options {
	extern bool demangle;
	extern bool smart_demangle;
	extern bool source;
	extern bool assembly;
	extern string_filter symbol_filter;
	extern path_filter file_filter;
	extern std::string output_dir;
	extern std::string source_dir;
	extern std::vector<std::string> objdump_params;
	extern double threshold;
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

#endif // OPANNOTATE_OPTIONS_H