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
