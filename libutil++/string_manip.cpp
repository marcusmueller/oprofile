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
	char last_ch = '\0';
	string next;

	for (size_t pos = 0 ; pos != str.length() ; ++pos) {
		char ch = str[pos];
		if (last_ch == '\\') {
			if (ch != sep)
				// '\' not followed by ',' are taken as it
				next += last_ch;
			next += ch;
		} else if (ch == sep) {
			result.push_back(next);
			// some stl lacks string::clear()
			next.erase(next.begin(), next.end());
		} else {
			next += ch;
		}
		last_ch = ch;
	}

	if (!next.empty())
		result.push_back(next);
}
