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

// FIXME: printf -> ostream (and elsewhere) 
#include <cstdarg>
#include <algorithm>
#include <sstream>
#include <iomanip>

#include <elf.h>

#include "oprofpp.h"
#include "op_libiberty.h"
#include "op_file.h"
#include "file_manip.h"
#include "string_manip.h"
#include "op_events.h"
#include "op_events_desc.h"
 
using std::string;
using std::vector;
using std::ostream;

int verbose;
char const *samplefile;
char const * imagefile;
int demangle;
char const * exclude_symbols_str;
static vector<string> exclude_symbols;

void op_print_event(ostream & out, int i, op_cpu cpu_type,
		    u8 type, u8 um, u32 count)
{
	char * typenamep;
	char * typedescp;
	char * umdescp;

	op_get_event_desc(cpu_type, type, um, 
			  &typenamep, &typedescp, &umdescp);

	out << "Counter " << i << " counted " << typenamep << " events ("
	    << typedescp << ")";
	if (cpu_type != CPU_RTC) {
		out << " with a unit mask of 0x"
		    << hex << setw(2) << setfill('0') << unsigned(um) << " ("
		    << (umdescp ? umdescp : "Not set") << ")";
	}
	out << " count " << dec << count << endl;
}

/**
 * verbprintf
 */
void verbprintf(char const * fmt, ...)
{
	if (verbose) {
		va_list va;
		va_start(va, fmt);

		vprintf(fmt, va);

		va_end(va);
	}
}

/**
 * remangle - convert a filename into the related sample file name
 * @param image the image filename
 */
static char *remangle(char const * image)
{
	char *file;
	char *c; 

	file = (char *)xmalloc(strlen(OP_SAMPLES_DIR) + strlen(image) + 1);
	
	strcpy(file, OP_SAMPLES_DIR);
	c = &file[strlen(file)];
	strcat(file, image);

	while (*c) {
		if (*c == '/')
			*c = OPD_MANGLE_CHAR;
		c++;
	}
	
	return file;
}

/**
 * demangle_filename - convert a sample filenames into the related
 * image file name
 * @param samples_filename the samples image filename
 *
 * if samples_filename does not contain any %OPD_MANGLE_CHAR
 * the string samples_filename itself is returned.
 */
std::string demangle_filename(const std::string & samples_filename)
{
	std::string result(samples_filename);
	size_t pos = samples_filename.find_first_of(OPD_MANGLE_CHAR);
	if (pos != std::string::npos) {
		result.erase(0, pos);
		std::replace(result.begin(), result.end(), OPD_MANGLE_CHAR, '/');
	}

	return result;
}

/**
 * is_excluded_symbol - check if the symbol is in the exclude list
 * @param symbol symbol name to check
 *
 * return true if symbol is in the list of excluded symbol
 */
bool is_excluded_symbol(const std::string & symbol)
{
	return std::find(exclude_symbols.begin(), exclude_symbols.end(),
			 symbol) != exclude_symbols.end();
}

/**
 * quit_error - quit with error
 * @param err error to show
 * @param optcon the popt context
 *
 * err may be NULL
 */
