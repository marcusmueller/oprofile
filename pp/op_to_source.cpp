/**
 * @file op_to_source.cpp
 * Annotated source output
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author Philippe Elie <phil_el@wanadoo.fr>
 * @author John Levon <moz@compsoc.man.ac.uk>
 */

#include <fstream>
#include <iomanip>
#include <iostream>
#include <vector>
#include <algorithm>
#include <string>
#include <sstream>

#include <stdio.h>

#include "op_popt.h"

using std::vector;
using std::string;
using std::ostream;
using std::ofstream;
using std::istream;
using std::ifstream;
using std::endl;
using std::cout;
using std::cerr;
using std::hex;
using std::dec;
using std::ostringstream;

#include "samples_container.h"
#include "oprofpp.h"

#include "child_reader.h"
#include "string_manip.h"
#include "file_manip.h"
#include "filename_match.h"
#include "op_events_desc.h"

#include "version.h"

//---------------------------------------------------------------------------
// Free function.
namespace {

string extract_blank_at_begin(const string & str);

double do_ratio(size_t a, size_t total);

}

//---------------------------------------------------------------------------
/// Store the configuration of one counter. Construct from an opd_header
struct counter_setup {
	counter_setup() :
		enabled(false), event_count_sample(0) {}

	void print(ostream& out, op_cpu cpu_type, int counter) const;

	// if false other field are not meaningful.
	bool   enabled;
	u8 ctr_event;
	u8 unit_mask;
	size_t event_count_sample;
	// would be double?
	size_t total_samples;
};

//---------------------------------------------------------------------------
// All the work is made here.
class output {
 public:
	output(int argc, char const * argv[],
	       size_t threshold_percent,
	       bool until_more_than_samples,
	       size_t sort_by_counter,
	       string const & output_dir,
	       string const & source_dir,
	       string const & output_filter,
	       string const & no_output_filter,
	       bool assembly,
	       bool source_with_assembly,
	       int counter_mask);

	~output();

	bool treat_input(const string & image_name, const string & sample_file);

 private:
	/// this output a comment containaing the counter setup and command
	/// line.
	void output_header(ostream & out) const;

	void output_asm(const string & image_name);
	void output_objdump_asm_line(const std::string & str,
		     const vector<const symbol_entry *> & output_symbols,
		     bool & do_output);
	void output_objdump_asm(const vector<const symbol_entry *> & output_symbols, const string & app_name);
	void output_dasm_asm(const vector<const symbol_entry *> & output_symbols, const string & image_name);
	void output_source();

	// output one file unconditionally.
	void output_one_file(istream & in, const string & filename,
		const counter_array_t & total_samples_for_file);
	void do_output_one_file(ostream & out, istream & in, const string & filename,
		const counter_array_t & total_samples_for_file);

	bool setup_counter_param(const opp_samples_files & samples_files);
	bool calc_total_samples();

	void output_counter_for_file(ostream & out, const string & filename,
		const counter_array_t& count);
	void output_counter(ostream & out, const counter_array_t & counter,
		bool comment, const string & prefix = string()) const;
	void output_one_counter(ostream & out, size_t counter, size_t total) const;

	void find_and_output_symbol(ostream & out, const string & str,
		const string & blank) const;
	void find_and_output_counter(ostream & out, const string & str,
		const string & blank) const;

	void find_and_output_counter(ostream & out, const string & filename,
		size_t linenr, const string & blank) const;

	size_t get_sort_counter_nr() const;

	// used to output the command line.
	int argc;
	char const ** argv;

	// hold all info for samples
	samples_container_t * samples;

	// samples files header are stored here
	counter_setup counter_info[OP_MAX_COUNTERS];

	// FIXME : begin_comment, end_comment must be based on the current
	// extension and must be properties of source filename.
	string begin_comment;
	string end_comment;

	// This is usable only if one of the counter has been setup as: count
	// some sort of cycles events. In the other case trying to use it to
	// translate samples count to time is a non-sense.
	double cpu_speed;

	// percent from where the source file is output.
	size_t threshold_percent;

	// sort source by this counter.
	size_t sort_by_counter;

	op_cpu cpu_type;

	bool until_more_than_samples;

	string output_dir;
	string source_dir;
	bool output_separate_file;

	filename_match fn_match;

	bool assembly;
	bool source_with_assembly;

	int counter_mask;
};

//---------------------------------------------------------------------------

