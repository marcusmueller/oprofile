/* COPYRIGHT (C) 2001 Philippe Elie
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59 Temple
 * Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <sys/stat.h>

#include <fstream>
#include <iomanip>
#include <vector>
#include <set>
#include <algorithm>
#include <iomanip>

#include <stdio.h>
#include <fnmatch.h>
#include <popt.h>

using namespace std;

#include "opf_filter.h"
#include "oprofpp.h"

#include "../version.h"

//---------------------------------------------------------------------------
// Forward declaration.

class counter_setup;

//---------------------------------------------------------------------------
// Free function.
namespace {

string extract_blank_at_begin(const string & str);

ostream & operator<<(ostream & out, const counter_setup &);

double do_ratio(size_t a, size_t total);

bool create_dir(const std::string & dir);
bool create_path_name(const std::string& path);

}

// The correct value is passed by oprofpp on standard input.
uint op_nr_counters = 2;

// This have nothing to do here...

/// a class to encapsulate filename matching. The behavior look like
/// fnmatch(pattern, filename, 0); eg * match '/' character. See the man page
/// of fnmatch for further details.
class filename_match {
 public:
	// multiple pattern must be separate by ','. each pattern can contain
	// leading and trailing blank which are strip from pattern.
	filename_match(const std::string & include_patterns,
		       const std::string & exclude_patterns);


	/// return filename match include_pattern and not match exclude_pattern
	bool match(const std::string & filename);

 private:
	/// ctor helper.
	static void build_pattern(std::vector<std::string>& result, 
				  const std::string & patterns);

	/// match helper
	static bool match(const std::vector<std::string>& patterns,
			  const std::string & filename);

	std::vector<std::string> include_pattern;
	std::vector<std::string> exclude_pattern;
};

//---------------------------------------------------------------------------
// Just allow to read one line in advance and to put_back this line.
class input {
 public:
	input(istream & in_) : in(in_), put_back_area_valid(false) {}

	bool read_line(string & str);
	void put_back(const string &);

 private:
	istream & in;
	string put_back_area;
	bool put_back_area_valid;
};

//---------------------------------------------------------------------------
// store a complete source file.
struct source_file {
	source_file();
	source_file(istream & in);

	vector<string> file_line;
};

//---------------------------------------------------------------------------
/// Store the configuration of one counter. Construct from an opd_header
struct counter_setup {
	counter_setup() : 
		enabled(false), event_count_sample(0) {}

	// if false other field are not meaningful.
	bool   enabled;
	string event_name;
	string help_string;
	u32 unit_mask;
	string unit_mask_help;     // The string help for the unit mask
	size_t event_count_sample;
	// would be double?
	size_t total_samples;
};

//---------------------------------------------------------------------------
// All the work is made here.
class output {
 public:
	output(int argc, char const * argv[],
	       bool have_linenr_info_,
	       size_t threshold_percent,
	       bool until_more_than_samples,
	       size_t sort_by_counter,
	       const char * output_dir,
	       const char * source_dir,
	       const char * output_filter,
	       const char * no_output_filter);

	bool treat_input(input &);

	void debug_dump_vector() const;

 private:
	/// this output a comment containaing the counter setup and command
	/// the line.
	void output_header(ostream& out) const;

	void output_asm(input & in);
	void output_source();

	// output one file unconditionally.
	void output_one_file(istream & in, const string & filename, 
			     const counter_array_t & total_samples_for_file);
	void do_output_one_file(ostream& out, istream & in, const string & filename, 
				const counter_array_t & total_samples_for_file);

	// accumulate counter for a given (filename, linenr).
	void accumulate_and_output_counter(ostream& out, const string & filename, size_t linenr, 
					   const string & blank);

	void build_samples_containers();

	bool setup_counter_param();
	bool calc_total_samples();

	void output_counter_for_file(ostream& out, const string & filename, 
				     const counter_array_t& count);
	void output_counter(ostream& out, const counter_array_t & counter, 
			    bool comment, const string & prefix) const;
	void output_one_counter(ostream& out, size_t counter, size_t total) const;

	void find_and_output_symbol(ostream& out, const string& str, const char * blank) const;
	void find_and_output_counter(ostream& out, const string& str, const char * blank) const;

	void find_and_output_counter(ostream& out, const string & filename,
				     size_t linenr) const;

	size_t get_sort_counter_nr() const;

	// debug.
	bool sanity_check_symbol_entry(size_t index) const;

	// this order of declaration is required to ensure proper
	// initialisation of oprofpp
	opp_samples_files samples_files;
	opp_bfd abfd;

	// used to output the command line.
	int argc;
	char const ** argv;

	// The symbols collected by oprofpp sorted by increased vma, provide
	// also a sort order on samples count for each counter.
	symbol_container_t symbols;

	// The samples count collected by oprofpp sorted by increased vma,
	// provide also a sort order on (filename, linenr)
	sample_container_t samples;

	// samples files header are stored here
	counter_setup counter_info[OP_MAX_COUNTERS];

	// FIXME : begin_comment, end_comment must be based on the current 
	// extension and must be properties of source_file.
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

	int cpu_type;

	bool until_more_than_samples;

	bool have_linenr_info;

	std::string output_dir;
	std::string source_dir;
	bool output_separate_file;

	filename_match fn_match;
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

// Convenience function : just output the setup of one counter.
ostream & operator<<(ostream & out, const counter_setup & rhs)
{
	out << (rhs.enabled ? "enabled :" : "disabled");

	if (rhs.enabled) {
		out << " ";

		out << rhs.event_name 
		    << " (" << rhs.help_string << ")" << endl;

		out << "unit mask : "
		    << "0x" << hex << setfill('0') << setw(2) 
		    << rhs.unit_mask 
		    << dec
		    << " (" << rhs.unit_mask_help << ")"
		    << " event_count : " << rhs.event_count_sample 
		    << " total samples : " << rhs.total_samples;
	}

	return out;
}

inline double do_ratio(size_t counter, size_t total)
{
	return total == 0 ? 1.0 : ((double)counter / total);
}

bool create_dir(const std::string & dir)
{
	if (access(dir.c_str(), F_OK)) {
		if (mkdir(dir.c_str(), 0700)) {
			return false;
		}
	}

	return true;
}

bool create_path_name(const std::string& path)
{
	std::vector<std::string> path_component;

	size_t slash = 0;
	while (slash < path.length()) {
		size_t new_pos = path.find_first_of('/', slash);
		if (new_pos == std::string::npos) {
			new_pos = path.length();
		}

		path_component.push_back(path.substr(slash, (new_pos - slash) + 1));
		slash = new_pos + 1;

	}

	std::string dir_name;
	for (size_t i = 0 ; i < path_component.size() ; ++i) {
		dir_name += '/' + path_component[i];
		if (create_dir(dir_name) == false)
			return false;
	}

	return true;
}

/**
 * pathname - get the path component of a filename
 * @file_name: filename
 *
 * Returns the path name of a filename with trailing '/' removed.
 */
