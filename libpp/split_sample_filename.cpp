/**
 * @file split_sample_filename.cpp
 * Split a sample filename into its constituent parts
 *
 * @remark Copyright 2003 OProfile authors
 * @remark Read the file COPYING
 *
 * @author Philippe Elie
 */

#include <stdexcept>
#include <vector>
#include <string>
#include <iostream>

#include "split_sample_filename.h"
#include "string_manip.h"

using namespace std;

namespace {

// PP:3.19 event_name.count.unitmask.tgid.tid.cpu
split_sample_filename split_event_spec(string const & event_spec)
{
	typedef vector<string> parts_type;
	typedef parts_type::size_type size_type;

	size_type const nr_parts = 6;

	parts_type parts;
	separate_token(parts, event_spec, '.');

	if (parts.size() != nr_parts) {
		throw invalid_argument("split_event_spec(): bad event specification: " + event_spec);
	}

	for (size_type i = 0; i < nr_parts ; ++i) {
		if (parts[i].empty()) {
			throw invalid_argument("split_event_spec(): bad event specification: " + event_spec);
		}
	}

	split_sample_filename result;

	size_type i = 0;
	result.event = parts[i++];
	result.count = parts[i++];
	result.unitmask = parts[i++];
	result.tgid = parts[i++];
	result.tid = parts[i++];
	result.cpu = parts[i++];

	return result;
}

/**
 * @param component  path component
 *
 * remove from path_component all directory left to {root} or {kern}
 *
 * return the directory name formed from this
 */
string remove_base_dir(vector<string> & path)
{
	string dir_name;

	while (!path.empty()) {
		if (path[0] == "{root}" || path[0] == "{kern}")
			break;
		dir_name += path[0] + '/';
		path.erase(path.begin());
	}

	return dir_name;
}

}  // anonymous namespace


/*
 *  valid filename are:
 *
 * {kern}/name/event_spec
 * {root}/path/to/bin/event_spec
 * {root}/path/to/bin/{dep}/{root}/path/to/bin/event_spec
 * {root}/path/to/bin/{dep}/{kern}/name/event_spec
 *
 * where /name/ denote a unique path component
 */
split_sample_filename split_sample_file(string const & filename)
{
	string::size_type pos = filename.find_last_of('/');
	if (pos == string::npos) {
		throw invalid_argument("split_filename() invalid filename: " +
				       filename);
	}
	string event_spec = filename.substr(pos + 1);
	string filename_spec = filename.substr(0, pos);

	split_sample_filename result = split_event_spec(event_spec);

	result.sample_filename = filename;

	vector<string> path;
	separate_token(path, filename_spec, '/');

	result.base_dir = remove_base_dir(path);

	// pp_interface PP:3.19 to PP:3.23 path must start either with {root}
	// or {kern} and we must found at least 2 component
	if (path.size() < 2 || (path[0] != "{root}" && path[0] != "{kern}")) {
		throw invalid_argument("split_filename() invalid filename: " +
				       filename);
	}

	// PP:3.23 {kern} must be followed by a single path component
	if (path[0] == "{kern}" && path.size() != 2) {
		throw invalid_argument("split_filename() invalid filename: " +
				       filename);
	}

	size_t i;
	for (i = 1 ; i < path.size() ; ++i) {
		if (path[i] == "{dep}")
			break;

		result.image += "/" + path[i];
	}

	if (i == path.size())
		return result;

	++i;

	// PP:3.19 {dep}/ must be followed by {kern}/ or {root}/
	if (path[i] == "{kern}") {
		// PP:3.23 {kern} must be followed by a single path component
		if (path.size() - i != 2) {
			throw invalid_argument("split_filename() invalid filename: " +
					       filename);
		}
	} else if (path[i] != "{root}") {
		throw invalid_argument("split_filename() invalid filename: " +
				       filename);
	}

	++i;

	for (size_t pos = i ; pos < path.size() ; ++pos) {
		result.lib_image += "/" + path[pos];
	}

	return result;
}


ostream & operator<<(ostream & out, split_sample_filename const & data)
{
	out << data.sample_filename << endl;
	out << data.base_dir << " " << data.image << " "
	    << data.lib_image << " " << data.event << " "
	    << data.count << " " << data.unitmask << " "
	    << data.tgid << " " << data.tid << " "
	    << data.cpu << endl;

	return out;
}
