/**
 * @file op_to_source_options.h
 * Command-line options for op_to_source
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author John Levon <moz@compsoc.man.ac.uk>
 * @author Philippe Elie <phil_el@wanadoo.fr>
 */
 
#ifndef OP_TO_SOURCE_OPTIONS_H
#define OP_TO_SOURCE_OPTIONS_H

#include <string>
#include <vector>

// FIXME

/// command line option values
namespace options {
	extern int with_more_than_samples;
	extern int until_more_than_samples;
	extern int sort_by_counter;
	extern std::string source_dir;
	extern std::string output_dir;
	extern std::string output_filter;
	extern std::string no_output_filter;
	extern bool assembly;
	extern bool source_with_assembly;
	/** control the behavior of verbprintf() */
	extern bool verbose;
	/** control the behavior of demangle_symbol() */
	extern bool demangle;
	/** a sample filename */
	extern std::string samplefile;
	/** an image filename */
	extern std::string imagefile;
	/** the set of symbols to ignore */
	extern std::vector<std::string> exclude_symbols;
};

/**
 * get_options - process command line
 * @param argc program arg count
 * @param argv program arg array
 * @return an additional argument
 *
 * Process the arguments, fatally complaining on
 * error. This fills the values in the above options
 * namespace.
 */
std::string const get_options(int argc, char const **argv);
 
#endif // OP_TO_SOURCE_OPTIONS_H