void quit_error(poptContext optcon, char const *err)
{
	if (err)
		fprintf(stderr, err); 
	poptPrintHelp(optcon, stderr, 0);
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
 * add to the exclude symbol list the symbols contained in the comma
 * separated list of symbols through the gloval var exclude_symbols_str
 */
void handle_exclude_symbol_option()
{
	if (exclude_symbols_str)
		separate_token(exclude_symbols, exclude_symbols_str, ',');  
}
 
/**
 * opp_treat_options - process command line options
 * @param file a filename passed on the command line, can be %NULL
 * @param optcon poptContext to allow better message handling
 * @param image_file where to store the image file name
 * @param sample_file ditto for sample filename
 * @param counter where to put the counter command line argument
 * @param sort_by_counter FIXME
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
void opp_treat_options(char const * file, poptContext optcon,
		       string & image_file, string & sample_file,
		       int & counter, int & sort_by_counter)
{
	char *file_ctr_str;
	int temp_counter;

	/* add to the exclude symbol list the symbols contained in the comma
	 * separated list of symbols */
	handle_exclude_symbol_option();

	/* some minor memory leak from the next calls */
	if (imagefile)
		imagefile = op_relative_to_absolute_path(imagefile, NULL);

	if (samplefile)
		samplefile = op_relative_to_absolute_path(samplefile, NULL);

	if (file) {
		if (imagefile && samplefile) {
			quit_error(optcon, "oprofpp: too many filenames given on command line:" 
				"you can specify at most one sample filename"
				" and one image filename.\n");
		}

		file = op_relative_to_absolute_path(file, NULL);
		if (strchr(file, OPD_MANGLE_CHAR))
			samplefile = file;
		else
			imagefile = file;
	}

	if (!samplefile) { 
		if (!imagefile) { 
			quit_error(optcon, "oprofpp: no samples file specified.\n");
		} else {
			/* we'll "leak" this memory */
			samplefile = remangle(imagefile);
		}
	} 

	/* we can not complete filename checking of imagefile because
	 * it can be derived from the sample filename, we must process
	 * and chop optionnal suffixe "#%d" first */

	/* check for a valid counter suffix in a given sample file */
	temp_counter = -1;
	file_ctr_str = strrchr(samplefile, '#');
	if (file_ctr_str) {
		sscanf(file_ctr_str + 1, "%d", &temp_counter);
	}

	if (temp_counter != -1 && counter != -1 && counter != 0) {
		if ((counter & (1 << temp_counter)) == 0)
			quit_error(optcon, "oprofpp: conflict between given counter and counter of samples file.\n");
	}

	if (counter == -1 || counter == 0) {
		if (temp_counter != -1)
			counter = 1 << temp_counter;
		else
			counter = 1 << 0;	// use counter 0
	}

	/* chop suffixes */
	if (file_ctr_str)
		file_ctr_str[0] = '\0';

	sample_file = samplefile;

	if (!imagefile) {
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
uint counter_mask(const std::string & str)
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

counter_array_t & counter_array_t::operator+=(const counter_array_t & rhs)
{
	for (size_t i = 0 ; i < OP_MAX_COUNTERS ; ++i)
		value[i] += rhs.value[i];

	return *this;
}

/**
 * check_headers - check coherence between two headers.
 * @param f1 first header
 * @param f2 second header
 *
 * verify that header f1 and f2 are coherent.
 * all error are fatal
 */
void check_headers(opd_header const * f1, opd_header const * f2)
{
	if (f1->mtime != f2->mtime) {
		fprintf(stderr, "oprofpp: header timestamps are different (%ld, %ld)\n", f1->mtime, f2->mtime);
		exit(EXIT_FAILURE);
	}

	if (f1->is_kernel != f2->is_kernel) {
		fprintf(stderr, "oprofpp: header is_kernel flags are different\n");
		exit(EXIT_FAILURE);
	}

	if (f1->cpu_speed != f2->cpu_speed) {
		fprintf(stderr, "oprofpp: header cpu speeds are different (%f, %f)",
			f2->cpu_speed, f2->cpu_speed);
		exit(EXIT_FAILURE);
	}

	if (f1->separate_samples != f2->separate_samples) {
		fprintf(stderr, "oprofpp: header separate_samples are different (%d, %d)",
			f2->separate_samples, f2->separate_samples);
		exit(EXIT_FAILURE);
	}
}

void check_event(const struct opd_header * header)
{
	char * ctr_name;
	char * ctr_desc;
	char * ctr_um_desc;

	op_cpu cpu = static_cast<op_cpu>(header->cpu_type);
	op_get_event_desc(cpu, header->ctr_event, header->ctr_um,
			  &ctr_name, &ctr_desc, &ctr_um_desc);
}
