/**
 * @file cverb.h
 * verbose output stream
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author Philippe Elie
 * @author John Levon
 */

#include <fstream>
#include "cverb.h"

using namespace std;

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
