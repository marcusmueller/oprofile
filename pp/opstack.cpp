/**
 * @file opstack.cpp
 * Implement callgraph utility
 *
 * @remark Copyright 2004 OProfile authors
 * @remark Read the file COPYING
 *
 * @author John Levon
 * @author Philippe Elie
 */

#include <iostream>

#include "format_output.h"
#include "string_filter.h"
#include "opstack_options.h"
#include "arrange_profiles.h"
#include "callgraph_container.h"
#include "image_errors.h"
#include "populate.h"

using namespace std;

namespace {

format_flags const get_format_flags(column_flags const & cf)
{
	format_flags flags(ff_none);
	flags = format_flags(flags | ff_nr_samples);
	flags = format_flags(flags | ff_percent | ff_symb_name);

	if (options::show_address)
		flags = format_flags(flags | ff_vma);

	if (options::debug_info)
		flags = format_flags(flags | ff_linenr_info);

	if (options::accumulated) {
		flags = format_flags(flags | ff_nr_samples_cumulated);
		flags = format_flags(flags | ff_percent_cumulated);
	}

	if (cf & cf_image_name)
		flags = format_flags(flags | ff_image_name);

	return flags;
}


int opstack(vector<string> const & non_options)
{
	handle_options(non_options);

	bool multiple_apps = false;

	for (size_t i = 0; i < classes.v.size(); ++i) {
		if (classes.v[i].profiles.size() > 1)
			multiple_apps = true;
	}

	list<inverted_profile> iprofiles
		= invert_profiles(classes, options::extra_found_images);

	report_image_errors(iprofiles);

	callgraph_container cg_container;
	cg_container.populate(iprofiles, options::extra_found_images,
		options::debug_info, options::threshold,
			      options::merge_by.lib);

	column_flags output_hints = cg_container.output_hint();

	format_output::cg_formatter out(cg_container);

	out.set_nr_classes(classes.v.size());
	out.show_header(options::show_header);
	out.vma_format_64bit(output_hints & cf_64bit_vma);
	out.show_long_filenames(options::long_filenames);
	format_flags flags = get_format_flags(output_hints);
	if (multiple_apps)
		flags = format_flags(flags | ff_app_name);
	out.add_format(flags);

	out.output(cout);

	return 0;
}

}  // anonymous namespace


int main(int argc, char const * argv[])
{
	run_pp_tool(argc, argv, opstack);
}
