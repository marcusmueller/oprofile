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

// FIXME:  how make gcc 2.91 accept correctly anonymous namespace
// namespace {

// TODO: many things here are mis-placed

// output default_value if value is empty (empty <==> contains non blank char)
void save_value(std::ostream& out, const std::string& value, 
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

// return the ~ expansion suffixed with a '/'
std::string get_user_dir()
{
	static std::string user_dir;

	if (user_dir.empty()) {
		std::ostringstream out;

		exec_command("echo -n ~", out);

		user_dir = out.str();

		if (user_dir.length() && user_dir[user_dir.length() -1] != '/')
			user_dir += '/';
	}

	return user_dir;
}

// return get_user_dir() + filename
std::string get_user_filename(const std::string& filename)
{
	return get_user_dir() + "/" + filename;
}

// too tricky perhaps: exec a command and redirect stdout / stdout to the
// corresponding ostream.
int exec_command(const std::string& cmd_line, std::ostream& out, 
		 std::ostream& err)
{
	char name_stdout[L_tmpnam];
	char name_stderr[L_tmpnam];

	// using tmpnam is not recommanded...
	tmpnam(name_stdout);
	tmpnam(name_stderr);

	std::string cmd = cmd_line ;
	cmd += std::string(" 2> ") + name_stderr;
	cmd += std::string(" > ")  + name_stdout;

	int ret = system(cmd.c_str());

	std::ifstream in_stdout(name_stdout);
	if (!in_stdout) {
		std::cerr << "fail to open stdout " << name_stdout << std::endl;
	}
	std::ifstream in_stderr(name_stderr);
	if (!in_stderr) {
		std::cerr << "fail to open stderr " << name_stderr << std::endl;
	}

	// this order is preferable in case we pass the same stream as out and
	// err, the stdout things come first in the output generally
	out << in_stdout.rdbuf();
	err << in_stderr.rdbuf();

	remove(name_stdout);
	remove(name_stderr);

	return ret;
}

#if 0
// FIXME: better but do not work, see bad comment.
int exec_command(const std::string& cmd_line, std::ostream& output)
{
	char name_output[L_tmpnam];

	tmpnam(name_output);

	std::string cmd = cmd_line ;
	// bad: return the exit code of cat so there is never error.
//	cmd += std::string(" 2>&1 | cat > ") + name_output;
	// bad: command receive the temp filename as command line option. Why?
//	cmd += std::string(" 2>&1 ") + name_output;

	int ret = system(cmd.c_str());

	std::ifstream in_output(name_output);
	if (!in_output) {
		std::cerr << "fail to open output " << name_output
			  << std::endl;
	}

	output << in_output.rdbuf();

	remove(name_output);

	return ret;
}
#else
// this work but assume than a command which make an error exit without
// any output to stdout after the first output to stderr...
int exec_command(const std::string& cmd_line, std::ostream& out)
{
	return exec_command(cmd_line, out, out);
}
#endif

bool is_profiler_started()
{
	return !system("ps aux | grep oprofiled | grep -v grep > /dev/null");
}

// } // anonymous namespace

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

	// TODO: member of config, hardcoded value probably come from ? 
	base_opd_dir("/var/opd/"),
	samples_files_dir("samples"),
	device_file("opdev"),
	hash_map_device("ophashmapdev"),
	daemon_log_file("oprofiled.log"),
	kernel_filename(BUILD_DIR "vmlinux"),
	map_filename(BUILD_DIR "System.map"),
	kernel_only(0),
	ignore_daemon_samples(0),
	pgrp_filter(0),
	pid_filter(0)
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
	in >> hash_map_device;
	in >> daemon_log_file;
	in >> kernel_filename;
	in >> map_filename;
	in >> kernel_only;
	in >> ignore_daemon_samples;
	in >> pgrp_filter;
}

// sanitize needed ?
void config_setting::save(std::ostream& out) const
{
	out << buffer_size << std::endl;
	out << hash_table_size << std::endl;

	// for this we need always to put something sensible, else if we save
	// empty string reload is confused by this empty string.
	config_setting def_val;

	save_value(out, base_opd_dir, def_val.base_opd_dir);
	out << std::endl;
	save_value(out, samples_files_dir, def_val.samples_files_dir);
	out << std::endl;
	save_value(out, device_file, def_val.device_file);
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
	out << pgrp_filter << std::endl;
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
