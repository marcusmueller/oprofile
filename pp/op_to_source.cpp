/**
 * @file op_to_source.cpp
 * Annotated source output
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author Philippe Elie
 * @author John Levon
 */

#include <fstream>
#include <iomanip>
#include <iostream>
#include <vector>
#include <algorithm>
#include <string>
#include <sstream>

#include <stdio.h>

#include "profile_container.h"
#include "profile.h"
#include "demangle_symbol.h"
#include "derive_files.h"
#include "counter_util.h"
#include "op_sample_file.h"
#include "op_print_event.h"
#include "op_events_desc.h"
#include "op_to_source_options.h"
#include "session.h"

#include "child_reader.h"
#include "string_manip.h"
#include "file_manip.h"
#include "filename_match.h"

#include "version.h"

using namespace std;
using namespace options;

/// Store the configuration of one counter. Construct from an opd_header
struct counter_setup {
	counter_setup() :
		enabled(false), event_count_sample(0) {}

	/**
	 * @param out output stream
	 * @param cpu_type cpu type
	 * @param counter counter number
	 *
	 * Convenience function : just output the setup of one counter.
	 */
	void print(ostream & out, op_cpu cpu_type, int counter) const;

	// if false other field are not meaningful.
	bool   enabled;
	u8 ctr_event;
	u16 unit_mask;
	size_t event_count_sample;
	size_t total_samples;
};


