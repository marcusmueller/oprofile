#include <sstream>
#include <iomanip>

#include "../util/file_manip.h"

#include "opp_symbol.h"
#include "opf_filter.h"

using std::string;
using std::cout;
using std::cerr;
using std::vector;
using std::ostream;
using std::ostringstream;


#include <stdio.h>
/**
 * backward compatibility until the OutputSymbol class is ok, remove it in
 * future
 */
void output_symbol(const symbol_entry* symb, bool show_image_name,
		   bool output_linenr_info, int counter, u32 total_count)
{
	if (show_image_name)
		printf("%s ", symb->sample.file_loc.image_name.c_str());

	if (output_linenr_info)
		printf("%s:%u ",
		       symb->sample.file_loc.filename.c_str(),
		       symb->sample.file_loc.linenr);

	int const is_anon = symb->name[0] == '?';

	if (!is_anon)
		printf("%s", symb->name.c_str());

	u32 count = symb->sample.counter[counter];

	if (count) {
		if (!is_anon)
			printf("[0x%.8lx]: ", symb->sample.vma);
		else
			printf("(no symbols) "); 

		printf("%2.4f%% (%u samples)\n", 
		       (((double)count) / total_count)*100.0, count);
	} else {
		printf(" (0 samples)\n");
	}
}

struct output_option {
	char option;
	OutSymbFlag flag;
	const char * help_string;
};

static const output_option output_options[] = {
	{ 'v', osf_vma, "vma offset" },
	{ 's', osf_nr_samples, "nr samples" },
	{ 'S', osf_nr_samples_cumulated, "nr cumulated samples" },
	{ 'p', osf_percent_samples, "nr percent samples" },
	{ 'P', osf_percent_samples_cumulated, "nr cumulated percent samples" },
//	{ 't', osf_time, "time" },
//	{ 'T', ofs_time_cumulated, "cumulated time" },
	{ 'n', osf_symb_name, "symbol name" },
//	{ 'd', osf_demangle, "demangle symbol" },
	{ 'l', osf_linenr_info, "source file name and line nr" },
	{ 'L', osf_short_linenr_info, "base name of source file and line nr" },
	{ 'i', osf_image_name, "image name" },
	{ 'I', osf_short_image_name, "base name of image name" },
	{ 'h', osf_header, "header" },
//	{ 'd', osf_details, "detailed samples for aech selected symbol" }
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

OutSymbFlag ParseOutputOption(const string & option)
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

typedef string (OutputSymbol::*fct_format)(const symbol_entry * symb);

/// decribe one field of the colummned output.
struct field_description {
	OutSymbFlag flag;
	size_t width;
	const char * header_name;
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
	{ osf_percent_samples, 12,
	  "%-age", &OutputSymbol::format_percent },
	{ osf_percent_samples_cumulated, 10,
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
	cumulated_samples(0),
	cumulated_percent_samples(0),
	time_cumulated(0),
	counter(counter_),
	first_output(true)
{
	total_count = samples_files.samples_count(counter);
}

void OutputSymbol::SetFlag(OutSymbFlag flag)
{
	flags = static_cast<OutSymbFlag>(flags | flag);
}

// TODO: (and also in OutputHeader(). Add the blank when outputting
// the next field not the current to avoid filling with blank the eol.
void OutputSymbol::Output(std::ostream & out, const symbol_entry * symb)
{
#if 0
	// the old behavior.
	output_symbol(symb, flags & osf_image_name, flags & osf_linenr_info,
		      counter, total_count);
#else
	OutputHeader(out);

	// avoid to clobber the screen if user are required to output nothing
	bool have_output_something = false;

	size_t temp_flag = flags;
	for (size_t i = 0 ; temp_flag != 0 ; ++i) {
		if ((flags & (1 << i)) != 0) {
			OutSymbFlag fl = static_cast<OutSymbFlag>(1 << i);
			const field_description * field = GetFieldDescr(fl);
			if (field) {
				string str = (this->*field->formater)(symb);
				out << str;

				size_t sz = 1;	// at least one separator char
				if (str.length() < field->width) {
					sz = field->width - str.length();
				}
				out << string(sz, ' ');

				have_output_something = true;
			}
			temp_flag &= ~(1 << i);
		}
	}

	if (have_output_something)
		out << "\n";
#endif
}

const field_description * OutputSymbol::GetFieldDescr(OutSymbFlag flag)
{
	for (size_t i = 0 ; i < nr_field_descr ; ++i) {
		if (flag == field_descr[i].flag)
			return &field_descr[i];
	}

	return 0;
}

void OutputSymbol::OutputHeader(ostream & out)
{
	if (first_output == false) {
		return;
	}

	first_output = false;

	if ((flags & osf_header) == 0) {
		return;
	}

	// avoid to clobber the screen if user are required to output nothing
	bool have_output_something = false;

	size_t temp_flag = flags;
	for (size_t i = 0 ; temp_flag != 0 ; ++i) {
		if ((flags & (1 << i)) != 0) {
			OutSymbFlag fl = static_cast<OutSymbFlag>(1 << i);
			const field_description * field = GetFieldDescr(fl);
			if (field) {
				// TODO: center the field header_name ?
				out << field->header_name;

				size_t sz = 1;	// at least one separator char
				if (strlen(field->header_name) < field->width)
					sz = field->width -
						strlen(field->header_name);

				out << string(sz, ' ');

				have_output_something = true;
			}
			temp_flag &= ~(1 << i);
		}
	}

	if (have_output_something)
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

string OutputSymbol::format_vma(const symbol_entry * symb)
{
	ostringstream out;
	
	out << hex << setw(8) << setfill('0') << symb->sample.vma;

	return out.str();
}

string OutputSymbol::format_symb_name(const symbol_entry * symb)
{
	ostringstream out;

	int const is_anon = symb->name[0] == '?';

	if (!is_anon)
		out << symb->name;
	else
		out << "(no symbols)";

	return out.str();
}

string OutputSymbol::format_image_name(const symbol_entry * symb)
{
	ostringstream out;
	
	out << symb->sample.file_loc.image_name;

	return out.str();
}

string OutputSymbol::format_short_image_name(const symbol_entry * symb)
{
	ostringstream out;

	out << basename(symb->sample.file_loc.image_name);

	return out.str();
}

string OutputSymbol::format_linenr_info(const symbol_entry * symb)
{
	ostringstream out;
	
	out << symb->sample.file_loc.filename
	    << ":" << symb->sample.file_loc.linenr;

	return out.str();
}

string OutputSymbol::format_short_linenr_info(const symbol_entry * symb)
{
	ostringstream out;

	out << basename(symb->sample.file_loc.filename)
	    << ":" << symb->sample.file_loc.linenr;

	return out.str();
}

string OutputSymbol::format_nr_samples(const symbol_entry * symb)
{
	ostringstream out;

	out << symb->sample.counter[counter];

	return out.str();
}

string OutputSymbol::format_nr_cumulated_samples(const symbol_entry * symb)
{
	ostringstream out;

	cumulated_samples += symb->sample.counter[counter];

	out << cumulated_samples;

	return out.str();
}

string OutputSymbol::format_percent(const symbol_entry * symb)
{
	ostringstream out;

	out << (double(symb->sample.counter[counter]) / total_count) * 100;

	return out.str();
}

string OutputSymbol::format_cumulated_percent(const symbol_entry * symb)
{
	ostringstream out;

	cumulated_percent_samples += symb->sample.counter[counter];

	out << (double(cumulated_percent_samples) / total_count) * 100;

	return out.str();
}
