/**
 * @file generic_spec.cpp
 * Container holding an item or a special "match all" item
 *
 * @remark Copyright 2003 OProfile authors
 * @remark Read the file COPYING
 *
 * @author Philippe Elie
 */

#include "generic_spec.h"

using namespace std;


template <>
void generic_spec<string>::set(string const & str)
{
	is_all = false;
	data = str;
}
