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

#ifndef COUNT_ARRAY_H
#define COUNT_ARRAY_H

#include "op_types.h"
#include <vector>

/**
 * A simple container of sample counts for a set of profile classes.
 * This is used by opreport for side-by-side, describing the sample
 * counts for a row of output. For example a report with two events
 * would use two profile classes, one for each event.
 */
class count_array_t {
public:
	/// all counts are intialized to zero
	count_array_t();

	typedef std::vector<u32> container_type;
	typedef container_type::size_type size_type;

	/**
	 * Index into the classes for a count value. An out of
	 * bounds index will return a value of zero.
	 */
	u32 operator[](size_type index) const;

	/**
	 * Index into the classes for a count value. If the index
	 * is larger than the current max index, the array is expanded,
	 * zero-filling any intermediary gaps.
	 */
	u32 & operator[](size_type index);

	/// return true if all values are zero
	bool zero() const;

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
	container_type container;
};

#endif // COUNT_ARRAY_H
