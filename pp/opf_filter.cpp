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

#include <fstream>
#include <iomanip>
#include <vector>
#include <set>
#include <algorithm>

#include <stdio.h>

#include <popt.h>

using namespace std;

#include "opf_filter.h"

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

}

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
	source_file(istream & in, const string & filename);

	string filename;
	vector<string> file_line;
};

//---------------------------------------------------------------------------
// To hold the setup of the profiler for one counter.
struct counter_setup {
	counter_setup() : 
		enabled(false), event_count_sample(0) {}

	// if false other field are not meaningful.
	bool   enabled;
	string event_name;
	string help_string;
	string unit_mask;
	string unit_mask_help;     // The string help for the unit mask
	size_t event_count_sample;
	// would be double?
	size_t total_samples;
};

//---------------------------------------------------------------------------
// All the work is made here.
class output {
 public:
	output(ostream & out_, int argc, char const * argv[],
	       bool have_linenr_info_,
	       size_t threshold_percent,
	       bool until_more_than_samples,
	       size_t sort_by_counter);

	bool treat_input(input &);

	void debug_dump_vector(ostream & out) const;

 private:
	void output_command_line() const;

	void output_asm(input & in);
	void output_source(input & in);

	// output one file unconditionally.
	void do_output_one_file(istream & in, const string & filename, 
				const counter_array_t & total_samples_for_file);

	// accumulate counter for a given (filename, linenr).
	void accumulate_and_output_counter(const string & filename, size_t linenr, 
					   const string & blank);

	void read_input(input & in);
	void treat_line(const string & str);

	void setup_counter_param(input & in);
	bool calc_total_samples();

	void output_counter(const counter_array_t & counter, 
			    bool comment, const string & prefix) const;
	void output_one_counter(size_t counter, size_t total) const;

	void find_and_output_symbol(const string& str, const char * blank) const;
	void find_and_output_counter(const string& str, const char * blank) const;

	void find_and_output_counter(const string & filename,
				     size_t linenr) const;

	size_t get_sort_counter_nr() const;

	// debug.
	bool sanity_check_symbol_entry(size_t index) const;

	// The output stream.
	ostream & out;

	// used to output the command line.
	int argc;
	char const ** argv;

	// The symbols collected by oprofpp sorted by increased vma, provide also a sort
	// order on samples count for each counter.
	symbol_container_t symbols;

	// The samples count collected by oprofpp sorted by increased vma, provide also a sort
	// order on (filename, linenr)
	sample_container_t samples;

	// oprofpp give some info on the setting of the counters. These
	// are stored here.
	counter_setup counter_info[max_counter_number];

	// TODO : begin_comment, end_comment must be based on the current 
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

	bool until_more_than_samples;

	bool have_linenr_info;
};

//---------------------------------------------------------------------------

namespace {

// Return the substring at beginning of str which is only made of blank or tabulation.
string extract_blank_at_begin(const string & str) {

	size_t end_pos = str.find_first_not_of(" \t");
	if (end_pos == string::npos)
		end_pos = 0;

	return str.substr(0, end_pos);
}

// Convenience function : just output the setup of one counter.
ostream & operator<<(ostream & out, const counter_setup & rhs) {

	out << (rhs.enabled ? "enabled" : "disabled")  << " :";

	if (rhs.enabled) {
		out << " ";

		out << rhs.event_name 
		    << " (" << rhs.help_string << ")" << endl;

		out << "unit mask : "
		    << rhs.unit_mask
		    << " (" << rhs.unit_mask_help << ")"
		    << " event_count : " << rhs.event_count_sample 
		    << " total samples : " << rhs.total_samples;
	}

	return out;
}

inline double do_ratio(size_t counter, size_t total) {
	return total == 0 ? 1.0 : ((double)counter / total);
}

} // anonymous namespace

//---------------------------------------------------------------------------

counter_array_t::counter_array_t()
{
	for (size_t i = 0 ; i < max_counter_number ; ++i)
		value[i] = 0;
}

counter_array_t & counter_array_t::operator+=(const counter_array_t & rhs)
{
	for (size_t i = 0 ; i < max_counter_number ; ++i)
		value[i] += rhs.value[i];

	return *this;
}

