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

#include <string>

// FIXME: doc 
void derive_files(std::string const & argument,
	std::string & image_file, std::string & sample_file,
	int & counter);
