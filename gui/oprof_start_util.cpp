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

#include "oprof_start_util.h"

#include <sys/types.h>
#include <sys/stat.h> 
#include <unistd.h> 
 
#include <vector> 
#include <cmath>
#include <sstream>
#include <iostream>
#include <fstream>

#include <qfiledialog.h>
#include <qmessagebox.h>

// return the ~ expansion suffixed with a '/'
static std::string const get_user_dir()
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
std::string const get_user_filename(std::string const & filename)
{
	return get_user_dir() + "/" + filename;
}

// FIXME: let's use a proper fork/exec with pipes
// too tricky perhaps: exec a command and redirect stdout / stderr to the
// corresponding ostream.
int exec_command(std::string const & cmd_line, std::ostream& out, 
		 std::ostream& err)
{
	char name_stdout[L_tmpnam];
	char name_stderr[L_tmpnam];

	// FIXME: using tmpnam is not recommanded...
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
int exec_command(std::string const & cmd_line, std::ostream& out)
{
	return exec_command(cmd_line, out, out);
}
#endif

 
bool check_and_create_config_dir()
{
	// create the directory if necessary.
	std::string dir = get_user_filename(".oprofile");

	if (access(dir.c_str(), F_OK)) {
		if (mkdir(dir.c_str(), 0700)) {
			std::ostringstream out;
			out << "unable to create " << dir << " directory: ";
			QMessageBox::warning(0, 0, out.str().c_str());

			return false;
		}
	}
	return true;
}

 
std::string const format(std::string const & orig, uint const maxlen)
{
	string text(orig);

	std::istringstream ss(text);
	std::vector<std::string> lines;

	std::string oline;
	std::string line;

	while (getline(ss, oline)) {
		if (line.size() + oline.size() < maxlen) {
			lines.push_back(line + oline);
			line.erase();
		} else {
			lines.push_back(line);
			line.erase();
			std::string s;
			std::string word;
			std::istringstream oss(oline);
			while (oss >> word) {
				if (line.size() + word.size() > maxlen) {
					lines.push_back(line);
					line.erase();
				}
				line += word + " ";
			}
		}
	}

	if (line.size())
		lines.push_back(line);

	std::string ret;

	for(std::vector<std::string>::const_iterator it = lines.begin(); it != lines.end(); ++it)
		ret += *it + "\n";

	return ret;
}


int do_exec_command(std::string const & cmd)
{
	std::ostringstream out;
	std::ostringstream err;

	int ret = exec_command(cmd, out, err);

	// FIXME: err is empty e.g. if you are not root !!
 
	if (ret) {
		std::string error = "Failed: with error \"" + err.str() + "\"\n";
		error += "Command was :\n\n" + cmd + "\n";

		QMessageBox::warning(0, 0, format(error, 50).c_str());
	}

	return ret;
}

 
std::string const do_open_file_or_dir(std::string const & base_dir, bool dir_only)
{
	QString result;

	if (dir_only) {
		result = QFileDialog::getExistingDirectory(base_dir.c_str(), 0,
			   "open_file_or_dir", "Get directory name", true);
	} else {
		result = QFileDialog::getOpenFileName(base_dir.c_str(), 0, 0,
			   "open_file_or_dir", "Get filename");
	}

	return result.latin1();
}
 

// like posix shell utils basename, do not append trailing '/' to result.
std::string const basename(std::string const & path_name)
{
	std::string result = path_name;

	// remove all trailing '/'
	size_t last_delimiter = result.find_last_of('/');
	if (last_delimiter != std::string::npos) {
		while (last_delimiter && result[last_delimiter] == '/')
			--last_delimiter;

		result.erase(last_delimiter);
	}

	last_delimiter = result.find_last_of('/');
	if (last_delimiter != std::string::npos)
		result.erase(last_delimiter);

	return result;
}