//--------------------------------------------------------------------------

void sample_entry::debug_dump(ostream & out) const {
	if (file_loc.filename.length())
		out << file_loc.filename << ":" << file_loc.linenr << " ";

	out << hex << vma << dec << " ";

	for (size_t i = 0 ; i < max_counter_number ; ++i)
		out << counter[i] << " ";
}

size_t sample_entry::build(const string& str, size_t pos, bool have_linenr_info) {

	int number_of_char_read;
	if (have_linenr_info) {
		// must find filename:linenr
		size_t end_pos = str.find(':', pos);
		if (end_pos == string::npos)
			throw "vma_info::build(string, size_t, bool) invalid input line";

		file_loc.filename = str.substr(pos, end_pos - pos);

		sscanf(str.c_str() + end_pos + 1, "%d", &file_loc.linenr);

		pos = str.find(' ', end_pos + 1);
	}

	sscanf(str.c_str() + pos, "%lx%n", &vma, &number_of_char_read);

	for (size_t i = 0 ; i < max_counter_number ; ++i) {
		int temp;

		sscanf(str.c_str() + pos + number_of_char_read, "%u%n", &counter[i], &temp);

		number_of_char_read += temp;
	}

	return number_of_char_read + pos;
}

//--------------------------------------------------------------------------

void symbol_entry::debug_dump(ostream & out) const {

	out << "[" << name << "]" << endl;

	out << "counters number range [" << first << ", " << last << "[" << endl;

	sample.debug_dump(out);
}

//---------------------------------------------------------------------------

bool input::read_line(string & str) {

	if (put_back_area_valid) {
		put_back_area_valid = false;

		str = put_back_area;

		return true;
	}

	return getline(in, str);
}

