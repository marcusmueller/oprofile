/**
 * @file populate.cpp
 * Fill up a profile_container from inverted profiles
 *
 * @remark Copyright 2003 OProfile authors
 * @remark Read the file COPYING
 *
 * @author John Levon
 * @author Philippe Elie
 */

#include "profile.h"
#include "profile_container.h"
#include "arrange_profiles.h"
#include "op_bfd.h"
#include "op_header.h"

#include "image_errors.h"

using namespace std;

namespace options {
	extern string_filter symbol_filter;
};


namespace {

/// load merged files for one set of sample files
void
populate_from_files(profile_t & profile, list<string> const & files, u32 offset)
{
	list<string>::const_iterator it = files.begin();
	list<string>::const_iterator const end = files.end();

	for (; it != end; ++it)
		profile.add_sample_file(*it, offset);
}

}  // anon namespace


bool populate_for_image(profile_container & samples, inverted_profile & ip)
{
	bool ok = ip.error == image_ok;
	op_bfd abfd(ip.image, options::symbol_filter, ok);
	if (!ok && ip.error == image_ok)
		ip.error = image_format_failure;

	if (ip.error == image_format_failure)
		report_image_error(ip, false);

	u32 offset = abfd.get_start_offset();

	opd_header header;

	for (size_t i = 0; i < ip.groups.size(); ++i) {
		list<image_set>::const_iterator it
			= ip.groups[i].begin();
		list<image_set>::const_iterator const end
			= ip.groups[i].end();

		// we can only share a profile_t amongst each
		// image_set's files - this is because it->app_image
		// changes, and the .add() would mis-attribute
		// to the wrong app_image otherwise
		for (; it != end; ++it) {
			profile_t profile;
			populate_from_files(profile, it->files, offset);
			header = profile.get_header();
			samples.add(profile, abfd, it->app_image, i);
		}
	}

	if (ip.error == image_ok)
		check_mtime(abfd.get_filename(), header);

	return abfd.has_debug_info();
}
