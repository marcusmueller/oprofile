/**
 * @file string_manip.cpp
 * std::string helpers
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author Philippe Elie
 * @author John Levon
 */

#include <sstream>

#include "string_manip.h"

using namespace std;

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

string tostr(unsigned int i)
{
	ostringstream ss;
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

string sample_filename(string const& sample_dir,
			string const& sample_filename, int counter)
{
	ostringstream s;

	s << sample_dir;
	if (sample_dir.length() && sample_dir[sample_dir.length() - 1] != '/')
		s << "/";
	s << sample_filename << '#' << counter;

	return s.str();
}
