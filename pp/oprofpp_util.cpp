/**
 * @file oprofpp_util.cpp
 * Helpers for post-profiling analysis
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 * 
 * @author Philippe Elie <phil_el@wanadoo.fr>
 * @author John Levon <moz@compsoc.man.ac.uk>
 */

// FIXME: this entire file is screwed !
 
// FIXME: printf -> ostream (and elsewhere) 
#include <cstdarg>
#include <algorithm>
#include <sstream>
#include <iomanip>

#include <elf.h>

#include "oprofpp.h"
#include "oprofpp_options.h"
#include "op_libiberty.h"
#include "op_file.h"
#include "op_mangling.h"
#include "file_manip.h"
#include "string_manip.h"
#include "op_events.h"
#include "op_events_desc.h"
 
using std::string;
using std::hex;
using std::setfill;
using std::dec;
using std::setw;
using std::istringstream;
using std::vector;
using std::ostream;
using std::cerr;
using std::endl;


/**
 * verbprintf
 */
void verbprintf(char const * fmt, ...)
{
	if (options::verbose) {
		va_list va;
		va_start(va, fmt);

		vprintf(fmt, va);

		va_end(va);
	}
}


/**
 * is_excluded_symbol - check if the symbol is in the exclude list
 * @param symbol symbol name to check
 *
 * return true if symbol is in the list of excluded symbol
 */
bool is_excluded_symbol(string const & symbol)
{
	return std::find(options::exclude_symbols.begin(), options::exclude_symbols.end(),
			 symbol) != options::exclude_symbols.end();
}

/**
 * quit_error - quit with error
 * @param err error to show
 *
 * err may be NULL
 */
void quit_error(char const *err)
{
	if (err)
		cerr << err; 
	exit(EXIT_FAILURE);
}

/**
 * validate_counter - validate the counter nr
 * @param counter_mask bit mask specifying the counter nr to use
 * @param sort_by_counter the counter nr from which we sort
 *
 * all error are fatal
 */
void validate_counter(int counter_mask, int & sort_by_counter)
{
	if (counter_mask + 1 > 1 << OP_MAX_COUNTERS) {
		cerr << "invalid counter mask " << counter_mask << "\n";
		exit(EXIT_FAILURE);
	}

	if (sort_by_counter == -1) {
		// get the first counter selected and use it as sort order
		for (size_t i = 0 ; i < OP_MAX_COUNTERS ; ++i) {
			if ((counter_mask & (1 << i)) != 0)
				sort_by_counter = i;
		}
	}

	if ((counter_mask & (1 << sort_by_counter)) == 0) {
		cerr << "invalid sort counter nr " << sort_by_counter << "\n";
		exit(EXIT_FAILURE);
	}
}

 
/**
 * opp_treat_options - process command line options
 * @param file a filename passed on the command line, can be %NULL
 * @param image_file where to store the image file name
 * @param sample_file ditto for sample filename
 * @param counter where to put the counter command line argument
 * @param sort_by_counter which counter usefor sort purpose
 *
 * Process the arguments, fatally complaining on
 * error. 
 *
 * Most of the complexity here is to process
 * filename. file is considered as a sample file
 * if it contains at least one OPD_MANGLE_CHAR else
 * it is an image file. If no image file is given
 * on command line the sample file name is un-mangled
 * -after- stripping the optionnal "#d" suffixe. This
 * give some limitations on the image filename.
 *
 * all filename checking is made here only with a
 * syntactical approch. (ie existence of filename is
 * not tested)
 *
 * post-condition: sample_file and image_file are setup
 */
