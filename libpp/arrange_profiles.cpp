/**
 * @file arrange_profiles.cpp
 * Classify and process a list of candidate sample files
 * into merged sets and classes.
 *
 * @remark Copyright 2003 OProfile authors
 * @remark Read the file COPYING
 *
 * @author John Levon
 */

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <map>
#include <set>

#include "string_manip.h"
#include "op_header.h"
#include "op_exception.h"

#include "arrange_profiles.h"
#include "parse_filename.h"
#include "locate_images.h"

using namespace std;

namespace {

/**
 * The "axis" says what we've used to split the sample
 * files into the classes. Only one is allowed.
 */
enum axis_types {
	AXIS_EVENT,
	AXIS_UNITMASK,
	AXIS_TGID,
	AXIS_TID,
	AXIS_CPU,
	AXIS_MAX
};

struct axis_t {
	string name;
	string suggestion;
} axes[AXIS_MAX] = {
	{ "event", "specify event: or count:" },
	{ "unitmask", "specify unitmask: or -m unitmask" },
	{ "tgid", "specify tgid: or -m tgid" },
	{ "tid", "specify tid: or -m tid" },
	{ "cpu", "specify cpu: or -m cpu" },
};

/**
 * We have more than axis of classification, tell the user.
 */
void
report_error(int axis, int newaxis)
{
	string str = "attempted to display results for parameter ";
	str += axes[newaxis].name;
	str += " but already displaying results for parameter ";
	str += axes[axis].name;
	str += "\n";
	str += "suggestion: ";
	str += axes[newaxis].suggestion;
	throw op_fatal_error(str);
}


/// find the first sample file header in the class
opd_header const get_header(profile_class const & pclass)
{
	profile_set const & profile = *(pclass.profiles.begin());

	string file;

	// could be only one main app, with no samples for the main image
	if (profile.files.empty()) {
		profile_dep_set const & dep = *(profile.deps.begin());
		list<string> const & files = dep.files;
		file = *(files.begin());
	} else {
		file = *(profile.files.begin());
	}

	return read_header(file);
}


/// Look up the detailed event info for placing in the header.
string const get_event_info(profile_class const & pclass)
{
	return describe_header(get_header(pclass));
}


string const get_cpu_info(profile_class const & pclass)
{
	return describe_cpu(get_header(pclass));
}


/// Give human-readable names to each class.
void name_classes(profile_classes & classes, axis_types axis)
{
	classes.event = get_event_info(classes.v[0]);
	classes.cpuinfo = get_cpu_info(classes.v[0]);

	// If we're splitting on event anyway, clear out the
	// global event name
	if (axis == 0)
		classes.event.erase();

	vector<profile_class>::iterator it = classes.v.begin();
	vector<profile_class>::iterator const end = classes.v.end();

	for (; it != end; ++it) {
		it->name = axes[axis].name + ":";
		switch (axis) {
		case AXIS_EVENT:
			it->name = it->ptemplate.event
				+ ":" + it->ptemplate.count;
			it->longname = get_event_info(*it);
			break;
		case AXIS_UNITMASK:
			it->name += it->ptemplate.unitmask;
			it->longname = "Samples matching a unit mask of ";
			it->longname += it->ptemplate.unitmask;
			break;
		case AXIS_TGID:
			it->name += it->ptemplate.tgid;
			it->longname = "Processes with a thread group ID of ";
			it->longname += it->ptemplate.tgid;
			break;
		case AXIS_TID:
			it->name += it->ptemplate.tid;
			it->longname = "Processes with a thread ID of ";
			it->longname += it->ptemplate.tid;
			break;
		case AXIS_CPU:
			it->name += it->ptemplate.cpu;
			it->longname = "Samples on CPU " + it->ptemplate.cpu;
			break;
		case AXIS_MAX:;
		}
	}
}


/**
 * Name and verify classes.
 */
void identify_classes(profile_classes & classes,
                      merge_option const & merge_by)
{
	profile_template & ptemplate = classes.v[0].ptemplate;
	bool changed[AXIS_MAX] = { false, };

	vector<profile_class>::iterator it = classes.v.begin();
	++it;
	vector<profile_class>::iterator end = classes.v.end();

	// only one class, name it after the event
	if (it == end) {
		changed[AXIS_EVENT] = true;
	}

	for (; it != end; ++it) {
		if (it->ptemplate.event != ptemplate.event
		    ||  it->ptemplate.count != ptemplate.count)
			changed[AXIS_EVENT] = true;

		// we need the merge checks here because each
		// template is filled in from the first non
		// matching profile, so just because they differ
		// doesn't mean it's the axis we care about

		if (!merge_by.unitmask
		    && it->ptemplate.unitmask != ptemplate.unitmask)
			changed[AXIS_UNITMASK] = true;

		if (!merge_by.tgid && it->ptemplate.tgid != ptemplate.tgid)
			changed[AXIS_TGID] = true;

		if (!merge_by.tid && it->ptemplate.tid != ptemplate.tid)
			changed[AXIS_TID] = true;

		if (!merge_by.cpu && it->ptemplate.cpu != ptemplate.cpu)
			changed[AXIS_CPU] = true;
	}

	axis_types axis = AXIS_MAX;

	for (size_t i = 0; i < AXIS_MAX; ++i) {
		if (changed[i]) {
			if (axis != AXIS_MAX)
				report_error(axis, i);
			axis = axis_types(i);
		}
	}

	if (axis == AXIS_MAX) {
		cerr << "Internal error - no equivalence class axis" << endl;
		abort();
	}

	name_classes(classes, axis);
}


/// construct a class template from a profile
profile_template const
template_from_profile(parsed_filename const & parsed,
                      merge_option const & merge_by)
{
	profile_template ptemplate;

	ptemplate.event = parsed.event;
	ptemplate.count = parsed.count;

	if (!merge_by.unitmask)
		ptemplate.unitmask = parsed.unitmask;
	if (!merge_by.tgid)
		ptemplate.tgid = parsed.tgid;
	if (!merge_by.tid)
		ptemplate.tid = parsed.tid;
	if (!merge_by.cpu)
		ptemplate.cpu = parsed.cpu;
	return ptemplate;
}


/**
 * Find a matching class the sample file could go in, or generate
 * a new class if needed.
 * This is the heart of the merging and classification process.
 * The returned value is non-const reference but the ptemplate member
 * must be considered as const
 */
profile_class & find_class(set<profile_class> & classes,
                           parsed_filename const & parsed,
                           merge_option const & merge_by)
{
	profile_class cls;
	cls.ptemplate = template_from_profile(parsed, merge_by);

	pair<set<profile_class>::iterator, bool> ret = classes.insert(cls);

	return const_cast<profile_class &>(*ret.first);
}


/**
 * Add a profile to particular profile set. If the new profile is
 * a dependent image, it gets added to the dep list, or just placed
 * on the normal list of profiles otherwise.
 */
void
add_to_profile_set(profile_set & set, parsed_filename const & parsed)
{
	if (parsed.image == parsed.lib_image) {
		set.files.push_back(parsed.filename);
		return;
	}

	list<profile_dep_set>::iterator it = set.deps.begin();
	list<profile_dep_set>::iterator const end = set.deps.end();

	for (; it != end; ++it) {
		if (it->lib_image == parsed.lib_image) {
			it->files.push_back(parsed.filename);
			return;
		}
	}

	profile_dep_set depset;
	depset.lib_image = parsed.lib_image;
	depset.files.push_back(parsed.filename);
	set.deps.push_back(depset);
}


/**
 * Add a profile to a particular equivalence class. The previous matching
 * will have ensured the profile "fits", so now it's just a matter of
 * finding which sample file list it needs to go on.
 */
void add_profile(profile_class & pclass, parsed_filename const & parsed)
{
	list<profile_set>::iterator it = pclass.profiles.begin();
	list<profile_set>::iterator const end = pclass.profiles.end();

	for (; it != end; ++it) {
		if (it->image == parsed.image) {
			add_to_profile_set(*it, parsed);
			return;
		}
	}

	profile_set set;
	set.image = parsed.image;
	add_to_profile_set(set, parsed);
	pclass.profiles.push_back(set);
}


int numeric_compare(string const & lhs, string const & rhs)
{
	// FIXME: do we need to handle "all" ??
	unsigned int lhsval = touint(lhs);
	unsigned int rhsval = touint(rhs);
	if (lhsval == rhsval)
		return 0;
	if (lhsval < rhsval)
		return -1;
	return 1;
}

};


