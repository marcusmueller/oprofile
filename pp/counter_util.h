/**
 * @file counter_util.h
 * Counter utility functions
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author John Levon <moz@compsoc.man.ac.uk>
 * @author Philippe Elie <phil_el@wanadoo.fr>
 */

#ifndef COUNTER_UTIL_H
#define COUNTER_UTIL_H
 
#include <string>
 
/**
 * parse_counter_mask -  given a --counter=0,1,..., option parameter return a mask
 * representing each counter. Bit i is on if counter i was specified.
 * So we allow up to sizeof(uint) * CHAR_BIT different counter
 */
int parse_counter_mask(std::string const & str);
 
/**
 * validate_counter - validate the counter nr
 * @param counter_mask bit mask specifying the counter nr to use
 * @param sort_by_counter the counter nr from which we sort
 *
 * All errors are fatal.
 */
void validate_counter(int counter_mask, int & sort_by_counter);

#endif // COUNTER_UTIL_H
