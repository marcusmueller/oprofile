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

#include "config.h"

#include <string>
#include <map>
#include <vector>
#include <iosfwd>

#include "counter_array.h"
#include "outsymbflag.h"

class profile_container_t;
class symbol_entry;

namespace format_output {
 
/// output to out the formatting options available
void show_help(std::ostream & out);

/** return osf_none if the option string is ill formed, so you can call
 * output_symbol::show_help() to notify user on available options */
outsymbflag parse_format(std::string const & option);

/// class to output in a columned format symbols and associated samples
class formatter {
public:
	/// build an output_symbol object, the profile_container_t life time
	/// object must be > of the life time of the output_symbol object.
	formatter(profile_container_t const & profile_container, int counter);

	/// convenience to set output options flags w/o worrying about cast
	void set_format(outsymbflag flag);

	/** output one symbol symb to out according to the output format
	 * specifier previously set by call(s) to set_format() */
	void output(std::ostream & out, symbol_entry const * symb);
	/** output a vector of symbols to out according to the output format
	 * specifier previously set by call(s) to set_format() */
	void output(std::ostream & out,
		    std::vector<symbol_entry const *> const & v, bool reverse);

private:

	/// data passed for output
	struct field_datum {
		field_datum(std::string const & n, sample_entry const & s, size_t c)
			: name(n), sample(s), ctr(c) {}
		std::string const & name;
		sample_entry const & sample;
		size_t ctr;
	};
 
	/// format callback type
	typedef std::string (formatter::*fct_format)(field_datum const &);
 
	/** @name format functions.
	 * The set of formatting functions, used internally by output().
	 */
	//@{
	std::string format_vma(field_datum const &);
	std::string format_symb_name(field_datum const &);
	std::string format_image_name(field_datum const &);
	std::string format_short_image_name(field_datum const &);
	std::string format_app_name(field_datum const &);
	std::string format_short_app_name(field_datum const &);
	std::string format_linenr_info(field_datum const &);
	std::string format_short_linenr_info(field_datum const &);
	std::string format_nr_samples(field_datum const &);
	std::string format_nr_cumulated_samples(field_datum const &);
	std::string format_percent(field_datum const &);
	std::string format_cumulated_percent(field_datum const &);
	std::string format_percent_details(field_datum const &);
	std::string format_cumulated_percent_details(field_datum const &);
	//@}
 
	/// decribe one field of the colummned output.
	struct field_description {
		field_description() {}
		field_description(std::size_t w, std::string h, fct_format f)
			: width(w), header_name(h), formatter(f) {}
 
		std::size_t width;
		std::string header_name;
		fct_format formatter;
	};
 
	typedef std::map<outsymbflag, field_description> format_map_t;

	/// stores functors for doing actual formatting
	format_map_t format_map;
 
	/// actually do output
	void do_output(std::ostream & out, std::string const & name,
		      sample_entry const & sample, outsymbflag flags);
 
	/// output details for the symbol
	void output_details(std::ostream & out, symbol_entry const * symb);
 
	/// output table header
	void output_header(std::ostream & out);
 
	/// returns the nr of char needed to pad this field
	size_t output_field(std::ostream & out, std::string const & name,
			   sample_entry const & sample,
			   outsymbflag fl, size_t ctr, size_t padding);
 
	/// returns the nr of char needed to pad this field
	size_t output_header_field(std::ostream & out, outsymbflag fl,
				 size_t padding);

	/// formatting flags set
	outsymbflag flags;
 
	/// container we work from
	profile_container_t const & profile_container;
 
	/// total sample count
	u32 total_count[OP_MAX_COUNTERS];
	/// samples so far
	u32 cumulated_samples[OP_MAX_COUNTERS];
	/// percentage so far
	u32 cumulated_percent[OP_MAX_COUNTERS];
	/// detailed total count
	u32 total_count_details[OP_MAX_COUNTERS];
	/// detailed percentage so far
	u32 cumulated_percent_details[OP_MAX_COUNTERS];
	/// counter to use
	int counter;
	/// used for outputting header
	bool first_output;
};

}; // namespace format_output 
 
#endif /* !FORMAT_OUTPUT_H */
