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

#include <sstream>

#include "string_manip.h"

using std::string;
using std::vector;


/**
 * erase_from_last_of - erase a sequence from the last occurence
 * of a given char to the end of string
 * @str: string
 * @ch: the char from where we erase character
 *
 * erase char from the last occurence of @ch to the
 * end of @str and return the string
 */
string erase_from_last_of(string const & str, char ch)
{
	string result = str;

	string::size_type pos = result.find_last_of(ch);
	if (pos != string::npos)
		result.erase(pos, result.length() - pos);

	return result;
}

/**
 * erase to_last_of - erase a sequence of character from
 * the begining of string to the last occurence of a given char
 * @str: string
 * @ch: the characterto search
 *
 * erase char from the begin of @str to the last
 * occurence of @ch from and return the string
 */
string erase_to_last_of(string const & str, char ch)
{
	string result = str;
	string::size_type pos = result.find_last_of(ch);
	if (pos != string::npos)
		result.erase(0, pos + 1);

	return result;
}


/**
 * rtrim - remove last trailing character
 * @str: the string
 * @ch: the character to remove
 *
 * Returns the @str removed of its trailing @ch
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

/**
 * separate_token - separate a list of tokens
 * @result: where to put results
 * @str: the string to tokenize
 * @ch: the separator_char
 *
 */
void separate_token(vector<string> & result, const string & str, char sep)
{
	string const temp = str;

	size_t last_pos = 0;
	for (size_t pos = 0 ; pos != temp.length() ; ) {
		pos = temp.find_first_of(sep, last_pos);
		if (pos == string::npos)
			pos = temp.length();

		string token = temp.substr(last_pos, pos - last_pos);

		result.push_back(token);

		if (pos != temp.length())
			last_pos = pos + 1;
	}
}