void input::put_back(const string & str) {
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
 
source_file::source_file(istream & in, const string & filename)
	:
	filename(filename)
{
	string str;
	while (getline(in, str))
		file_line.push_back(str);
}

//---------------------------------------------------------------------------

output::output(ostream & out_, int argc_, char const * argv_[],
	       bool have_linenr_info_,
	       size_t threshold_percent_,
	       bool until_more_than_samples_,
	       size_t sort_by_counter_)
	: 
	out(out_),
	argc(argc_),
	argv(argv_),
	begin_comment("/*"),
	end_comment("*/"),
	cpu_speed(0.0),
	threshold_percent(threshold_percent_),
	sort_by_counter(sort_by_counter_),
	until_more_than_samples(until_more_than_samples_),
	have_linenr_info(have_linenr_info_)
{
}

void output::debug_dump_vector(ostream & out) const {

	out << "total samples :";

	for (size_t i = 0 ; i < max_counter_number ; ++i)
		cout << " " << counter_info[i].total_samples;
	
	cout << endl;

	for (size_t i = 0 ; i < symbols.size() ; ++i) {

		symbols[i].debug_dump(cout);
		cout << endl;

		for (size_t j = symbols[i].first ; j < symbols[i].last; ++j) {
			samples[j].debug_dump(cout);
			cout << endl;
		}
	}
}

// Some unnecessary complexity to handle the output format of oprofpp
// TODO : Perhaps fix oprofpp output to simplify this code? FOR NOW don't touch
// a this code, see with John if the code of oprofpp can be modified. Or modify
// the design to use oprofpp function as a libray, this need more work on libbfd
// because we want in some case the disassembly code.
void output::setup_counter_param(input & in) {
	string str;
	in.read_line(str);

	size_t pos = str.find("Counter ");
	if (pos == 0) {
		size_t counter_number;
		if (sscanf(str.c_str(), "Counter %u", &counter_number) == 1 &&
		    counter_number < max_counter_number) {
			counter_info[counter_number].enabled = true;

			size_t pos = str.find("counted ");
			if (pos == string::npos) {
				counter_info[counter_number].event_name = "UNKNOWN_EVENT";
			} else {
				pos += strlen("counted ");
				size_t end_pos = str.find(" ", pos);
				if (end_pos == string::npos) {
					counter_info[counter_number].event_name = "UNKNOWN_EVENT";
				} else {
					counter_info[counter_number].event_name = 
						str.substr(pos, end_pos - pos);
				}

				pos = str.find('(', end_pos);
				if (pos != string::npos) {
					end_pos = str.find(')', pos); 
					if (end_pos != string::npos) {
						counter_info[counter_number].help_string =
							str.substr(pos + 1, end_pos - (pos + 1));
					}
				}
			}

			pos = str.find("unit mask of ");
			if (pos != string::npos) {
				pos += strlen("unit mask of ");
				size_t end_pos = str.find(' ', pos);
				if (end_pos != string::npos) {
					counter_info[counter_number].unit_mask = 
						str.substr(pos, end_pos - pos);
				}
			}

			pos = str.find('(', pos);
			if (pos != string::npos) {
				size_t end_pos = str.find(')', pos);
				counter_info[counter_number].unit_mask_help =
					str.substr(pos + 1, (end_pos - pos) - 1);
			}

			counter_info[counter_number].event_count_sample = 0;
			pos = str.rfind(") count ");
			if (pos != string::npos) {
				sscanf(str.c_str() + pos, ") count %u",
				       &counter_info[counter_number].event_count_sample);
				}
		}
	} else {
		in.put_back(str);
	}
}

bool output::calc_total_samples() {

	for (size_t i = 0 ; i < max_counter_number ; ++i)
		counter_info[i].total_samples = 0;

	for (size_t i = 0 ; i < symbols.size() ; ++i) {
		for (size_t j = 0 ; j < max_counter_number ; ++j)
			counter_info[j].total_samples += symbols[i].sample.counter[j];
	}

	if (sanity_check) {
		counter_array_t total_counter;

		for (size_t i = 0 ; i < samples.size() ; ++i) {
			total_counter += samples[i].counter;
		}

		for (size_t i = 0 ; i < max_counter_number ; ++i) {
			if (total_counter[i] != counter_info[i].total_samples) {
				cerr << "output::calc_total_samples() : "
				     << "bad counter accumulation"
				     << " " << total_counter[i] 
				     << " " << counter_info[i].total_samples;
				exit(1);
			}
		}
	}

	for (size_t i = 0 ; i < max_counter_number ; ++i)
		if (counter_info[i].total_samples != 0)
			return true;

	return false;
}

void output::output_one_counter(size_t counter, size_t total) const {

	out << " ";

	out << counter << " ";

	out << setprecision(4) << (do_ratio(counter, total) * 100.0) << "%";
}

void output::
output_counter(const counter_array_t & counter, bool comment, 
	       const string & prefix) const
{
	if (comment)
		out << begin_comment;

	if (prefix.length())
		out << " " << prefix;

	for (size_t i = 0 ; i < max_counter_number ; ++i)
		if (counter_info[i].enabled)
			output_one_counter(counter[i], counter_info[i].total_samples);

	out << " ";
      
	if (comment)
		out << end_comment;

	out << '\n';
}

// Complexity: log(container.size())
void output::find_and_output_symbol(const string& str, const char * blank) const {
	unsigned long vma;

	sscanf(str.c_str(), "%lx",  &vma);

	const symbol_entry* symbol = symbols.find_by_vma(vma);

	if (symbol) {
		out <<  blank;

		output_counter(symbol->sample.counter, true, string());
	}
}

// Complexity: log(samples.size())
void output::find_and_output_counter(const string& str, const char * blank) const {
	unsigned long vma;

	sscanf(str.c_str(), "%lx",  &vma);

	const sample_entry * sample = samples.find_by_vma(vma);
	if (sample) {
		out <<  blank;

		output_counter(sample->counter, true, string());
	}
}

// Complexity: log(symbols.size())
void output::find_and_output_counter(const string & filename, size_t linenr) const
{
	const symbol_entry * symbol = symbols.find(filename, linenr);
	if (symbol)
		output_counter(symbol->sample.counter, true, symbol->name);
}

void output::output_asm(input & in) {
	size_t index = get_sort_counter_nr();

	vector<const symbol_entry*> v;
	symbols.get_symbols_by_count(index , v);

	vector<const symbol_entry*> output_symbols;

	// select the subset of symbols which statisfy the threshold condition.
	double threshold = threshold_percent / 100.0;

	for (size_t i = 0 ; i < v.size() && threshold >= 0 ; ++i) {
		double percent = do_ratio(v[i]->sample.counter[index],
					  counter_info[index].total_samples);

		if (until_more_than_samples || percent >= threshold) {
			output_symbols.push_back(v[i]);
		}

		if (until_more_than_samples) {
			threshold -=  percent;
		}
	}

	bool do_output = true;

	string str;
	while (in.read_line(str)) {
		if (str.length()) {
			// line of interest begins with a space (disam line) 
			// or a '0' (symbol line)
			if (str[0] == '0') {
				unsigned long vma;

				sscanf(str.c_str(), "%lx",  &vma);

				const symbol_entry* symbol = symbols.find_by_vma(vma);

				// Note this use a pointer comparison. It work because symbols 
				// container warranty than symbol pointer are unique.
				if (find(output_symbols.begin(), output_symbols.end(), symbol) !=
				    output_symbols.end()) {
					do_output = true;
				} else if (threshold_percent == 0) {
					// if the user have not requested threshold we must output
					// all symbols even if it contains no samples.
					do_output = true;
				} else {
					do_output = false;
				}

				if (do_output) {
					find_and_output_symbol(str, "");
				}
				
			// Fix this test : first non blank is a digit
			} else if (str[0] == ' ' && str.length() > 1 && 
				   isdigit(str[1]) && do_output) {
				find_and_output_counter(str, " ");
			}
		}

		if (do_output) {
			out << str << '\n';
		}
	}
}

void output::accumulate_and_output_counter(const string & filename, size_t linenr,
					   const string & blank) {
	counter_array_t counter;
	if (samples.accumulate_samples(counter, filename, linenr)) {
		out << blank;

		output_counter(counter, true, string());
	}
}

// Pre condition:
//  the file has not been output.
//  in is a valid file stream ie ifstream(filename)
// Post condition:
//  the entire file source and the associated samples has been output to
//  the standard output.
void output::do_output_one_file(istream & in, const string & filename, 
				const counter_array_t & total_count_for_file) {
	
	out << begin_comment << endl;

	out << " Total samples for file : " << endl;

	out << " \"" << filename << "\"" << endl;

	output_counter(total_count_for_file, false, string());

	out << end_comment << endl;

	source_file source(in, filename);

	//   This is a simple heuristic, we probably need another output format.
	for (size_t linenr = 0; linenr <= source.file_line.size(); ++linenr) {
		string blank;
		string str;

		if (linenr != 0) {
			str = source.file_line[linenr-1];

			blank = extract_blank_at_begin(str);
		}

		find_and_output_counter(filename, linenr);

		accumulate_and_output_counter(filename, linenr, blank);

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

void output::output_source(input & /*in*/) {

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
			
		if (counter_info[index].enabled) {
			percent = do_ratio(total_count_for_file[index],
					   counter_info[index].total_samples);
		}

		filename_by_samples f(*it, percent, total_count_for_file);

		file_by_samples.push_back(f);
	}

	// now sort the file_by_samples entry.
	sort(file_by_samples.begin(), file_by_samples.end());

	// please do not delete this portion of debug code for now. (phe 2001/06/15)
#if 0
	{
		// At this point file_by_samples is a sorted array of filename sorted
		// by increasing value of counter number index.
		out << "sorted order for counter " << index << endl;
		vector<filename_by_samples>::const_iterator it;
		for (it = file_by_samples.begin() ; it != file_by_samples.end() ; ++it) {
			out << it->filename << " " << it->percent << endl;
		}
	}
#endif

	double threshold = threshold_percent / 100.0;

	for (size_t i = 0 ; i < file_by_samples.size() && threshold >= 0 ; ++i) {
		ifstream in(file_by_samples[i].filename.c_str());

		if (!in) {
			cerr << "unable to open for reading: " << file_by_samples[i].filename << endl;
		} else {
			if (until_more_than_samples) {
				do_output_one_file(in, file_by_samples[i].filename, 
						   file_by_samples[i].counter);
			} else {
				if (do_ratio(file_by_samples[i].counter[index],
					     counter_info[index].total_samples) >= threshold) {
					do_output_one_file(in, file_by_samples[i].filename, 
							   file_by_samples[i].counter);
				}

			}
		}

		// Always decrease the threshold if needed, even if the file has not
		// been found to avoid in pathalogical case the output of many files
		// which contains low counter value.
		if (until_more_than_samples) {
			threshold -=  file_by_samples[i].percent;
		}
	}
}

size_t output::get_sort_counter_nr() const {
	size_t index = sort_by_counter;
	if (index >= max_counter_number || counter_info[index].enabled == false) {
		cerr << "sort_by_counter invalid or counter[sort_by_counter] disabled : switch "
		     << "to the first valid counter" << endl;

		for (index = 0 ; index < max_counter_number ; ++index) {
			if (counter_info[index].enabled)
				break;
		}
	}

	// paranoid checking.
	if (index == max_counter_number)
		throw "output::output_source(input &) cannot find a counter enabled";

	return index;
}

bool output::sanity_check_symbol_entry(size_t index) const
{
	if (index == 0) {
		if (symbols[0].first != 0) 
			return false;

		if (symbols[0].last > samples.size()) 
			return false;
	} else {
		if (symbols[index-1].last != symbols[index].first)
			return false;
	}

	if (index == symbols.size() - 1) {
		if (symbols[index].last != samples.size())
			return false;
	}

	return true;
}

// Post condition: 
//  the symbols/samples are sorted by increasing vma.
//  the range of sample_entry inside each symbol entry are valid, see 
//    sanity_check_symbol_entry()
//  the samples_by_file_loc member var is correctly setup.
void output::read_input(input & in) {
	string str;
	while (in.read_line(str) && str != "DISASSEMBLY_MARKER") {
		treat_line(str);
	}

	// -- tricky : update the last field for the last symbol.
	if (symbols.size()) {
		symbols[symbols.size() - 1].last = samples.size();
	}

	// and the set of symbol entry sorted by samples count.
	symbols.flush_input_symbol();

	// Now we can update the set of sample_entry sorted by file_loc
	samples.flush_input_counter();

	if (sanity_check) {
		// All the range in the symbol vector must be valid.
		for (size_t i = 0 ; i < symbols.size() ; ++i) {
			if (sanity_check_symbol_entry(i) == false) {

				cerr << "post condition fail : symbols range failure" << endl;

				exit(1);
			}
		}
	}

	if ((str == "DISASSEMBLY_MARKER") == have_linenr_info) {
		//  Means than op-to-source script fail to pass the correct option.
		throw "Incoherence between disassembly marker and have_linr_info";
	}
}

// Precondition:
// input are either
// "[filename::linenr] addr count0 count1 symbol_name"
// " [filename:linenr] addr count0 count1"
// filename can be "(null)" or "??" to mark invalid filename 
// linenr can be 0 to mark invalid line number.
// Note than a space begins lines which do not contain symbol name
void output::treat_line(const string & str) {

	if (str.length()) {
		if (str[0] == ' ') {
			if (symbols.size() == 0) {
				throw "found line info before any symbol info";
			}

			sample_entry sample;

			sample.build(str, 1, have_linenr_info);

			samples.push_back(sample);
		} else { 
			if (symbols.size() != 0) {
				symbols[symbols.size() - 1].last = samples.size();
			}

			symbol_entry symbol;

			size_t pos = symbol.sample.build(str, 0, have_linenr_info);

			pos = str.find(' ', pos);
			symbol.name = str.substr(pos + 1, (str.length() - pos) - 1);

			symbol.first = samples.size();
			symbol.last = symbol.first;  // safety init.

			symbols.push_back(symbol);
		}
	}
}

// It is usefull for the user to see the command line and the interpreted effect of the option
void output::output_command_line() const {
	// It is usefull for the user to see the exact effect of the command line.

	out << "Command line:" << endl;
	for (int i = 0 ; i < argc ; ++i)
		out << argv[i] << " ";
	out << endl << endl;

	out << "Output option (interpreted):" << endl;

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
		// TODO : is there any way to check if the objdump output mix source within
		// assembly ?
	}

	out << "Cpu speed (MHz estimation) : " << cpu_speed << endl;

	out << endl;
}

bool output::treat_input(input & in) {

	setup_counter_param(in);
	setup_counter_param(in);

	string str;
	in.read_line(str);

	if (sscanf(str.c_str(), "Cpu speed was (MHz estimation) : %lf", &cpu_speed) != 1) {
		cerr << "unable to read cpu_speed\n";

		return false;
	}

	bool have_counter_info = false;
	for (size_t i = 0 ; i < max_counter_number && !have_counter_info ; ++i) {
		if (counter_info[i].enabled)
			have_counter_info = true;
	}

	if (!have_counter_info) {
		cerr << "Malformed input, expect at least one counter description" << endl;
		
		return false;
	}

	read_input(in);

	if (calc_total_samples() == false) {
		cerr << "The input contains zero samples" << endl;

		return false;
	}

	out << begin_comment << endl;

	output_command_line();

	for (size_t i = 0 ; i < max_counter_number ; ++i) {
		if (i != 0)
			out << endl;

		out << "Counter " << i << " ";
		out << counter_info[i] << endl;
	}
	out << end_comment << endl;

//	debug_dump_vector(out);

	if (have_linenr_info == false) {
		output_asm(in);
	} else {
		output_source(in);
	}

	return true;
}

//---------------------------------------------------------------------------

static int have_linenr_info;
static int with_more_than_samples;
static int until_more_than_samples;
static int showvers;
static int sort_by_counter;

static struct poptOption options[] = {
	{ "use-linenr-info", 'o', POPT_ARG_NONE, &have_linenr_info, 0,
	  "input contain linenr info", NULL, },
	{ "with-more-than-samples", 'w', POPT_ARG_INT, &with_more_than_samples, 0,
	  "show all source file if the percent of samples in this file is more than argument", "percent in 0-100" },
	{ "until-more-than-samples", 'm', POPT_ARG_INT, &until_more_than_samples, 0,
	  "show all source files until the percent of samples specified is reached", NULL },
	{ "sort-by-counter", 'c', POPT_ARG_INT, &sort_by_counter, 0,
	  "sort by counter", "counter nr", },
	{ "version", 'v', POPT_ARG_NONE, &showvers, 0, "show version", NULL, },
	POPT_AUTOHELP
	{ NULL, 0, 0, NULL, 0, NULL, NULL, },
};

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
	
	// The cast to (char **) is neccessary on some version of popt.
	optcon = poptGetContext(NULL, argc, argv, options, 0);

	c=poptGetNextOpt(optcon);

	if (c<-1) {
		fprintf(stderr, "oprofpp: %s: %s\n",
			poptBadOption(optcon, POPT_BADOPTION_NOALIAS),
			poptStrerror(c));

		poptPrintHelp(optcon, stderr, 0);

	        exit(1);
	}

	if (showvers) {
		printf("%s : " VERSION_STRING " compiled on " __DATE__ " " __TIME__ "\n", argv[0]);
		exit(0);
	}

	if (with_more_than_samples && until_more_than_samples) {
		fprintf(stderr, "--with-more-than-samples and -until-more-than-samples can not specified together\n");
		exit(1);
	}
}

//---------------------------------------------------------------------------

int main(int argc, char const * argv[]) {

#if (__GNUC__ >= 3)
	// this improve a little what performance with gcc 3.0.
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

		output output(cout, argc, argv,
			      have_linenr_info, 
			      threshold_percent,
			      do_until_more_than_samples,
			      sort_by_counter);

		if (output.treat_input(in) == false) {
			return 1;
		}
	} 

	catch (const string & e) {
		cerr << "Exception : " << e << endl;
		return 1;
	}
	catch (const char * e) {
		cerr << "Exception : " << e << endl;
		return 1;
	}
	catch (...) {
		cerr << "Unknown exception : really sorry " << endl;
		return 1;
	}

	return 0;
}
