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

#include "string_manip.h"

#include "format_output.h"
#include "sample_container.h"
#include "symbol.h"

using namespace std;

namespace format_output {

formatter::formatter(profile_container const & profile_)
	:
	flags(ff_none),
	nr_classes(1),
	profile(profile_),
	vma_64(false),
	need_details(false),
	need_header(true),
	long_filenames(false)
{
	total_count = profile.samples_count();
	total_count_details = profile.samples_count();

	format_map[ff_vma] = field_description(9, "vma", &formatter::format_vma);
	format_map[ff_nr_samples] = field_description(9, "samples", &formatter::format_nr_samples);
	format_map[ff_nr_samples_cumulated] = field_description(14, "cum. samples", &formatter::format_nr_cumulated_samples);
	format_map[ff_percent] = field_description(9, "%", &formatter::format_percent);
	format_map[ff_percent_cumulated] = field_description(11, "cum. %", &formatter::format_cumulated_percent);
	format_map[ff_linenr_info] = field_description(28, "linenr info", &formatter::format_linenr_info);
	format_map[ff_image_name] = field_description(25, "image name", &formatter::format_image_name);
	format_map[ff_app_name] = field_description(25, "app name", &formatter::format_app_name);
	format_map[ff_symb_name] = field_description(30, "symbol name", &formatter::format_symb_name);
	format_map[ff_percent_details] = field_description(9, "%", &formatter::format_percent_details);
	format_map[ff_percent_cumulated_details] = field_description(10, "cum. %", &formatter::format_cumulated_percent_details);
}

 
void formatter::show_details(bool on_off)
{
	need_details = on_off;
}


void formatter::show_header(bool on_off)
{
	need_header = on_off;
}


void formatter::show_long_filenames(bool on_off)
{
	long_filenames = on_off;
}
 

void formatter::vma_format_64bit(bool on_off)
{
	vma_64 = on_off;
}


void formatter::show_global_percent(bool on_off)
{
	global_percent = on_off;
}


void formatter::set_nr_classes(size_t nr)
{
	nr_classes = nr;
}


void formatter::add_format(format_flags flag)
{
	flags = static_cast<format_flags>(flags | flag);
}


void formatter::output(ostream & out, symbol_entry const * symb)
{
	do_output(out, *symb, symb->sample, false);

	if (need_details) {
		output_details(out, symb);
	}
}


void formatter::output(ostream & out, symbol_collection const & symbols)
{
	output_header(out);

	symbol_collection::const_iterator it = symbols.begin();
	symbol_collection::const_iterator end = symbols.end();
	for (; it != end; ++it) {
		output(out, *it);
	}
}


/// describe each possible field of colummned output.
// FIXME: use % of the screen width here. sum of % equal to 100, then calculate
// ratio between 100 and the selected % to grow non fixed field use also
// lib[n?]curses to get the console width (look info source) (so on add a fixed
// field flags)
size_t formatter::output_field(ostream & out, field_datum const & datum,
			 format_flags fl, size_t padding, bool hide_immutable)
{
	if (!hide_immutable) {
		out << string(padding, ' ');

		field_description const & field(format_map[fl]);
		string str = (this->*field.formatter)(datum);
		out << str;

		// at least one separator char
		padding = 1;
		if (str.length() < field.width)
			padding = field.width - str.length();
	} else {
		field_description const & field(format_map[fl]);
		padding += field.width;
	}

	return padding;
}

 
size_t formatter::output_header_field(ostream & out, format_flags fl,
					size_t padding)
{
	out << string(padding, ' ');

	field_description const & field(format_map[fl]);
	out << field.header_name;

	// at least one separator char
	padding = 1;
	if (field.header_name.length() < field.width)
		padding = field.width - field.header_name.length();

	return padding;
}
 

void formatter::output_details(ostream & out, symbol_entry const * symb)
{
	// We need to save the accumulated count and to restore it on
	// exit so global cumulation and detailed cumulation are separate
	count_array_t temp_total_count;
	count_array_t temp_cumulated_samples;
	count_array_t temp_cumulated_percent;

	temp_total_count = total_count;
	temp_cumulated_samples = cumulated_samples;
	temp_cumulated_percent = cumulated_percent;

	if (!global_percent)
		total_count = symb->sample.counts;
	cumulated_percent_details -= symb->sample.counts;
	// cumulated percent are relative to current symbol.
	cumulated_samples = count_array_t();
	cumulated_percent = count_array_t();

	sample_container::samples_iterator it = profile.begin(symb);
	sample_container::samples_iterator end = profile.end(symb);
	for (; it != end; ++it) {
		out << "  ";
		do_output(out, *symb, it->second, true);
	}

	total_count = temp_total_count;
	cumulated_samples = temp_cumulated_samples;
	cumulated_percent = temp_cumulated_percent;
}

 
void formatter::do_output(ostream & out, symbol_entry const & symb,
			  sample_entry const & sample, bool hide_immutable)
{
	size_t padding = 0;

	// first output the vma field
	field_datum datum(symb, sample, 0);
	if (flags & ff_vma)
		padding = output_field(out, datum, ff_vma, padding, false);

	// repeated fields for each profile class
	for (size_t pclass = 0 ; pclass < nr_classes; ++pclass) {
		field_datum datum(symb, sample, pclass);

		if (flags & ff_nr_samples)
			padding = output_field(out, datum,
			       ff_nr_samples, padding, false);

		if (flags & ff_nr_samples_cumulated)
			padding = output_field(out, datum, 
			       ff_nr_samples_cumulated, padding, false);

		if (flags & ff_percent)
			padding = output_field(out, datum,
			       ff_percent, padding, false);

		if (flags & ff_percent_cumulated)
			padding = output_field(out, datum,
			       ff_percent_cumulated, padding, false);

		if (flags & ff_percent_details)
			padding = output_field(out, datum,
			       ff_percent_details, padding, false);

		if (flags & ff_percent_cumulated_details)
			padding = output_field(out, datum,
			       ff_percent_cumulated_details, padding, false);
	}

	// now the remaining field
	if (flags & ff_linenr_info)
		padding = output_field(out, datum, ff_linenr_info,
		       padding, false);

	if (flags & ff_image_name)
		padding = output_field(out, datum, ff_image_name,
		       padding, hide_immutable);

	if (flags & ff_app_name)
		padding = output_field(out, datum, ff_app_name,
		       padding, hide_immutable);

	if (flags & ff_symb_name)
		padding = output_field(out, datum, ff_symb_name,
		       padding, hide_immutable);

	out << "\n";
}
 

void formatter::output_header(ostream & out)
{
	if (!need_header)
		return;

	size_t padding = 0;

	// first output the vma field
	if (flags & ff_vma)
		padding = output_header_field(out, ff_vma, padding);

	// the field repeated for each profile class
	for (size_t pclass = 0 ; pclass < nr_classes; ++pclass) {
		if (flags & ff_nr_samples)
			padding = output_header_field(out,
			      ff_nr_samples, padding);

		if (flags & ff_nr_samples_cumulated)
			padding = output_header_field(out, 
			       ff_nr_samples_cumulated, padding);

		if (flags & ff_percent)
			padding = output_header_field(out,
			       ff_percent, padding);

		if (flags & ff_percent_cumulated)
			padding = output_header_field(out,
			       ff_percent_cumulated, padding);

		if (flags & ff_percent_details)
			padding = output_header_field(out,
			       ff_percent_details, padding);

		if (flags & ff_percent_cumulated_details)
			padding = output_header_field(out,
			       ff_percent_cumulated_details, padding);
	}

	// now the remaining field
	if (flags & ff_linenr_info)
		padding = output_header_field(out, ff_linenr_info, padding);

	if (flags & ff_image_name)
		padding = output_header_field(out, ff_image_name, padding);

	if (flags & ff_app_name)
		padding = output_header_field(out, ff_app_name, padding);

	if (flags & ff_symb_name)
		padding = output_header_field(out, ff_symb_name, padding);

	out << "\n";
}

 
string formatter::format_vma(field_datum const & f)
{
	ostringstream out;
	int width = vma_64 ? 16 : 8;

	out << hex << setw(width) << setfill('0') << f.sample.vma;

	return out.str();
}

 
string formatter::format_symb_name(field_datum const & f)
{
	return symbol_names.demangle(f.symbol.name);
}


namespace {

inline string const & get_image(image_name_id id, bool lf)
{
	return lf ? image_names.name(id) : image_names.basename(id);
}

}

 
string formatter::format_image_name(field_datum const & f)
{
	return get_image(f.symbol.image_name, long_filenames);
}

 
string formatter::format_app_name(field_datum const & f)
{
	return get_image(f.symbol.app_name, long_filenames);
}

 
string formatter::format_linenr_info(field_datum const & f)
{
	ostringstream out;

	string const & filename = long_filenames
		? debug_names.name(f.sample.file_loc.filename)
		: debug_names.basename(f.sample.file_loc.filename);

	if (!filename.empty()) {
		out << filename << ":" << f.sample.file_loc.linenr;
	} else {
		out << "(no location information)";
	}

	return out.str();
}

 
string formatter::format_nr_samples(field_datum const & f)
{
	ostringstream out;
	out << f.sample.counts[f.pclass];
	return out.str();
}

 
string formatter::format_nr_cumulated_samples(field_datum const & f)
{
	ostringstream out;
	cumulated_samples[f.pclass] += f.sample.counts[f.pclass];
	out << cumulated_samples[f.pclass];
	return out.str();
}

 
string formatter::format_percent(field_datum const & f)
{
	ostringstream out;
	double ratio = op_ratio(f.sample.counts[f.pclass],
	                        total_count[f.pclass]);

	return ::format_percent(ratio * 100, percent_int_width,
	                     percent_fract_width);
}

 
string formatter::format_cumulated_percent(field_datum const & f)
{
	ostringstream out;
	cumulated_percent[f.pclass] += f.sample.counts[f.pclass];
	double ratio = op_ratio(cumulated_percent[f.pclass],
	                        total_count[f.pclass]);

	return ::format_percent(ratio * 100, percent_int_width,
	                     percent_fract_width);
}

 
string formatter::format_percent_details(field_datum const & f)
{
	ostringstream out;
	double ratio = op_ratio(f.sample.counts[f.pclass],
	                        total_count_details[f.pclass]);

	return ::format_percent(ratio * 100, percent_int_width,
	                     percent_fract_width);
}

 
string formatter::format_cumulated_percent_details(field_datum const & f)
{
	ostringstream out;
	cumulated_percent_details[f.pclass]
		+= f.sample.counts[f.pclass];

	double ratio = op_ratio(cumulated_percent_details[f.pclass],
	                        total_count_details[f.pclass]);

	return ::format_percent(ratio * 100, percent_int_width,
			     percent_fract_width);
}

} // namespace format_output
