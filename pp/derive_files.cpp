/**
 * @file derive_files.cpp
 * Command-line helper
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author Philippe Elie <phil_el@wanadoo.fr>
 * @author John Levon <moz@compsoc.man.ac.uk>
 */

#include "file_manip.h"
#include "op_mangling.h"
#include "derive_files.h"

#include <iostream>
#include <sstream>
#include <cstdlib>

using std::string;
using std::istringstream;
using std::cerr;
using std::endl;

namespace {

bool possible_sample_file(string const & name)
{
	return name.find_first_of('}') != string::npos;
}

void set_counter_from_filename(string const & name, int & counter)
{
	size_t pos = name.find_last_of('#');

	// if there's no counter number, it must be a derived
	// filename, so let's leave it
	if (pos == string::npos)
		return;

	istringstream is(name.substr(pos + 1));
	is >> counter;
}

}


void derive_files(string const & argument,
	string & image_file, string & sample_file,
	int & counter_mask)
{
	string unknown_file(argument);

	// FIXME: follow links  ...

	if (!image_file.empty())
		image_file = relative_to_absolute_path(image_file);
	if (!sample_file.empty())
		sample_file = relative_to_absolute_path(sample_file);
	if (!unknown_file.empty())
		unknown_file = relative_to_absolute_path(unknown_file);

	// an argument has been specified
	if (!unknown_file.empty()) {
		if (possible_sample_file(unknown_file)) {
			if (!sample_file.empty()) {
				cerr << "Two sample files given." << endl;
				exit(EXIT_FAILURE);
			}
			sample_file = unknown_file;
		} else {
			if (!image_file.empty()) {
				cerr << "Two image files given." << endl;
				exit(EXIT_FAILURE);
			}
			image_file = unknown_file;
		}
	}

	if (sample_file.empty() && image_file.empty()) {
		cerr << "no file specified." << endl;
		exit(EXIT_FAILURE);
	}

	if (sample_file.empty())
		sample_file = remangle_filename(image_file);

	int counter = -1;
	set_counter_from_filename(sample_file, counter);
	if (counter != -1) {
		counter_mask = 1 << counter;
	}

	// default to counter 0
	if (counter_mask == 0)
		counter_mask = 1 << 0;

	strip_counter_suffix(sample_file);

	/* we allow for user to specify a sample filename on the form
	 * /var/lib/oprofile/samples/}bin}nash}}}lib}libc.so so we need to
	 * check against this form of mangled filename
	 */
	if (image_file.empty()) {
		string lib_name;
		string app_name = extract_app_name(sample_file, lib_name);
		if (!lib_name.empty())
			app_name = lib_name;
		image_file = demangle_filename(app_name);
	}
}
