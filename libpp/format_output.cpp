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
#include "callgraph_container.h"
#include "sample_container.h"
#include "symbol.h"

using namespace std;

namespace format_output {


formatter::formatter()
	:
	nr_classes(1),
	flags(ff_none),
	vma_64(false),
	long_filenames(false),
	need_header(true)
{
}


formatter::~formatter()
{
}


void formatter::set_nr_classes(size_t nr)
{
	nr_classes = nr;
}


void formatter::add_format(format_flags flag)
{
	flags = static_cast<format_flags>(flags | flag);
}


void formatter::show_header(bool on_off)
{
	need_header = on_off;
}
 

void formatter::vma_format_64bit(bool on_off)
{
	vma_64 = on_off;
}


void formatter::show_long_filenames(bool on_off)
{
	long_filenames = on_off;
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

namespace {

string const & get_image_name(image_name_id id, bool lf)
{
	return lf ? image_names.name(id) : image_names.basename(id);
}

string const format_linenr_info(file_location const floc, bool lf)
{
	ostringstream out;

	string const & filename = lf
		? debug_names.name(floc.filename)
		: debug_names.basename(floc.filename);

	if (!filename.empty()) {
		out << filename << ":" << floc.linenr;
	} else {
		out << "(no location information)";
	}

	return out.str();
}

string format_vma(bfd_vma vma, bool vma_64)
{
	ostringstream out;
	int width = vma_64 ? 16 : 8;

	out << hex << setw(width) << setfill('0') << vma;

	return out.str();
}

string format_percent(size_t dividend, size_t divisor)
{
	double ratio = op_ratio(dividend, divisor);

	return ::format_percent(ratio * 100, percent_int_width,
	                     percent_fract_width);
}

} // anonymous namespace


opreport_formatter::opreport_formatter(profile_container const & profile_)
	:
	profile(profile_),
	need_details(false)
{
	total_count = profile.samples_count();
	total_count_details = profile.samples_count();

	format_map[ff_vma] = field_description(9, "vma", &opreport_formatter::format_vma);
	format_map[ff_nr_samples] = field_description(9, "samples", &opreport_formatter::format_nr_samples);
	format_map[ff_nr_samples_cumulated] = field_description(14, "cum. samples", &opreport_formatter::format_nr_cumulated_samples);
	format_map[ff_percent] = field_description(9, "%", &opreport_formatter::format_percent);
	format_map[ff_percent_cumulated] = field_description(11, "cum. %", &opreport_formatter::format_cumulated_percent);
	format_map[ff_linenr_info] = field_description(28, "linenr info", &opreport_formatter::format_linenr_info);
	format_map[ff_image_name] = field_description(25, "image name", &opreport_formatter::format_image_name);
	format_map[ff_app_name] = field_description(25, "app name", &opreport_formatter::format_app_name);
	format_map[ff_symb_name] = field_description(30, "symbol name", &opreport_formatter::format_symb_name);
	format_map[ff_percent_details] = field_description(9, "%", &opreport_formatter::format_percent_details);
	format_map[ff_percent_cumulated_details] = field_description(10, "cum. %", &opreport_formatter::format_cumulated_percent_details);
}

 
void opreport_formatter::show_details(bool on_off)
{
	need_details = on_off;
}


void opreport_formatter::show_global_percent(bool on_off)
{
	global_percent = on_off;
}


void opreport_formatter::output(ostream & out, symbol_entry const * symb)
{
	do_output(out, *symb, symb->sample, false);

	if (need_details) {
		output_details(out, symb);
	}
}


void opreport_formatter::
output(ostream & out, symbol_collection const & symbols)
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
size_t opreport_formatter::
output_field(ostream & out, field_datum const & datum,
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

 
size_t opreport_formatter::output_header_field(ostream & out, format_flags fl,
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
 

void opreport_formatter::
output_details(ostream & out, symbol_entry const * symb)
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

 
void opreport_formatter::do_output(ostream & out, symbol_entry const & symb,
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
 

string opreport_formatter::format_vma(field_datum const & f)
{
	return format_output::format_vma(f.sample.vma, vma_64);
}

 
string opreport_formatter::format_symb_name(field_datum const & f)
{
	return symbol_names.demangle(f.symbol.name);
}


string opreport_formatter::format_image_name(field_datum const & f)
{
	return get_image_name(f.symbol.image_name, long_filenames);
}

 
string opreport_formatter::format_app_name(field_datum const & f)
{
	return get_image_name(f.symbol.app_name, long_filenames);
}

 
string opreport_formatter::format_linenr_info(field_datum const & f)
{
	return format_output::format_linenr_info(f.sample.file_loc,
		long_filenames);
}

 
string opreport_formatter::format_nr_samples(field_datum const & f)
{
	ostringstream out;
	out << f.sample.counts[f.pclass];
	return out.str();
}

 
string opreport_formatter::format_nr_cumulated_samples(field_datum const & f)
{
	ostringstream out;
	cumulated_samples[f.pclass] += f.sample.counts[f.pclass];
	out << cumulated_samples[f.pclass];
	return out.str();
}

 
string opreport_formatter::format_percent(field_datum const & f)
{
	return format_output::format_percent(f.sample.counts[f.pclass],
		total_count[f.pclass]);
}

 
string opreport_formatter::format_cumulated_percent(field_datum const & f)
{
	cumulated_percent[f.pclass] += f.sample.counts[f.pclass];

	return format_output::format_percent(cumulated_percent[f.pclass],
		total_count[f.pclass]);
}

 
string opreport_formatter::format_percent_details(field_datum const & f)
{
	return format_output::format_percent(f.sample.counts[f.pclass],
		total_count_details[f.pclass]);
}

 
string opreport_formatter::
format_cumulated_percent_details(field_datum const & f)
{
	cumulated_percent_details[f.pclass] += f.sample.counts[f.pclass];

	return format_output::format_percent(
		cumulated_percent_details[f.pclass],
		total_count_details[f.pclass]);
}


cg_formatter::cg_formatter(callgraph_container const & profile_)
	:
	profile(profile_)
{
	total_count_self = profile.samples_count();

	format_map[ff_vma] = field_description(9, "vma", &cg_formatter::format_vma);
	format_map[ff_nr_samples] = field_description(16, "samples", &cg_formatter::format_nr_samples);
	format_map[ff_nr_samples_cumulated] = field_description(16, "cum. samples", &cg_formatter::format_nr_cumulated_samples);
	format_map[ff_percent] = field_description(16, "%", &cg_formatter::format_percent);
	format_map[ff_percent_cumulated] = field_description(16, "cum. %", &cg_formatter::format_cumulated_percent);
	format_map[ff_linenr_info] = field_description(28, "linenr info", &cg_formatter::format_linenr_info);
	format_map[ff_image_name] = field_description(25, "image name", &cg_formatter::format_image_name);
	format_map[ff_app_name] = field_description(25, "app name", &cg_formatter::format_app_name);
	format_map[ff_symb_name] = field_description(30, "symbol name", &cg_formatter::format_symb_name);
}


/// describe each possible field of colummned output.
// FIXME: use % of the screen width here. sum of % equal to 100, then calculate
// ratio between 100 and the selected % to grow non fixed field use also
// lib[n?]curses to get the console width (look info source) (so on add a fixed
// field flags)
size_t cg_formatter::
output_field(ostream & out, field_datum const & datum,
             format_flags fl, size_t padding)
{
	out << string(padding, ' ');

	field_description const & field(format_map[fl]);
	string str = (this->*field.formatter)(datum);
	out << str;

	// at least one separator char
	padding = 1;
	if (str.length() < field.width)
		padding = field.width - str.length();

	return padding;
}


void cg_formatter::do_output(std::ostream & out, cg_symbol const & symb)
{
	size_t padding = 0;

	// first output the vma field
	field_datum datum(symb, 0);
	if (flags & ff_vma)
		padding = output_field(out, datum, ff_vma, padding);

	// repeated fields for each profile class
	for (size_t pclass = 0 ; pclass < nr_classes; ++pclass) {
		field_datum datum(symb, pclass);

		if (flags & ff_nr_samples)
			padding = output_field(out, datum,
			       ff_nr_samples, padding);

		if (flags & ff_nr_samples_cumulated)
			padding = output_field(out, datum, 
			       ff_nr_samples_cumulated, padding);

		if (flags & ff_percent)
			padding = output_field(out, datum,
			       ff_percent, padding);

		if (flags & ff_percent_cumulated)
			padding = output_field(out, datum,
			       ff_percent_cumulated, padding);
	}

	// now the remaining field
	if (flags & ff_linenr_info)
		padding = output_field(out, datum, ff_linenr_info, padding);

	if (flags & ff_image_name)
		padding = output_field(out, datum, ff_image_name, padding);

	if (flags & ff_app_name)
		padding = output_field(out, datum, ff_app_name, padding);

	if (flags & ff_symb_name)
		padding = output_field(out, datum, ff_symb_name, padding);

	out << "\n";
}


size_t cg_formatter::output_header_field(ostream & out, format_flags fl,
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


void cg_formatter::output(std::ostream & out)
{
	output_header(out);

	cg_collection arcs = profile.get_arc();

	for (size_t i = 0; i < arcs.size(); ++i) {
		count_array_t temp_total_count_self;
		count_array_t temp_total_count_callee;
		count_array_t temp_cumulated_samples_self;
		count_array_t temp_cumulated_percent_self;

		temp_total_count_self = total_count_self;
		temp_total_count_callee = total_count_callee;
		temp_cumulated_samples_self = cumulated_samples_self;
		temp_cumulated_percent_self = cumulated_percent_self;

		total_count_self = count_array_t();
		total_count_callee = count_array_t();
		cumulated_samples_callee = count_array_t();
		cumulated_percent_callee = count_array_t();

		cg_collection callee_arcs = profile.get_callee(arcs[i]);

		for (size_t j = 0; j < callee_arcs.size(); ++j) {
			total_count_self += callee_arcs[j].self_counts;
			total_count_callee += callee_arcs[j].callee_counts;
		}

		for (size_t j = 0; j < callee_arcs.size(); ++j) {
			out << "    ";
			do_output(out, callee_arcs[j]);
		}

		total_count_self = temp_total_count_self;
		total_count_callee = temp_total_count_callee;
		cumulated_samples_self = temp_cumulated_samples_self;
		cumulated_percent_self = temp_cumulated_percent_self;

		do_output(out, arcs[i]);

		temp_total_count_self = total_count_self;
		temp_total_count_callee = total_count_callee;
		temp_cumulated_samples_self = cumulated_samples_self;
		temp_cumulated_percent_self = cumulated_percent_self;

		total_count_self = count_array_t();
		total_count_callee = count_array_t();
		cumulated_samples_callee = count_array_t();
		cumulated_percent_callee = count_array_t();

		cg_collection caller_arcs = profile.get_caller(arcs[i]);

		for (size_t j = 0; j < caller_arcs.size(); ++j) {
			total_count_self += caller_arcs[j].self_counts;
			total_count_callee += caller_arcs[j].callee_counts;
		}

		for (size_t j = 0; j < caller_arcs.size(); ++j) {
			out << "    ";
			do_output(out, caller_arcs[j]);
		}

		total_count_self = temp_total_count_self;
		total_count_callee = temp_total_count_callee;
		cumulated_samples_self = temp_cumulated_samples_self;
		cumulated_percent_self = temp_cumulated_percent_self;

		out << string(79, '-') << endl;
	}
}


string cg_formatter::format_vma(field_datum const & f)
{
	return format_output::format_vma(f.symbol.sample.vma, vma_64);
}

 
string cg_formatter::format_symb_name(field_datum const & f)
{
	return symbol_names.demangle(f.symbol.name);
}


string cg_formatter::format_image_name(field_datum const & f)
{
	return get_image_name(f.symbol.image_name, long_filenames);
}


string cg_formatter::format_app_name(field_datum const & f)
{
	return get_image_name(f.symbol.app_name, long_filenames);
}


string cg_formatter::format_linenr_info(field_datum const & f)
{
	return format_output::format_linenr_info(f.symbol.sample.file_loc,
		long_filenames);
}


string cg_formatter::format_nr_samples(field_datum const & f)
{
	ostringstream out;
	out << f.symbol.self_counts[f.pclass] << "/"
	    << f.symbol.callee_counts[f.pclass];
	return out.str();
}


string cg_formatter::format_nr_cumulated_samples(field_datum const & f)
{
	ostringstream out;
	cumulated_samples_self[f.pclass] += f.symbol.self_counts[f.pclass];
	cumulated_samples_callee[f.pclass] += f.symbol.callee_counts[f.pclass];
	out << cumulated_samples_self[f.pclass] << "/"
	    << cumulated_samples_callee[f.pclass];
	return out.str();
}

 
string cg_formatter::format_percent(field_datum const & f)
{
	return format_output::format_percent(f.symbol.self_counts[f.pclass],
			total_count_self[f.pclass]) + "/" +
		format_output::format_percent(f.symbol.callee_counts[f.pclass],
			total_count_self[f.pclass]);
}


string cg_formatter::format_cumulated_percent(field_datum const & f)
{
	cumulated_percent_self[f.pclass] += f.symbol.self_counts[f.pclass];
	cumulated_percent_callee[f.pclass] += f.symbol.callee_counts[f.pclass];

	return format_output::format_percent(cumulated_percent_self[f.pclass],
			total_count_self[f.pclass]) + "/" +
	   format_output::format_percent(cumulated_percent_callee[f.pclass],
			total_count_callee[f.pclass]);
}


} // namespace format_output
