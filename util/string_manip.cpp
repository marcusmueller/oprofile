/* COPYRIGHT (C) 2001 by ?
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
 * first written by John Levon and P. Elie
 */

#include <sstream>

#include "string_manip.h"

using std::string;


/**
 * dirname - get the path component of a filename
 * @file_name: filename
 *
 * Returns the path name of a filename with trailing '/' removed.
 */
string dirname(string const & file_name)
{
	string result = file_name;

	string::size_type slash = result.find_last_of('/');
	if (slash != string::npos)
		result.erase(slash, result.length() - slash);

	return result;
}

/**
 * basename - get the basename of a path
 * @path_name: path
 *
 * Returns the basename of a path with trailing '/' removed.
 */
string basename(string const & path_name)
{
	string result = rtrim(path_name, '/');

	string::size_type slash = result.find_last_of('/');
	if (slash != string::npos)
		result.erase(0, slash + 1);

	return result;
}


/**
 * rtrim - remove trailing character
 * @str: the string
 * @ch: the character to remove
 *
 * Returns the @str removed of it's trailing @ch
 */
string rtrim(string const & str, char ch)
{
	string result = str;

	// a more efficient implementation is possible if we need it.
	string::size_type slash = result.find_last_of(ch);
	if (slash != string::npos)
		result.erase(0, slash + 1);

	return result;
}

/**
 * tostr - convert integer to str
 * i: the integer
 *
 * Returns the converted string
 */ 
string tostr(unsigned int i)
{
	string str;
	std::ostringstream ss(str);
	ss << i;
	return ss.str(); 
}