std::string const pathname(std::string const & file_name)
{
	std::string result = file_name;

	std::string::size_type slash = result.find_last_of('/');
	if (slash != std::string::npos)
		result.erase(slash, result.length() - slash);

	return result;
}

} // anonymous namespace

//---------------------------------------------------------------------------
filename_match::filename_match(const std::string & include_patterns,
			       const std::string & exclude_patterns)
{
	build_pattern(include_pattern, include_patterns);
	build_pattern(exclude_pattern, exclude_patterns);
}

bool filename_match::match(const std::string & filename)
{
	bool ok = match(include_pattern, filename);
	if (ok == true)
		ok = !match(exclude_pattern, filename);

	return ok;
}

bool filename_match::match(const std::vector<std::string>& patterns,
			   const std::string & filename)
{
	bool ok = false;
	for (size_t i = 0 ; i < patterns.size() && ok == false ; ++i) {
		if (fnmatch(patterns[i].c_str(), filename.c_str(), 0) != FNM_NOMATCH)
			ok = true;
	}

	return ok;
}

void filename_match::build_pattern(std::vector<std::string>& result, 
				   const std::string & patterns)
{
	std::string temp = patterns;

	// unquote the pattern if necessary
	if (temp.find_first_of('\'') == 0 &&
	    temp.find_last_of('\'')  == temp.length() - 1) {
		temp =  temp.substr(1, temp.length() - 2);
	}

	// separate the patterns 
	size_t last_pos = 0;
	for (size_t pos = 0 ; pos != temp.length() ; ) {
		pos = temp.find_first_of(',', last_pos);
		if (pos == std::string::npos)
			pos = temp.length();

		std::string pat = temp.substr(last_pos, pos - last_pos);
		result.push_back(pat);

		if (pos != temp.length())
			last_pos = pos + 1;
	}
}

