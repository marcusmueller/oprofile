/**
 * @file opp_symbol.cpp
 * Symbol and sample management
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author Philippe Elie <phil_el@wanadoo.fr>
 * @author John Levon <moz@compsoc.man.ac.uk>
 */

#include <sstream>
#include <iomanip>

#include "file_manip.h"

#include "opp_symbol.h"
#include "opf_filter.h"

using std::string;
using std::cout;
using std::cerr;
using std::vector;
using std::ostream;
using std::ostringstream;
using std::endl;

struct output_option {
	char option;
	OutSymbFlag flag;
	char const * help_string;
};

static const output_option output_options[] = {
	{ 'v', osf_vma, "vma offset" },
	{ 's', osf_nr_samples, "nr samples" },
	{ 'S', osf_nr_samples_cumulated, "nr cumulated samples" },
	{ 'p', osf_percent, "nr percent samples" },
	{ 'P', osf_percent_cumulated, "nr cumulated percent samples" },
//	{ 't', osf_time, "time" },
//	{ 'T', ofs_time_cumulated, "cumulated time" },
	{ 'n', osf_symb_name, "symbol name" },
//	{ 'd', osf_demangle, "demangle symbol" },
	{ 'l', osf_linenr_info, "source file name and line nr" },
	{ 'L', osf_short_linenr_info, "base name of source file and line nr" },
	{ 'i', osf_image_name, "image name" },
	{ 'I', osf_short_image_name, "base name of image name" },
	{ 'h', osf_header, "header" },
	{ 'd', osf_details, "detailed samples for each selected symbol" }
};

const size_t nr_output_option = sizeof(output_options) / sizeof(output_options[0]);

static const output_option * find_option(char ch)
{
	for (size_t i = 0 ; i < nr_output_option ; ++i) {
		if (ch == output_options[i].option) {
			return &output_options[i];
		}
	}

	return 0;
}

OutSymbFlag OutputSymbol::ParseOutputOption(const string & option)
{
	size_t flag = 0;
	for (size_t i = 0 ; i < option.length() ; ++i) {
		const output_option * opt = find_option(option[i]);
		if (!opt)
			return osf_none;
		flag |= opt->flag;
	}

	return static_cast<OutSymbFlag>(flag);
}

void OutputSymbol::ShowHelp()
{
	for (size_t i = 0 ; i < nr_output_option ; ++i) {
		cerr << output_options[i].option << "\t" 
		     << output_options[i].help_string << endl;
	}
}

typedef string (OutputSymbol::*fct_format)(const string & symb_name,
					   const sample_entry & symb,
					   size_t ctr);

/// decribe one field of the colummned output.
struct field_description {
	OutSymbFlag flag;
	size_t width;
	char const * header_name;
	fct_format formater;
};

/// describe each possible field of colummned output.
// FIXME: some field have header_name too long (> field_description::width)
// TODO: use % of the screen width here. sum of % equal to 100, then calculate
// ratio between 100 and the selected % to grow non fixed field use also
// lib[n?]curses to get the console width (look info source) (so on add a fixed
// field flags)
static const field_description field_descr[] = {
	{ osf_vma, 9,
	  "vma", &OutputSymbol::format_vma },
	{ osf_nr_samples, 9,
	  "samples", &OutputSymbol::format_nr_samples },
	{ osf_nr_samples_cumulated, 9,
	  "cum. samples", &OutputSymbol::format_nr_cumulated_samples },
	{ osf_percent, 12,
	  "%-age", &OutputSymbol::format_percent },
	{ osf_percent_cumulated, 10,
	  "cum %-age", &OutputSymbol::format_cumulated_percent },
// future	{ osf_time, 12, "time", 0 },
// future	{ osf_time_cumulated, 12, "cumul time", 0 },
	{ osf_symb_name, 24,
	  "symbol name", &OutputSymbol::format_symb_name },
	{ osf_linenr_info, 28, "linenr info",
	  &OutputSymbol::format_linenr_info },
	{ osf_short_linenr_info, 20, "linenr info",
	  &OutputSymbol::format_short_linenr_info },
	{ osf_image_name, 24, "image name",
	  &OutputSymbol::format_image_name },
	{ osf_short_image_name, 16, "image name",
	  &OutputSymbol::format_short_image_name },
};

const size_t nr_field_descr = sizeof(field_descr) / sizeof(field_descr[0]);

