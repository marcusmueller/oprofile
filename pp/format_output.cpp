/**
 * @file format_output.cpp
 * outputting format for symbol lists
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author Philippe Elie
 * @author John Levon
 */

#include <sstream>
#include <iomanip>

#include "file_manip.h"

#include "opp_symbol.h"
#include "format_output.h"
#include "profile_container.h"
#include "demangle_symbol.h"

using namespace std;

namespace options {
	extern bool demangle;
}

struct output_option {
	char option;
	outsymbflag flag;
	string help_string;
};

static output_option const output_options[] = {
	{ 'v', osf_vma, "vma offset" },
	{ 's', osf_nr_samples, "nr samples" },
	{ 'S', osf_nr_samples_cumulated, "nr cumulated samples" },
	{ 'p', osf_percent, "nr percent samples" },
	{ 'P', osf_percent_cumulated, "nr cumulated percent samples" },
	{ 'q', osf_percent_details, "nr percent samples details" },
	{ 'Q', osf_percent_cumulated_details, "nr cumulated percent samples details" },
	{ 'n', osf_symb_name, "symbol name" },
	{ 'l', osf_linenr_info, "source file name and line nr" },
	{ 'L', osf_short_linenr_info, "base name of source file and line nr" },
	{ 'i', osf_image_name, "image name" },
	{ 'I', osf_short_image_name, "base name of image name" },
	{ 'e', osf_app_name, "owning application name" },
	{ 'E', osf_short_app_name, "base name of owning application" },
	{ 'h', osf_header, "header" },
	{ 'd', osf_details, "detailed samples for each selected symbol" }
};

size_t const nr_output_option = sizeof(output_options) / sizeof(output_options[0]);

