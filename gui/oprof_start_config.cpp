/**
 * @file oprof_start_config.cpp
 * GUI startup config management
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author John Levon
 * @author Philippe Elie
 */

#include <stdio.h>

#include <sstream>
#include <fstream>
#include <iomanip>
#include <sys/utsname.h>

#include "oprof_start_config.h"
#include "oprof_start_util.h"
#include "op_config.h"
#include "op_config_24.h"

namespace {

// output default_value if value is empty (empty <==> contains non blank char)
static void save_value(std::ostream & out, std::string const & value,
		       std::string const & default_value)
{
	std::istringstream in(value);
	std::string word;
	in >> word;
	if (word.empty())
		out << default_value;
	else
		out << value;
}

} // namespace anon

event_setting::event_setting()
	:
	count(0),
	umask(0),
	os_ring_count(0),
	user_ring_count(0)
{
}

void event_setting::save(std::ostream & out) const
{
	out << count << " ";
	out << umask << " ";
	out << os_ring_count << " ";
	out << user_ring_count << " ";
}

void event_setting::load(std::istream& in)
{
	in >> count;
	in >> umask;
	in >> os_ring_count;
	in >> user_ring_count;
}

std::ostream& operator<<(std::ostream& out, const event_setting& object)
{
	object.save(out);

	return out;
}

std::istream& operator>>(std::istream& in, event_setting& object)
{
	object.load(in);

	return in;
}

config_setting::config_setting()
	:
	buffer_size(OP_DEFAULT_BUF_SIZE),
	hash_table_size(OP_DEFAULT_HASH_SIZE),
	note_table_size(OP_DEFAULT_NOTE_SIZE),
	kernel_only(0),
	ignore_daemon_samples(0),
	verbose(0),
	pgrp_filter(0),
	pid_filter(0),
	separate_samples(0)
{
	struct utsname info;

	/* Guess path to vmlinux based on kernel currently running. */
	if (uname(&info)) {
		perror("oprof_start: Unable to determine OS release.");
	} else {
		std::string const version(info.release);
		std::string const vmlinux_path("/lib/modules/" + version
					 + "/build/vmlinux");
		kernel_filename = vmlinux_path;
	}
}

// sanitize needed ?
void config_setting::load(std::istream& in)
{
	in >> buffer_size;
	in >> hash_table_size;
	in >> kernel_filename;
	std::string obsolete_map_filename;
	in >> obsolete_map_filename;
	in >> kernel_only;
	in >> ignore_daemon_samples;
	in >> verbose;
	in >> pgrp_filter;
	in >> note_table_size;
	in >> separate_samples;
	// the 3 following config item was kernel_range which are obsolete
	std::string garbage;
	in >> garbage;
	in >> garbage;
	in >> garbage;
	in >> std::dec;
}

// sanitize needed ?
void config_setting::save(std::ostream& out) const
{
	out << buffer_size << std::endl;
	out << hash_table_size << std::endl;

	// for these we need always to put something sensible, else if we save
	// empty string reload is confused by this empty string.
	config_setting def_val;

	save_value(out, kernel_filename, def_val.kernel_filename);
	out << std::endl;
	out << "map_filename_obsolete_placeholder" << std::endl;

	out << kernel_only << std::endl;
	out << ignore_daemon_samples << std::endl;
	out << verbose << std::endl;
	out << pgrp_filter << std::endl;
	out << note_table_size << std::endl;
	out << separate_samples << std::endl;

	// the 3 following config item was kernel_range which are obsolete
	out << "kernnel_range_auto_obsolete_placeholder" << std::endl;
	out << "kernnel_range_start_obsolete_placeholder" << std::endl;
	out << "kernnel_range_end_obsolete_placeholder" << std::endl;
}

std::ostream& operator<<(std::ostream& out, config_setting const & object)
{
	object.save(out);
	return out;
}

std::istream& operator>>(std::istream& in, config_setting& object)
{
	object.load(in);
	return in;
}