OutputSymbol::OutputSymbol(const samples_files_t & samples_files_,
			   int counter_)
	:
	flags(osf_none),
	samples_files(samples_files_),
	counter(counter_),
	first_output(true)
{
	for (size_t i = 0 ; i < samples_files.get_nr_counters() ; ++i) {
		total_count[i] = samples_files.samples_count(i);
		cumulated_samples[i] = 0;
		cumulated_percent[i] = 0;
	}
}

const field_description * OutputSymbol::GetFieldDescr(OutSymbFlag flag)
{
	for (size_t i = 0 ; i < nr_field_descr ; ++i) {
		if (flag == field_descr[i].flag)
			return &field_descr[i];
	}

	return 0;
}

void OutputSymbol::SetFlag(OutSymbFlag flag)
{
	flags = static_cast<OutSymbFlag>(flags | flag);
}

size_t OutputSymbol::OutputField(std::ostream & out, const std::string & name,
			       const sample_entry & sample,
			       OutSymbFlag fl, size_t ctr)
{
	size_t sz = 0;
	const field_description * field = GetFieldDescr(fl);
	if (field) {
		string str = (this->*field->formater)(name, sample, ctr);
		out << str;

		sz = 1;	// at least one separator char
		if (str.length() < field->width)
			sz = field->width - str.length();
	}

	return sz;
}

size_t OutputSymbol::OutputHeaderField(std::ostream & out, OutSymbFlag fl)
{
	size_t sz = 0;
	const field_description * field = GetFieldDescr(fl);
	if (field) {
		out << field->header_name;

		sz = 1;	// at least one separator char
		if (strlen(field->header_name) < field->width)
			sz = field->width - strlen(field->header_name);
	}

	return sz;
}

void OutputSymbol::Output(std::ostream & out, const symbol_entry * symb)
{
	DoOutput(out, symb->name, symb->sample, flags);

	if (flags & osf_details) {
		OutputDetails(out, symb);
	}
}

void OutputSymbol::OutputDetails(std::ostream & out, const symbol_entry * symb)
{
	// We need to save the accumulated count and to restore it on
	// exit so global cumulation and detailed cumulation are separate
	u32 temp_total_count[OP_MAX_COUNTERS];
	u32 temp_cumulated_samples[OP_MAX_COUNTERS];
	u32 temp_cumulated_percent[OP_MAX_COUNTERS];

	for (size_t i = 0 ; i < samples_files.get_nr_counters() ; ++i) {
		temp_total_count[i] = total_count[i];
		temp_cumulated_samples[i] = cumulated_samples[i];
		temp_cumulated_percent[i] = cumulated_percent[i];

		total_count[i] = symb->sample.counter[i];
		cumulated_samples[i] = 0;
		cumulated_percent[i] = 0;
	}

	for (size_t cur = symb->first ; cur != symb->last ; ++cur) {
		out << ' ';

		DoOutput(out, symb->name, samples_files.get_samples(cur),
			 static_cast<OutSymbFlag>(flags & osf_details_mask));
	}

	for (size_t i = 0 ; i < samples_files.get_nr_counters() ; ++i) {
		total_count[i] = temp_total_count[i];
		cumulated_samples[i] = temp_cumulated_samples[i];
		cumulated_percent[i] = temp_cumulated_percent[i];
	}
}

void OutputSymbol::DoOutput(std::ostream & out, const std::string & name,
			    const sample_entry & sample, OutSymbFlag flag)
{
	OutputHeader(out);

	size_t last_field_pad = 0;

	// first output the vma field
	if (flag & osf_vma) {
		last_field_pad = OutputField(out, name, sample, osf_vma, 0);
	}

	// now the repeated field.
	for (size_t ctr = 0 ; ctr < samples_files.get_nr_counters(); ++ctr) {
		if ((counter & (1 << ctr)) != 0) {
			size_t repeated_flag = (flag & osf_repeat_mask);
			for (size_t i = 0 ; repeated_flag != 0 ; ++i) {
				if ((repeated_flag & (1 << i)) != 0) {
					out << string(last_field_pad, ' ');
					OutSymbFlag fl =
					  static_cast<OutSymbFlag>(1 << i);
					last_field_pad = OutputField(out, name,
								     sample,
								     fl, ctr);
					repeated_flag &= ~(1 << i);
				}
			}
		}
		
	}

	// now the remaining field
	size_t temp_flag = flag & ~(osf_vma|osf_repeat_mask|osf_details|osf_show_all_counters|osf_header);
	for (size_t i = 0 ; temp_flag != 0 ; ++i) {
		if ((temp_flag & (1 << i)) != 0) {
			out << string(last_field_pad, ' ');
			OutSymbFlag fl = static_cast<OutSymbFlag>(1 << i);
			last_field_pad = OutputField(out, name, sample, fl, 0);
			temp_flag &= ~(1 << i);
		}
	}

	out << "\n";
}

