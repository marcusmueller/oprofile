/**
 * @file cverb.h
 * verbose output stream
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author Philippe Elie <phil_el@wanadoo.fr>
 * @author John Levon <moz@compsoc.man.ac.uk>
 */

#ifndef CVERB_H
#define CVERB_H

#include <iostream>

/** verbose outpust stream, all output through this stream are made only
 * if a set_verbose(true); call is issued.
 */
extern std::ostream cverb;

/** 
 * @param verbose: verbose state
 * 
 *  Set the cverb ostream in a verbose/non verbose mode depending on the
 * verbose parameter. Currently set_verbose() can be called only one time. If
 * this function is never called the default state of cverb is non-verbose mode
 */
void set_verbose(bool verbose);

#endif /* !CVERB_H */
