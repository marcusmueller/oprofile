/**
 * @file common_option.h
 * Declaration of entry point of pp tools, implementation file add common
 * options of pp tools and some miscelleaneous functions
 *
 * @remark Copyright 2003 OProfile authors
 * @remark Read the file COPYING
 *
 * @author Philippe Elie
 */

#ifndef COMMON_OPTION_H
#define COMMON_OPTION_H

#include "locate_images.h"
#include "demangle_symbol.h"

namespace options {
	extern bool verbose;
	extern extra_images extra_found_images;
	extern double threshold;
};

/**
 * prototype of a pp tool entry point. This entry point is called
 * by run_pp_tool
 */
typedef int (*pp_fct_run_t)(std::vector<std::string> const & non_options);

/**
 * @param argc  command line number of argument
 * @param argv  command line argument pointer array
 * @param fct  function to run to start this pp tool
 *
 * Provide a common entry to all pp tools, parsing all options, handling
 * common options and providing the necessary try catch clause
 */
int run_pp_tool(int argc, char const * argv[], pp_fct_run_t fct);

/**
 * @param option one of [smart,none,normal]
 *
 * return the demangle_type of option or throw an exception if option
 * is not valid.
 */
demangle_type handle_demangle_option(string const & option);

#endif /* !COMMON_OPTION_H */
