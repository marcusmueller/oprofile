/**
 * @file derive_files.h
 * Command-line helper
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 * 
 * @author Philippe Elie <phil_el@wanadoo.fr>
 * @author John Levon <moz@compsoc.man.ac.uk>
 */

#include <string>
 
void derive_files(std::string const & argument,
	std::string & image_file, std::string & sample_file,
	int & counter);
