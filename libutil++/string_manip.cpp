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
#include <iomanip>

#include <cstdlib>

#include "string_manip.h"

using namespace std;


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


string ltrim(string const & str, string const & totrim)
{
	string result(str);

	return result.erase(0, result.find_first_not_of(totrim));
}


string rtrim(string const & str, string const & totrim)
{
	string result(str);

	return result.erase(result.find_last_not_of(totrim) + 1);
}


string trim(string const & str, string const & totrim)
{
	return rtrim(ltrim(str, totrim), totrim);
}


string const format_double(double value, size_t int_width, size_t fract_width)
{
	ostringstream os;

	if (value > .001) {
		// os << fixed << value unsupported by gcc 2.95
		os.setf(ios::fixed, ios::floatfield);
		os << setw(int_width + fract_width + 1)
		   << setprecision(fract_width) << value;
	} else {
		// os << scientific << value unsupported by gcc 2.95
		os.setf(ios::scientific, ios::floatfield);
		os << setw(int_width + fract_width + 1)
		   // - 3 to count exponent part
		   << setprecision(fract_width - 3) << value;
	}

	return os.str();
}

template <>
unsigned int lexical_cast_no_ws<unsigned int>(std::string const & str)
{
	char* endptr;

	// 2.91.66 fix
	unsigned long ret = 0;
	ret = strtoul(str.c_str(), &endptr, 0);
	if (*endptr) {
		throw std::invalid_argument("lexical_cast_no_ws<T>(\""+ str +"\")");
	}
	return ret;
}
