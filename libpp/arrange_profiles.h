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

	/// human-readable column name
	std::string name;

	/// human-readable long name
	std::string longname;

	/// merging matches against this
	profile_template ptemplate;
};

bool operator<(profile_class const & lhs,
               profile_class const & rhs);


struct profile_classes {
	/// this is only set if we're not classifying on event/count anyway
	std::string event;

	/// CPU info
	std::string cpuinfo;

	/// the actual classes
	std::vector<profile_class> v;
};


/**
 * Take a list of sample filenames, and process them into a set of
 * classes containing profile_sets. Merging is done at this stage
 * as well as attaching dependent profiles to the main image.
 */
profile_classes const
arrange_profiles(std::list<std::string> const & files,
                 merge_option const & merge_by);


/**
 * A set of sample files where the image binary to open
 * are all the same.
 */
struct image_set {
	/// this is main app image, *not* necessarily
	/// the one we need to open
	std::string app_image;

	/// the sample files
	std::list<std::string> files;
};

typedef std::list<image_set> image_group_set;

/**
 * All sample files where the binary image to open is
 * the same.
 */
struct inverted_profile {
	/// the image to open
	std::string image;

	/// all sample files with data for the above image
	std::vector<image_group_set> groups;
};


/**
 * Invert the profile set. For opreport -l, opannotate etc.,
 * processing the profile_classes directly is slow, because
 * we end up opening BFDs multiple times (for each class,
 * dependent images etc.). This function returns an inverted
 * set of sample files, where the primary sort is on the binary
 * image to open.
 */
std::list<inverted_profile> const
invert_profiles(profile_classes const & classes);

#endif /* !ARRANGE_PROFILES_H */
