/**
 * @file derive_files.cpp
 * Command-line helper
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author Philippe Elie
 * @author John Levon
 */

#include "file_manip.h"
#include "op_mangling.h"
#include "derive_files.h"

#include <iostream>
#include <sstream>
#include <cstdlib>
#include <cerrno>

using namespace std;

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
 
	string const sub = name.substr(pos + 1);
	istringstream is(sub);
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


void add_to_alternate_filename(alt_filename_t & alternate_filename,
			       vector<string> const & path_names)
{
	vector<string>::const_iterator path;
	for (path = path_names.begin() ; path != path_names.end() ; ++path) {
		list<string> file_list;
		create_file_list(file_list, *path, "*", true);
		list<string>::const_iterator it;
		for (it = file_list.begin() ; it != file_list.end() ; ++it) {
			typedef alt_filename_t::value_type value_t;
			value_t value(basename(*it), dirname(*it));
			alternate_filename.insert(value);
		}
	}
}

/**
 * @param candidate  the condidate filename
 * @param image  the image name to match
 *
 * helper for check_module_name - return true if candidate match image name
 */
static bool match_module_name(string const & candidate, string const & image)
{
	if (candidate.length() != image.length()) {
		return false;
	}

	for (string::size_type i = 0 ; i < image.length() ; ++i) {
		if (image[i] == candidate[i])
			continue;
		if (image[i] == '_' && (candidate[i] == ',' || candidate[i] == '-'))
			continue;
		return false;
	}

	return true;
}

/**
 * @param alt_filename container where all candidate filename are stored
 * @param image_name binary image name
 * @param samples_filename samples filename
 *
 * helper for check_image_name either return image name on success or an empty
 * string, output also a warning if we failt to retrieve the image name. All
 * this handling is special for 2.5/2.6 module where daemon are no way to know
 * full path name of module
 */
static string check_module_name(alt_filename_t const & alt_filename,
				string const & image_name,
				string const & samples_filename)
{
	vector<string> result;
	typedef alt_filename_t::const_iterator it_t;

	for (it_t it = alt_filename.begin(); it != alt_filename.end(); ++it) {
		string const & candidate = it->first;
		if (match_module_name(candidate, image_name)) {
			result.push_back(it->second + "/" + candidate);
		}
	}

	if (result.empty()) {
		static bool first_warn = true;
		if (first_warn) {
			cerr << "I can't locate some binary image file, all\n"
			     << "of this file(s) will be ignored in statistics"
			     << endl
			     << "Have you provided the right -p option ?"
			     << endl;
			first_warn = false;
		}

		cerr << "warning: can't locate image file for samples files : "
		     << samples_filename << endl;

		return string();
	}

	if (result.size() > 1) {
		cerr << "The image name " << samples_filename
		     << " matches more than one filename." << endl;
	        cerr << "I have used " << result[0] << endl;
	}

	return result[0];
}


string check_image_name(alt_filename_t const & alternate_filename,
			string const & image_name,
			string const & samples_filename)
{
	if (op_file_readable(image_name))
		return image_name;

	if (errno == EACCES) {
		static bool first_warn = true;
		if (first_warn) {
			cerr << "You do not have read access to some binary images"
			     << ", all\nof these files will be ignored in"
			     << " the statistics\n";
			first_warn = false;
		}
		cerr << "Access denied for : " << image_name << endl;

		return string(); 
	}

	typedef alt_filename_t::const_iterator it_t;
	pair<it_t, it_t> p_it =
		alternate_filename.equal_range(basename(image_name));

	// Special handling for 2.5/2.6 module
	if (p_it.first == p_it.second) {
		return check_module_name(alternate_filename,
					 basename(image_name) + ".ko",
					 samples_filename);
	}

	if (distance(p_it.first, p_it.second) != 1) {
		return string();
		cerr << "The image name " << samples_filename
		     << " matches more than one filename, and will be ignored." << endl;
		return string();
	}

	return p_it.first->second + '/' + p_it.first->first;
}
