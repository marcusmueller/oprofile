/**
 * @file partition_files.h
 * Encapsulation for merging and partitioning samples filename set
 *
 * @remark Copyright 2003 OProfile authors
 * @remark Read the file COPYING
 *
 * @author Philippe Elie
 */

#ifndef PARTITION_FILES_H
#define PARTITION_FILES_H

#include <string>
#include <iostream>
#include <vector>
#include <list>
#include <set>
#include <map>

#include "split_sample_filename.h"
#include "locate_images.h"
#include "arrange_profiles.h"

/**
 * unmergeable profile specification are those with distinct event/count
 */
struct unmergeable_profile {
	std::string event;
	std::string count;
	unmergeable_profile(std::string const & event_, 
			    std::string const & count_);

	bool operator<(unmergeable_profile const & rhs) const;
};

/**
 * @param files files the file list from we extract unmergeable spec
 *
 * return a vector of unmergeable profile
 *
 */
std::vector<unmergeable_profile> merge_profile(std::list<std::string> const & files);

/// convenience function for debug/verbose
std::ostream & operator<<(std::ostream & out, unmergeable_profile const & lhs);

/// split samples filenames lists by unmergeable profile
typedef std::vector<std::list<std::string> > unmergeable_samplefile;

/**
 * @param files samples filename list
 * @param profiles different unmergeable profile built from files
 */
unmergeable_samplefile
unmerge_samplefile(std::list<std::string> const & files,
	std::vector<unmergeable_profile> const & profiles);


/// Partition a list of sample filename.
class partition_files {

public:
	typedef std::list<split_sample_filename> filename_set;

	/**
	 * @param files a list of filename to partition
	 * @param merge_by  specify what merging are allowed
	 *
	 * complexity: f(N*log(N)) N: files.size()
	 */
	partition_files(std::list<std::string> const & files,
			merge_option const & merge_by);


	/**
	 * return the number of unmerged set of filename
	 */
	size_t nr_set() const;

	/*
	 * @param index filename set index
	 *
	 * return the filename set at position index
	 */
	filename_set const & set(size_t index) const;

private:
	typedef std::list<filename_set> filename_partition;
	filename_partition filenames;
};


typedef std::multimap<std::string, split_sample_filename const> image_set;

/**
 * @param files a set of sample filename to sort
 * @param extra_images container of extra images found
 *
 * return the same set as passed in files but sorted by image name, where
 * image name will be the bfd file to open. This is to allow caller to avoid
 * to bfd_open more than one time each binary image
 */
image_set sort_by_image(partition_files const & files,
                        extra_images const & extra_images);


#endif /* !PARTITION_FILES_H */
