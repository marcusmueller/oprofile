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

/// Store the setup of one event
struct event_setting {

	event_setting();

	uint count;
	uint umask;
	bool os_ring_count;
	bool user_ring_count;
};

/**
 * Store the general  configuration of the profiler.
 * There is no save(), instead opcontrol --setup must be
 * called. This uses opcontrol's daemonrc file.
 */
struct config_setting {
	config_setting();

	void load(std::istream & in);

	uint buffer_size;
	uint note_table_size;
	std::string kernel_filename;
	bool no_kernel;
	bool kernel_only;
	bool verbose;
	pid_t pgrp_filter;
	pid_t pid_filter;
	bool separate_lib;
	bool separate_kernel;
};

std::istream & operator>>(std::istream & in, config_setting & object);

#endif // ! OPROF_START_CONFIG_H
