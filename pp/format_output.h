/**
 * @file format_output.h
 * outputting format for symbol lists
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author Philippe Elie
 * @author John Levon
 */

#ifndef FORMAT_OUTPUT_H
#define FORMAT_OUTPUT_H

//#include "config.h"

#include <string>
//#include <vector>
//#include <iostream>

//#include <bfd.h>	/* for bfd_vma */

//#include "counter_array.h"
#include "outsymbflag.h"

class samples_container_t;
class field_description;
class symbol_entry;

namespace format_output {
 
/// output to stdout the formatting options available
void show_help();

/** return osf_none if the option string is ill formed, so you can call
 * output_symbol::show_help() to notify user on available options */
outsymbflag parse_format(std::string const & option);

/// class to output in a columned format symbols and associated samples
class formatter {
public:
	/// build an output_symbol object, the samples_container_t life time
	/// object must be > of the life time of the output_symbol object.
	formatter(samples_container_t const & samples_container, int counter);

	/// convenience to set output options flags w/o worrying about cast
	void set_format(outsymbflag flag);

	/** output one symbol symb to out according to the output format
	 * specifier previously set by call(s) to set_format() */
	void output(std::ostream & out, symbol_entry const * symb);
	/** output a vector of symbols to out according to the output format
	 * specifier previously set by call(s) to set_format() */
	void output(std::ostream & out,
		    std::vector<symbol_entry const *> const & v, bool reverse);

	/** @name format functions.
	 * The set of formatting functions, used internally by output().
	 * Exposed as public because we need to use them in an array of
	 * pointer to member function
	 */
	//@{
	std::string format_vma(std::string const & name,
			       sample_entry const & sample, size_t);
	std::string format_symb_name(std::string const & name,
				     sample_entry const & sample, size_t);
	std::string format_image_name(std::string const & name,
				      sample_entry const & sample, size_t);
	std::string format_short_image_name(std::string const & name,
					    sample_entry const & sample,
					    size_t);
	std::string format_linenr_info(std::string const & name,
				       sample_entry const & sample, size_t);
	std::string format_short_linenr_info(std::string const & name,
					     sample_entry const & sample,
					     size_t);
	std::string format_nr_samples(std::string const & name,
				      sample_entry const & sample, size_t ctr);
	std::string format_nr_cumulated_samples(std::string const & name,
					sample_entry const & sample, size_t);
	std::string format_percent(std::string const & name,
				   sample_entry const & sample, size_t);
	std::string format_cumulated_percent(std::string const & name,
					     sample_entry const & sample,
					     size_t);
	std::string format_percent_details(std::string const & name,
				   sample_entry const & sample, size_t);
	std::string format_cumulated_percent_details(std::string const & name,
					     sample_entry const & sample,
					     size_t);
	//@}
private:
	void do_output(std::ostream & out, std::string const & name,
		      sample_entry const & sample, outsymbflag flags);
	void output_details(std::ostream & out, symbol_entry const * symb);
	void output_header(std::ostream & out);
	// returns the nr of char needed to pad this field
	size_t output_field(std::ostream & out, std::string const & name,
			   sample_entry const & sample,
			   outsymbflag fl, size_t ctr, size_t padding);
	// returns the nr of char needed to pad this field
	size_t output_header_field(std::ostream & out, outsymbflag fl,
				 size_t padding);

	outsymbflag flags;
	samples_container_t const & samples_container;
	u32 total_count[OP_MAX_COUNTERS];
	u32 cumulated_samples[OP_MAX_COUNTERS];
	u32 cumulated_percent[OP_MAX_COUNTERS];
	u32 total_count_details[OP_MAX_COUNTERS];
	u32 cumulated_percent_details[OP_MAX_COUNTERS];
	int counter;
	bool first_output;
};

}; // namespace format_output 
 
#endif /* !FORMAT_OUTPUT_H */