//--------------------------------------------------------------------------

void sample_entry::debug_dump(ostream & out) const 
{
	if (file_loc.filename.length())
		out << file_loc.filename << ":" << file_loc.linenr << " ";

	out << hex << vma << dec << " ";

	for (size_t i = 0 ; i < op_nr_counters ; ++i)
		out << counter[i] << " ";
}

//--------------------------------------------------------------------------

void symbol_entry::debug_dump(ostream & out) const
{

	out << "[" << name << "]" << endl;

	out << "counters number range [" << first << ", " << last << "[" << endl;

	sample.debug_dump(out);
}

//---------------------------------------------------------------------------

bool input::read_line(string & str)
{
	if (put_back_area_valid) {
		put_back_area_valid = false;

		str = put_back_area;

		return true;
	}

	return getline(in, str);
}

void input::put_back(const string & str)
{
	if (put_back_area_valid) {
		throw "attempt to put_back() but put_back area full";
	}
  
	put_back_area_valid = true;

	put_back_area = str;
}

//---------------------------------------------------------------------------

source_file::source_file()
{
}
 
source_file::source_file(istream & in)
{
	string str;
	while (getline(in, str))
		file_line.push_back(str);
}

//---------------------------------------------------------------------------

output::output(int argc_, char const * argv_[],
	       bool have_linenr_info_,
	       size_t threshold_percent_,
	       bool until_more_than_samples_,
	       size_t sort_by_counter_,
	       const char * output_dir_,
	       const char * source_dir_,
	       const char * output_filter_,
	       const char * no_output_filter_)
	: 
	samples_files(),
	abfd(samples_files.header[samples_files.first_file]),
	argc(argc_),
	argv(argv_),
	begin_comment("/*"),
	end_comment("*/"),
	cpu_speed(0.0),
	threshold_percent(threshold_percent_),
	sort_by_counter(sort_by_counter_),
	cpu_type(-1),
	until_more_than_samples(until_more_than_samples_),
	have_linenr_info(have_linenr_info_),
	output_dir(output_dir_ ? output_dir_ : ""),
	source_dir(source_dir_ ? source_dir_ : ""),
	output_separate_file(false),
	fn_match(output_filter_ ? output_filter_ : "*", 
		 no_output_filter_ ? no_output_filter_ : "")
{
	if (have_linenr_info && !abfd.have_debug_info()) {
		std:: cerr << "Request for source file annotated "
			   << "with sample but no debug info available"
			   << std::endl;

		exit(EXIT_FAILURE);
	}

	if (!output_dir.empty() || !source_dir.empty()) {
		char* temp;

		output_separate_file = true;

		temp = opd_relative_to_absolute_path(output_dir.c_str(), NULL);
		output_dir = temp;
		opd_free(temp);

		temp = opd_relative_to_absolute_path(source_dir.c_str(), NULL);
		source_dir = temp;
		opd_free(temp);

		if (output_dir == source_dir) {
			cerr << "You can not specify the same directory for "
			     << "--output-dir and --source-dir" << endl;

			exit(EXIT_FAILURE);
		}

		if (create_dir(output_dir) == false) {
			cerr << "unable to create " << output_dir << " directory: " << endl;

			exit(EXIT_FAILURE);
		}
	}
}

void output::debug_dump_vector() const
{
	cerr << "total samples :";

	for (size_t i = 0 ; i < op_nr_counters ; ++i)
		cerr << " " << counter_info[i].total_samples;
	
	cerr << endl;

	for (size_t i = 0 ; i < symbols.size() ; ++i) {

		symbols[i].debug_dump(cerr);
		cerr << endl;

		for (size_t j = symbols[i].first ; j < symbols[i].last; ++j) {
			samples[j].debug_dump(cerr);
			cerr << endl;
		}
	}
}

