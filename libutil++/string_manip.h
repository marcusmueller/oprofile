/* COPYRIGHT (C) 2001 by various authors
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59 Temple
 * Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * Part written by John Levon and P. Elie
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
 * @param ch the separator_char
 *
 * separate fild in a string in a list of token; field are
 * separated by the sep character
 */
void separate_token(std::vector<std::string> & result, const std::string & str,
		    char sep);

#endif /* !STRING_MANIP_H */
