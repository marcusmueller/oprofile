/**
 * @file filename_spec.cpp
 * Container holding a sample filename split into its components
 *
 * @remark Copyright 2003 OProfile authors
 * @remark Read the file COPYING
 *
 * @author Philippe Elie
 */

#include <string>

#include "filename_spec.h"
#include "split_sample_filename.h"
#include "generic_spec.h"


using namespace std;


filename_spec::filename_spec(string const & filename)
{
	set_sample_filename(filename);
}


filename_spec::filename_spec()
	: image("*"), lib_image("*")
{
}


bool filename_spec::match(filename_spec const & rhs,
                          string const & binary) const
{
	if (!tid.match(rhs.tid) || !cpu.match(rhs.cpu) ||
	    !tgid.match(rhs.tgid) || count != rhs.count ||
	    unitmask != rhs.unitmask || event != rhs.event) {
		return false;
	}

	if (binary.empty()) {
		return image == rhs.image && lib_image == rhs.lib_image;
	}

	// PP:3.3 if binary is not empty we must match either the
	// lib_name if present or the image name
	if (!rhs.lib_image.empty()) {
		// FIXME: use fnmatch ?
		return rhs.lib_image == binary;
	}

	// FIXME: use fnmatch ?
	return rhs.image == binary;
}


void filename_spec::set_sample_filename(string const & filename)
{
	split_sample_filename split = split_sample_file(filename);

	image = split.image;
	lib_image = split.lib_image;
	event = split.event;
	count = lexical_cast_no_ws<int>(split.count);
	unitmask = lexical_cast_no_ws<unsigned int>(split.unitmask);
	tgid.set(split.tgid);
	tid.set(split.tid);
	cpu.set(split.cpu);
}
