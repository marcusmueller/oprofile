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

std::ostream& operator<<(std::ostream& out, const event_setting& object);
std::istream& operator>>(std::istream& in, event_setting& object);

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
	uint hash_table_size;
	uint note_table_size;
	std::string kernel_filename;
	std::string map_filename;
	int kernel_only;
	int ignore_daemon_samples;
	int verbose;
	// as string to allow symbolic group name ?
	int pgrp_filter;
	// not persistent, no interest to save from one session to another
	int pid_filter;
	int separate_samples;
};

std::ostream& operator<<(std::ostream& out, const config_setting& object);
std::istream& operator>>(std::istream& in, config_setting& object);

#endif // ! OPROF_START_CONFIG_H