void opp_treat_options(string const & file,
	string & image_file, string & sample_file,
	int & counter, int & sort_by_counter)
{
	using namespace options;
 
	char * file_ctr_str;
	int temp_counter;

	if (!imagefile.empty())
		imagefile = relative_to_absolute_path(imagefile);

	if (!samplefile.empty())
		samplefile = relative_to_absolute_path(samplefile);

	if (!file.empty()) {
		if (!imagefile.empty() && !samplefile.empty()) {
			quit_error("oprofpp: too many filenames given on command line:" 
				"you can specify at most one sample filename"
				" and one image filename.\n");
		}

		string temp = relative_to_absolute_path(file);
		if (temp.find_first_of(OPD_MANGLE_CHAR) != string::npos)
			samplefile = temp;
		else
			imagefile = temp;
	}

	if (samplefile.empty()) { 
		if (imagefile.empty()) { 
			quit_error("oprofpp: no samples file specified.\n");
		} else {
			/* we'll "leak" this memory */
			samplefile = remangle_filename(imagefile);
		}
	}

	/* we can not complete filename checking of imagefile because
	 * it can be derived from the sample filename, we must process
	 * and chop optionnal suffixe "#%d" first */

	/* check for a valid counter suffix in a given sample file */
	/* FIXME we need probably generalized std::string filename manipulation
	 * stripping suffix, getting counter nr etc ... */
	temp_counter = -1;
	file_ctr_str = strrchr(samplefile.c_str(), '#');
	if (file_ctr_str) {
		sscanf(file_ctr_str + 1, "%d", &temp_counter);
	}

	if (temp_counter != -1 && counter != -1 && counter != 0) {
		if ((counter & (1 << temp_counter)) == 0)
			quit_error("oprofpp: conflict between given counter and counter of samples file.\n");
	}

	if (counter == -1 || counter == 0) {
		if (temp_counter != -1)
			counter = 1 << temp_counter;
		else
			counter = 1 << 0;	// use counter 0
	}

	sample_file = strip_filename_suffix(samplefile);

	if (imagefile.empty()) {
		/* we allow for user to specify a sample filename on the form
		 * /var/lib/oprofile/samples/}bin}nash}}}lib}libc.so so we need to
		 * check against this form of mangled filename */
		string lib_name;
		string app_name = extract_app_name(sample_file, lib_name);
		if (lib_name.length())
			app_name = lib_name;
		image_file = demangle_filename(app_name);
	}
	else
		image_file = imagefile;

	validate_counter(counter, sort_by_counter);
}

 
// FIXME: only use char arrays and pointers if you MUST. Otherwise std::string
// and references everywhere please.

/**
 * counter_mask -  given a --counter=0,1,..., option parameter return a mask
 * representing each counter. Bit i is on if counter i was specified.
 * So we allow up to sizeof(uint) * CHAR_BIT different counter
 */
uint counter_mask(string const & str)
{
	vector<string> result;
	separate_token(result, str, ',');

	uint mask = 0;
	for (size_t i = 0 ; i < result.size(); ++i) {
		istringstream stream(result[i]);
		int counter;
		stream >> counter;
		mask |= 1 << counter;
	}

	return mask;
}

counter_array_t::counter_array_t()
{
	for (size_t i = 0 ; i < OP_MAX_COUNTERS ; ++i)
		value[i] = 0;
}

counter_array_t & counter_array_t::operator+=(counter_array_t const & rhs)
{
	for (size_t i = 0 ; i < OP_MAX_COUNTERS ; ++i)
		value[i] += rhs.value[i];

	return *this;
}

void check_event(opd_header const * header)
{
	char * ctr_name;
	char * ctr_desc;
	char * ctr_um_desc;

	op_cpu cpu = static_cast<op_cpu>(header->cpu_type);
	op_get_event_desc(cpu, header->ctr_event, header->ctr_um,
			  &ctr_name, &ctr_desc, &ctr_um_desc);
}

void check_mtime(opp_samples_files const & samples, string image_name)
{
	time_t newmtime = op_get_mtime(image_name.c_str());
	if (newmtime != samples.first_header().mtime) {
		cerr << "oprofpp: WARNING: the last modified time of the binary file " << image_name << " does not match\n"
		     << "that of the sample file. Either this is the wrong binary or the binary\n"
		     << "has been modified since the sample file was created.\n";
	}
}
