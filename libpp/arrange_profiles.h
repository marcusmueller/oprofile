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
 *
 * For example, we could have image == "/bin/bash", where files
 * contains all profiles against /bin/bash, and deps contains
 * the sample file list for /lib/libc.so, /lib/ld.so etc.
 */
struct profile_set {
	std::string image;

	/// the actual sample files for the main image
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
	/**
	 * This is only set if we're not classifying on event/count
	 * anyway - if we're classifying on event/count, then we'll
	 * already output the details of each class's event/count.
	 *
	 * It's only used when classifying by CPU, tgid etc. so the
	 * user can still see what perfctr event was used.
	 */
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
 *
 * The classes correspond to the columns you'll get in opreport:
 * this can be a number of events, or different CPUs, etc.
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
 *
 * This is the "inverse" to some degree of profile_set.
 * For example, here we might have image = "/lib/libc.so",
 * with groups being the profile classifications
 * tgid:404, tgid:301, etc.
 *
 * Within each group there's a number of image_sets.
 * All the sample files listed within the image_sets
 * are still for /lib/libc.so, but they may have
 * different app_image values, e.g. /bin/bash.
 * We need to keep track of the app_image values to
 * make opreport give the right info in the "app"
 * column.
 */
struct inverted_profile {
	/// the image to open
	std::string image;

	/// all sample files with data for the above image
	std::vector<image_group_set> groups;
};


class extra_images;

/**
 * Invert the profile set. For opreport -l, opannotate etc.,
 * processing the profile_classes directly is slow, because
 * we end up opening BFDs multiple times (for each class,
 * dependent images etc.). This function returns an inverted
 * set of sample files, where the primary sort is on the binary
 * image to open.
 *
 * Thus each element in the returned list is for exactly one
 * binary file that we're going to bfd_openr(). Attached to that
 * is the actual sample files we need to process for that binary
 * file. In order to get the output right, these have to be
 * marked with the profile class they're from (hence the groups
 * vector), and the app image that owned the sample file, if
 * applicable (hence image_set).
 */
std::list<inverted_profile> const
invert_profiles(profile_classes const & classes, extra_images const & extra);

#endif /* !ARRANGE_PROFILES_H */
