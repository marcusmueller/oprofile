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

#include "arrange_profiles.h"
#include "split_sample_filename.h" // FIXME: merge this code

using namespace std;

namespace {

/**
 * Check if a profile can fit into an equivalence class.
 * This is the heart of the merging and classification process.
 */
bool class_match(profile_template const & ptemplate,
                 split_sample_filename const & split)
{
	if (ptemplate.event != split.event)
		return false;
	if (ptemplate.count != split.count)
		return false;
		
	// remember that if we're merging on any of
	// these, the template value will be empty
	if (!ptemplate.unitmask.empty() && ptemplate.unitmask != split.unitmask)
		return false;
	if (!ptemplate.tgid.empty() && ptemplate.tgid != split.tgid)
		return false;
	if (!ptemplate.tid.empty() && ptemplate.tid != split.tid)
		return false;
	if (!ptemplate.cpu.empty() && ptemplate.cpu != split.cpu)
		return false;
	return true;
}


/// construct a class template from a profile
profile_template const
template_from_profile(split_sample_filename const & split,
                      merge_option const & merge_by)
{
	profile_template ptemplate;

	ptemplate.event = split.event;
	ptemplate.count = split.count;

	if (!merge_by.unitmask)
		ptemplate.unitmask = split.unitmask;
	if (!merge_by.tgid)
		ptemplate.tgid = split.tgid;
	if (!merge_by.tid)
		ptemplate.tid = split.tid;
	if (!merge_by.cpu)
		ptemplate.cpu = split.cpu;
	return ptemplate;
}


/**
 * Find a matching class the sample file could go in, or generate
 * a new class if needed.
 */
profile_class & find_class(vector<profile_class> & classes,
                           split_sample_filename const & split,
                           merge_option const & merge_by)
{
	vector<profile_class>::iterator it = classes.begin();
	vector<profile_class>::iterator const end = classes.end();

	for (; it != end; ++it) {
		if (class_match(it->ptemplate, split))
			return *it;
	}

	// FIXME: here we must verify new classes match only one axis
	// of difference - opreport is not n-dimensional for n > 1
	profile_class pclass;
	pclass.ptemplate = template_from_profile(split, merge_by);
	classes.push_back(pclass);
	return classes.back();
}


/**
 * Add a profile to particular profile set. If the new profile is
 * a dependent image, it gets added to the dep list, or just placed
 * on the normal list of profiles otherwise.
 */
void
add_to_profile_set(profile_set & set, split_sample_filename const & split)
{
	if (split.image == split.lib_image) {
		set.files.push_back(split.sample_filename);
		return;
	}

	list<profile_dep_set>::iterator it = set.deps.begin();
	list<profile_dep_set>::iterator const end = set.deps.end();

	for (; it != end; ++it) {
		if (it->lib_image == split.lib_image) {
			it->files.push_back(split.sample_filename);
			return;
		}
	}

	profile_dep_set depset;
	depset.lib_image = split.lib_image;
	depset.files.push_back(split.sample_filename);
	set.deps.push_back(depset);
}


/**
 * Add a profile to a particular equivalence class. The previous matching
 * will have ensured the profile "fits", so now it's just a matter of
 * finding which sample file list it needs to go on.
 */
void add_profile(profile_class & pclass, split_sample_filename const & split)
{
	list<profile_set>::iterator it = pclass.profiles.begin();
	list<profile_set>::iterator const end = pclass.profiles.end();

	for (; it != end; ++it) {
		if (it->image == split.image) {
			add_to_profile_set(*it, split);
			return;
		}
	}

	profile_set set;
	set.image = split.image;
	add_to_profile_set(set, split);
	pclass.profiles.push_back(set);
}

};


vector<profile_class> const
arrange_profiles(list<string> const & files, merge_option const & merge_by)
{
	vector<profile_class> classes;

	list<string>::const_iterator it = files.begin();
	list<string>::const_iterator const end = files.end();

	for (; it != end; ++it) {
		split_sample_filename split = split_sample_file(*it);

		if (split.lib_image.empty())
			split.lib_image = split.image;

		// This simplifies the add of the profile later,
		// if we're lib-merging, then the app_image cannot
		// matter. After this, any non-dependent split
		// has image == lib_image
		if (merge_by.lib)
			split.image = split.lib_image;

		profile_class & pclass =
			find_class(classes, split, merge_by);
		add_profile(pclass, split);
	}

	// FIXME: sort the class list by template so it is ordered
	// sensibly in opreport columnar output

	return classes;
}