namespace {

// Return the substring at beginning of str which is only made of blank or tabulation.
string extract_blank_at_begin(const string & str)
{
	size_t end_pos = str.find_first_not_of(" \t");
	if (end_pos == string::npos)
		end_pos = 0;

	return str.substr(0, end_pos);
}

inline double do_ratio(size_t counter, size_t total)
{
	return total == 0 ? 1.0 : ((double)counter / total);
}

} // anonymous namespace

// Convenience function : just output the setup of one counter.
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
//---------------------------------------------------------------------------

output::output(int argc_, char const * argv_[],
	       size_t threshold_percent_,
	       bool until_more_than_samples_,
	       size_t sort_by_counter_,
	       string const & output_dir_,
	       string const & source_dir_,
	       string const & output_filter_,
	       string const & no_output_filter_,
	       bool assembly_,
	       bool source_with_assembly_,
	       int counter_mask_)
	:
	argc(argc_),
	argv(argv_),
	samples(0),
	begin_comment("/*"),
	end_comment("*/"),
	cpu_speed(0.0),
	threshold_percent(threshold_percent_),
	sort_by_counter(sort_by_counter_),
	cpu_type(CPU_NO_GOOD),
	until_more_than_samples(until_more_than_samples_),
	output_dir(output_dir_),
	source_dir(source_dir_),
	output_separate_file(false),
	fn_match(output_filter_, no_output_filter_),
	assembly(assembly_),
	source_with_assembly(source_with_assembly_),
	counter_mask(counter_mask_)
{
	if (source_dir.empty() == false) {
		output_separate_file = true;

		source_dir = relative_to_absolute_path(source_dir);
		if (source_dir.length() &&
		    source_dir[source_dir.length() - 1] != '/')
			source_dir += '/';
	}

	if (output_dir.empty() == false || output_separate_file == true) {
		output_separate_file = true;

		output_dir = relative_to_absolute_path(output_dir);
		if (output_dir.length() &&
		    output_dir[output_dir.length() - 1] != '/')
			output_dir += '/';


		if (create_dir(output_dir) == false) {
			cerr << "unable to create " << output_dir
			     << " directory: " << endl;
			exit(EXIT_FAILURE);
		}
	}

	if (output_separate_file == true && output_dir == source_dir) {
		cerr << "You can not specify the same directory for "
		     << "--output-dir and --source-dir" << endl;
		exit(EXIT_FAILURE);
	}

	OutSymbFlag flag = osf_details;
	if (!assembly)
		flag = static_cast<OutSymbFlag>(flag | osf_linenr_info);
	samples = new samples_container_t(false, flag, false, counter_mask);
}

output::~output()
{
	delete samples;
}

// build a counter_setup from a header.
bool output::setup_counter_param(const opp_samples_files & samples_files)
{
	bool have_counter_info = false;

	for (size_t i = 0 ; i < samples_files.nr_counters ; ++i) {
		if (samples_files.is_open(i) == false)
			continue;

		counter_info[i].enabled = true;

		opd_header const & header = samples_files.samples[i]->header();
		counter_info[i].ctr_event = header.ctr_event;
		counter_info[i].unit_mask = header.ctr_um;
		counter_info[i].event_count_sample = header.ctr_count;

		have_counter_info = true;
	}

	if (!have_counter_info)
		cerr << "op_to_source: no counter enabled ?" << endl;

	return have_counter_info;
}

bool output::calc_total_samples()
{
	for (size_t i = 0 ; i < samples->get_nr_counters() ; ++i)
		counter_info[i].total_samples = samples->samples_count(i);

	for (size_t i = 0 ; i < samples->get_nr_counters() ; ++i)
		if (counter_info[i].total_samples != 0)
			return true;

	return false;
}

void output::output_one_counter(ostream & out, size_t counter, size_t total) const
{
	out << " ";
	out << counter << " ";
	out << std::setprecision(4) << (do_ratio(counter, total) * 100.0) << "%";
}

void output::output_counter(ostream & out, const counter_array_t & counter, 
			    bool comment, const string & prefix) const
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

// Complexity: log(nr symbols)
void output::find_and_output_symbol(ostream & out, const string & str, const string & blank) const
{
	bfd_vma vma = strtoul(str.c_str(), NULL, 16);

	const symbol_entry* symbol = samples->find_symbol(vma);

	if (symbol) {
		out << blank;
		output_counter(out, symbol->sample.counter, true, string());
	}
}

// Complexity: log(nr samples))
void output::find_and_output_counter(ostream & out, const string & str, const string & blank) const
{
	bfd_vma vma = strtoul(str.c_str(), NULL, 16);

	const sample_entry * sample = samples->find_sample(vma);
	if (sample) {
		out << blank;
		output_counter(out, sample->counter, true, string());
	}
}

