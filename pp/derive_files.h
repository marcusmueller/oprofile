/**
 * @file derive_files.h
 * Command-line helper
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author Philippe Elie
 * @author John Levon
 */

#ifndef DERIVE_FILES_H
#define DERIVE_FILES_H

#include <string>
#include <map>
#include <vector>

/**
 * container type used to store alternative location of binary image. We need a
 * multimap to warn against ambiguity between mutiple time found image name.
 * \sa add_to_alternate_filename(), check_image_name()
 */
typedef std::multimap<std::string, std::string> alt_filename_t;

/**
 * @param alternate_filename a container where all filename belonging to the
 * following path are stored
 * @param path_names a vector of path to consider
 *
 * add all file name below path_name recursively, to the the set of
 * alternative filename used to retrieve image name when a samples image name
 * directory is not accurate
 */
void add_to_alternate_filename(alt_filename_t & alternate_filename,
			       std::vector<std::string> const & path_names);

/**
 * @param alternate_filename container where all candidate filename are stored
 * @param image_name binary image name
 * @param samples_filename samples filename
 *
 * check than image_name belonging to samples_filename exist. If not it try to
 * retrieve it through the alternate_filename location. If we fail to retrieve
 * the file or if it is not readable we provide a warning and return an empty
 * string
 */
std::string check_image_name(alt_filename_t const & alternate_filename,
			     std::string const & image_name,
			     std::string const & samples_filename);

/**
 * derive an image filename and a sample file name from the given argument
 * @param argument in optional argument, either a samples filename or a binary
 *  filename
 * @param image_file in/out optional argument: binary image filename
 * @param sample_file in/out optional argument: sample file name
 * @param counter_mask in/out: counter_mask
 *
 * derive image_file and sample_file from the given arguments. Only one sample
 * filename and image filename can be provided.
 * if counter == -1 counter is derived from the #counter_nr prefix of sample
 * filename else if counter == 0 counter is set to the counter nr 0.
 *
 * All error are fatal:  specifying twice time sample filename or
 * image filename, specifying nor image filename nor sample filename.
 *
 */
void derive_files(std::string const & argument,
	std::string & image_file, std::string & sample_file,
	int & counter_mask);

#endif /* ! DERIVE_FILES_H */
