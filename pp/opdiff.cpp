/**
 * @file opdiff.cpp
 * Implement opdiff utility
 *
 * @remark Copyright 2003 OProfile authors
 * @remark Read the file COPYING
 *
 * @author John Levon
 * @author Philippe Elie
 */

#include <iostream>

#include "opdiff_options.h"

using namespace std;


int opdiff(vector<string> const & non_options)
{
	handle_options(non_options);

	cerr << "N/A\n";
	return 0;
}


int main(int argc, char const * argv[])
{
	run_pp_tool(argc, argv, opdiff);
}
