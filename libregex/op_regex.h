/**
 * @file op_regex.h
 * This file contains various definitions and interface for a
 * lightweight wrapper around libc regex, providing match
 * and replace facility.
 *
 * @remark Copyright 2003 OProfile authors
 * @remark Read the file COPYING
 * @remark Idea comes from TextFilt project <http://textfilt.sourceforge.net>
 *
 * @author Philippe Elie
 */

#ifndef OP_REGEX_H
#define OP_REGEX_H

#include <sys/types.h>  // required by posix before including regex.h
#include <regex.h>

#include <string>
#include <vector>
#include <map>

#include "op_exception.h"

/**
 * ill formed regular expression or expression throw such exception
 */
struct bad_regex : op_exception {
	bad_regex(std::string const & pattern);
};

/**
 * lightweight encapsulation of regex lib search and replace
 */
class regular_expression_replace {
public:
	regular_expression_replace(size_t limit = 100,
				   size_t limit_defs_expansion = 100);
	~regular_expression_replace();

	void add_definition(std::string const & name,
			    std::string const & replace);
	bool add_pattern(std::string const & pattern,
			 std::string const & replace);

	bool execute(std::string & str) const;
private:
	// helper to execute
	bool do_execute(std::string & str, regex_t const & regexp,
			std::string const& replace) const;
	void do_replace(std::string & str, size_t start_pos,
			std::string const & replace,
			regmatch_t const * match) const;

	bool expand_string(std::string const & input, std::string & result);

	// helper to add_pattern
	std::string substitute_definition(std::string const & pattern);

	// don't increase too, it have direct impact on performance
	static const size_t max_match = 16;

	size_t limit;
	size_t limit_defs_expansion;
	std::vector<regex_t> v_regexp;
	std::vector<std::string> v_replace;
	/// dictionary of regular definition
	typedef std::map<std::string, std::string> defs_dict;
	defs_dict defs;
};

/**
 * @param regex the regular_expression_replace to fill
 * @param filename the filename from where the deifnition and pattern are read
 *
 * add to regex pattern and regular definition read from the given file
 */
void setup_regex(regular_expression_replace& regex,
		 std::string const & filename);

#endif /* !OP_REGEX_H */
