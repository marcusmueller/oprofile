/**
 * @file session.h
 * handle --session option
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author Philippe Elie
 * @author John Levon
 */

#ifndef SESSION_H
#define SESSION_H

#include <string>

/**
 * derive samples directory from the --session options
 *
 * return the samples directory derived from the session name or
 * OP_SAMPLES_DIR if no session name has been specified
 */
std::string handle_session_options();

#endif /* !SESSION_H */
