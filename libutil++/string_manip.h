/**
 * @file string_manip.h
 * std::string helpers
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author Philippe Elie <phil_el@wanadoo.fr>
 * @author John Levon <moz@compsoc.man.ac.uk>
 */

#ifndef STRING_MANIP_H
#define STRING_MANIP_H

#include <string>
#include <vector>

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


/**
 * @param str the string
 * @param ch the character to remove
 *
 * Returns str removed of its trailing ch
 */
std::string rtrim(std::string const & str, char ch);

/// conversion to std::string
std::string tostr(unsigned int i);

/**
 * @param result where to put results
 * @param str the string to tokenize
 * @param sep the separator_char
 *
 * separate fild in a string in a list of token; field are
 * separated by the sep character
 */
void separate_token(std::vector<std::string> & result, std::string const & str,
		    char sep);

#endif /* !STRING_MANIP_H */
