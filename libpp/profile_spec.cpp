/**
 * @file profile_spec.cpp
 * Contains a PP profile specification
 *
 * @remark Copyright 2003 OProfile authors
 * @remark Read the file COPYING
 *
 * @author Philippe Elie
 */

#include <algorithm>
#include <set>
#include <sstream>
#include <iterator>
#include <iostream>

#include "op_fileio.h"
#include "file_manip.h"
#include "op_config.h"
#include "profile_spec.h"
#include "string_manip.h"
#include "glob_filter.h"
#include "locate_images.h"
#include "op_exception.h"

using namespace std;

namespace {

// PP:3.7, full path, or relative path. If we can't find it,
// we should maintain the original to maintain the wordexp etc.
string const fixup_image_spec(string const & str, extra_images const & extra)
{
	// FIXME: what todo if an error in find_image_path() ?
	image_flags flags;
	return find_image_path(str, extra, flags);
}


void fixup_image_spec(vector<string> & images, extra_images const & extra)
{
	vector<string>::iterator it = images.begin();
	vector<string>::iterator const end = images.end();

	for (; it != end; ++it) {
		*it = fixup_image_spec(*it, extra);
	}
}

}


profile_spec::profile_spec(extra_images const & extra)
	:
	normal_tag_set(false),
	sample_file_set(false),
	extra(extra)
{
	parse_table["sample-file"] = &profile_spec::parse_sample_file;
	parse_table["binary"] = &profile_spec::parse_binary;
	parse_table["session"] = &profile_spec::parse_session;
	parse_table["session-exclude"] =
		&profile_spec::parse_session_exclude;
	parse_table["image"] = &profile_spec::parse_image;
	parse_table["image-exclude"] = &profile_spec::parse_image_exclude;
	parse_table["lib-image"] = &profile_spec::parse_lib_image;
	parse_table["event"] = &profile_spec::parse_event;
	parse_table["count"] = &profile_spec::parse_count;
	parse_table["unit-mask"] = &profile_spec::parse_unitmask;
	parse_table["tid"] = &profile_spec::parse_tid;
	parse_table["tgid"] = &profile_spec::parse_tgid;
	parse_table["cpu"] = &profile_spec::parse_cpu;
}


void profile_spec::parse(string const & tag_value)
{
	string value;
	action_t action = get_handler(tag_value, value);
	if (!action) {
		throw invalid_argument("profile_spec::parse(): not "
				       "a valid tag \"" + tag_value + "\"");
	}

	(this->*action)(value);
}


bool profile_spec::is_valid_tag(string const & tag_value)
{
	string value;
	return get_handler(tag_value, value);
}


void profile_spec::validate()
{
	// 3.3 sample_file can be used only with binary
	// 3.4 binary can be used only with sample_file
	if (normal_tag_set && (sample_file_set || !binary.empty())) {
		throw invalid_argument("Cannot specify sample-file: or "
			"binary: tag with another tag");
	}

	// PP:3.5 no session given means use the current session.
	if (session.empty()) {
		session.push_back("current");
	}

	// PP:3.7 3.8 3.9 3.10: is it the right time to translate all filename
	// to absolute path ? (if yes do it after plugging this code in
	// oprofile)
}


void profile_spec::set_image_or_lib_name(string const & str)
{
	normal_tag_set = true;
	/* FIXME: what does spec say about this being allowed to be
	 * a comma list or not ? */
	image_or_lib_image.push_back(fixup_image_spec(str, extra));
}


void profile_spec::parse_sample_file(string const & str)
{
	sample_file_set = true;
	file_spec.set_sample_filename(str);
}


void profile_spec::parse_binary(string const & str)
{
	binary = str;
}


void profile_spec::parse_session(string const & str)
{
	normal_tag_set = true;
	separate_token(session, str, ',');
}


void profile_spec::parse_session_exclude(string const & str)
{
	normal_tag_set = true;
	separate_token(session_exclude, str, ',');
}


void profile_spec::parse_image(string const & str)
{
	normal_tag_set = true;
	separate_token(image, str, ',');
	fixup_image_spec(image, extra);
}


void profile_spec::parse_image_exclude(string const & str)
{
	normal_tag_set = true;
	separate_token(image_exclude, str, ',');
}


void profile_spec::parse_lib_image(string const & str)
{
	normal_tag_set = true;
	separate_token(lib_image, str, ',');
	fixup_image_spec(image, extra);
}


void profile_spec::parse_event(string const & str)
{
	normal_tag_set = true;
	event.set(str, false);
}


void profile_spec::parse_count(string const & str)
{
	normal_tag_set = true;
	count.set(str, false);
}


void profile_spec::parse_unitmask(string const & str)
{
	normal_tag_set = true;
	unitmask.set(str, false);
}


void profile_spec::parse_tid(string const & str)
{
	normal_tag_set = true;
	tid.set(str, false);
}


void profile_spec::parse_tgid(string const & str)
{
	normal_tag_set = true;
	tgid.set(str, false);
}


void profile_spec::parse_cpu(string const & str)
{
	normal_tag_set = true;
	cpu.set(str, false);
}


profile_spec::action_t
profile_spec::get_handler(string const & tag_value, string & value)
{
	string::size_type pos = tag_value.find_first_of(':');
	if (pos == string::npos) {
		return 0;
	}

	string tag(tag_value.substr(0, pos));
	value = tag_value.substr(pos + 1);

	parse_table_t::const_iterator it = parse_table.find(tag);
	if (it == parse_table.end()) {
		return 0;
	}

	return it->second;
}


