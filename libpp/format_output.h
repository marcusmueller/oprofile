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
#include <iosfwd>

#include "format_flags.h"
#include "symbol.h"
#include "profile_container.h"

class symbol_entry;
class sample_entry;
class callgraph_container;
class cg_symbol;

namespace format_output {

/// base class for formater, handle common options to formater
class formatter {
public:
	formatter();
	virtual ~formatter();

	/// add a given column
	void add_format(format_flags flag);

	/// set the need_header boolean to false
	void show_header(bool);
	/// format for 64 bit wide VMAs
	void vma_format_64bit(bool);
	/// show long (full path) filenames
	void show_long_filenames(bool);

	/**
	 * Set the number of collected profile classes. Each class
	 * will output sample count and percentage in extra columns.
	 *
	 * This class assumes that the profile information has been
	 * populated with the right number of classes.
	 */
	void set_nr_classes(size_t nr_classes);

	/// output table header, implemented by calling the virtual function
	/// output_header_field()
	void output_header(std::ostream & out);
protected:
	/// returns the nr of char needed to pad this field
	virtual size_t output_header_field(std::ostream & out, format_flags fl,
		 size_t padding) = 0;

	/// number of profile classes
	size_t nr_classes;

	/// formatting flags set
	format_flags flags;
	/// true if we need to format as 64 bits quantities
	bool vma_64;
	/// false if we use basename(filename) in output rather filename
	bool long_filenames;
	/// true if we need to show header before the before the first output
	bool need_header;
};
 

/// class to output in a columned format symbols and associated samples
class opreport_formatter : public formatter {
public:
	/// build a ready to use formatter
	opreport_formatter(profile_container const & profile);

	/** output a vector of symbols to out according to the output format
	 * specifier previously set by call(s) to add_format() */
	void output(std::ostream & out, symbol_collection const & v);

	/// set the output_details boolean
	void show_details(bool);
	/// use global count rather symbol count for details percent
	void show_global_percent(bool);

private:

	/// data passed for output
	struct field_datum {
		field_datum(symbol_entry const & sym,
		            sample_entry const & s,
			    size_t pc)
			: symbol(sym), sample(s), pclass(pc) {}
		symbol_entry const & symbol;
		sample_entry const & sample;
		size_t pclass;
	};
 
	/// format callback type
	typedef std::string (opreport_formatter::*fct_format)(field_datum const &);
 
	/** @name format functions.
	 * The set of formatting functions, used internally by output().
	 */
	//@{
	std::string format_vma(field_datum const &);
	std::string format_symb_name(field_datum const &);
	std::string format_image_name(field_datum const &);
	std::string format_app_name(field_datum const &);
	std::string format_linenr_info(field_datum const &);
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
 
	typedef std::map<format_flags, field_description> format_map_t;

	/// stores functors for doing actual formatting
	format_map_t format_map;
 
	/** output one symbol symb to out according to the output format
	 * specifier previously set by call(s) to add_format() */
	void output(std::ostream & out, symbol_entry const * symb);

	/// actually do output
	void do_output(std::ostream & out, symbol_entry const & symbol,
		      sample_entry const & sample, bool hide_immutable_field);
 
	/// output details for the symbol
	void output_details(std::ostream & out, symbol_entry const * symb);
 
	/// returns the nr of char needed to pad this field
	size_t output_field(std::ostream & out, field_datum const & datum,
			   format_flags fl, size_t padding,
			   bool hide_immutable);
 
	/// returns the nr of char needed to pad this field
	size_t output_header_field(std::ostream & out, format_flags fl,
				 size_t padding);

	/// container we work from
	profile_container const & profile;
 
	/// total sample count
	count_array_t total_count;
	/// samples so far
	count_array_t cumulated_samples;
	/// percentage so far
	count_array_t cumulated_percent;
	/// detailed total count
	count_array_t total_count_details;
	/// detailed percentage so far
	count_array_t cumulated_percent_details;
	/// true if we need to show details for each symbols
	bool need_details;
	/// bool if details percentage are relative to total count rather to
	/// symbol count
	bool global_percent;
};


/// class to output in a columned format caller/callee and associated samples
class cg_formatter : public formatter {
public:
	/// build a ready to use formatter
	cg_formatter(callgraph_container const & profile);

	/** output callgraph information according to the previously format
	 * specifier set by call(s) to add_format() */
	void output(std::ostream & out);

private:
	/// output a cg_symbol
	void do_output(std::ostream & out, cg_symbol const & symb);

	// FIXME: factorize this with opreport_formatter ?
	/// data passed for output
	struct field_datum {
		field_datum(cg_symbol const & sym, size_t pc)
			: symbol(sym), pclass(pc) {}
		cg_symbol const & symbol;
		size_t pclass;
	};

	/// format callback type
	typedef std::string (cg_formatter::*fct_format)(field_datum const &);

	/** @name format functions.
	 * The set of formatting functions, used internally by output().
	 */
	//@{
	std::string format_vma(field_datum const &);
	std::string format_symb_name(field_datum const &);
	std::string format_image_name(field_datum const &);
	std::string format_app_name(field_datum const &);
	std::string format_linenr_info(field_datum const &);
	std::string format_nr_samples_self(field_datum const &);
	std::string format_percent_self(field_datum const &);
	std::string format_nr_samples_child(field_datum const &);
	std::string format_percent_child(field_datum const &);
	std::string format_nr_samples_self_cumulated(field_datum const &);
	std::string format_percent_self_cumulated(field_datum const &);
	std::string format_nr_samples_child_cumulated(field_datum const &);
	std::string format_percent_child_cumulated(field_datum const &);
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
 
	typedef std::map<format_flags, field_description> format_map_t;

	/// returns the nr of char needed to pad this field
	size_t output_field(std::ostream & out, field_datum const & datum,
			    format_flags fl, size_t padding);

	/// returns the nr of char needed to pad this field
	size_t output_header_field(std::ostream & out, format_flags fl,
				 size_t padding);

	/// stores functors for doing actual formatting
	format_map_t format_map;

	/// total sample count
	count_array_t total_count_self;
	/// total sample count
	count_array_t total_count_callee;
	/// samples so far
	count_array_t cumulated_samples_self;
	/// percentage so far
	count_array_t cumulated_percent_self;
	/// samples so far
	count_array_t cumulated_samples_callee;
	/// percent so far
	count_array_t cumulated_percent_callee;

	/// container we work from
	callgraph_container const & profile;
};

} // namespace format_output 

#endif /* !FORMAT_OUTPUT_H */
