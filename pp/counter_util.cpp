/**
 * @file counter_util.cpp
 * Counter utility functions
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author John Levon <moz@compsoc.man.ac.uk>
 * @author Philippe Elie <phil_el@wanadoo.fr>
 */

#include "counter_util.h"
#include "op_hw_config.h"

#include "string_manip.h"

#include <vector>
#include <sstream>
#include <iostream>

using std::string;
using std::vector;
using std::cerr;
using std::endl;
using std::istringstream;

/**
 * parse_counter_mask -  given a --counter=0,1,..., option parameter return a mask
 * representing each counter. Bit i is on if counter i was specified.
 * So we allow up to sizeof(uint) * CHAR_BIT different counter
 */
int parse_counter_mask(string const & str)
{
	vector<string> result;
	separate_token(result, str, ',');

	int mask = 0;
	for (size_t i = 0 ; i < result.size(); ++i) {
		istringstream stream(result[i]);
		int counter;
		stream >> counter;
		mask |= 1 << counter;
	}

	return mask;
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
			if ((counter_mask & (1 << i)) != 0) {
				sort_by_counter = i;
				break;
			}
		}
	}

	if ((counter_mask & (1 << sort_by_counter)) == 0) {
		cerr << "invalid sort counter nr " << sort_by_counter << "\n";
		exit(EXIT_FAILURE);
	}
}
