/**
 * @file opp_symbol.cpp
 * Symbol and sample management
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
#include "samples_container.h"
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
	{ 'h', osf_header, "header" },
	{ 'd', osf_details, "detailed samples for each selected symbol" }
};

size_t const nr_output_option = sizeof(output_options) / sizeof(output_options[0]);

static output_option const * find_option(char ch)
{
	for (size_t i = 0 ; i < nr_output_option ; ++i) {
		if (ch == output_options[i].option) {
			return &output_options[i];
		}
	}

	return 0;
}

outsymbflag output_symbol::ParseOutputOption(string const & option)
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

void output_symbol::ShowHelp()
{
	for (size_t i = 0 ; i < nr_output_option ; ++i) {
		cerr << output_options[i].option << "\t"
		     << output_options[i].help_string << endl;
	}
}

typedef string (output_symbol::*fct_format)(string const & symb_name,
					   sample_entry const & symb,
					   size_t ctr);

/// decribe one field of the colummned output.
struct field_description {
	outsymbflag flag;
	size_t width;
	string header_name;
	fct_format formater;
};

/// describe each possible field of colummned output.
// FIXME: some field have header_name too long (> field_description::width)
// TODO: use % of the screen width here. sum of % equal to 100, then calculate
// ratio between 100 and the selected % to grow non fixed field use also
// lib[n?]curses to get the console width (look info source) (so on add a fixed
// field flags)
static field_description const field_descr[] = {
	{ osf_vma, 9,
	  "vma", &output_symbol::format_vma },
	{ osf_nr_samples, 9,
	  "samples", &output_symbol::format_nr_samples },
	{ osf_nr_samples_cumulated, 9,
	  "cum. samples", &output_symbol::format_nr_cumulated_samples },
	{ osf_percent, 12,
	  "%-age", &output_symbol::format_percent },
	{ osf_percent_cumulated, 10,
	  "cum %-age", &output_symbol::format_cumulated_percent },
	{ osf_symb_name, 24,
	  "symbol name", &output_symbol::format_symb_name },
	{ osf_linenr_info, 28, "linenr info",
	  &output_symbol::format_linenr_info },
	{ osf_short_linenr_info, 20, "linenr info",
	  &output_symbol::format_short_linenr_info },
	{ osf_image_name, 24, "image name",
	  &output_symbol::format_image_name },
	{ osf_short_image_name, 16, "image name",
	  &output_symbol::format_short_image_name },
	{ osf_percent, 12,
	  "%-age", &output_symbol::format_percent },
	{ osf_percent_cumulated, 10,
	  "cum %-age", &output_symbol::format_cumulated_percent },
	{ osf_percent_details, 12,
	  "%-age", &output_symbol::format_percent_details },
	{ osf_percent_cumulated_details, 10,
	  "cum %-age", &output_symbol::format_cumulated_percent_details },
};

size_t const nr_field_descr = sizeof(field_descr) / sizeof(field_descr[0]);

output_symbol::output_symbol(samples_container_t const & samples_container_,
			   int counter_)
	:
	flags(osf_none),
	samples_container(samples_container_),
	counter(counter_),
	first_output(true)
{
	for (size_t i = 0 ; i < samples_container.get_nr_counters() ; ++i) {
		total_count[i] = samples_container.samples_count(i);
		total_count_details[i] = samples_container.samples_count(i);
		cumulated_samples[i] = 0;
		cumulated_percent[i] = 0;
		cumulated_percent_details[i] = 0;
	}
}

field_description const * output_symbol::GetFieldDescr(outsymbflag flag)
{
	for (size_t i = 0 ; i < nr_field_descr ; ++i) {
		if (flag == field_descr[i].flag)
			return &field_descr[i];
	}

	return 0;
}

void output_symbol::SetFlag(outsymbflag flag)
{
	flags = static_cast<outsymbflag>(flags | flag);
}

size_t output_symbol::OutputField(ostream & out, string const & name,
				 sample_entry const & sample,
				 outsymbflag fl, size_t ctr, size_t padding)
{
	out << string(padding, ' ');
	padding = 0;

	field_description const * field = GetFieldDescr(fl);
	if (field) {
		string str = (this->*field->formater)(name, sample, ctr);
		out << str;

		padding = 1;	// at least one separator char
		if (str.length() < field->width)
			padding = field->width - str.length();
	}

	return padding;
}

size_t output_symbol::OutputHeaderField(ostream & out, outsymbflag fl,
					size_t padding)
{
	out << string(padding, ' ');
	padding = 0;

	field_description const * field = GetFieldDescr(fl);
	if (field) {
		out << field->header_name;

		padding = 1;	// at least one separator char
		if (field->header_name.length() < field->width)
			padding = field->width - field->header_name.length();
	}

	return padding;
}

void output_symbol::Output(ostream & out, symbol_entry const * symb)
{
	DoOutput(out, symb->name, symb->sample, flags);

	if (flags & osf_details) {
		OutputDetails(out, symb);
	}
}

void output_symbol::OutputDetails(ostream & out, symbol_entry const * symb)
{
	// We need to save the accumulated count and to restore it on
	// exit so global cumulation and detailed cumulation are separate
	u32 temp_total_count[OP_MAX_COUNTERS];
	u32 temp_cumulated_samples[OP_MAX_COUNTERS];
	u32 temp_cumulated_percent[OP_MAX_COUNTERS];

	for (size_t i = 0 ; i < samples_container.get_nr_counters() ; ++i) {
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

		DoOutput(out, symb->name, samples_container.get_samples(cur),
			 static_cast<outsymbflag>(flags & osf_details_mask));
	}

	for (size_t i = 0 ; i < samples_container.get_nr_counters() ; ++i) {
		total_count[i] = temp_total_count[i];
		cumulated_samples[i] = temp_cumulated_samples[i];
		cumulated_percent[i] = temp_cumulated_percent[i];
	}
}

void output_symbol::DoOutput(ostream & out, string const & name,
			    sample_entry const & sample, outsymbflag flag)
{
	OutputHeader(out);

	size_t padding = 0;

	// first output the vma field
	if (flag & osf_vma) {
		padding = OutputField(out, name, sample, osf_vma, 0, padding);
	}

	// now the repeated field.
	for (size_t ctr = 0 ; ctr < samples_container.get_nr_counters(); ++ctr) {
		if ((counter & (1 << ctr)) != 0) {
			size_t repeated_flag = (flag & osf_repeat_mask);
			for (size_t i = 1 ; repeated_flag != 0 ; i <<= 1) {
				if ((repeated_flag & i) != 0) {
					outsymbflag fl =
					  static_cast<outsymbflag>(i);
					padding = OutputField(out, name,
						sample, fl, ctr, padding);
					repeated_flag &= ~i;
				}
			}
		}
	}

	// now the remaining field
	size_t temp_flag = flag & ~(osf_vma|osf_repeat_mask|osf_details|osf_header);
	size_t true_flags = flags & ~(osf_vma|osf_repeat_mask|osf_header);
	for (size_t i = 1 ; temp_flag != 0 ; i <<= 1) {
		outsymbflag fl = static_cast<outsymbflag>(i);
		if ((temp_flag & fl) != 0) {
			padding = OutputField(out, name, sample, fl, 0, padding);
			temp_flag &= ~i;
		} else if ((true_flags & fl) != 0) {
			field_description const * field = GetFieldDescr(fl);
			if (field) {
				padding += field->width;
			}
		}
	}

	out << "\n";
}

void output_symbol::OutputHeader(ostream & out)
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
		padding = OutputHeaderField(out, osf_vma, padding);
	}

	// now the repeated field.
	for (size_t ctr = 0 ; ctr < samples_container.get_nr_counters(); ++ctr) {
		if ((counter & (1 << ctr)) != 0) {
			size_t repeated_flag = (flags & osf_repeat_mask);
			for (size_t i = 1 ; repeated_flag != 0 ; i <<= 1) {
				if ((repeated_flag & i) != 0) {
					outsymbflag fl =
					  static_cast<outsymbflag>(i);
					padding =
					  OutputHeaderField(out, fl, padding);
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
			padding = OutputHeaderField(out, fl, padding);
			temp_flag &= ~i;
		}
	}

	out << "\n";
}

void output_symbol::Output(ostream & out,
			  vector<symbol_entry const *> const & symbols,
			  bool reverse)
{
	if (reverse) {
		vector<symbol_entry const *>::const_reverse_iterator it;
		for (it = symbols.rbegin(); it != symbols.rend(); ++it) {
			Output(out, *it);
		}
	} else {
		vector<symbol_entry const *>::const_iterator it;
		for (it = symbols.begin(); it != symbols.end(); ++it) {
			Output(out, *it);
		}
	}
}

string output_symbol::format_vma(string const &,
				sample_entry const & sample, size_t)
{
	ostringstream out;

	out << hex << setw(8) << setfill('0') << sample.vma;

	return out.str();
}

string output_symbol::format_symb_name(string const & name,
				      sample_entry const &, size_t)
{
	if (name[0] == '?')
		return "(no symbol)";
	return options::demangle ? demangle_symbol(name) : name;
}

string output_symbol::format_image_name(string const &,
				       sample_entry const & sample, size_t)
{
	return sample.file_loc.image_name;
}

string output_symbol::format_short_image_name(string const &,
					     sample_entry const & sample,
					     size_t)
{
	return basename(sample.file_loc.image_name);
}

string output_symbol::format_linenr_info(string const &,
					sample_entry const & sample, size_t)
{
	ostringstream out;

	if (sample.file_loc.filename.length()) {
		out << sample.file_loc.filename << ":"
		    << sample.file_loc.linenr;
	} else {
		out << "(no location information)";
	}

	return out.str();
}

string output_symbol::format_short_linenr_info(string const &,
					      sample_entry const & sample,
					      size_t)
{
	ostringstream out;

	if (sample.file_loc.filename.length()) {
		out << basename(sample.file_loc.filename)
		    << ":" << sample.file_loc.linenr;
	} else {
		out << "(no location information)";
	}

	return out.str();
}

string output_symbol::format_nr_samples(string const &,
				       sample_entry const & sample, size_t ctr)
{
	ostringstream out;

	out << sample.counter[ctr];

	return out.str();
}

string output_symbol::format_nr_cumulated_samples(string const &,
						 sample_entry const & sample,
						 size_t ctr)
{
	ostringstream out;

	cumulated_samples[ctr] += sample.counter[ctr];

	out << cumulated_samples[ctr];

	return out.str();
}

string output_symbol::format_percent(string const &,
				    sample_entry const & sample, size_t ctr)
{
	ostringstream out;

	double ratio = total_count[ctr]
		? double(sample.counter[ctr]) / total_count[ctr]
		: 0.0;

	out << ratio * 100.0;

	return out.str();
}

string output_symbol::format_cumulated_percent(string const &,
					      sample_entry const & sample,
					      size_t ctr)
{
	ostringstream out;

	cumulated_percent[ctr] += sample.counter[ctr];

	double ratio = total_count[ctr]
		? double(cumulated_percent[ctr]) / total_count[ctr]
		: 0.0;

	out << ratio * 100.0;

	return out.str();
}

string output_symbol::format_percent_details(string const &,
				    sample_entry const & sample, size_t ctr)
{
	ostringstream out;

	double ratio = total_count_details[ctr]
		? double(sample.counter[ctr]) / total_count_details[ctr]
		: 0.0;

	out << ratio * 100.0;

	return out.str();
}

string output_symbol::format_cumulated_percent_details(string const &,
					      sample_entry const & sample,
					      size_t ctr)
{
	ostringstream out;

	cumulated_percent_details[ctr] += sample.counter[ctr];

	double ratio = total_count_details[ctr]
		? double(cumulated_percent_details[ctr]) / total_count_details[ctr]
		: 0.0;

	out << ratio * 100.0;

	return out.str();
}
