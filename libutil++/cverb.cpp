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

std::ofstream fout("/dev/null");
std::ostream cverb(fout.rdbuf());

void set_verbose(bool verbose)
{
	// Note: should really be std::ios_base::badbit
	// but for now the old version will do
 
	if (verbose)
		cverb.rdbuf(std::cout.rdbuf());
	else
		cverb.clear(std::ios::badbit);
}
