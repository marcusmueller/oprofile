/**
 * @file generic_spec.h
 * Container holding an item or a special "match all" item
 *
 * @remark Copyright 2003 OProfile authors
 * @remark Read the file COPYING
 *
 * @author Philippe Elie
 */

#ifndef GENERIC_SPEC_H
#define GENERIC_SPEC_H

#include <stdexcept>
#include <string>
#include <sstream>

#include "string_manip.h"


/**
 * used to hold something like { int cpu_nr, bool is_all };
 * to store a sub part of a samples filename see PP:3.21.
 */
template <class T>
class generic_spec
{
public:
	/**
	 * build a default spec which match anything
	 */
	generic_spec();

	/// build a spec from a string, valid argument are "all"
	/// or a string convertible to T through istringtream(str) >> data
	/// conversion is strict, no space are allowed at begin or end of str
	void set(std::string const &);

	/// return true if rhs match this spec. Sub part of PP:3.24
	bool match(T const & rhs, bool allow_all_match = true) const {
		return (allow_all_match && is_all) || rhs == data;
	}

	/// return true if rhs match this spec. Sub part of PP:3.24
	bool match(generic_spec<T> const & rhs) const {
		return is_all || rhs.is_all || rhs.data == data;
	}

private:
	T data;
	bool is_all;
};


template <class T>
generic_spec<T>::generic_spec()
	:
	data(T()),
	is_all(true)
{
}


template <class T>
void generic_spec<T>::set(std::string const & str)
{
	if (str == "all") {
		is_all = true;
		return;
	}

	is_all = false;
	data = lexical_cast_no_ws<T>(str);
}


/// An explicit specialization because generic_spec<string> doesn't want
/// a strict conversion but a simple copy
template <>
void generic_spec<std::string>::set(std::string const & str);

#endif /* !GENERIC_SPEC_H */
