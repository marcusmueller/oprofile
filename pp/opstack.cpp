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

#include "string_filter.h"
#include "opstack_options.h"
#include "arrange_profiles.h"
#include "callgraph_container.h"
#include "image_errors.h"
#include "populate.h"

using namespace std;

namespace {

callgraph_container cg_container;

// Debug. Note : no \n at end of output
ostream & operator<<(ostream & out, symbol_entry const & symbol)
{
#if 0
	out << symbol_names.demangle(symbol.name)
	    << " " << hex << symbol.sample.vma
	    << " " << image_names.name(symbol.app_name)
	    << " " << image_names.name(symbol.image_name)
	    << dec << " (" << symbol.size << ")";
#else
	out << symbol_names.demangle(symbol.name);
#endif
	return out;
}

// just to get some sort of output. temporary before we write proper output
// support
void dump(ostream & out, callgraph_container const & container)
{
	vector<cg_symbol> arcs = container.get_arc();
	for (size_t i = 0; i < arcs.size(); ++i) {
		vector<cg_symbol> callee_arcs = container.get_callee(arcs[i]);
		for (size_t j = 0; j < callee_arcs.size(); ++j) {
			out << "\t" << callee_arcs[j]
			    << " " << callee_arcs[j].self_counts[0]
			    << "/" << callee_arcs[j].callee_counts[0]
			    << endl;
		}

		out << arcs[i]
		    << " " << arcs[i].self_counts[0]
		    << "/" << arcs[i].callee_counts[0]
		    << endl;

		vector<cg_symbol> caller_arcs = container.get_caller(arcs[i]);
		for (size_t j = 0; j < caller_arcs.size(); ++j) {
			out << "\t" << caller_arcs[j]
			    << " " << caller_arcs[j].self_counts[0]
			    << "/" << caller_arcs[j].callee_counts[0]
			    << endl;
		}

		out << "--------------------------------------------------\n";
	}
}


int opstack(vector<string> const & non_options)
{
	handle_options(non_options);

	list<inverted_profile> iprofiles
		= invert_profiles(classes, options::extra_found_images);

	report_image_errors(iprofiles);

	cg_container.populate(iprofiles);

	dump(cout, cg_container);

	return 0;
}

}  // anonymous namespace


int main(int argc, char const * argv[])
{
	run_pp_tool(argc, argv, opstack);
}
