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

#include "string_manip.h"

#include "arrange_profiles.h"
#include "parse_filename.h"

using namespace std;

namespace {

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

	// FIXME: here we must verify new classes match only one axis
	// of difference - opreport is not n-dimensional for n > 1
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


vector<profile_class> const
arrange_profiles(list<string> const & files, merge_option const & merge_by)
{
	vector<profile_class> classes;

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
			find_class(classes, parsed, merge_by);
		add_profile(pclass, parsed);
	}

	// sort by template for nicely ordered columns
	stable_sort(classes.begin(), classes.end());

	return classes;
}
