/**
 * @file symbol_functors.cpp
 * Functors for symbol/sample comparison
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author Philippe Elie
 * @author John Levon
 */

#include "symbol_functors.h"

bool less_symbol::operator()(symbol_entry const & lhs,
			     symbol_entry const & rhs) const
{
	if (lhs.image_name.id != rhs.image_name.id)
		return lhs.image_name.id < rhs.image_name.id;

	if (lhs.app_name.id != rhs.app_name.id)
		return lhs.app_name.id < rhs.app_name.id;

	if (lhs.sample.vma != rhs.sample.vma)
		return lhs.sample.vma < rhs.sample.vma;

	if (lhs.name.id != rhs.name.id)
		return lhs.name.id < rhs.name.id;

	return lhs.size < rhs.size;
}