bool profile_spec::match(string const & filename) const
{
	filename_spec spec(filename);

	// PP:3.3 if spec was defined through sample-file: match it directly
	if (sample_file_set) {
		return file_spec.match(spec, binary);
	}

	bool matched_by_image_or_lib_image = false;

	// PP:3.19
	if (!image_or_lib_image.empty()) {
		// Need the path search for the benefit of modules
		// which have "/oprofile" or similar
		string simage = fixup_image_spec(spec.image, extra);
		string slib_image = fixup_image_spec(spec.lib_image, extra);
		glob_filter filter(image_or_lib_image, image_exclude);
		if (filter.match(simage) || filter.match(slib_image)) {
			matched_by_image_or_lib_image = true;
		}
	}

	if (!matched_by_image_or_lib_image) {
		// PP:3.7 3.8
		if (!image.empty()) {
			glob_filter filter(image, image_exclude);
			if (!filter.match(spec.image)) {
				return false;
			}
		} else if (!image_or_lib_image.empty()) {
			// image.empty() means match all except if user
			// specified image_or_lib_image
			return false;
		}

		// PP:3.9 3.10
		if (!lib_image.empty()) {
			glob_filter filter(lib_image, image_exclude);
			if (!filter.match(spec.lib_image)) {
				return false;
			}
		} else if (image.empty() && !image_or_lib_image.empty()) {
			// lib_image empty means match all except if user
			// specified image_or_lib_image *or* we already
			// matched this spec through image
			return false;
		}
	}

	if (!matched_by_image_or_lib_image) {
		// if we don't match by image_or_lib_image we must try to
		// exclude from spec, exclusion from image_or_lib_image has
		// been handled above
		vector<string> empty;
		glob_filter filter(empty, image_exclude);
		if (!filter.match(spec.image)) {
			return false;
		}
		if (!spec.lib_image.empty() && !filter.match(spec.lib_image)) {
			return false;
		}
	}

	if (!event.match(spec.event)) {
		return false;
	}

	if (!count.match(spec.count)) {
		return false;
	}

	if (!unitmask.match(spec.unitmask)) {
		return false;
	}

	if (!cpu.match(spec.cpu)) {
		return false;
	}

	if (!tid.match(spec.tid)) {
		return false;
	}

	if (!tgid.match(spec.tgid)) {
		return false;
	}

	return true;
}


/* TODO */
static bool substitute_alias(profile_spec & /*parser*/,
			     string const & /*arg*/)
{
	return false;
}


profile_spec profile_spec::create(vector<string> const & args,
                                  extra_images const & extra)
{
	profile_spec spec(extra);

	for (size_t i = 0 ; i < args.size() ; ++i) {
		if (spec.is_valid_tag(args[i])) {
			spec.parse(args[i]);
		} else if (!substitute_alias(spec, args[i])) {
			char * filename = op_get_link(args[i].c_str());
			string file = filename ? filename : args[i].c_str();
			file = relative_to_absolute_path(file,
			                                 dirname(args[i]));
			spec.set_image_or_lib_name(file);
			if (filename)
				free(filename);
		}
	}

	spec.validate();

	return spec;
}

namespace {

vector<string> filter_session(vector<string> const & session,
			      vector<string> const & session_exclude)
{
	vector<string> result(session);

	if (result.empty()) {
		result.push_back("current");
	}

	for (size_t i = 0 ; i < session_exclude.size() ; ++i) {
		// FIXME: would we use fnmatch on each item, are we allowed
		// to --session=current* ?
		vector<string>::iterator it =
			find(result.begin(), result.end(), session_exclude[i]);

		if (it != result.end()) {
			result.erase(it);
		}
	}

	return result;
}


bool valid_candidate(string const & filename, profile_spec const & spec,
		     bool exclude_dependent)
{
	if (spec.match(filename)) {
		if (exclude_dependent &&
		    filename.find("{dep}") != string::npos)
			return false;
		return true;
	}

	return false;
}

}  // anonymous namespace


list<string> profile_spec::generate_file_list(bool exclude_dependent) const
{
	// FIXME: isn't remove_duplicates faster than doing this, then copy() ?
	set<string> unique_files;

	vector<string> sessions = filter_session(session, session_exclude);

	if (sessions.empty()) {
		ostringstream os;
		os << "No session given" << endl;
		os << "included session was:" << endl;
		copy(session.begin(), session.end(),
		     ostream_iterator<string>(os, "\n"));
		os << "excluded session was:" << endl;
		copy(session_exclude.begin(), session_exclude.end(),
		     ostream_iterator<string>(os, "\n"));
		throw invalid_argument(os.str());
	}

	bool found_file = false;

	vector<string>::const_iterator cit = sessions.begin();
	vector<string>::const_iterator end = sessions.end();

	for (; cit != end; ++cit) {
		if (cit->empty())
			continue;

		string base_dir;
		if ((*cit)[0] != '.' && (*cit)[0] != '/')
			base_dir = OP_SAMPLES_DIR;
		base_dir += *cit;

		base_dir = relative_to_absolute_path(base_dir);

		list<string> files;
		create_file_list(files, base_dir, "*", true);

		if (!files.empty())
			found_file = true;

		list<string>::const_iterator it = files.begin();
		list<string>::const_iterator fend = files.end();
		for (; it != fend; ++it) {
			if (valid_candidate(*it, *this, exclude_dependent)) {
				unique_files.insert(*it);
			}
		}
	}

	if (!found_file) {
		ostringstream os;
		os  << "No sample file found: try running opcontrol --dump\n"
		    << "or specify a session containing sample files\n";
		throw op_fatal_error(os.str());
	}

	list<string> result;
	copy(unique_files.begin(), unique_files.end(), back_inserter(result));

	return result;
}
