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
 
u32 count_array_t::operator[](size_type index) const
{
	if (index >= container.size())
		return 0;
	return container[index];
}


u32 & count_array_t::operator[](size_type index)
{
	if (index >= container.size())
		container.resize(index + 1);
	return container[index];
}


count_array_t & count_array_t::operator+=(count_array_t const & rhs)
{
	if (rhs.container.size() > container.size())
		container.resize(rhs.container.size());

	size_type min_size = min(container.size(), rhs.container.size());
	for (size_type i = 0 ; i < min_size; ++i)
		container[i] += rhs.container[i];

	return *this;
}


count_array_t & count_array_t::operator-=(count_array_t const & rhs)
{
	if (rhs.container.size() > container.size())
		container.resize(rhs.container.size());

	size_type min_size = min(container.size(), rhs.container.size());
	for (size_type i = 0 ; i < min_size; ++i)
		container[i] -= rhs.container[i];

	return *this;
}


bool count_array_t::zero() const
{
	return find_if(container.begin(), container.end(),
	               bind2nd(not_equal_to<int>(), 0)) == container.end();
}
