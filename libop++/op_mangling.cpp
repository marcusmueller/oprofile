/**
 * @file op_mangling.cpp
 * Mangling and unmangling of sample filenames
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author John Levon <moz@compsoc.man.ac.uk>
 * @author Philippe Elie <phil_el@wanadoo.fr>
 */

#include "op_mangling.h"
#include "op_sample_file.h"

#include <algorithm>
 
using std::string;
 
string remangle_filename(string const & filename)
{
	string result = filename;

	std::replace(result.begin(), result.end(), '/', OPD_MANGLE_CHAR);

	return result;
}

/**
 * demangle_filename - convert a sample filenames into the related
 * image file name
 * @param samples_filename the samples image filename
 *
 * if samples_filename does not contain any %OPD_MANGLE_CHAR
 * the string samples_filename itself is returned.
 */
string demangle_filename(string const & samples_filename)
{
	string result(samples_filename);
	size_t pos = samples_filename.find_first_of(OPD_MANGLE_CHAR);
	if (pos != string::npos) {
		result.erase(0, pos);
		std::replace(result.begin(), result.end(), OPD_MANGLE_CHAR, '/');
	}

	return result;
}
