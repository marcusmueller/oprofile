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

#include <stdio.h>

#include <sstream>
#include <fstream>

#include "oprof_start_config.h"
#include "oprof_start_util.h"

namespace {
 
// output default_value if value is empty (empty <==> contains non blank char)
static void save_value(std::ostream& out, const std::string& value, 
		const std::string& default_value)
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

void event_setting::save(std::ostream& out) const
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
	buffer_size(OP_DEFAULT_BUFFER_SIZE),
	hash_table_size(OP_DEFAULT_HASH_SIZE),
	note_table_size(OP_DEFAULT_NOTE_SIZE),
	// FIXME: member of config, hardcoded value probably come from ? 
	base_opd_dir("/var/opd/"),
	samples_files_dir("samples"),
	device_file("opdev"),
	note_device_file("opnotedev"),
	hash_map_device("ophashmapdev"),
	daemon_log_file("oprofiled.log"),
	kernel_filename(BUILD_DIR "vmlinux"),
	map_filename(BUILD_DIR "System.map"),
	kernel_only(0),
	ignore_daemon_samples(0),
	verbose(0),
	pgrp_filter(0),
	pid_filter(0),
	separate_samples(0)
{
}

// sanitize needed ?
void config_setting::load(std::istream& in)
{
	in >> buffer_size;
	in >> hash_table_size;
	in >> base_opd_dir;
	in >> samples_files_dir;
	in >> device_file;
	in >> note_device_file;
	in >> hash_map_device;
	in >> daemon_log_file;
	in >> kernel_filename;
	in >> map_filename;
	in >> kernel_only;
	in >> ignore_daemon_samples;
	in >> verbose;
	in >> pgrp_filter;
	in >> note_table_size;
	in >> separate_samples;
}

// sanitize needed ?
void config_setting::save(std::ostream& out) const
{
	out << buffer_size << std::endl;
	out << hash_table_size << std::endl;

	// for these we need always to put something sensible, else if we save
	// empty string reload is confused by this empty string.
	config_setting def_val;

	save_value(out, base_opd_dir, def_val.base_opd_dir);
	out << std::endl;
	save_value(out, samples_files_dir, def_val.samples_files_dir);
	out << std::endl;
	save_value(out, device_file, def_val.device_file);
	out << std::endl;
	save_value(out, note_device_file, def_val.note_device_file);
	out << std::endl;
	save_value(out, hash_map_device, def_val.hash_map_device);
	out << std::endl;
	save_value(out, daemon_log_file, def_val.daemon_log_file);
	out << std::endl;
	save_value(out, kernel_filename, def_val.kernel_filename);
	out << std::endl;
	save_value(out, map_filename, def_val.map_filename);
	out << std::endl;

	out << kernel_only << std::endl;
	out << ignore_daemon_samples << std::endl;
	out << verbose << std::endl;
	out << pgrp_filter << std::endl;
	out << note_table_size << std::endl;
	out << separate_samples << std::endl;
}

std::ostream& operator<<(std::ostream& out, const config_setting& object)
{
	object.save(out);
	return out;
}

std::istream& operator>>(std::istream& in, config_setting& object)
{
	object.load(in);
	return in;
}
