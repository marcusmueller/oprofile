/**
 * @file op_get_time.c
 * Get current time as a string
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author John Levon <moz@compsoc.man.ac.uk>
 * @author Philippe Elie <phil_el@wanadoo.fr>
 */

#include "op_get_time.h"

#include <time.h>

/**
 * op_get_time - get current date and time
 *
 * Returns a string representing the current date
 * and time, or an empty string on error.
 *
 * The string is statically allocated and should not be freed.
 */
char * op_get_time(void)
{
	time_t t = time(NULL);

	if (t == -1)
		return "";

	return ctime(&t);
}