// This named namespace help doxygen to hierarchies documentation
namespace op_to_source {

/// string used as start / end comment to annotate source
string const begin_comment("/*");
string const end_comment("*/");
/// This is usable only if one of the counter has been setup as: count
/// some sort of cycles events. In the other case trying to use it to
/// translate samples count to time is a non-sense.
double cpu_speed = 0.0;
/// samples files header are stored here
counter_setup counter_info[OP_MAX_COUNTERS];
/// percent from where the source file is output.
size_t threshold_percent;
/// must we output source until we get threshold_percent or output source
/// with more samples than threshold_percent
bool do_until_more_than_samples;
/// the cpu type, we fill this var from the header of samples files
op_cpu cpu_type = CPU_NO_GOOD;
/// hold all info for samples
scoped_ptr<profile_container_t> samples(0);

/**
 * @param str the input string
 *
 * Return the substring at beginning of str which is only made of blank or
 * tabulation.
 */
string extract_blank_at_begin(string const & str);

/**
 * @param image_name the samples owner image name
 * @param sample_file the sample file base name (w/o counter nr suffix)
 * @param fn_match only source filename which match this filter will be output
 * @param argc command line number of argument
 * @param argv command line argument array
 *
 * This is the entry point of the source annotation utility called after
 * parsing and checking commnd line argument
 */
bool annotate_source(string const & image_name, string const & sample_file,
		     filename_match const & fn_match,
		     int argc, char const * argv[]);

/**
 * @param out output stream
 * @param argc the command line number of argument
 * @param argv the command line number of argument
 *
 * Ouput an 'header comment'
 *
 * Each annotated source file is prepended with a comment containing various
 * things such as counter setup, command line used to produce the annotated
 * file and interpretation of command line
 */
void output_header(ostream & out, int argc, char const * argv[]);

/**
 * @param argc the command line number of argument
 * @param argv the command line number of argument
 * @param image_name the binary image name
 *
 * output annotated disassembly optionally mixed with source file.
 * Disassembly itself is provided by forking an objdump child process
 * which is filtered through op_to_source process
 */
void output_asm(int argc, char const * argv[], string const & image_name);

/**
 * @param str one assembly line coming from objdump -d
 * @param output_symbols symbols for the binary image being processed
 *  filtered by --exclude-symbols
 * @param do_output input/output parameters : true if we must output this line.
 * We need an input/output parameters here because we must filter some symbols
 * according to the user request
 *
 * Process one objdump -d output line, outputting and annotating it if
 * necessary
 */
void output_objdump_asm_line(string const & str,
		     vector<symbol_entry const *> const & output_symbols,
	     bool & do_output);

/**
 * @param output_symbols symbols for the binary image being processed
 *  filtered by --exclude-symbols
 * @param app_name the binary image name
 * @param start vma start address to disassemble
 * @param end vma end address to disassemble
 *
 * fork objdump for the vma range requested then filter the objdump output
 * echoing it to the proper stream with optional annotation
 */
void do_one_output_objdump(vector<symbol_entry const *> const & output_symbols,
		   string const & app_name, bfd_vma start, bfd_vma end);

/**
 * @param output_symbols symbols for the binary image being processed
 *  filtered by --exclude-symbols
 * @param app_name the binary image name
 *
 * fork an 'objdump -d app_name' then filter the objdump output echoing it to
 * the proper stream with optional annotation
 */
void output_objdump_asm(vector<symbol_entry const *> const & output_symbols,
			string const & app_name);

/**
 * @param argc the command line number of argument
 * @param argv the command line number of argument
 * @param fn_match only source filename which match this filter will be output
 * @param output_separate_file true if user request for creating on file for
 * each annotated source else op_to_source output only one report on cout
 *
 * output all annotated source matching the fn_match parameters
 */
void output_source(int argc, char const * argv[],
		   filename_match const & fn_match,
		   bool output_separate_file);

/**
 * @param in input stream, in is not necessary valid if input source file is
 *  not avaialble
 * @param filename: input source filename
 * @param output_separate_file specify if we output all annotated source to
 *  separate file or to cout
 *
 * output one annotated source file
 */
void output_one_file(istream & in, string const & filename,
		     bool output_separate_file);

/**
 * @param in input stream (a source file) or an invalid stream if source file
 *  is not avaialble
 * @param out output stream (the annotated source file)
 * @param filename input source filename
 *
 * helper function for output_one_file
 */
void do_output_one_file(ostream & out, istream & in, string const & filename);

/**
 * @param profile storage container for openeded samples files
 *
 * store opd_header information from the samples to the global
 * var counter_info. Caculate the total amount of samples for these samples
 * files.
 *
 * return false if no samples are openeded or if no cumulated count of
 * samples is zero
 */
bool setup_counter_param(profile_t const & profile);

/**
 * @param out output stream
 * @param filename output filename
 * @param count counter to output
 *
 * output a comment containing samples count for this source file filename
 */
void output_counter_for_file(ostream & out, string const & filename,
			     const counter_array_t& count);

/**
 * @param out output stream
 * @param counter count to output
 * @param total total nr of samples allowing to calculate the ratio
 *
 * output to out count followed by the ration total/count
 */
void output_one_counter(ostream & out, size_t counter, size_t total);

/**
 * @param out output stream
 * @param counter counter to output
 * @param comment true if we quote the output with begin_comment / end_comment
 * @param prefix a string, optionally empty, to output before counter
 *
 * output to out the number of samples in counter optionally prefixed by
 * the prefix string and quoted by comment
 */
void output_counter(ostream & out, const counter_array_t & counter,
		    bool comment, string const & prefix = string());

/**
 * @param str_vma the input string produced by objdump
 *
 * parse str_vma, which is in the format xxxxxxx: or xxxxxx <symb_name>:
 * to extract the vma address and search the associated symbol_entry object
 * with this vma. return NULL if the search fail.
 */
symbol_entry const * find_symbol(string const & str_vma);

/**
 * @param out output stream
 * @param str an objdump output string on the form hexa_number: or
 *   hexa_number <symbol_name>:
 * @param blank a string containing a certain amount of space/tabulation
 *   character allowing the output to be aligned with the previous line
 *
 * from the vma at start of str_vma find the associated symbol name
 * then output to out the symbol name and numbers of samples belonging
 * to this symbol
 */
void find_and_output_symbol(ostream & out, string const & str,
			    string const & blank);

/**
 * @param out output stream
 * @param str an objdump output string on the form hexa_number:
 * @param blank a string containing a certain amount of space/tabulation
 *   character allowing the output to be aligned with the previous line
 *
 * from the vma at start of str_vma find the associated samples number
 * belonging to this vma then output them
 */
void find_and_output_counter(ostream & out, string const & str,
			     string const & blank);

/**
 * @param out output stream
 * @param filename a source filename
 * @param linenr a line number into the source file
 * @param blank a string containing a certain amount of space/tabulation
 *   character allowing the output to be aligned with the previous line
 *
 * from filename, linenr find the associated samples numbers belonging
 * to this source file, line nr then output them
 */
void find_and_output_counter(ostream & out, string const & filename,
			     size_t linenr, string const & blank);

/**
 * from the command line specification retrieve the counter nr used to
 * as sort field when sorting samples nr. If command line did not specify
 * any sort by counter fall back to the first valid counter number
 */
size_t get_sort_counter_nr();

} // op_to_source namespace


