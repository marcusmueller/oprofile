/**
 * @file verbose_ostream.h
 * An ostream that outputs nothing when disabled
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author John Levon <moz@compsoc.man.ac.uk>
 */

#ifndef VERBOSE_OSTREAM_H
#define VERBOSE_OSTREAM_H

#include <iostream>
#include <fstream>
 
// FIXME, better way
 
class verbose_ostream : public std::ostream {
public:
	verbose_ostream(std::ostream const & o)
		: std::ostream(o.rdbuf()) {}

	/// set quiet mode
	void go_silent() {
		std::ofstream f("/dev/null");
		rdbuf(f.rdbuf());
		clear(badbit);
	}
};

extern verbose_ostream cverb;
 
#endif // VERBOSE_OSTREAM_H
