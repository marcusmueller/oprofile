/**
 * @file split_sample_filename.h
 * Split a sample filename into its constituent parts
 *
 * @remark Copyright 2003 OProfile authors
 * @remark Read the file COPYING
 *
 * @author Philippe Elie
 */

#ifndef SPLIT_SAMPLE_FILENAME_H
#define SPLIT_SAMPLE_FILENAME_H

#include <string>

/**
 * a convenience class to store result of split_filename
 */
struct split_sample_filename
{
	std::string base_dir;
	std::string image;
	std::string lib_image;
	std::string event;
	std::string count;
	std::string unitmask;
	std::string tgid;
	std::string tid;
	std::string cpu;

	/// the original sample filename from where above components are built
	std::string sample_filename;
};


/// debugging helper
std::ostream & operator<<(std::ostream &, split_sample_filename const &);


/**
 * split a sample filename
 * @param filename in: samples filename
 *
 * filename is split into eight parts, the lib_image is optionnal and can
 * be empty on successfull call. All other error are fatal. filenames
 * are encoded according to PP:3.19 to PP:3.25
 *
 * all error throw an std::invalid_argument exception
 *
 * return the split filename
 */
split_sample_filename split_sample_file(std::string const & filename);

#endif /* !SPLIT_SAMPLE_FILENAME_H */
