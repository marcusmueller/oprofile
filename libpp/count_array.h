/**
 * @file count_array.h
 * Container for holding sample counts
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author John Levon
 * @author Philippe Elie
 */

#ifndef COUNTER_ARRAY_H
#define COUNTER_ARRAY_H

#include "op_hw_config.h"
#include "op_types.h"

#include <cstddef>

/**
 * A simple container of sample counts for a set of count groups.
 * This is used by opreport for side-by-side, describing the sample
 * counts for a row of output. For example a report with two events
 * would use two count groups, one for each event.
 */
class count_array_t {
public:
	/// all counts are intialized to zero
	count_array_t();

	// FIXME: premature optimisation ?
	/**
	 * Index into the count groups for a count value, no at()
	 * style checking
	 */
	u32 operator[](size_t index) const {
		return value[index];
	}

	// FIXME: premature optimisation ?
	/**
	 * Index into the count groups for a count value, no at()
	 * style checking
	 */
	u32 & operator[](size_t index) {
		return value[index];
	}

	/**
	 * return true if all values are zero
	 *
	 * FIXME: I do not like this name, it's not natural for the values
	 * contained to affect a container's "empty()"
	 */
	bool empty() const;

	/**
	 * vectorized += operator
	 */
	count_array_t & operator+=(count_array_t const & rhs);

	/**
	 * vectorized -= operator, overflow shouldn't occur during substraction
	 * (iow: for each components lhs[i] >= rhs[i]
	 */
	count_array_t & operator-=(count_array_t const & rhs);

private:
	/// container for sample counts
	// FIXME too inneficient at memory use point of view. Also OP_MAX_COUNTERS
	// is incorrect: there's no concept of counters in the new pp code,
	// and we can exceed OP_MAX_COUNTERS easily (several events / tids / cpus)
	u32 value[OP_MAX_COUNTERS];
};

#endif // COUNTER_ARRAY_H