void OutputSymbol::OutputHeader(std::ostream & out)
{
	if (first_output == false) {
		return;
	}

	first_output = false;

	if ((flags & osf_header) == 0) {
		return;
	}

	size_t last_field_pad = 0;

	// first output the vma field
	if (flags & osf_vma) {
		last_field_pad = OutputHeaderField(out, osf_vma);
	}

	// now the repeated field.
	for (size_t ctr = 0 ; ctr < samples_files.get_nr_counters(); ++ctr) {
		if ((counter & (1 << ctr)) != 0) {
			size_t repeated_flag = (flags & osf_repeat_mask);
			for (size_t i = 0 ; repeated_flag != 0 ; ++i) {
				if ((repeated_flag & (1 << i)) != 0) {
					out << string(last_field_pad, ' ');
					OutSymbFlag fl =
					  static_cast<OutSymbFlag>(1 << i);
					last_field_pad = 
						OutputHeaderField(out, fl);
					repeated_flag &= ~(1 << i);
				}
			}
		}
		
	}

	// now the remaining field
	size_t temp_flag = flags & ~(osf_vma|osf_repeat_mask|osf_details|osf_show_all_counters|osf_header);
	for (size_t i = 0 ; temp_flag != 0 ; ++i) {
		if ((temp_flag & (1 << i)) != 0) {
			out << string(last_field_pad, ' ');
			OutSymbFlag fl = static_cast<OutSymbFlag>(1 << i);
			last_field_pad = OutputHeaderField(out, fl);
			temp_flag &= ~(1 << i);
		}
	}

	out << "\n";
}

void OutputSymbol::Output(std::ostream & out,
			  const std::vector<const symbol_entry *> & symbols,
			  bool reverse)
{
	if (reverse) {
		vector<const symbol_entry*>::const_reverse_iterator it;
		for (it = symbols.rbegin(); it != symbols.rend(); ++it) {
			Output(out, *it);
		}
	} else {
		vector<const symbol_entry*>::const_iterator it;
		for (it = symbols.begin(); it != symbols.end(); ++it) {
			Output(out, *it);
		}
	}
}

string OutputSymbol::format_vma(const std::string &,
				const sample_entry & sample, size_t)
{
	ostringstream out;
	
	out << std::hex << std::setw(8) << std::setfill('0') << sample.vma;

	return out.str();
}

string OutputSymbol::format_symb_name(const std::string & name,
				      const sample_entry &, size_t)
{
	int const is_anon = name[0] == '?';

	return is_anon ? string("(no symbol)") : name;
}

string OutputSymbol::format_image_name(const std::string &,
				       const sample_entry & sample, size_t)
{
	return sample.file_loc.image_name;
}

string OutputSymbol::format_short_image_name(const std::string &,
					     const sample_entry & sample,
					     size_t)
{
	return basename(sample.file_loc.image_name);
}

string OutputSymbol::format_linenr_info(const std::string &,
					const sample_entry & sample, size_t)
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

string OutputSymbol::format_short_linenr_info(const std::string &,
					      const sample_entry & sample,
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

string OutputSymbol::format_nr_samples(const std::string &,
				       const sample_entry & sample, size_t ctr)
{
	ostringstream out;

	out << sample.counter[ctr];

	return out.str();
}

string OutputSymbol::format_nr_cumulated_samples(const std::string &,
						 const sample_entry & sample,
						 size_t ctr)
{
	ostringstream out;

	cumulated_samples[ctr] += sample.counter[ctr];

	out << cumulated_samples[ctr];

	return out.str();
}

string OutputSymbol::format_percent(const std::string &,
				    const sample_entry & sample, size_t ctr)
{
	ostringstream out;

	double ratio = total_count[ctr]
		? double(sample.counter[ctr]) / total_count[ctr]
		: 0.0;

	out << ratio * 100.0;

	return out.str();
}

string OutputSymbol::format_cumulated_percent(const std::string &,
					      const sample_entry & sample,
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
