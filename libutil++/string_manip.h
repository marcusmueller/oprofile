/**
 * @file string_manip.h
 * std::string helpers
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author Philippe Elie
 * @author John Levon
 */

#ifndef STRING_MANIP_H
#define STRING_MANIP_H

#include <string>
#include <vector>
#include <sstream>
#include <stdexcept>

/**
 * @param str string
 * @param ch the char from where we erase character
 *
 * erase char from the last occurence of ch to the end of str and return
 * the string
 */
std::string erase_from_last_of(std::string const & str, char ch);

/**
 * @param str string
 * @param ch the characterto search
 *
 * erase char from the begin of str to the last
 * occurence of ch from and return the string
 */
std::string erase_to_last_of(std::string const & str, char ch);


/// conversion to std::string
std::string tostr(unsigned int i);

/// conversion to uint
unsigned int touint(std::string const & s);

/// conversion to bool
bool tobool(std::string const & s);

/// split string s by first occurence of char c, returning the second part.
/// s is set to the first part. Neither include the split character
std::string split(std::string & s, char c);

/// return true if "prefix" is a prefix of "s"
bool is_prefix(std::string const & s, std::string const & prefix);

/**
 * @param result where to put results
 * @param str the string to tokenize
 * @param sep the separator_char
 *
 * separate fild in a string in a list of token; field are
 * separated by the sep character, sep char can be escaped
 * by '\\' to specify a sep char in a token, '\\' not followed
 * by a sep is taken as it e.g. "\,\a" --> ",\a"
 */
void separate_token(std::vector<std::string> & result, std::string const & str,
		    char sep);

/// remove trim chars from start of input string return the new string
std::string ltrim(std::string const & str, std::string const & totrim = "\t ");
/// remove trim chars from end of input string return the new string
std::string rtrim(std::string const & str, std::string const & totrim = "\t ");
/// ltrim(rtrim(str))
std::string trim(std::string const & str, std::string const & totrim = "\t ");

/**
 * format_double - smart format of double value
 * @param value - the value
 * @param int_width - the maximum integer integer width default to 2
 * @param frac_width - the fractionnary width default to 4
 *
 * This formats a percentage into exactly the given width and returns
 * it. If the integer part is larger than the given int_width, the
 * returned string will be wider. The returned string is never
 * shorter than (fract_with + int_width + 1)
 *
 */
std::string const format_double(double value, size_t int_width,
				size_t frac_width);

/// prefered width to format percentage
static unsigned int const percent_int_width = 2;
static unsigned int const percent_fract_width = 4;
static unsigned int const percent_width = percent_int_width + percent_fract_width + 1;


/**
 * convert str to a T through an istringstream.
 * No leading or trailing whitespace is allowed.
 *
 * Throws invalid_argument if conversion fail.
 *
 * Note that this is not as foolproof as boost's lexical_cast
 */
template <class T>
T lexical_cast_no_ws(std::string const & str)
{
	T value;

	std::istringstream in(str);
	// this doesn't work properly for 2.95/2.91 so with these
	// compiler " 33" is accepted as valid input, no big deal.
	in.unsetf(std::ios::skipws);

	in >> value;

	if (in.fail()) {
		throw std::invalid_argument("lexical_cast_no_ws<T>(\""+ str +"\")");
	}

	// we can't check eof here, eof is reached at next read.
	char ch;
	in >> ch;
	if (!in.eof()) {
		throw std::invalid_argument("lexical_cast_no_ws<T>(\""+ str +"\")");
	}

	return value;
}

// FIXME: a hack to accept hexadecimal for unsigned int. We must fix it in a
// better way (removing touint(), tobool(), tostr()). Do we really need
// a strict checking against WS ? (this strict conversion was to help
// validating sample filename)
template <>
unsigned int lexical_cast_no_ws<unsigned int>(std::string const & str);

#endif /* !STRING_MANIP_H */