void counter_setup::print(ostream & out, op_cpu cpu_type, int counter) const
{
	if (enabled) {
		op_print_event(out, counter, cpu_type, ctr_event, unit_mask,
			       event_count_sample);
		out << "total samples : " << total_samples;
	} else {
		out << "Counter " << counter << " disabled";
	}

	out << endl;
}


namespace op_to_source {

string extract_blank_at_begin(string const & str)
{
	size_t end_pos = str.find_first_not_of(" \t");
	if (end_pos == string::npos)
		end_pos = 0;

	return str.substr(0, end_pos);
}
 

bool annotate_source(string const & image_name, string const & sample_file,
		     filename_match const & fn_match,
		     int argc, char const * argv[])
{
	bool output_separate_file = false;
	if (!source_dir.empty()) {
		output_separate_file = true;

		source_dir = relative_to_absolute_path(source_dir);
		if (source_dir.length() &&
		    source_dir[source_dir.length() - 1] != '/')
			source_dir += '/';
	}

	if (!output_dir.empty() || output_separate_file) {
		output_separate_file = true;

		output_dir = relative_to_absolute_path(output_dir);
		if (output_dir.length() &&
		    output_dir[output_dir.length() - 1] != '/')
			output_dir += '/';


		if (!create_dir(output_dir)) {
			cerr << "unable to create " << output_dir
			     << " directory: " << endl;
			return false;
		}
	}

	if (output_separate_file && output_dir == source_dir) {
		cerr << "You can not specify the same directory for "
		     << "--output-dir and --source-dir" << endl;
		return false;
	}

	outsymbflag flag = osf_details;
	if (!assembly)
		flag = static_cast<outsymbflag>(flag | osf_linenr_info);
 
	samples.reset(new profile_container_t(false, flag, -1));

	// this lexical scope just optimize the memory use by relaxing
	// the op_bfd and profile_t as short as we can.
	{
		profile_t profile(sample_file, -1);
		profile.check_mtime(image_name);

		op_bfd abfd(image_name, exclude_symbols, include_symbols);

		profile.set_start_offset(abfd.get_start_offset());

		if (!assembly && !abfd.have_debug_info()) {
			cerr << "Request for source file annotated "
			     << "with samples but no debug info available"
			     << endl;
			return false;
		}

		cpu_speed = profile.first_header().cpu_speed;
		uint tmp = profile.first_header().cpu_type;
		cpu_type = static_cast<op_cpu>(tmp);

		samples->add(profile, abfd, image_name);

		if (!setup_counter_param(profile))
			return false;
	}

	if (assembly)
		output_asm(argc, argv, image_name);
	else
		output_source(argc, argv, fn_match, output_separate_file);

	return true;
}
 

void output_header(ostream & out, int argc, char const * argv[])
{
	out << begin_comment << endl;

	out << "Command line:" << endl;
	for (int i = 0 ; i < argc ; ++i)
		out << argv[i] << " ";
	out << endl << endl;

	out << "interpretation of command line:" << endl;

	if (!assembly) {
		out << "output annotated source file with samples" << endl;

		if (threshold_percent != 0) {
			if (!do_until_more_than_samples) {
				out << "output files where the selected counter reach "
				    << threshold_percent << "% of the samples"
				    << endl;
			} else {
				out << "output files until " << threshold_percent
				    << "% of the samples is reached on the selected counter"
				    << endl;
			}
		} else {
			out << "output all files" << endl;
		}
	} else {
		out << "output annotated assembly listing with samples" << endl;
		if (!objdump_params.empty()) {
			out << "passing the following additional arguments to objdump ; \"";
			for (size_t i = 0 ; i < objdump_params.size() ; ++i)
				out << objdump_params[i] << " ";
			out << "\"" << endl;
		}
	}

	out << endl;
	out << "Cpu type: " << op_get_cpu_type_str(cpu_type) << endl;
	out << "Cpu speed (MHz estimation) : " << cpu_speed << endl;
	out << endl;

	for (size_t i = 0 ; i < samples->get_nr_counters() ; ++i) {
		counter_info[i].print(out, cpu_type, i);
	}

	out << end_comment << endl;
	out << endl;
}
 

void output_asm(int argc, char const * argv[], string const & image_name)
{
	// select the subset of symbols which statisfy the user requests
	size_t index = get_sort_counter_nr();

	vector<symbol_entry const *> output_symbols;

	double threshold = threshold_percent / 100.0;

	output_symbols =
	 samples->select_symbols(index, threshold, do_until_more_than_samples);

	output_header(cout, argc, argv);

	output_objdump_asm(output_symbols, image_name);
}


void output_objdump_asm_line(string const & str,
		     vector<symbol_entry const *> const & output_symbols,
		     bool & do_output)
{
	// output of objdump is a human read-able form and can contain some
	// ambiguity so this code is dirty. It is also optimized a little what
	// so it is difficult to simplify it without beraking something ...

	// line of interest are: "[:space:]*[:xdigit:]?[ :]", the last char of
	// this regexp dis-ambiguate between a symbol line and an asm line. If
	// source contain line of this form an ambiguity occur and we rely on
	// the robustness of this code.

	// Do not use high level C++ construct such ostringstream: we need
	// efficiency here.
	size_t pos = 0;
	while (pos < str.length() && isspace(str[pos]))
		++pos;

	if (pos == str.length() || !isxdigit(str[pos]))
		return;

	while (pos < str.length() && isxdigit(str[pos]))
		++pos;

	if (pos == str.length() || (!isspace(str[pos]) && str[pos] != ':'))
		return;

	// symbol are on the form 08030434 <symbol_name>:  we need to be strict
	// here to avoid any interpretation of a source line as a symbol line
	if (str[pos] == ' ' && str[pos+1] == '<' && str[str.length() - 1] == ':') {  // is the line contain a symbol
		symbol_entry const * symbol = find_symbol(str);

		// ! complexity: linear in number of symbol must use sorted
		// by address vector and lower_bound ?
		// Note this use a pointer comparison. It work because symbols
		// pointer are unique
		if (find(output_symbols.begin(),
		       output_symbols.end(), symbol) != output_symbols.end()) {
			do_output = true;
		} else {
			do_output = false;
		}

		if (do_output)
			find_and_output_symbol(cout, str, string());

	} else { // not a symbol, probably an asm line.
		if (do_output)
			find_and_output_counter(cout, str, " ");
	}
}
 

void do_one_output_objdump(vector<symbol_entry const *> const & output_symbols,
			   string const & app_name, bfd_vma start, bfd_vma end)
{
	vector<string> args;

	args.push_back("-d");
	args.push_back("--no-show-raw-insn");
	if (source_with_assembly)
		args.push_back("-S");

	if (start || end != ~(bfd_vma)0) {
		ostringstream arg1, arg2;
		arg1 << "--start-address=" << start;
		arg2 << "--stop-address=" << end;
		args.push_back(arg1.str());
		args.push_back(arg2.str());
	}

	if (!objdump_params.empty()) {
		for (size_t i = 0 ; i < objdump_params.size() ; ++i)
			args.push_back(objdump_params[i]);
	}

	args.push_back(app_name);
	child_reader reader("objdump", args);
	if (reader.error()) {
		cerr << "An error occur during the execution of objdump:\n\n";
		cerr << reader.error_str() << endl;
		return;
	}

	// to filter output of symbols (filter based on command line options)
	bool do_output = true;

	string str;
	while (reader.getline(str)) {
		output_objdump_asm_line(str, output_symbols, do_output);
		if (do_output)
			cout << str << '\n';
	}

	// objdump always returns SUCCESS so we must rely on the stderr state
	// of objdump. If objdump error message is cryptic our own error
	// message will be probably also cryptic
	ostringstream std_err;
	ostringstream std_out;
	reader.get_data(std_out, std_err);
	if (std_err.str().length()) {
		cerr << "An error occur during the execution of objdump:\n\n";
		cerr << std_err.str() << endl;
		return ;
	}

	reader.terminate_process();  // force error code to be acquired

	// required because if objdump stop by signal all above things suceeed
	// (signal error message are not output through stdout/stderr)
	if (reader.error()) {
		cerr << "An error occur during the execution of objdump:\n\n";
		cerr << reader.error_str() << endl;
		return;
	}
}

 
void output_objdump_asm(vector<symbol_entry const *> const & output_symbols,
			string const & app_name)
{
	// this is only an optimisation, we can either filter output by
	// directly calling objdump and rely on the symbol filtering or
	// we can call objdump with the right parameter to just disassemble
	// the needed part. This is a real win only when calling objdump
	// a medium number of times, I dunno if the used threshold is optimal
	// but it is a conservative value.
	size_t const max_objdump_exec = 10;
	if (output_symbols.size() <= max_objdump_exec) {
		for (size_t i = 0 ; i < output_symbols.size() ; ++i) {
			bfd_vma start = output_symbols[i]->sample.vma;
			bfd_vma end  = start + output_symbols[i]->size;
			do_one_output_objdump(output_symbols, app_name,
					      start, end);
		}
	} else {
		do_one_output_objdump(output_symbols, app_name,
				      0, ~(bfd_vma)0);
	}
}
 

void output_source(int argc, char const * argv[],
		   filename_match const & fn_match,
		   bool output_separate_file)
{
	size_t index = get_sort_counter_nr();

	vector<string> filenames =
		samples->select_filename(index, threshold_percent / 100.0,
			do_until_more_than_samples);

	if (!output_separate_file)
		output_header(cout, argc, argv);

	for (size_t i = 0 ; i < filenames.size() ; ++i) {
		if (!fn_match.match(filenames[i]))
			continue;

		ifstream in(filenames[i].c_str());

		if (!in) {
			// it is common to have empty filename due to the lack
			// of debug info (eg _init function) so warn only
			// if the filename is non empty. The case: no debug
			// info at all has already been checked.
			if (filenames[i].length())
				cerr << "op_to_source (warning): unable to "
				     << "open for reading: "
				     << filenames[i] << endl;
		} 

		if (filenames[i].length()) {
			output_one_file(in, filenames[i],output_separate_file);
		}
	}
}

 
void output_one_file(istream & in, string const & filename,
		     bool output_separate_file)
{
	if (!output_separate_file) {
		do_output_one_file(cout, in, filename);
		return;
	}

	string out_filename = filename;

	size_t pos = out_filename.find(source_dir);
	if (pos == 0) {
		out_filename.erase(0, source_dir.length());
	} else if (pos == string::npos) {
		// filename is outside the source dir: ignore this file
		cerr << "op_to_source: file "
		     << '"' << out_filename << '"' << " ignored" << endl;
		return;
	}

	out_filename = output_dir + out_filename;

	string path = dirname(out_filename);
	if (!create_path(path)) {
		cerr << "unable to create directory: "
		     << '"' << path << '"' << endl;
		return;
	}

	// paranoid checking: out_filename and filename must be distinct file.
	if (is_files_identical(filename, out_filename)) {
		cerr << "input and output_filename are identical: "
		     << '"' << filename << '"'
		     << ','
		     << '"' << out_filename << '"'
		     << endl;
		return;
	}

	ofstream out(out_filename.c_str());
	if (!out) {
		cerr << "unable to open output file "
		     << '"' << out_filename << '"' << endl;
	} else
		do_output_one_file(out, in, filename);
}

 
void do_output_one_file(ostream & out, istream & in, string const & filename)
{
	counter_array_t count;
	samples->samples_count(count, filename);

	output_counter_for_file(out, filename, count);

	find_and_output_counter(out, filename, 0, string());

	if (in) {
		string str;
		for (size_t linenr = 1 ; getline(in, str) ; ++linenr) {
			string blank = extract_blank_at_begin(str);

			find_and_output_counter(out, filename, linenr, blank);

			out << str << '\n';
		}
	} else {
		// TODO : we have no input file : for now we just output header
		// so on user can known total nr of samples for this source
		// later we must add code that iterate through symbol in this
		// file to output one annotation for each symbol. To do this we
		// need a select_symbol(filename); in profile_container_t which
		// fall back to the implementation in symbol_container_imp_t
		// using a lazilly build symbol_map sorted by filename
		// (necessary functors already exist in symbol_functors.h)
	}
}

 
bool setup_counter_param(profile_t const & profile)
{
	bool have_counter_info = false;

	for (size_t i = 0 ; i < profile.nr_counters ; ++i) {
		if (!profile.is_open(i))
			continue;

		counter_info[i].enabled = true;

		opd_header const & header = profile.samples[i]->header();
		counter_info[i].ctr_event = header.ctr_event;
		counter_info[i].unit_mask = header.ctr_um;
		counter_info[i].event_count_sample = header.ctr_count;

		counter_info[i].total_samples = samples->samples_count(i);

		have_counter_info = true;
	}

	if (!have_counter_info) {
		cerr << "op_to_source: no counter enabled ?" << endl;
		return false;
	}

	for (size_t i = 0 ; i < samples->get_nr_counters() ; ++i)
		if (counter_info[i].total_samples != 0)
			return true;

	cerr << "op_to_source: the input contains zero samples" << endl;
	return false;
}
 

void output_counter_for_file(ostream & out, string const & filename,
			     counter_array_t const & total_count_for_file)
{
	out << begin_comment << endl
	     << " Total samples for file : " << '"' << filename << '"'
	     << endl;
	output_counter(out, total_count_for_file, false, string());
	out << end_comment << endl << endl;
}


void output_one_counter(ostream & out, size_t counter, size_t total)
{
	out << " ";
	out << counter << " ";
	out << setprecision(4) << (op_ratio(counter, total) * 100.0) << "%";
}


void output_counter(ostream & out, counter_array_t const & counter,
		    bool comment, string const & prefix)
{
	if (comment)
		out << begin_comment;

	if (prefix.length())
		out << " " << prefix;

	for (size_t i = 0 ; i < samples->get_nr_counters() ; ++i)
		if (counter_info[i].enabled)
			output_one_counter(out, counter[i],
					   counter_info[i].total_samples);

	out << " ";

	if (comment)
		out << end_comment;

	out << '\n';
}


symbol_entry const * find_symbol(string const & str_vma)
{
	// do not use the bfd equivalent:
	//  - it does not skip space at begin
	//  - we does not need cross architecture compile so the native
	// strtoull must work, assuming unsigned long long can contain a vma
	// and on 32/64 bits box bfd_vma is 64 bits
	bfd_vma vma = strtoull(str_vma.c_str(), NULL, 16);

	return samples->find_symbol(vma);
}


void find_and_output_symbol(ostream & out, string const & str,
			    string const & blank)
{
	symbol_entry const * symbol = find_symbol(str);

	if (symbol) {
		out << blank;
		output_counter(out, symbol->sample.counter, true, string());
	}
}

 
void find_and_output_counter(ostream & out, string const & str,
			     string const & blank)
{
	// do not use the bfd equivalent:
	//  - it does not skip space at begin
	//  - we does not need cross architecture compile so the native
	// strtoull must work, assuming unsigned long long can contain a vma
	// and on 32/64 bits box bfd_vma is 64 bits
	bfd_vma vma = strtoull(str.c_str(), NULL, 16);

	sample_entry const * sample = samples->find_sample(vma);
	if (sample) {
		out << blank;
		output_counter(out, sample->counter, true, string());
	}
}

 
void find_and_output_counter(ostream & out, string const & filename,
			     size_t linenr, string const & blank)
{
	symbol_entry const * symbol = samples->find_symbol(filename, linenr);
	if (symbol) {
		string const symname =
			demangle ? demangle_symbol(symbol->name)
			: symbol->name;

		output_counter(out, symbol->sample.counter, true, symname);
	}

	counter_array_t counter;
	if (samples->samples_count(counter, filename, linenr)) {
		out << blank;
		output_counter(out, counter, true, string());
	}
}


size_t get_sort_counter_nr()
{
	size_t index = sort_by_counter;

	if (index >= samples->get_nr_counters() ||
	    !counter_info[index].enabled) {
		for (index = 0 ; index < samples->get_nr_counters(); ++index) {
			if (counter_info[index].enabled)
				break;
		}

		if (sort_by_counter != -1)
			cerr << "op_to_source (warning): sort_by_counter "
			     << "invalid or counter[sort_by_counter] disabled "
			     << ": switch to the first valid counter "
			     << index << endl;
	}

	// paranoid checking.
	if (index == samples->get_nr_counters()) {
		cerr << "get_sort_counter_nr() cannot find a counter enabled"
		     << endl;
		exit(EXIT_FAILURE);
	}

	return index;
}

} // op_to_source namespace