namespace {

output_option const * find_option(char ch)
{
	for (size_t i = 0 ; i < nr_output_option ; ++i) {
		if (ch == output_options[i].option) {
			return &output_options[i];
		}
	}

	return 0;
}

}

 
namespace format_output {
 
outsymbflag parse_format(string const & option)
{
	size_t flag = 0;
	for (size_t i = 0 ; i < option.length() ; ++i) {
		output_option const * opt = find_option(option[i]);
		if (!opt)
			return osf_none;
		flag |= opt->flag;
	}

	return static_cast<outsymbflag>(flag);
}

void show_help()
{
	for (size_t i = 0 ; i < nr_output_option ; ++i) {
		cerr << output_options[i].option << "\t"
		     << output_options[i].help_string << endl;
	}
}


formatter::formatter(profile_container_t const & profile_container_, int counter_)
	: flags(osf_none), profile_container(profile_container_),
	  counter(counter_), first_output(true)
{
	for (size_t i = 0 ; i < profile_container.get_nr_counters() ; ++i) {
		total_count[i] = profile_container.samples_count(i);
		total_count_details[i] = profile_container.samples_count(i);
		cumulated_samples[i] = 0;
		cumulated_percent[i] = 0;
		cumulated_percent_details[i] = 0;
	}

	format_map[osf_vma] = field_description(9, "vma", &formatter::format_vma);
	format_map[osf_nr_samples] = field_description(9, "samples", &formatter::format_nr_samples);
	format_map[osf_nr_samples_cumulated] = field_description(9, "cum. samples", &formatter::format_nr_cumulated_samples);
	format_map[osf_percent] = field_description(12, "%-age", &formatter::format_percent);
	format_map[osf_percent_cumulated] = field_description(10, "cum %-age", &formatter::format_cumulated_percent);
	format_map[osf_symb_name] = field_description(24, "symbol name", &formatter::format_symb_name);
	format_map[osf_linenr_info] = field_description(28, "linenr info", &formatter::format_linenr_info);
	format_map[osf_short_linenr_info] = field_description(20, "linenr info", &formatter::format_short_linenr_info);
	format_map[osf_image_name] = field_description(24, "image name", &formatter::format_image_name);
	format_map[osf_short_image_name] = field_description(16, "image name", &formatter::format_short_image_name);
	format_map[osf_percent] = field_description(12, "%-age", &formatter::format_percent);
	format_map[osf_percent_cumulated] = field_description(10, "cum %-age", &formatter::format_cumulated_percent);
	format_map[osf_percent_details] = field_description(12, "%-age", &formatter::format_percent_details);
	format_map[osf_percent_cumulated_details] =field_description(10, "cum %-age", &formatter::format_cumulated_percent_details);
	format_map[osf_app_name] = field_description(24, "app name", &formatter::format_app_name);
	format_map[osf_short_app_name] = field_description(16, "app name", &formatter::format_short_app_name);
}

 
void formatter::set_format(outsymbflag flag)
{
	flags = static_cast<outsymbflag>(flags | flag);
}
 

/// describe each possible field of colummned output.
// FIXME: some field have header_name too long (> field_description::width)
// TODO: use % of the screen width here. sum of % equal to 100, then calculate
// ratio between 100 and the selected % to grow non fixed field use also
// lib[n?]curses to get the console width (look info source) (so on add a fixed
// field flags)
size_t formatter::output_field(ostream & out, string const & name,
				 sample_entry const & sample,
				 outsymbflag fl, size_t ctr, size_t padding)
{
	out << string(padding, ' ');
	padding = 0;

	field_description const & field(format_map[fl]);
	string str = (this->*field.formatter)(field_datum(name, sample, ctr));
	out << str;

	padding = 1;	// at least one separator char
	if (str.length() < field.width)
		padding = field.width - str.length();

	return padding;
}

 
size_t formatter::output_header_field(ostream & out, outsymbflag fl,
					size_t padding)
{
	out << string(padding, ' ');
	padding = 0;

	field_description const & field(format_map[fl]);
	out << field.header_name;

	padding = 1;	// at least one separator char
	if (field.header_name.length() < field.width)
		padding = field.width - field.header_name.length();

	return padding;
}
 

void formatter::output(ostream & out, symbol_entry const * symb)
{
	do_output(out, symb->name, symb->sample, flags);

	if (flags & osf_details) {
		output_details(out, symb);
	}
}
 

void formatter::output_details(ostream & out, symbol_entry const * symb)
{
	// We need to save the accumulated count and to restore it on
	// exit so global cumulation and detailed cumulation are separate
	u32 temp_total_count[OP_MAX_COUNTERS];
	u32 temp_cumulated_samples[OP_MAX_COUNTERS];
	u32 temp_cumulated_percent[OP_MAX_COUNTERS];

	for (size_t i = 0 ; i < profile_container.get_nr_counters() ; ++i) {
		temp_total_count[i] = total_count[i];
		temp_cumulated_samples[i] = cumulated_samples[i];
		temp_cumulated_percent[i] = cumulated_percent[i];

		total_count[i] = symb->sample.counter[i];
		cumulated_percent_details[i] -= symb->sample.counter[i];
		cumulated_samples[i] = 0;
		cumulated_percent[i] = 0;
	}

	for (sample_index_t cur = symb->first ; cur != symb->last ; ++cur) {
		out << ' ';

		do_output(out, symb->name, profile_container.get_samples(cur),
			 static_cast<outsymbflag>(flags & osf_details_mask));
	}

	for (size_t i = 0 ; i < profile_container.get_nr_counters() ; ++i) {
		total_count[i] = temp_total_count[i];
		cumulated_samples[i] = temp_cumulated_samples[i];
		cumulated_percent[i] = temp_cumulated_percent[i];
	}
}

 
void formatter::do_output(ostream & out, string const & name,
			    sample_entry const & sample, outsymbflag flag)
{
	output_header(out);

	size_t padding = 0;

	// first output the vma field
	if (flag & osf_vma) {
		padding = output_field(out, name, sample, osf_vma, 0, padding);
	}

	// now the repeated field.
	for (size_t ctr = 0 ; ctr < profile_container.get_nr_counters(); ++ctr) {
		if ((counter & (1 << ctr)) != 0) {
			size_t repeated_flag = (flag & osf_repeat_mask);
			for (size_t i = 1 ; repeated_flag != 0 ; i <<= 1) {
				if ((repeated_flag & i) != 0) {
					outsymbflag fl =
					  static_cast<outsymbflag>(i);
					padding = output_field(out, name,
						sample, fl, ctr, padding);
					repeated_flag &= ~i;
				}
			}
		}
	}

	// now the remaining field
	// vma and repeated field has already been output. It's vital to mask
	// too osf_details|osf_header cause they are not field but rather
	// output modifier.
	int const mask = osf_vma|osf_repeat_mask| osf_details|osf_header;

	// don't be confused between flags member variable and flag passed as
	// parameters, flags show the real requested field whilst flag contain
	// the field which are really output. output flags wich are in flags
	// but not in flag are blank output. this is used to re-use this
	// function for detailed output, see various caller. FIXME: can be
	// re-written by passing rather than outsymbflag flag parameter
	// a bool do_output_details. Also we are trying to put too many things
	// in output flags: osf_header and osf_details would be different
	// boolean parameters rather encoded in flags.
	size_t temp_flag = flag & ~mask;
	size_t true_flags = flags & ~mask;
	for (size_t i = 1 ; temp_flag != 0 ; i <<= 1) {
		outsymbflag fl = static_cast<outsymbflag>(i);
		if ((temp_flag & fl) != 0) {
			padding = output_field(out, name, sample, fl, 0, padding);
			temp_flag &= ~i;
		} else if ((true_flags & fl) != 0) {
			field_description const & field(format_map[fl]);
			padding += field.width;
		}
	}

	out << "\n";
}
 

void formatter::output_header(ostream & out)
{
	if (!first_output) {
		return;
	}

	first_output = false;

	if ((flags & osf_header) == 0) {
		return;
	}

	size_t padding = 0;

	// first output the vma field
	if (flags & osf_vma) {
		padding = output_header_field(out, osf_vma, padding);
	}

	// now the repeated field.
	for (size_t ctr = 0 ; ctr < profile_container.get_nr_counters(); ++ctr) {
		if ((counter & (1 << ctr)) != 0) {
			size_t repeated_flag = (flags & osf_repeat_mask);
			for (size_t i = 1 ; repeated_flag != 0 ; i <<= 1) {
				if ((repeated_flag & i) != 0) {
					outsymbflag fl =
					  static_cast<outsymbflag>(i);
					padding =
					  output_header_field(out, fl, padding);
					repeated_flag &= ~i;
				}
			}
		}
	}

	// now the remaining field
	size_t temp_flag = flags & ~(osf_vma|osf_repeat_mask|osf_details|osf_header);
	for (size_t i = 1 ; temp_flag != 0 ; i <<= 1) {
		if ((temp_flag & i) != 0) {
			outsymbflag fl = static_cast<outsymbflag>(i);
			padding = output_header_field(out, fl, padding);
			temp_flag &= ~i;
		}
	}

	out << "\n";
}

 
void formatter::output(ostream & out,
			  vector<symbol_entry const *> const & symbols,
			  bool reverse)
{
	if (reverse) {
		vector<symbol_entry const *>::const_reverse_iterator it;
		for (it = symbols.rbegin(); it != symbols.rend(); ++it) {
			output(out, *it);
		}
	} else {
		vector<symbol_entry const *>::const_iterator it;
		for (it = symbols.begin(); it != symbols.end(); ++it) {
			output(out, *it);
		}
	}
}

 
string formatter::format_vma(field_datum const & f)
{
	ostringstream out;

	out << hex << setw(8) << setfill('0') << f.sample.vma;

	return out.str();
}

 
string formatter::format_symb_name(field_datum const & f)
{
	if (f.name[0] == '?')
		return "(no symbol)";
	return options::demangle ? demangle_symbol(f.name) : f.name;
}

 
string formatter::format_image_name(field_datum const & f)
{
	return f.sample.file_loc.image_name;
}

 
string formatter::format_short_image_name(field_datum const & f)
{
	return basename(f.sample.file_loc.image_name);
}

string formatter::format_app_name(field_datum const & f)
{
	return f.sample.file_loc.app_name;
}

 
string formatter::format_short_app_name(field_datum const & f)
{
	return basename(f.sample.file_loc.app_name);
}
 
string formatter::format_linenr_info(field_datum const & f)
{
	ostringstream out;

	if (f.sample.file_loc.filename.length()) {
		out << f.sample.file_loc.filename << ":"
		    << f.sample.file_loc.linenr;
	} else {
		out << "(no location information)";
	}

	return out.str();
}

 
string formatter::format_short_linenr_info(field_datum const & f)
{
	ostringstream out;

	if (f.sample.file_loc.filename.length()) {
		out << basename(f.sample.file_loc.filename)
		    << ":" << f.sample.file_loc.linenr;
	} else {
		out << "(no location information)";
	}

	return out.str();
}

 
string formatter::format_nr_samples(field_datum const & f)
{
	ostringstream out;
	out << f.sample.counter[f.ctr];
	return out.str();
}

 
string formatter::format_nr_cumulated_samples(field_datum const & f)
{
	ostringstream out;
	cumulated_samples[f.ctr] += f.sample.counter[f.ctr];
	out << cumulated_samples[f.ctr];
	return out.str();
}

 
string formatter::format_percent(field_datum const & f)
{
	ostringstream out;
	double ratio = op_ratio(f.sample.counter[f.ctr], total_count[f.ctr]);
	out << ratio * 100.0;
	return out.str();
}

 
string formatter::format_cumulated_percent(field_datum const & f)
{
	ostringstream out;
	cumulated_percent[f.ctr] += f.sample.counter[f.ctr];
	double ratio = op_ratio(cumulated_percent[f.ctr], total_count[f.ctr]);
	out << ratio * 100.0;
	return out.str();
}

 
string formatter::format_percent_details(field_datum const & f)
{
	ostringstream out;
	double ratio = op_ratio(f.sample.counter[f.ctr], total_count_details[f.ctr]);
	out << ratio * 100.0;
	return out.str();
}

 
string formatter::format_cumulated_percent_details(field_datum const & f)
{
	ostringstream out;
	cumulated_percent_details[f.ctr] += f.sample.counter[f.ctr];
	double ratio = op_ratio(cumulated_percent_details[f.ctr],
				total_count_details[f.ctr]);
	out << ratio * 100.0;
	return out.str();
}

}; // namespace format_output
