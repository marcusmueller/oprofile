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


unsigned int touint(string const & s)
{
	unsigned int i;
	istringstream ss(s);
	ss >> i;
	return i;
}


bool tobool(string const & s)
{
	return touint(s);
}


/// split string s by first occurence of char c, returning the second part
string split(string & s, char c)
{
	string::size_type i = s.find_first_of(c);
	if (i == string::npos)
		return string();

	string const tail = s.substr(i + 1);
	s = s.substr(0, i);
	return tail;
}


bool is_prefix(string const & s, string const & prefix)
{
	// gcc 2.95 and below don't have this
	// return s.compare(0, prefix.length(), prefix) == 0;
	return s.find(prefix) == 0;
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


string ltrim(string const & str, string const & totrim)
{
	string result;
	string::size_type pos = str.find_first_not_of(totrim);
	if (pos != string::npos) {
		result = str.substr(pos);
	}
	return result;
}


string rtrim(string const & str, string const & totrim)
{
	string result(str);
	string::size_type pos = str.find_last_not_of(totrim);
	if (pos != string::npos) {
		result = str.substr(0, pos + 1);
	}
	return result;
}


string trim(string const & str, string const & totrim)
{
	return rtrim(ltrim(str, totrim), totrim);
}


string const format_percent(double value, unsigned int width)
{
	ostringstream os;
	// we don't use os << fixed << value; to support gcc 2.95
	os.setf(ios::fixed, ios::floatfield);
	os << value;
	string const orig = os.str();
	if (orig.length() < width) {
		string pad = string(width - (orig.length() + 1), ' ');
		return pad + orig + '%';
	}

	string integer = orig;
	string const fractional = trim(split(integer, '.'));

	// we just overflow here
	if (integer.length() >= width - 2)
		return integer + '%';

	// take off integer, '.', and '%';
	string::size_type remaining = width - (integer.length() + 2);

	string frac;
	string pad;

	if (fractional.length() < remaining) {
		pad = string(remaining - fractional.length(), ' ');
		frac = fractional;
	} else {
		// FIXME: round
		frac = fractional.substr(0, remaining);
	}

	return pad + integer + '.' + frac + '%';
}


