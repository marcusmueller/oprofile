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
#include "file_manip.h"

#include <algorithm>
#include <iostream>
#include <cerrno>

using std::string;
using std::list;
using std::cerr;
using std::endl;

void strip_counter_suffix(string & name)
{
	size_t pos = name.find_last_of('#');
	name = name.substr(0, pos);
}

string remangle_filename(string const & filename)
{
	string result = filename;

	std::replace(result.begin(), result.end(), '/', OPD_MANGLE_CHAR);

	return result;
}


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


string extract_app_name(const string & name, string & lib_name)
{
	string result(name);
	lib_name = string();

	size_t pos = result.find("}}");
	if (pos != string::npos) {
		result.erase(pos, result.length() - pos);
		lib_name = name.substr(pos + 2);
	}

	return result;
}


void get_sample_file_list(list<string> & file_list,
			  string const & base_dir,
			  string const & filter)
{
	file_list.clear();

	list <string> files;
	if (create_file_list(files, base_dir, filter) == false) {
		cerr << "Can't open directory \"" << base_dir << "\": "
		     << strerror(errno) << endl;
		exit(EXIT_FAILURE);
	}

	list<string>::iterator it;
	for (it = files.begin(); it != files.end(); ++it) {

		// even if caller specify "*" as filter we avoid to get
		// invalid filename
		if (it->find_first_of(OPD_MANGLE_CHAR) == string::npos)
			continue;

		string filename = *it;
		strip_counter_suffix(filename);

		// After stripping the # suffix multiples identicals filenames
		// can exist.
		if (find(file_list.begin(), file_list.end(), filename) ==
		    file_list.end())
			file_list.push_back(filename);
	}
}
