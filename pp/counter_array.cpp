/**
 * @file counter_array.cpp
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

#include "counter_array.h"

using namespace std;
 
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

bool counter_array_t::empty(size_t nr_counters) const
{
	u32 const * first = value;
	u32 const * last  = value + nr_counters;

	return find_if(first, last, bind2nd(not_equal_to<int>(), 0)) == last;
}
