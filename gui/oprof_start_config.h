/* COPYRIGHT (C) 2001 Philippe Elie
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59 Temple
 * Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef OPROF_START_CONFIG_H
#define OPROF_START_CONFIG_H

#include <linux/version.h>

#include <string>
#include <iostream>

#include "persistent_config.h"
#include "../op_user.h"

// FIXME: must be shared.
#define OP_MAX_PERF_COUNT	2147483647UL

#define OP_DEFAULT_BUFFER_SIZE	262144
#define OP_MAX_BUFFER_SIZE	1048576
#define OP_MIN_BUFFER_SIZE	1024

#define OP_DEFAULT_HASH_SIZE	65536
#define OP_MAX_HASH_TABLE_SIZE	262144
#define OP_MIN_HASH_TABLE_SIZE	256

// This is a standard non-portable assumption we make. 
#define OP_MIN_PID		0
#define OP_MAX_PID		32767
#define OP_MIN_PGRP		0
#define OP_MAX_PGRP		32767

#define BUILD_DIR		"/lib/modules/" UTS_RELEASE "/build/"

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

std::ostream& operator<<(std::ostream& out, const event_setting& object);
std::istream& operator>>(std::istream& in, event_setting& object);

// Store the general  configuration of the profiler. File/path name buffer
// size ETC.
struct config_setting {
	config_setting();

	void load(std::istream& in);
	void save(std::ostream& out) const;


	uint buffer_size;
	uint hash_table_size;
	std::string base_opd_dir;
	std::string samples_files_dir;
	std::string device_file;
	std::string hash_map_device;
	std::string daemon_log_file;
	std::string kernel_filename;
	std::string map_filename;
	int kernel_only;
	int ignore_daemon_samples;
	int verbose;
	// as string to allow symbolic group name ?
	int pgrp_filter;
	// not persistent, no interest to save from one session to another?
	int pid_filter;
};

std::ostream& operator<<(std::ostream& out, const config_setting& object);
std::istream& operator>>(std::istream& in, config_setting& object);

#endif // ! OPROF_START_CONFIG_H
