/**
 * @file count_array.cpp
 * Container for holding sample counts
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author Philippe Elie
 * @author John Levon
 */

#include <algorithm>
#include <functional>

#include "count_array.h"

using namespace std;
 
count_array_t::count_array_t()
{
	fill_n(value, OP_MAX_COUNTERS, 0);
}


count_array_t & count_array_t::operator+=(count_array_t const & rhs)
{
	for (size_t i = 0 ; i < OP_MAX_COUNTERS ; ++i)
		value[i] += rhs.value[i];

	return *this;
}


count_array_t & count_array_t::operator-=(count_array_t const & rhs)
{
	for (size_t i = 0 ; i < OP_MAX_COUNTERS ; ++i)
		value[i] -= rhs.value[i];

	return *this;
}


bool count_array_t::empty() const
{
	u32 const * first = value;
	u32 const * last  = value + OP_MAX_COUNTERS;

	return find_if(first, last, bind2nd(not_equal_to<int>(), 0)) == last;
}