// build a counter_setup from a header.
bool output::setup_counter_param()
{
	bool have_counter_info = false;

	for (size_t i = 0 ; i < op_nr_counters ; ++i) {
		if (samples_files.is_open(i) == false)
			continue;

		counter_info[i].enabled = true;
		counter_info[i].event_name = samples_files.ctr_name[i];
		counter_info[i].help_string = samples_files.ctr_desc[i];
		counter_info[i].unit_mask = samples_files.header[i]->ctr_um;
		counter_info[i].unit_mask_help = samples_files.ctr_um_desc[i] ? samples_files.ctr_um_desc[i] : "Not set";
		counter_info[i].event_count_sample = samples_files.header[i]->ctr_count;

		have_counter_info = true;
	}

	if (!have_counter_info) {
		cerr << "opf_filter: malformed input, expect at least one counter description" << endl;
		
		return false;
	}

	return true;
}

bool output::calc_total_samples()
{
	for (size_t i = 0 ; i < op_nr_counters ; ++i)
		counter_info[i].total_samples = 0;

	for (size_t i = 0 ; i < symbols.size() ; ++i) {
		for (size_t j = 0 ; j < op_nr_counters ; ++j)
			counter_info[j].total_samples += symbols[i].sample.counter[j];
	}

	if (sanity_check) {
		counter_array_t total_counter;

		for (size_t i = 0 ; i < samples.size() ; ++i) {
			total_counter += samples[i].counter;
		}

		for (size_t i = 0 ; i < op_nr_counters ; ++i) {
			if (total_counter[i] != counter_info[i].total_samples) {
				cerr << "opf_filter: output::calc_total_samples() : bad counter accumulation"
				     << " " << total_counter[i] 
				     << " " << counter_info[i].total_samples
				     << endl;
				exit(EXIT_FAILURE);
			}
		}
	}

	for (size_t i = 0 ; i < op_nr_counters ; ++i)
		if (counter_info[i].total_samples != 0)
			return true;

	return false;
}

void output::output_one_counter(ostream& out, size_t counter, size_t total) const
{

	out << " ";

	out << counter << " ";

	out << setprecision(4) << (do_ratio(counter, total) * 100.0) << "%";
}

void output::
output_counter(ostream& out, const counter_array_t & counter, bool comment, 
	       const string & prefix) const
{
	if (comment)
		out << begin_comment;

	if (prefix.length())
		out << " " << prefix;

	for (size_t i = 0 ; i < op_nr_counters ; ++i)
		if (counter_info[i].enabled)
			output_one_counter(out, counter[i], counter_info[i].total_samples);

	out << " ";
      
	if (comment)
		out << end_comment;

	out << '\n';
}

// Complexity: log(container.size())
void output::find_and_output_symbol(ostream& out, const string& str, const char * blank) const
{
	bfd_vma vma = strtoul(str.c_str(), NULL, 16);

	const symbol_entry* symbol = symbols.find_by_vma(vma);

	if (symbol) {
		out <<  blank;

		output_counter(out, symbol->sample.counter, true, string());
	}
}

// Complexity: log(samples.size())
void output::find_and_output_counter(ostream& out, const string& str, const char * blank) const
{
	bfd_vma vma = strtoul(str.c_str(), NULL, 16);

	const sample_entry * sample = samples.find_by_vma(vma);
	if (sample) {
		out <<  blank;

		output_counter(out, sample->counter, true, string());
	}
}

// Complexity: log(symbols.size())
void output::find_and_output_counter(ostream& out, const string & filename, size_t linenr) const
{
	const symbol_entry * symbol = symbols.find(filename, linenr);
	if (symbol)
		output_counter(out, symbol->sample.counter, true, symbol->name);
}

