/**
 * @file counter_array.h
 * Container for holding sample counts
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 * 
 * @author John Levon <moz@compsoc.man.ac.uk>
 * @author Philippe Elie <phil_el@wanadoo.fr>
 */

#ifndef COUNTER_ARRAY_H
#define COUNTER_ARRAY_H

#include "op_hw_config.h"
#include "op_types.h"

#include <cstddef>
 
/**
 * A simple container of samples counts for a particular address.
 * Can hold OP_MAX_COUNTERS counters
 */
class counter_array_t {
public:
	/// all counters are intialized to zero
	counter_array_t();

	// FIXME: premature optimisation ?
	/**
	 * subscript operator indexed by a counter_nr, no bound check
	 * is performed. 
	 */
	u32 operator[](size_t counter_nr) const {
		return value[counter_nr];
	}

	// FIXME: premature optimisation ?
	/**
	 * subscript operator indexed by a counter_nr, no bound check
	 * is performed.
	 */
	u32 & operator[](size_t counter_nr) {
		return value[counter_nr];
	}

	counter_array_t & operator+=(counter_array_t const & rhs);

private:
	/// container for sample counts
	u32 value[OP_MAX_COUNTERS];
};

#endif // COUNTER_ARRAY_H