bool operator<(profile_class const & lhs,
               profile_class const & rhs)
{
	profile_template const & lt = lhs.ptemplate;
	profile_template const & rt = rhs.ptemplate;

	int comp = numeric_compare(lt.cpu, rt.cpu);
	if (comp)
		return comp < 0;

	comp = numeric_compare(lt.tgid, rt.tgid);
	if (comp)
		return comp < 0;

	comp = numeric_compare(lt.tid, rt.tid);
	if (comp)
		return comp < 0;

	comp = numeric_compare(lt.unitmask, rt.unitmask);
	if (comp)
		return comp < 0;

	if (lt.event == rt.event)
		return lt.count < rt.count;
	return lt.event < rt.event;
}


profile_classes const
arrange_profiles(list<string> const & files, merge_option const & merge_by)
{
	set<profile_class> temp_classes;

	list<string>::const_iterator it = files.begin();
	list<string>::const_iterator const end = files.end();

	for (; it != end; ++it) {
		parsed_filename parsed = parse_filename(*it);

		if (parsed.lib_image.empty())
			parsed.lib_image = parsed.image;

		// This simplifies the add of the profile later,
		// if we're lib-merging, then the app_image cannot
		// matter. After this, any non-dependent has
		// image == lib_image
		if (merge_by.lib)
			parsed.image = parsed.lib_image;

		profile_class & pclass =
			find_class(temp_classes, parsed, merge_by);
		add_profile(pclass, parsed);
	}

	profile_classes classes;
	copy(temp_classes.begin(), temp_classes.end(),
	     back_inserter(classes.v));

	if (classes.v.empty())
		return classes;

	// sort by template for nicely ordered columns
	stable_sort(classes.v.begin(), classes.v.end());

	// name and check
	identify_classes(classes, merge_by);

	return classes;
}


