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
	: size(0)
{
}


void count_array_t::resize(size_type newsize)
{
	container.resize(newsize);
	for (size_type i = size; i < newsize; ++i)
		container[i] = 0;
	size = newsize;
}


u32 count_array_t::operator[](size_type index) const
{
	if (index >= size)
		return 0;
	return container[index];
}


u32 & count_array_t::operator[](size_type index)
{
	if (index >= size)
		resize(index + 1);
	return container[index];
}


count_array_t & count_array_t::operator+=(count_array_t const & rhs)
{
	if (rhs.size > size)
		resize(rhs.size);

	for (size_type i = 0 ; i < size ; ++i)
		container[i] += rhs.container[i];

	return *this;
}


count_array_t & count_array_t::operator-=(count_array_t const & rhs)
{
	if (rhs.size > size)
		resize(rhs.size);

	for (size_type i = 0 ; i < size ; ++i)
		container[i] -= rhs.container[i];

	return *this;
}


bool count_array_t::zero() const
{
	return find_if(container.begin(), container.end(),
	               bind2nd(not_equal_to<int>(), 0)) == container.end();
}
