/**
 * @file filename_match.h
 * filename matching
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author Philippe Elie <phil_el@wanadoo.fr>
 * @author John Levon <moz@compsoc.man.ac.uk>
 */

#ifndef FILENAME_MATCH_H
#define FILENAME_MATCH_H

#include <vector>

/// a class to encapsulate filename matching. The behavior look like
/// fnmatch(pattern, filename, 0); eg * match '/' character. See the man page
/// of fnmatch for further details.
class filename_match {
 public:
	/// multiple pattern must be separate by ','. each pattern can contain
	/// leading and trailing blank which are stripped from pattern.
	filename_match(std::string const & include_patterns,
		       std::string const & exclude_patterns);

	/// directly specify include/exclude pattern through the argument
	filename_match(std::vector<std::string> const & include_patterns,
		       std::vector<std::string> const & exclude_patterns);


	/// return if true filename match include_pattern and does not match
	/// exclude_pattern
	bool match(std::string const & filename);

 private:
	// match helper
	static bool match(std::vector<std::string> const & patterns,
			  std::string const & filename);

	std::vector<std::string> include_pattern;
	std::vector<std::string> exclude_pattern;
};

#endif /* !FILENAME_MATCH_H */
