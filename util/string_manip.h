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

string erase_from_last_of(string const & str, char ch);
string erase_to_last_of(string const & str, char ch);

std::string rtrim(std::string const & str, char ch);

/// conversion to std::string
std::string tostr(unsigned int i);

#endif /* !STRING_MANIP_H */
