/**
 * @file filename_match.cpp
 * filename matching
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author Philippe Elie
 * @author John Levon
 */

#include <fnmatch.h>

#include <string>
#include <vector>

#include "filename_match.h"
#include "string_manip.h"
#include "file_manip.h"

using namespace std;

filename_match::filename_match(string const & include_patterns,
			       string const & exclude_patterns)
{
	separate_token(include_pattern, include_patterns, ',');
	separate_token(exclude_pattern, exclude_patterns, ',');
}

filename_match::filename_match(vector<string> const & include_patterns,
			       vector<string> const & exclude_patterns)
	:
	include_pattern(include_patterns),
	exclude_pattern(exclude_patterns)
{
}

bool filename_match::match(string const & filename) const
{
	string const & base = basename(filename);

	// first, if any component of the dir is listed in exclude -> no
	string comp = dirname(filename);
	while (!comp.empty() && comp != "/") {
		if (match(exclude_pattern, basename(comp)))
			return false;
		// FIXME: test uneccessary, wait a decent testsuite before
		// removing
		if (comp == dirname(comp))
			break;
		comp = dirname(comp);
	}

	// now if the file name is specifically excluded -> no
	if (match(exclude_pattern, base))
		return false;

	// now if the file name is specifically included -> yes
	if (match(include_pattern, base))
		return true;

	// now if any component of the path is included -> yes
	// note that the include pattern defaults to '*'
	string compi = dirname(filename);
	while (!compi.empty() && compi != "/") {
		if (match(include_pattern, basename(compi)))
			return true;
		// FIXME see above.
		if (compi == dirname(compi))
			break;
		compi = dirname(compi);
	}

	return false;
}

bool filename_match::match(vector<string> const & patterns,
			   string const & filename)
{
	bool ok = false;
	for (size_t i = 0 ; i < patterns.size() && !ok; ++i) {
		if (fnmatch(patterns[i].c_str(), filename.c_str(), 0) != FNM_NOMATCH)
			ok = true;
	}

	return ok;
}