// Complexity: log(nr symbols) + log(nr filename/linenr)
void output::find_and_output_counter(ostream & out, const string & filename, size_t linenr, const string & blank) const
{
	const symbol_entry * symbol = samples->find_symbol(filename, linenr);
	if (symbol)
		output_counter(out, symbol->sample.counter, true, symbol->name);

	counter_array_t counter;
	if (samples->samples_count(counter, filename, linenr)) {
		out << blank;
		output_counter(out, counter, true, string());
	}
}

/**
 * output::output_objdump_asm_line - helper for output_objdump_asm
 * @param str the string reading from objdump output
 * @param output_symbols the symbols set to output
 * @param do_output in/out parameter which says if the current line
 * must be output
 *
 */
void output::
output_objdump_asm_line(const std::string & str,
			const vector<const symbol_entry *> & output_symbols,
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

	if (str[pos] != ':') {  // is the line contain a symbol
		// do not use the bfd equivalent:
		//  - it does not skip space at begin
		//  - we does not need cross architecture compile so the native
		// strtoul must work (assuming unsigned long can contain a vma)
		bfd_vma vma = strtoul(str.c_str(), NULL, 16);

		const symbol_entry* symbol = samples->find_symbol(vma);

		// ! complexity: linear in number of symbol must use sorted
		// by address vector and lower_bound ?
		// Note this use a pointer comparison. It work because symbols
		// pointer are unique
		if (find(output_symbols.begin(),
		       output_symbols.end(), symbol) != output_symbols.end()) {
			// an error due to ambiguity in the input: source file
			// mixed with asm contain a line which is taken as a
			// valid symbol, in doubt turn output on
			do_output = true;
		} else if (threshold_percent == 0) {
			// if the user have not requested threshold we must
			// output all symbols even if it contains no samples.
			do_output = true;
		} else {
			do_output = false;
		}

		if (do_output)
			find_and_output_symbol(cout, str, "");

	} else { // not a symbol, probably an asm line.
		if (do_output)
			find_and_output_counter(cout, str, " ");
	}
}

/**
 * output::output_objdump_asm - output asm disassembly
 * @param output_symbols the set of symbols to output
 *
 * Output asm (optionnaly mixed with source) annotated
 * with samples using objdump as external disassembler.
 * This is the generic implementation if our own disassembler
 * do not work for this architecture.
 */
void output::output_objdump_asm(const vector<const symbol_entry *> & output_symbols, const string & app_name)
{
	vector<string> args;
	args.push_back("-d");
	args.push_back("--no-show-raw-insn");
	if (source_with_assembly)
		args.push_back("-S");

	args.push_back(app_name);
	child_reader reader("objdump", args);
	if (reader.error())
		// child_reader output an error message, the only way I see to
		// go here is a failure to exec objdump.
		return;

	// to filter output of symbols (filter based on command line options)
	bool do_output = true;

	string str;
	while (reader.getline(str)) {
		output_objdump_asm_line(str, output_symbols, do_output);
		if (do_output)
			cout << str << '\n';
	}

	// objdump always return SUCESS so we must rely on the stderr state
	// of objdump. If objdump error message is cryptic our own error
	// message will be probably also cryptic
	ostringstream std_err;
	ostringstream std_out;
	reader.get_data(std_out, std_err);
	if (std_err.str().length()) {
		cerr << "An error occur during the execution of objdump:\n\n";
		cerr << std_err.str() << endl;
	}
}

/**
 * output::output_dasm_asm - output asm disassembly
 * @param output_symbols the set of symbols to output
 *
 * Output asm (optionnaly mixed with source) annotated
 * with samples using dasm as external disassembler.
 */
void output::output_dasm_asm(const vector<const symbol_entry *> & /*output_symbols*/, const string  & /*image_name*/)
{
	// Not yet implemented :/
}

void output::output_asm(const string & image_name)
{
	// select the subset of symbols which statisfy the user requests
	size_t index = get_sort_counter_nr();

	vector<const symbol_entry *> output_symbols;

	double threshold = threshold_percent / 100.0;

	samples->select_symbols(output_symbols, index,
			       threshold, until_more_than_samples);

	output_header(cout);

	output_objdump_asm(output_symbols, image_name);
}

void output::output_counter_for_file(ostream& out, const string & filename,
	const counter_array_t & total_count_for_file)
{
	out << begin_comment << endl
		<< " Total samples for file : " << '"' << filename << '"'
		<< endl;
	output_counter(out, total_count_for_file, false, string());
	out << end_comment << endl << endl;
}

