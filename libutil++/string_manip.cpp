/**
 * @file string_manip.cpp
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author Philippe Elie <phil_el@wanadoo.fr>
 * @author John Levon <moz@compsoc.man.ac.uk>
 */

#include <sstream>

#include "string_manip.h"

using std::string;
using std::vector;


/**
 * erase_from_last_of - erase a sequence from the last occurence
 * of a given char to the end of string
 * @param str  string
 * @param ch  the char from where we erase character
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
 * @param str  string
 * @param ch  the characterto search
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
 * @param str  the string
 * @param ch  the character to remove
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
 * @param result  where to put results
 * @param str  the string to tokenize
 * @param ch  the separator_char
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
