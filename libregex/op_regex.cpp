/**
 * @file op_regex.cpp
 * This file contains implementation for a lightweight wrapper around
 * libc regex, providing regular expression match and replace facility.
 *
 * @remark Copyright 2003 OProfile authors
 * @remark Read the file COPYING
 * @remark Idea comes from TextFilt project <http://textfilt.sourceforge.net>
 *
 * @author Philippe Elie
 */

#include <cerrno>

#include <iostream>
#include <fstream>

#include "string_manip.h"

#include "op_regex.h"

using namespace std;

namespace {

struct regex_runtime_error : op_runtime_error {
	regex_runtime_error(std::string const & pattern, int cerrno);
};

bad_regex::bad_regex(string const & pattern)
	:
	op_exception(pattern)
{
}

regex_runtime_error::regex_runtime_error(string const & pattern, int cerrno)
	:
	op_runtime_error(pattern, cerrno)
{
}

string op_regerror(int err, regex_t const & regexp)
{
	size_t needed_size = regerror(err, &regexp, 0, 0);
	char * buffer = new char [needed_size];
	regerror(err, &regexp, buffer, needed_size);

	return buffer;
}

void op_regcomp(regex_t & regexp, string const & pattern)
{
	int err = regcomp(&regexp, pattern.c_str(), REG_EXTENDED);
	if (err) {
		throw bad_regex("regcomp error: " + op_regerror(err, regexp)
				+ " for pattern : " + pattern);
	}
}

bool op_regexec(regex_t const & regex, char const * str, regmatch_t * match,
	       size_t nmatch)
{
	return regexec(&regex, str, nmatch, match, 0) != REG_NOMATCH;
}

void op_regfree(regex_t & regexp)
{
	regfree(&regexp);
}

}  // anonymous namespace


regular_expression_replace::regular_expression_replace(size_t limit_,
						       size_t limit_defs)
	:
	limit(limit_),
	limit_defs_expansion(limit_defs)
{
}

regular_expression_replace::~regular_expression_replace()
{
	for (size_t i = 0 ; i < v_regexp.size() ; ++i)
		op_regfree(v_regexp[i]);
}

void regular_expression_replace::add_definition(string const & name,
						string const & definition)
{
	string expanded_definition;
	if (expand_string(definition, expanded_definition) == false)
		return;

	defs[name] = expanded_definition;
}

bool regular_expression_replace::add_pattern(string const & pattern,
					     string const & replace)
{
	string expanded_pattern;
	if (expand_string(pattern, expanded_pattern) == false)
		return false;

	regex_t regexp;
	op_regcomp(regexp, expanded_pattern);
	v_regexp.push_back(regexp);
	v_replace.push_back(replace);

	return true;
}

bool regular_expression_replace::expand_string(string const & input,
					       string & result)
{
	string last, expanded(input);
	size_t i = 0;
	for (i = 0 ; i < limit_defs_expansion ; ++i) {
		last = expanded;
		expanded = substitute_definition(last);
		if (expanded == last) {
			break;
		}
	}

	if (i == limit_defs_expansion) {
		cerr << "too many substitution for: " << input << endl;
		return false;
	}

	result = last;

	return true;
}

string regular_expression_replace::substitute_definition(string const & pattern)
{
	string result;
	bool previous_is_escape = false;

	for (size_t i = 0 ; i < pattern.length() ; ++i) {
		if (pattern[i] == '$' && !previous_is_escape) {
			size_t pos = pattern.find('{', i);
			if (pos != i + 1) {
				throw bad_regex("invalid $ in pattern: " + pattern);
			}
			size_t end = pattern.find('}', i);
			if (end == string::npos) {
				throw bad_regex("no matching '}' in pattern: " + pattern);
			}
			string def_name = pattern.substr(pos+1, (end-pos) - 1);
			if (defs.find(def_name) == defs.end()) {
				throw bad_regex("definition not found and used in pattern: (" + def_name + ") " + pattern);
			}
			result += defs[def_name];
			i = end;
		} else {
			if (pattern[i] == '\\' && !previous_is_escape) {
				previous_is_escape = true;
			} else {
				previous_is_escape = false;
			}
			result += pattern[i];
		}
	}

	return result;
}