// Pre condition:
//  the file has not been output.
//  in is a valid file stream ie ifstream(filename)
// Post condition:
//  the entire file source and the associated samples has been output to
//  the standard output.
void output::output_one_file(istream & in, const string & filename,
	const counter_array_t & total_count_for_file)
{
	if (!output_separate_file) {
		do_output_one_file(cout, in, filename, total_count_for_file);
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

	// filter
	if (!fn_match.match(filename))
		return;

	out_filename = output_dir + out_filename;

	string path = dirname(out_filename);
	if (create_path(path) == false) {
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
		do_output_one_file(out, in, filename, total_count_for_file);
}

// Pre condition:
//  the file has not been output.
//  in is a valid file stream ie ifstream(filename)
// Post condition:
//  the entire file source and the associated samples has been output to
//  the standard output.
void output::do_output_one_file(ostream& out, istream & in,
				const string & filename,
				const counter_array_t & total_count_for_file)
{
	output_counter_for_file(out, filename, total_count_for_file);

	find_and_output_counter(out, filename, 0, string());

	string str;
	for (size_t linenr = 1 ; getline(in, str) ; ++linenr) {
		string blank = extract_blank_at_begin(str);

		find_and_output_counter(out, filename, linenr, blank);

		out << str << '\n';
	}
}

void output::output_source()
{
	size_t index = get_sort_counter_nr();

	vector<string> filenames;
	samples->select_filename(filenames, index, threshold_percent / 100.0,
				until_more_than_samples);

	if (output_separate_file == false)
		output_header(cout);

	for (size_t i = 0 ; i < filenames.size() ; ++i) {
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
		} else {
			counter_array_t count;
			samples->samples_count(count, filenames[i]);

			output_one_file(in, filenames[i], count);
		}
	}
}

size_t output::get_sort_counter_nr() const
{
	size_t index = sort_by_counter;

	if (index >= samples->get_nr_counters() ||
	    !counter_info[index].enabled) {
		for (index = 0 ; index < samples->get_nr_counters(); ++index) {
			if (counter_info[index].enabled)
				break;
		}

		if (sort_by_counter != size_t(-1))
			cerr << "op_to_source (warning): sort_by_counter "
			     << "invalid or counter[sort_by_counter] disabled "
			     << ": switch to the first valid counter "
			     << index << endl;
	}

	// paranoid checking.
	if (index == samples->get_nr_counters())
		throw "output::get_sort_counter_nr() cannot find a counter enabled";

	return index;
}

// this output a comment containing the counter setup and command line. It can
// be usefull for beginners to understand how the command line is interpreted.
void output::output_header(ostream& out) const
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
			if (until_more_than_samples == false) {
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

bool output::treat_input(const string & image_name, const string & sample_file)
{
	// this lexcical scope just optimize the memory use by relaxing
	// the op_bfd and opp_samples_files as short as we can.
	{
	// this order of declaration is required to ensure proper
	// initialisation of oprofpp
	opp_samples_files samples_files(sample_file, counter_mask);
	op_bfd abfd(samples_files, image_name);

	if (!assembly && !abfd.have_debug_info()) {
		cerr << "Request for source file annotated "
		     << "with samples but no debug info available" << endl;
		exit(EXIT_FAILURE);
	}

	cpu_speed = samples_files.first_header().cpu_speed;
	uint tmp = samples_files.first_header().cpu_type;
	cpu_type = static_cast<op_cpu>(tmp);

	samples->add(samples_files, abfd);

	if (!setup_counter_param(samples_files))
		return false;
	}

	if (!calc_total_samples()) {
		cerr << "op_to_source: the input contains zero samples" << endl;
		return false;
	}

	if (assembly)
		output_asm(image_name);
	else
		output_source();

	return true;
}

//---------------------------------------------------------------------------

static int with_more_than_samples;
static int until_more_than_samples;
static int showvers;
static int sort_by_counter = -1;
static int assembly;
static int source_with_assembly;
static char const * source_dir;
static char const * output_dir;
static char const * output_filter;
static char const * no_output_filter;

/* -k is reserved for --show-shared-libs */
static struct poptOption options[] = {
	{ "samples-file", 'f', POPT_ARG_STRING, &samplefile, 0, "image sample file", "file", },
	{ "image-file", 'i', POPT_ARG_STRING, &imagefile, 0, "image file", "file", },
	{ "verbose", 'V', POPT_ARG_NONE, &verbose, 0, "verbose output", NULL, },
	{ "demangle", 'd', POPT_ARG_NONE, &demangle, 0, "demangle GNU C++ symbol names", NULL, },
	{ "with-more-than-samples", 'w', POPT_ARG_INT, &with_more_than_samples, 0,
	  "show all source file if the percent of samples in this file is more than argument", "[0-100]" },
	{ "until-more-than-samples", 'm', POPT_ARG_INT, &until_more_than_samples, 0,
	  "show all source files until the percent of samples specified is reached", "[0-100]" },
	{ "sort-by-counter", 'c', POPT_ARG_INT, &sort_by_counter, 0,
	  "sort by counter", "counter nr", },
	{ "source-dir", 0, POPT_ARG_STRING, &source_dir, 0, "source directory", "directory name" },
	{ "output-dir", 0, POPT_ARG_STRING, &output_dir, 0, "output directory", "directory name" },
        { "output", 0, POPT_ARG_STRING, &output_filter, 0, "output filename filter", "filter string" },
        { "no-output", 0, POPT_ARG_STRING, &no_output_filter, 0, "no output filename filter", "filter string" },
	{ "assembly", 'a', POPT_ARG_NONE, &assembly, 0, "output assembly code", NULL },
	{ "source-with-assembly", 's', POPT_ARG_NONE, &source_with_assembly, 0, "output assembly code mixed with source", NULL },
	{ "--exclude-symbol", 'e', POPT_ARG_STRING, &exclude_symbols_str, 0, "exclude these comma separated symbols", "symbol_name" },
	{ "version", 'v', POPT_ARG_NONE, &showvers, 0, "show version", NULL, },
	POPT_AUTOHELP
	{ NULL, 0, 0, NULL, 0, NULL, NULL, },
};

/**
 * get_options - process command line
 * @param argc program arg count
 * @param argv program arg array
 * @param image_name where to store the image filename
 * @param sample_file ditto for sample filename
 * @param counter where to put the counter command line argument
 *
 * Process the arguments, fatally complaining on error.
 */
static void get_options(int argc, char const * argv[],
			string & image_name, string & sample_file,
			int & counter)
{
	poptContext optcon;

	optcon = op_poptGetContext(NULL, argc, argv, options, 0);

	if (showvers) {
		show_version(argv[0]);
	}

	if (with_more_than_samples && until_more_than_samples) {
		fprintf(stderr, "op_to_source: --with-more-than-samples and -until-more-than-samples can not specified together\n");
		exit(EXIT_FAILURE);
	}

	/* non-option file, either a sample or binary image file */
	char const * file = poptGetArg(optcon);

	opp_treat_options(file, optcon, image_name, sample_file,
			  counter, sort_by_counter);

	poptFreeContext(optcon);
}

//---------------------------------------------------------------------------

int main(int argc, char const * argv[])
{
#if (__GNUC__ >= 3)
	// this improves performance with gcc 3.x a bit
	std::ios_base::sync_with_stdio(false);
#endif

	string image_name;
	string sample_file;
	/* global var sort_by_counter contains the counter used for sorting
	 * purpose (-1 for sorting on first available counter). This contains
	 * the counter to open (-1 for all). sort_by_counter is specified
	 * through the -c option while counter is specified through the
	 * samples filename suffix #xx. The point here is to allow to see
	 * all counter and specify explicitly which counter to use as sort
	 * order */
	int counter = -1;

	get_options(argc, argv, image_name, sample_file, counter);

	if (counter != -1 && sort_by_counter != -1 &&
	    counter != (1 << sort_by_counter)) {
		cerr << "mismatch between --sort-by-counter and samples filename counter suffix.\n";
		exit(EXIT_FAILURE);
	}

	try {
		bool do_until_more_than_samples = false;
		int threshold_percent = with_more_than_samples;
		if (until_more_than_samples) {
			threshold_percent = until_more_than_samples;
			do_until_more_than_samples = true;
		}

		if (source_with_assembly)
			assembly = 1;

		output output(argc, argv,
			      threshold_percent,
			      do_until_more_than_samples,
			      sort_by_counter,
			      output_dir ? output_dir : "",
			      source_dir ? source_dir : "",
			      output_filter ? output_filter : "*",
			      no_output_filter ? no_output_filter : "",
			      assembly, source_with_assembly, -1);

		if (output.treat_input(image_name, sample_file) == false)
			return EXIT_FAILURE;
	}

	catch (const string & e) {
		cerr << "op_to_source: Exception : " << e << endl;
		return EXIT_FAILURE;
	}
	catch (char const * e) {
		cerr << "op_to_source: Exception : " << e << endl;
		return EXIT_FAILURE;
	}
	catch (...) {
		cerr << "op_to_source: Unknown exception : really sorry " << endl;
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
