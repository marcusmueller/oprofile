/**
 * @file opp_symbol.h
 * Symbol and sample management
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author Philippe Elie <phil_el@wanadoo.fr>
 * @author John Levon <moz@compsoc.man.ac.uk>
 */

#ifndef OPP_SYMBOL_H
#define OPP_SYMBOL_H

#include <string>
#include <vector>
#include <iostream>

#include <bfd.h>	/* for bfd_vma */

#include "counter_array.h"
#include "outsymbflag.h"

class samples_container_t;
class field_description;

typedef size_t sample_index_t;

//---------------------------------------------------------------------------
/// A simple container for a fileno:linr location.
struct file_location {
	/// From where image come this file location
	std::string image_name;
	/// empty if not valid.
	std::string filename;
	/// 0 means invalid or code is generated internally by the compiler
	int linenr;

	bool operator<(file_location const & rhs) const {
		return filename < rhs.filename ||
			(filename == rhs.filename && linenr < rhs.linenr);
	}
};

//---------------------------------------------------------------------------
/// associate vma address with a file location and a samples count
struct sample_entry {
	/// From where file location comes the samples
	file_location file_loc;
	/// From where virtual memory address comes the samples
	bfd_vma vma;
	/// the samples count
	counter_array_t counter;
};

//---------------------------------------------------------------------------
/// associate a symbol with a file location, samples count and vma address
struct symbol_entry {
	/// file location, vma and cumulated samples count for this symbol
	sample_entry sample;
	/// name of symbol
	std::string name;
	/// [first, last[ gives the range of sample_entry.
	sample_index_t first;
	sample_index_t last;
};

// FIXME: I am rather dubious this should be a class..
 
/**
 * class to output in a columned format symbols and associated samples
 */
class output_symbol {
public:
	/// build an output_symbol object, the samples_container_t life time
	/// object must be > of the life time of the output_symbol object.
	output_symbol(samples_container_t const & samples_container, int counter);

	/// convenience to set output options flags w/o worrying about cast
	void SetFlag(outsymbflag flag);

	/** output one symbol symb to out according to the output format
	 * specifier previously set by call(s) to SetFlag() */
	void Output(std::ostream & out, symbol_entry const * symb);
	/** output a vector of symbols to out according to the output format
	 * specifier previously set by call(s) to SetFlag() */
	void Output(std::ostream & out,
		    std::vector<symbol_entry const *> const & v, bool reverse);

	/** output to stdout the formating options available */
	static void ShowHelp();

	/** return osf_none if the option string is ill formed, so you can call
	 * output_symbol::ShowHelp() to notify user on available options */
	static outsymbflag ParseOutputOption(std::string const & option);

	/** @name format functions.
	 * The set of formatting functions, used internally by Output().
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
	//@}
private:
	void DoOutput(std::ostream & out, std::string const & name,
		      sample_entry const & sample, outsymbflag flags);
	void OutputDetails(std::ostream & out, symbol_entry const * symb);
	void OutputHeader(std::ostream & out);
	// return the nr of char needed to padd this field
	size_t OutputField(std::ostream & out, std::string const & name,
			   sample_entry const & sample,
			   outsymbflag fl, size_t ctr);
	// return the nr of char needed to padd this field
	size_t OutputHeaderField(std::ostream & out, outsymbflag fl);
	static field_description const * GetFieldDescr(outsymbflag flag);

	outsymbflag flags;
	samples_container_t const & samples_container;
	u32 total_count[OP_MAX_COUNTERS];
	u32 cumulated_samples[OP_MAX_COUNTERS];
	u32 cumulated_percent[OP_MAX_COUNTERS];
	int counter;
	bool first_output;
};

#endif /* !OPP_SYMBOL_H */