// FIXME limit output string size ? (cause we can have exponential growing
// of output string through a rule "a" = "aa")
bool regular_expression_replace::execute(string & str) const
{
	bool changed = true;
	for (size_t nr_iter = 0; changed && nr_iter < limit ; ++nr_iter) {
		changed = false;
		for (size_t i = 0 ; i < v_regexp.size() ; ++i) {
			if (do_execute(str, v_regexp[i], v_replace[i])) {
				changed = true;
			}
		}
	}

	return changed == false;
}

bool regular_expression_replace::do_execute(string & str,
					    regex_t const & regexp,
					    string const & replace) const
{
	bool changed = false;

	regmatch_t match[max_match];
	size_t last_pos = 0;
	for (size_t nr_iter = 0;
	     op_regexec(regexp, str.c_str() + last_pos, match, max_match) &&
	       nr_iter < limit;
	     nr_iter++) {
		changed = true;
		do_replace(str, last_pos, replace, match);
	}

	return changed;
}

void regular_expression_replace::do_replace(string & str, size_t start_pos,
					    string const & replace,
					    regmatch_t const * match) const
{
	string inserted;
	for (size_t i = 0 ; i < replace.length() ; ++i) {
		if (replace[i] == '\\') {
			if (i == replace.length() - 1) {
				throw bad_regex("illegal \\ trailer: " + replace);
			}
			++i;
			if (replace[i] == '\\') {
				inserted += '\\';
			}  else if (isdigit(replace[i])) {
				size_t sub_expr = replace[i] - '0';
				if (sub_expr >= max_match) {
					throw bad_regex("illegal group index :" + replace);
				} else if (match[sub_expr].rm_so == -1 &&
					   match[sub_expr].rm_eo == -1) {
					// empty match: nothing todo
				} else if (match[sub_expr].rm_so == -1 ||
					   match[sub_expr].rm_eo == -1) {
					throw bad_regex("illegal match: " + replace);
				} else {
					inserted += str.substr(match[sub_expr].rm_so, match[sub_expr].rm_eo - match[sub_expr].rm_so);
				}
			} else {
				throw bad_regex("expect group index :" + replace);
			}
		} else {
			inserted += replace[i];
		}
	}

	size_t first = match[0].rm_so + start_pos;
	size_t count = match[0].rm_eo - match[0].rm_so;

	str.replace(first, count, inserted);
}

void setup_regex(regular_expression_replace& regex,
		 string const & filename)
{
	ifstream in(filename.c_str());
	if (!in) {
		throw regex_runtime_error("Can't open file " + filename +
					  " for reading", errno);
	}

	regular_expression_replace var_name;
	var_name.add_pattern("^\\$([_a-zA-Z][_a-zA-Z0-9]*)[ ]*=.*", "\\1");
	regular_expression_replace var_value;
	var_value.add_pattern(".*=[ ]*\"(.*)\"", "\\1");

	regular_expression_replace left_rule;
	left_rule.add_pattern("[ ]*\"(.*)\"[ ]*=.*", "\\1");
	regular_expression_replace right_rule;
	right_rule.add_pattern(".*=[ ]*\"(.*)\"", "\\1");

	string str;
	while (getline(in, str)) {
		str = trim(str);
		if (str.length() == 0 || str[0] == '#')
			continue;
		string temp = str;
		var_name.execute(temp);
		if (temp != str) {
			string name = temp;
			temp = str;
			var_value.execute(temp);
			if (temp != str) {
				regex.add_definition(name, temp);
			} else {
				throw bad_regex("invalid input file: " +
						'"' + str + '"');
			}
		} else {
			temp = str;
			left_rule.execute(temp);
			if (temp != str) {
				string left = temp;
				temp = str;
				right_rule.execute(temp);
				if (temp != str) {
					regex.add_pattern(left, temp);
				} else {
					throw bad_regex("invalid input file: "
							+ '"' + str + '"');
				}
			} else {
				throw bad_regex("invalid input file: " +
						'"' + str + '"');
			}
		}
	}
}

