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


void check_mtime(opp_samples_files const & samples, string image_name)
{
	time_t newmtime = op_get_mtime(image_name.c_str());
	if (newmtime != samples.first_header().mtime) {
		cerr << "oprofpp: WARNING: the last modified time of the binary file " << image_name << " does not match\n"
		     << "that of the sample file. Either this is the wrong binary or the binary\n"
		     << "has been modified since the sample file was created.\n";
	}
}
