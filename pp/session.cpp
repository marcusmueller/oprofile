/**
 * @file session.cpp
 * handle --session option
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author John Levon
 * @author Philippe Elie
 */

#include "op_config.h"
#include "popt_options.h"

#include "session.h"

using namespace std;

namespace {
	string session;
	popt::option opt_session(session, "session", '\0', "session to use", "name");
};

string handle_session_options(void)
{
	if (session.empty()) {
		return OP_SAMPLES_DIR;
	}

	if (session[0] == '/') {
		return session;
	}

	return OP_SAMPLES_DIR + session;
}