void output::output_asm(input & in) 
{
	// select the subset of symbols which statisfy the user requests
	size_t index = get_sort_counter_nr();

	vector<const symbol_entry*> v;
	symbols.get_symbols_by_count(index , v);

	vector<const symbol_entry*> output_symbols;

	double threshold = threshold_percent / 100.0;

	for (size_t i = 0 ; i < v.size() && threshold >= 0 ; ++i) {
		double const percent = do_ratio(v[i]->sample.counter[index],
					  counter_info[index].total_samples);

		if (until_more_than_samples || percent >= threshold) {
			output_symbols.push_back(v[i]);
		}

		if (until_more_than_samples) {
			threshold -=  percent;
		}
	}

	output_header(cout);

	// we want to avoid output of function that contain zero sample,
	// these symbol are not in our set of symbol so we can detect this
	// case and turn off outputting.
	bool do_output = true;

	string str;
	while (in.read_line(str)) {
		if (str.length())  {
			// Yeps, output of objdump is a human read-able form
			// and contain a few ambiguity so this code is fragile

			// line of interest are: "[:space:]*[:xdigit:]?[ :]"
			// the last char of this regexp dis-ambiguate between
			// a symbol line and an asm line. If source contain
			// line of this form an ambiguity occur and we must
			// rely on the robustness of this code.

			size_t pos = 0;
			while (pos < str.length() && isspace(str[pos]))
			       ++pos;

			if (pos == str.length() || !isxdigit(str[pos])) {
				if (do_output)
					cout << str << '\n';
				continue;				
			}

			while (pos < str.length() && isxdigit(str[pos]))
			       ++pos;

			if (pos == str.length() || 
			    (!isspace(str[pos]) && str[pos] != ':')) {
				if (do_output)
					cout << str << '\n';
				continue;				
			}

			if (str[pos] != ':') { // is the line contain a symbol
				bfd_vma vma = strtoul(str.c_str(), NULL, 16);

				const symbol_entry* symbol = symbols.find_by_vma(vma);

				// ! complexity: linear in number of symbol
				// must use sorted by address vector and
				// lower_bound ?
				// Note this use a pointer comparison. It work
				// because symbols pointer are unique
				if (find(output_symbols.begin(), 
					 output_symbols.end(), symbol) != output_symbols.end()) {
					// probably an error due to ambiguity
					// in the input: source file mixed with
					// asm contain a line which is taken as
					// a valid symbol, in doubt it is
					// probably better to turn output on.
					do_output = true;
				} else if (threshold_percent == 0) {
					// if the user have not requested
					// threshold we must output all symbols
					// even if it contains no samples.
					do_output = true;
				} else {
					do_output = false;
				}

				if (do_output) {
					find_and_output_symbol(cout, str, "");
				}
			} else { // not a symbol, probably an asm line.
				if (do_output)
					find_and_output_counter(cout, str, " ");
			}
		}

		if (do_output)
			cout << str << '\n';
	}
}

void output::accumulate_and_output_counter(ostream& out, const string & filename, size_t linenr,
					   const string & blank)
{
	counter_array_t counter;
	if (samples.accumulate_samples(counter, filename, linenr)) {
		out << blank;

		output_counter(out, counter, true, string());
	}
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
	std::string out_filename;

	if (output_separate_file == true) {
		if (fn_match.match(filename) == false)
			return;

		out_filename = filename;

		size_t pos = out_filename.find(source_dir);
		if (pos == 0) {
			out_filename.erase(0, source_dir.length());
		} else if (pos == string::npos) {
			// filename is outside the source dir: ignore this file
			cerr << "opf_filter: source file " 
			     << '"' << out_filename << '"' 
			     << " is outside the source directory "
			     << '"' << source_dir << '"'
			     << " specified through --source-dir" << endl
			     << "file is ignored"
			     << endl;

			return;
		}

		out_filename = output_dir + out_filename;

		std::string path = pathname(out_filename);
		if (create_path_name(path) == false) {
			cerr << "unable to create directory: " 
			     << '"' << path << '"' << endl;
			return;
		}

		// paranoid checking: out_filename and filename must be
		// distinct file. FIXME: is this the correct way to check
		// against identical file?
		struct stat stat1;
		struct stat stat2;
		if (stat(filename.c_str(), &stat1) == 0 &&
		    stat(out_filename.c_str(), &stat2) == 0) {
			if (stat1.st_dev == stat2.st_dev &&
			    stat1.st_ino == stat2.st_ino) {
				cerr << "input and output_filename are" 
				     << "identical ("
				     << '"' << filename << '"'
				     << ','
				     << '"' << out_filename << '"'
				     << endl;

				return;
			}
		}

		ofstream out(out_filename.c_str());
		if (!out){
			cerr << "unable to open output file "
			     << '"' << out_filename << '"'
			     << endl;
		}

		do_output_one_file(out, in, filename, total_count_for_file);
	} else {
		do_output_one_file(cout, in, filename, total_count_for_file);
	}
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

	source_file source(in);

	//  This is a simple heuristic, we probably need another output format
	for (size_t linenr = 0; linenr <= source.file_line.size(); ++linenr) {
		string blank;
		string str;

		if (linenr != 0) {
			str = source.file_line[linenr-1];

			blank = extract_blank_at_begin(str);
		}

		find_and_output_counter(out, filename, linenr);

		accumulate_and_output_counter(out, filename, linenr, blank);

		if (linenr != 0)
			out << str << '\n';
	}
}

