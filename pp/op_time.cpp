// TODO GPL
#include <popt.h>
#include <stdlib.h>

#include <string>
#include <list>
#include <iostream>
#include <sstream>
#include <fstream>
#include <algorithm>

#include "oprofpp.h"

#include "../util/file_manip.h"

using std::string;
using std::list;
using std::cerr;
using std::cout;
using std::endl;
using std::ostringstream;
using std::ifstream;

struct samples_file_header
{
	samples_file_header(const std::string & filename = std::string(), 
			    u32 count = 0)
		: count(count), filename(filename) {}

	bool operator<(const samples_file_header & rhs) const
		{ return count > rhs.count; }

	u32 count;
	string filename;
};

static int showvers;
static int reverse_sort;
static const char * base_dir = "/var/opd/samples";

static struct poptOption options[] = {
	{ "verbose", 'V', POPT_ARG_NONE, &verbose, 0, "verbose output", NULL, },
	{ "use-counter", 'c', POPT_ARG_INT, &ctr, 0,
	  "use counter", "counter nr", },
	{ "backup-number", 'b', POPT_ARG_INT, &backup_number, 0,
	  "suffixe number of sample file to use", "backup number", },
	{ "reverse", 'r', POPT_ARG_NONE, &reverse_sort, 0,
	  "reverse sort order", NULL, },
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
	char const * file;

	optcon = opd_poptGetContext(NULL, argc, argv, options, 0);

	c = poptGetNextOpt(optcon);

	if (c < -1) {
		fprintf(stderr, "op_time: %s: %s\n",
			poptBadOption(optcon, POPT_BADOPTION_NOALIAS),
			poptStrerror(c));

		poptPrintHelp(optcon, stderr, 0);
	        exit(EXIT_FAILURE);
	}

	if (showvers) {
		printf("op_time: %s : " VERSION_STRING " compiled on "
		       __DATE__ " " __TIME__ "\n", argv[0]);
		exit(EXIT_SUCCESS);
	}

	// non-option file, must be valid directory name
	file = poptGetArg(optcon);
	if (file)
		base_dir = file;

	if (ctr == -1)
		ctr = 0;
}

/**
 * strip_filename_suffix - strip the #nr suffix of a samples
 * filename
 */
static string strip_filename_suffix(const std::string & filename)
{
	std::string result(filename);

	size_t pos = result.find_last_of('#');
	if (pos != string::npos)
		result.erase(pos, result.length() - pos);

	return result;
}

/**
 * samples_file_exist - test for a samples file existence
 * @filename: the base samples filename
 *
 * return true if @filename#ctr exists
 */
static bool samples_file_exist(const std::string & filename)
{
	std::ostringstream s;

	s << filename << "#" << ctr;
	if (backup_number != -1)
		s << "-" << backup_number;

	ifstream in(s.str().c_str());

	return in;
}

/**
 * get_file_list - create a file list of base samples filename
 * @file_list: where to put the results
 *
 * fill @file_list with a list of base samples
 * filename where a base sample filename is a
 * samples filename without #nr suffix
 */
static void get_file_list(list<string> & file_list)
{
	file_list.clear();

	list <string> files;
	if (create_file_list(files, base_dir) == false) {
		cerr << "Can't open directory \"" << base_dir << "\": "
		     << strerror(errno) << endl;
		exit(EXIT_FAILURE);
	}

	list<string>::iterator it;
	for (it = files.begin(); it != files.end(); ++it) {

		if ((*it).find_first_of(OPD_MANGLE_CHAR) == string::npos)
			continue;

		string filename = strip_filename_suffix(*it);

		//  After stripping the # suffix multiples identicals
		// filenames can exist.
		if (find(file_list.begin(), file_list.end(), filename) == 
		    file_list.end())
			file_list.push_back(filename);
	}
}

/**
 * out_filename - display a filename and it assiocated ratio of samples
 */
static void out_filename(const samples_file_header & sfh, double total_count)
{
	cout << demangle_filename(sfh.filename) << " " << sfh.count  << " "
	     << (sfh.count / total_count) * 100 << "%" << endl;
}

/**
 * treat_file_list - open each samples file to cumulate samples count
 * and display a sorted list of filename and samples ratio
 * @files: the file list to treat.
 */
static void treat_file_list(const list<string>& files)
{
	vector<samples_file_header> headers;

	double total_count = 0;

	list<string>::const_iterator it;
	for (it = files.begin(); it != files.end(); ++it) {
		std::string filename = string(base_dir) + "/" + *it;
		samplefile = filename.c_str();

		if (samples_file_exist(filename) == false)
			continue;

		opp_samples_files samples;

		u32 count = 0;
		if (samples.is_open(ctr)) {
			opd_fentry * begin;
			opd_fentry * end;
			begin = samples.samples[ctr];
			end = begin + (samples.size[ctr] / sizeof(opd_fentry));
			for ( ; begin != end ; ++begin)
				count += begin->count;
			  
		}

		total_count += count;

		headers.push_back(samples_file_header(filename, count));
	}

	sort(headers.begin(), headers.end());

	if (!reverse_sort) {
		vector<samples_file_header>::const_iterator c_it;
		for (c_it = headers.begin(); c_it != headers.end() ; ++c_it)
			out_filename(*c_it, total_count);
	} else {
		vector<samples_file_header>::reverse_iterator c_it;
		for (c_it = headers.rbegin(); headers.rend() != c_it ; ++c_it)
			out_filename(*c_it, total_count);
	}
}

int main(int argc, char const * argv[])
{
	get_options(argc, argv);

	list<string> file_list;

	get_file_list(file_list);

	treat_file_list(file_list);

	return 0;
}
