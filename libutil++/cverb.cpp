/**
 * @file cverb.cpp
 * verbose output stream
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author Philippe Elie
 * @author John Levon
 */

#include <fstream>
#include <iostream>
#include "cverb.h"

using namespace std;

// The right way is to use: ofstream fout; but cverb(fout.rdbuf()) receive
// a null pointer and stl shipped with 2.91 segfault.
ofstream fout("/dev/null");
ostream cverb(fout.rdbuf());

void set_verbose(bool verbose)
{
	// Note: should really be std::ios_base::badbit
	// but for now the old version will do
 
	if (verbose)
		cverb.rdbuf(cout.rdbuf());
	else
		cverb.clear(ios::badbit);
}