struct filename_by_samples {
	filename_by_samples(string filename_, double percent_,
			    const counter_array_t & counter_)
		: filename(filename_), percent(percent_), counter(counter_) {}

	bool operator<(const filename_by_samples & lhs) const {
		return percent > lhs.percent;
	}

	string filename;
	// -- tricky: this info is valid only for one of the counter. The information: from wich
	// counter this percentage is valid is not self contained in this structure.
	double percent;
	counter_array_t counter;
};

void output::output_source()
{
	set<string> filename_set;

	for (size_t i = 0 ; i < samples.size() ; ++i) {
		filename_set.insert(samples[i].file_loc.filename);
	}

	size_t index = get_sort_counter_nr();

	// Give a sort order on filename for the selected sort counter.
	vector<filename_by_samples> file_by_samples;

	set<string>::const_iterator it;
	for (it = filename_set.begin() ; it != filename_set.end() ;  ++it) {
		double percent = 0.0;

		counter_array_t total_count_for_file;

		samples.accumulate_samples_for_file(total_count_for_file, *it);
			
		percent = do_ratio(total_count_for_file[index],
				   counter_info[index].total_samples);

		filename_by_samples f(*it, percent, total_count_for_file);

		file_by_samples.push_back(f);
	}

	// now sort the file_by_samples entry.
	sort(file_by_samples.begin(), file_by_samples.end());

	double threshold = threshold_percent / 100.0;

	if (output_separate_file == false)
		output_header(cout);

	for (size_t i = 0 ; i < file_by_samples.size() && threshold >= 0 ; ++i) {
		filename_by_samples & s = file_by_samples[i];
		ifstream in(s.filename.c_str());

		if (!in) {
			cerr << "opf_filter (warning): unable to open for reading: " << file_by_samples[i].filename << endl;
		} else {
			if (until_more_than_samples ||
				(do_ratio(s.counter[index], counter_info[index].total_samples) >= threshold))
				output_one_file(in, s.filename, s.counter);
		}

		// Always decrease the threshold if needed, even if the file has not
		// been found to avoid in pathalogical case the output of many files
		// which contains low counter value.
		if (until_more_than_samples)
			threshold -=  s.percent;
	}
}

size_t output::get_sort_counter_nr() const
{
	size_t index = sort_by_counter;

	if (index >= size_t(op_nr_counters) || counter_info[index].enabled == false) {
		for (index = 0 ; index < op_nr_counters ; ++index) {
			if (counter_info[index].enabled)
				break;
		}

		cerr << "opf_filter (warning): sort_by_counter invalid or "
		     << "counter[sort_by_counter] disabled : switch "
		     << "to the first valid counter " << index << endl;
	}

	// paranoid checking.
	if (index == op_nr_counters)
		throw "output::output_source(input &) cannot find a counter enabled";

	return index;
}

// FIXME: is this going to be removed when you're sure it's stable ???
bool output::sanity_check_symbol_entry(size_t index) const
{
	if (index == 0) {
		if (symbols[0].first != 0) {
			cerr << "opf_filter: symbol[0].first != 0" << endl;
			return false;
		}

		if (symbols[0].last > samples.size()) {
			cerr << "opf_filter: symbols[0].last > samples.size()" << endl;
			return false;
		}
	} else {
		if (symbols[index-1].last != symbols[index].first) {
			cerr << "opf_filter: symbols[" << index - 1
			     << "].last != symbols[" << index
			     << "].first" << endl;
			return false;
		}
	}

	if (index == symbols.size() - 1) {
		if (symbols[index].last != samples.size()) {
			cerr << "opf_filter: symbols[symboles.size() -1].last != symbols.size()" << endl;
			return false;
		}
	}

	return true;
}