namespace {

/// add the files to group of image sets
void add_to_group(image_group_set & group, string const & app_image,
                  list<string> const & files)
{
	image_set set;
	set.app_image = app_image;
	set.files = files;
	group.push_back(set);
}


typedef map<string, inverted_profile> app_map_t;


inverted_profile &
get_iprofile(app_map_t & app_map, string const & image, size_t nr_classes)
{
	app_map_t::iterator ait = app_map.find(image);
	if (ait != app_map.end())
		return ait->second;

	inverted_profile ip;
	ip.image = image;
	ip.groups.resize(nr_classes);
	app_map[image] = ip;
	return app_map[image];
}


/// Pull out all the images, removing any we can't access.
void
verify_and_fill(app_map_t & app_map, list<inverted_profile> & plist,
                extra_images const & extra)
{
	app_map_t::iterator it = app_map.begin();
	app_map_t::iterator const end = app_map.end();

	for (; it != end; ++it) {
		plist.push_back(it->second);
		inverted_profile & ip = plist.back();
		ip.image = find_image_path(ip.image, extra, ip.error);
	}
}

};


list<inverted_profile> const
invert_profiles(profile_classes const & classes, extra_images const & extra)
{
	app_map_t app_map;

	size_t nr_classes = classes.v.size();

	for (size_t i = 0; i < nr_classes; ++i) {
		list<profile_set>::const_iterator pit
			= classes.v[i].profiles.begin();
		list<profile_set>::const_iterator pend
			= classes.v[i].profiles.end();

		for (; pit != pend; ++pit) {
			// files can be empty if samples for a lib image
			// but none for the main image. Deal with it here
			// rather than later.
			if (pit->files.size()) {
				inverted_profile & ip = get_iprofile(app_map,
					pit->image, nr_classes);
				add_to_group(ip.groups[i], pit->image, pit->files);
			}

			list<profile_dep_set>::const_iterator dit
				= pit->deps.begin();
			list<profile_dep_set>::const_iterator const dend
				= pit->deps.end();

			for (;  dit != dend; ++dit) {
				inverted_profile & ip = get_iprofile(app_map,
					dit->lib_image, nr_classes);
				add_to_group(ip.groups[i], pit->image,
				             dit->files);
			}
		}
	}

	list<inverted_profile> inverted_list;

	verify_and_fill(app_map, inverted_list, extra);

	return inverted_list;
}
