/**
 * @file locate_images.cpp
 * Command-line helper
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author Philippe Elie
 * @author John Levon
 */

#include "file_manip.h"
#include "locate_images.h"

#include <cerrno>
#include <iostream>
#include <sstream>
#include <cstdlib>

using namespace std;


void extra_images::populate(vector<string> const & paths)
{
	vector<string>::const_iterator cit = paths.begin();
	vector<string>::const_iterator end = paths.end();
	for (; cit != end; ++cit) {
		string const path = relative_to_absolute_path(*cit);
		list<string> file_list;
		create_file_list(file_list, path, "*", true);
		list<string>::const_iterator lit = file_list.begin();
		list<string>::const_iterator lend = file_list.end();
		for (; lit != lend; ++lit) {
			value_type v(basename(*lit), dirname(*lit));
			images.insert(v);
		}
	}
}


vector<string> const extra_images::find(string const & name) const
{
	extra_images::matcher match(name);
	return find(match);
}


vector<string> const
extra_images::find(extra_images::matcher const & match) const
{
	vector<string> matches;

	const_iterator cit = images.begin();
	const_iterator end = images.end();

	for (; cit != end; ++cit) {
		if (match(cit->first))
			matches.push_back(cit->second + '/' + cit->first);
	}

	return matches;
}


namespace {

/**
 * Function object for matching a module filename, which
 * has its own special mangling rules in 2.5 kernels.
 */
struct module_matcher : public extra_images::matcher {
public:
	explicit module_matcher(string const & s)
		: extra_images::matcher(s) {}

	virtual bool operator()(string const & candidate) const {
		if (candidate.length() != value.length())
			return false;

		for (string::size_type i = 0 ; i < value.length() ; ++i) {
			if (value[i] == candidate[i])
				continue;
			if (value[i] == '_' &&
				(candidate[i] == ',' || candidate[i] == '-'))
				continue;
			return false;
		}

		return true;
	}
};


/**
 * @param extra_images container where all candidate filename are stored
 * @param image_name binary image name
 *
 * helper for find_image_path either return image name on success or an empty
 * string, output also a warning if we failt to retrieve the image name. All
 * this handling is special for 2.5/2.6 module where daemon are no way to know
 * full path name of module
 */
string const find_module_path(string const & module_name,
                              extra_images const & extra_images)
{
	vector<string> result =
		extra_images.find(module_matcher(module_name));

	if (result.empty()) {
		return string();
	}

	if (result.size() > 1) {
		cerr << "The image name " << module_name
		     << " matches more than one filename." << endl;
	        cerr << "I have used " << result[0] << endl;
	}

	return result[0];
}

} // anon namespace


string const find_image_path(string const & image_name,
                             extra_images const & extra_images)
{
	string const image = relative_to_absolute_path(image_name);

	// simplest case
	if (op_file_readable(image))
		return image;

	if (errno == EACCES)
		return image;

	string const base = basename(image);

	vector<string> result = extra_images.find(base);

	// not found, try a module search
	if (result.empty()) {
		return find_module_path(base + ".ko", extra_images);
	}

	if (result.size() > 1) {
		cerr << "The image name " << image_name
		     << " matches more than one filename, "
		     << "and will be ignored." << endl;
		return string();
	}

	return result[0];
}