// Post condition: 
//  the symbols/samples are sorted by increasing vma.
//  the range of sample_entry inside each symbol entry are valid, see 
//    sanity_check_symbol_entry()
//  the samples_by_file_loc member var is correctly setup.
void output::build_samples_containers() 
{
	// fill the symbol table.
	for (size_t i = 0 ; i < abfd.syms.size(); ++i) {
		u32 start, end;
		const char* filename;
		uint linenr;
		symbol_entry symb_entry;

		abfd.get_symbol_range(i, start, end);

		bool found_samples = false;
		for (uint j = start; j < end; ++j)
			found_samples |= samples_files.accumulate_samples(symb_entry.sample.counter, j);

		if (found_samples == 0)
			continue;
		
		symb_entry.name = demangle_symbol(abfd.syms[i]->name);
		symb_entry.first = samples.size();

		if (abfd.get_linenr(i, start, filename, linenr)) {
			symb_entry.sample.file_loc.filename = filename;
			symb_entry.sample.file_loc.linenr = linenr;
		} else {
			symb_entry.sample.file_loc.filename = std::string();
			symb_entry.sample.file_loc.linenr = 0;
		}

		bfd_vma base_vma = abfd.syms[i]->value + abfd.syms[i]->section->vma;
		symb_entry.sample.vma = abfd.sym_offset(i, start) + base_vma;

		for (u32 pos = start; pos < end ; ++pos) {
			sample_entry sample;

			if (samples_files.accumulate_samples(sample.counter, pos) == false)
				continue;

			if (abfd.get_linenr(i, pos, filename, linenr)) {
				sample.file_loc.filename = filename;
				sample.file_loc.linenr = linenr;
			} else {
				sample.file_loc.filename = std::string();
				sample.file_loc.linenr = 0;
			}

			sample.vma = abfd.sym_offset(i, pos) + base_vma;

			samples.push_back(sample);
		}

		symb_entry.last = samples.size();

		symbols.push_back(symb_entry);
	}

	if (sanity_check) {
		// All the range in the symbol vector must be valid.
		for (size_t i = 0 ; i < symbols.size() ; ++i) {
			if (sanity_check_symbol_entry(i) == false) {
				exit(EXIT_FAILURE);
			}
		}
	}
}

// this output a comment containaing the counter setup and command the line.
void output::output_header(ostream& out) const 
{
	out << begin_comment << endl;

	out << "Command line:" << endl;
	for (int i = 0 ; i < argc ; ++i)
		out << argv[i] << " ";
	out << endl << endl;

	out << "interpretation of command line:" << endl;

	if (have_linenr_info) {
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
		// FIXME : is there any way to check if the objdump output mix
		// source within assembly ?
	}

	out << endl;

	out << "Cpu type: " << op_get_cpu_type_str(cpu_type) << endl;

	out << "Cpu speed (MHz estimation) : " << cpu_speed << endl;

	out << endl;

	for (size_t i = 0 ; i < op_nr_counters ; ++i) {
		if (i != 0)
			out << endl;

		out << "Counter " << i << " " << counter_info[i] << endl;
	}

	out << end_comment << endl;
	out << endl;
}

bool output::treat_input(input & in) 
{
	cpu_type = samples_files.header[samples_files.first_file]->cpu_type;

	if (cpu_type == CPU_ATHLON)
		op_nr_counters = 4;

	cpu_speed = samples_files.header[samples_files.first_file]->cpu_speed;

	if (setup_counter_param() == false)
		return false;

	build_samples_containers();

//	debug_dump_vector(out);

	if (calc_total_samples() == false) {
		cerr << "opf_filter: the input contains zero samples" << endl;

		return false;
	}

	if (have_linenr_info == false)
		output_asm(in);
	else
		output_source();

	return true;
}

//---------------------------------------------------------------------------

static int have_linenr_info;
static int with_more_than_samples;
static int until_more_than_samples;
static int showvers;
static int sort_by_counter;
static const char * source_dir;
static const char * output_dir;
static const char * output_filter;
static const char * no_output_filter;

// Do not add option with longname == 0
static struct poptOption options[] = {
	{ "use-linenr-info", 'o', POPT_ARG_NONE, &have_linenr_info, 0,
	  "input contain linenr info", NULL, },
	{ "with-more-than-samples", 'w', POPT_ARG_INT, &with_more_than_samples, 0,
	  "show all source file if the percent of samples in this file is more than argument", "percent in 0-100" },
	{ "until-more-than-samples", 'm', POPT_ARG_INT, &until_more_than_samples, 0,
	  "show all source files until the percent of samples specified is reached", NULL },
	{ "sort-by-counter", 'c', POPT_ARG_INT, &sort_by_counter, 0,
	  "sort by counter", "counter nr", },
	{ "source-dir", 0, POPT_ARG_STRING, &source_dir, 0, "source directory", "directory name" },
	{ "output-dir", 0, POPT_ARG_STRING, &output_dir, 0, "output directory", "directory name" },
        { "output", 0, POPT_ARG_STRING, &output_filter, 0, "output filename filter", "filter string" },
        { "no-output", 0, POPT_ARG_STRING, &no_output_filter, 0, "no output filename filter", "filter string" },
	{ "version", 'v', POPT_ARG_NONE, &showvers, 0, "show version", NULL, },
	POPT_AUTOHELP
	{ NULL, 0, 0, NULL, 0, NULL, NULL, },
};

