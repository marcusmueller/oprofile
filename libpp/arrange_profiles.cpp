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

#include "string_manip.h"
#include "op_header.h"

#include "arrange_profiles.h"
#include "parse_filename.h"

using namespace std;

namespace {

/**
 * Make sure that if we have classes, that only one equivalence
 * relation has been triggered - for example, we can't have
 * both CPU and tgid varying - how would it be labelled ?
 */
void verify_one_dimension(vector<profile_class> const &, bool const *)
{
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
void name_classes(profile_classes & classes, int axis)
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
		switch (axis) {
		case 0:
			it->name = it->ptemplate.event
				+ ":" + it->ptemplate.count;
			it->longname = get_event_info(*it);
			break;
		case 1:
			it->name = "unitmask:";
			it->name += it->ptemplate.unitmask;
			it->longname = "Samples matching a unit mask of ";
			it->longname += it->ptemplate.unitmask;
			break;
		case 2:
			it->name = "tgid:";
			it->name += it->ptemplate.tgid;
			it->longname = "Processes with a thread group ID of ";
			it->longname += it->ptemplate.tgid;
			break;
		case 3:
			it->name = "tid:";
			it->name += it->ptemplate.tid;
			it->longname = "Processes with a thread ID of ";
			it->longname += it->ptemplate.tid;
			break;
		case 4:
			it->name = "cpu:";
			it->name += it->ptemplate.cpu;
			it->longname = "Samples on CPU " + it->ptemplate.cpu;
			break;
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
	bool changed[5] = { false, };

	vector<profile_class>::iterator it = classes.v.begin();
	++it;
	vector<profile_class>::iterator end = classes.v.end();

	// only one class, name it after the event
	if (it == end) {
		changed[0] = true;
	}

	for (; it != end; ++it) {
		if (it->ptemplate.event != ptemplate.event
		    ||  it->ptemplate.count != ptemplate.count)
			changed[0] = true;

		// we need the merge checks here because each
		// template is filled in from the first non
		// matching profile, so just because they differ
		// doesn't mean it's the axis we care about

		if (!merge_by.unitmask
		    && it->ptemplate.unitmask != ptemplate.unitmask)
			changed[1] = true;

		if (!merge_by.tgid && it->ptemplate.tgid != ptemplate.tgid)
			changed[2] = true;

		if (!merge_by.tid && it->ptemplate.tid != ptemplate.tid)
			changed[3] = true;

		if (!merge_by.cpu && it->ptemplate.cpu != ptemplate.cpu)
			changed[4] = true;
	}

	verify_one_dimension(classes.v, changed);

	int axis = -1;

	for (size_t i = 0; i < 5; ++i) {
		if (changed[i]) {
			axis = i;
			break;
		}
	}

	if (axis == -1) {
		cerr << "Internal error - no equivalence class axis" << endl;
		abort();
	}

	name_classes(classes, axis);
}


/**
 * Check if a profile can fit into an equivalence class.
 * This is the heart of the merging and classification process.
 */
bool class_match(profile_template const & ptemplate,
                 parsed_filename const & parsed)
{
	if (ptemplate.event != parsed.event)
		return false;
	if (ptemplate.count != parsed.count)
		return false;
		
	// remember that if we're merging on any of
	// these, the template value will be empty
	if (!ptemplate.unitmask.empty() && ptemplate.unitmask != parsed.unitmask)
		return false;
	if (!ptemplate.tgid.empty() && ptemplate.tgid != parsed.tgid)
		return false;
	if (!ptemplate.tid.empty() && ptemplate.tid != parsed.tid)
		return false;
	if (!ptemplate.cpu.empty() && ptemplate.cpu != parsed.cpu)
		return false;
	return true;
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
 */
profile_class & find_class(vector<profile_class> & classes,
                           parsed_filename const & parsed,
                           merge_option const & merge_by)
{
	vector<profile_class>::iterator it = classes.begin();
	vector<profile_class>::iterator const end = classes.end();

	for (; it != end; ++it) {
		if (class_match(it->ptemplate, parsed))
			return *it;
	}

	profile_class pclass;
	pclass.ptemplate = template_from_profile(parsed, merge_by);
	classes.push_back(pclass);
	return classes.back();
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
	profile_classes classes;

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
			find_class(classes.v, parsed, merge_by);
		add_profile(pclass, parsed);
	}

	if (classes.v.empty())
		return classes;

	// sort by template for nicely ordered columns
	stable_sort(classes.v.begin(), classes.v.end());

	// name and check
	identify_classes(classes, merge_by);

	return classes;
}
