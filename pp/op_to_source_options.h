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

/// command line option values
namespace options {
	/// sample file to work on
	extern std::string sample_file;
	/// image file to work on
	extern std::string image_file;
	/// show files with more than this nr. of samples 
	extern int with_more_than_samples;
	/// FIXME: doc 
	extern int until_more_than_samples;
	/// counter to sort by
	extern int sort_by_counter;
	/// top-level source directory
	extern std::string source_dir;
	/// top-level output directory
	extern std::string output_dir;
	/// comma-separated mask of files to output
	extern std::string output_filter;
	/// comma-separated mask of files to not output
	extern std::string no_output_filter;
	/// which symbols to exclude
	extern std::vector<std::string> exclude_symbols;
	/// whether to generate assembly
	extern bool assembly;
	/// whether to generate mixed source/assembly 
	extern bool source_with_assembly;
	/// whether to demangle
	extern bool demangle;
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