/**
 * is_opf_filter_option
 *
 * return the number of option eated by opf_filter,
 * return 0 means this option is not for oprofpp
 */
int is_opf_filter_option(const char* option)
{
	// skip [-]* prefix
	while (*option == '-')
		++option;

	const poptOption* opt;
	for (opt = options ; opt->longName; ++opt) {
		if (strcmp(opt->longName, option) == 0) {
			return opt->argInfo == POPT_ARG_NONE ? 1 : 2;
		}
	}

	return 0;
}

/**
 * get_options - process command line
 * @argc: program arg count
 * @argv: program arg array
 *
 * Process the arguments, fatally complaining on error.
 */
static void get_options(int argc, char const * argv[])
{
	poptContext optcon;
	char c; 

	// FIXME : too tricky: use popt sub-table capacity?

	// separate the option into two set, one for opp_get_options
	// and the other for opf_filter
	std::vector<char const*> oprofpp_opt;
	std::vector<char const*> opf_opt;

	oprofpp_opt.push_back(argv[0]);
	opf_opt.push_back(argv[0]);

	for (int i = 1; i < argc; ) {
		int nb_opt = is_opf_filter_option(argv[i]);
		if (nb_opt) {
			for (int end = i + nb_opt; i < end ; ++i)
				opf_opt.push_back(argv[i]);
		}
		else
			oprofpp_opt.push_back(argv[i++]);
	}

	opp_get_options(oprofpp_opt.size(), &oprofpp_opt[0]);

	// FIXME: std::vector<char const*> is not necessarily a const char * array !
	// why did you not change this ? 
	// I guess you need a vector_to_c_array template or something
	optcon = opd_poptGetContext(NULL, opf_opt.size(), &opf_opt[0],
				options, 0);

	c = poptGetNextOpt(optcon);

	if (c < -1) {
		fprintf(stderr, "opf_filter: %s: %s\n",
			poptBadOption(optcon, POPT_BADOPTION_NOALIAS),
			poptStrerror(c));

		poptPrintHelp(optcon, stderr, 0);

	        exit(EXIT_FAILURE);
	}

	if (showvers) {
		printf("opf_filter: %s : " VERSION_STRING " compiled on " __DATE__ " " __TIME__ "\n", argv[0]);
		exit(EXIT_SUCCESS);
	}

	if (with_more_than_samples && until_more_than_samples) {
		fprintf(stderr, "opf_filter: --with-more-than-samples and -until-more-than-samples can not specified together\n");
		exit(EXIT_FAILURE);
	}
}

//---------------------------------------------------------------------------

int main(int argc, char const * argv[]) {

#if (__GNUC__ >= 3)
	// this improves performance with gcc 3.x a bit
	std::ios_base::sync_with_stdio(false);
#endif

	get_options(argc, argv);

	try {
		input in(cin);

		bool do_until_more_than_samples = false;
		int threshold_percent = with_more_than_samples;
		if (until_more_than_samples) {
			threshold_percent = until_more_than_samples;
			do_until_more_than_samples = true;
		}

		output output(argc, argv,
			      have_linenr_info, 
			      threshold_percent,
			      do_until_more_than_samples,
			      sort_by_counter,
			      output_dir,
			      source_dir,
			      output_filter,
			      no_output_filter);

		if (output.treat_input(in) == false) {
			return EXIT_FAILURE;
		}
	} 

	catch (const string & e) {
		cerr << "opf_filter: Exception : " << e << endl;
		return EXIT_FAILURE;
	}
	catch (const char * e) {
		cerr << "opf_filter: Exception : " << e << endl;
		return EXIT_FAILURE;
	}
	catch (...) {
		cerr << "opf_filter: Unknown exception : really sorry " << endl;
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