int main(int argc, char const * argv[])
{
#if (__GNUC__ >= 3)
	// this improves performance with gcc 3.x a bit
	ios_base::sync_with_stdio(false);
#endif

	/* global var sort_by_counter contains the counter used for sorting
	 * purpose (-1 for sorting on first available counter). This contains
	 * the counter to open (-1 for all). sort_by_counter is specified
	 * through the -c option while counter is specified through the
	 * samples filename suffix #xx. The point here is to allow to see
	 * all counter and specify explicitly which counter to use as sort
	 * order */
	int ctr_mask = -1;

	string const arg = get_options(argc, argv);

	derive_files(arg, image_file, sample_file, ctr_mask);

	validate_counter(ctr_mask, sort_by_counter);

	if (ctr_mask != -1 && sort_by_counter != -1 &&
	    ctr_mask != (1 << sort_by_counter)) {
		cerr << "mismatch between --sort-by-counter and samples filename counter suffix.\n";
		exit(EXIT_FAILURE);
	}

	op_to_source::do_until_more_than_samples = false;
	op_to_source::threshold_percent = with_more_than_samples;
	if (until_more_than_samples) {
		op_to_source::threshold_percent = until_more_than_samples;
		op_to_source::do_until_more_than_samples = true;
	}

	if (source_with_assembly)
		assembly = true;

	if (!objdump_params.empty() && !assembly) {
		cerr << "You can't specificy --objdump-params without assembly output" << endl;
		exit(EXIT_FAILURE);
	}

	string samples_dir = handle_session_options();
	sample_file = relative_to_absolute_path(sample_file, samples_dir);

	filename_match fn_match(output_filter, no_output_filter);

	if (!op_to_source::annotate_source(image_file, sample_file,
					   fn_match, argc, argv))
		return EXIT_FAILURE;

	return EXIT_SUCCESS;
}
