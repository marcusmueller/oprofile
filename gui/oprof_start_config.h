/**
 * @file oprof_start_config.h
 * GUI startup config management
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author John Levon
 * @author Philippe Elie
 */

#ifndef OPROF_START_CONFIG_H
#define OPROF_START_CONFIG_H

#include <sys/types.h>
#include <string>
#include <iostream>

#include "persistent_config.h"

// Store the setup of one events.
struct event_setting {

	event_setting();

	void save(std::ostream& out) const;
	void load(std::istream& in);

	uint count;
	uint umask;
	int os_ring_count;
	int user_ring_count;
};

std::ostream& operator<<(std::ostream& out, event_setting const & object);
std::istream& operator>>(std::istream& in, event_setting & object);

// Store the general  configuration of the profiler. File/path name buffer
// size ETC.
// You can add field here at any position but you must add them to
// load()/save() at the end of loading/saving to ensure compatibility with
// previous version of config file. If you remove field you must preserve
// dummy read/write in load()/save() for the same reason. Obviously you must
// also provide sensible value in the ctor.
struct config_setting {
	config_setting();

	void load(std::istream& in);
	void save(std::ostream& out) const;

	uint buffer_size;
	uint note_table_size;
	std::string kernel_filename;
	// FIXME: unsuported for 2.5
	int kernel_only;
	int ignore_daemon_samples;
	int verbose;
	pid_t pgrp_filter;
	// not persistent, no interest to save from one session to another
	pid_t pid_filter;
	int separate_samples;
};

std::ostream& operator<<(std::ostream& out, const config_setting& object);
std::istream& operator>>(std::istream& in, config_setting& object);

#endif // ! OPROF_START_CONFIG_H
