/**
 * @file arrange_profiles.h
 * Classify and process a list of candidate sample files
 * into merged sets and classes.
 *
 * @remark Copyright 2003 OProfile authors
 * @remark Read the file COPYING
 *
 * @author John Levon
 */

#ifndef ARRANGE_PROFILES_H
#define ARRANGE_PROFILES_H

#include <string>
#include <list>
#include <vector>

/**
 * store merging options options used to classify profiles
 */
struct merge_option {
	bool cpu;
	bool lib;
	bool tid;
	bool tgid;
	bool unitmask;
};


/**
 * This describes which parameters are set for each
 * equivalence class.
 */
struct profile_template {
	std::string event;
	std::string count;
	std::string unitmask;
	std::string tgid;
	std::string tid;
	std::string cpu;
};


/**
 * A number of profiles files that are all dependent on
 * the same main (application) profile, for the same
 * dependent image.
 */
struct profile_dep_set {
	/// which dependent image is this set for
	std::string lib_image;

	/// the actual sample files
	std::list<std::string> files;
};

/**
 * A number of profile files all for the same binary with the same
 * profile specification (after merging). Includes the set of dependent
 * profile files, if any.
 */
struct profile_set {
	std::string image;

	/// the actual sample files
	std::list<std::string> files;

	/// all profile files dependent on the main image
	std::list<profile_dep_set> deps;
};


/**
 * A class collection of profiles. This is an equivalence class and
 * will correspond to columnar output of opreport.
 */
struct profile_class {
	std::list<profile_set> profiles;

	/// FIXME: for later
	std::string name;

	/// merging matches against this
	profile_template ptemplate;
};


/**
 * Take a list of sample filenames, and process them into a set of
 * classes containing profile_sets. Merging is done at this stage
 * as well as attaching dependent profiles to the main image.
 */
std::vector<profile_class> const
arrange_profiles(std::list<std::string> const & files,
                 merge_option const & merge_by);

#endif /* !ARRANGE_PROFILES_H */
