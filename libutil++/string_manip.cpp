/**
 * @file string_manip.cpp
 * std::string helpers
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

string erase_from_last_of(string const & str, char ch)
{
	string result = str;

	string::size_type pos = result.find_last_of(ch);
	if (pos != string::npos)
		result.erase(pos, result.length() - pos);

	return result;
}

string erase_to_last_of(string const & str, char ch)
{
	string result = str;
	string::size_type pos = result.find_last_of(ch);
	if (pos != string::npos)
		result.erase(0, pos + 1);

	return result;
}


string rtrim(string const & str, char ch)
{
	string result = str;

	// a more efficient implementation is possible if we need it.
	string::size_type slash = result.find_last_of(ch);
	if (slash != string::npos)
		result.erase(0, slash + 1);

	return result;
}

string tostr(unsigned int i)
{
	string str;
	std::ostringstream ss(str);
	ss << i;
	return ss.str(); 
}

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
