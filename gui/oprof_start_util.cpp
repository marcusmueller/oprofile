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
#include <sys/wait.h> 
#include <dirent.h> 
#include <unistd.h> 
#include <fcntl.h> 
 
#include <vector> 
#include <cmath>
#include <sstream>
#include <iostream>
#include <fstream>

#include <qfiledialog.h>
#include <qmessagebox.h>

typedef int fd_t;
 
namespace {
 
// return the ~ expansion suffixed with a '/'
std::string const get_user_dir()
{
	static std::string user_dir;

	if (user_dir.empty()) {
		char * dir = getenv("HOME");
		if (!dir) {
			cerr << "Can't determine home directory !\n" << endl;
			exit(EXIT_FAILURE);
		}

		user_dir = dir;

		if (user_dir.length() && user_dir[user_dir.length() -1] != '/')
			user_dir += '/';
	}

	return user_dir;
}

/**
 * fill_from_fd - output fd to a stream
 * @stream: the stream to output to
 * @fd: the fd to read from
 *
 * Read from @fd until a read would block and write
 * the output to @stream.
 */
static void fill_from_fd(std::ostream & stream, fd_t fd)
{
	// FIXME: there might be a better way than this; suggestions welcome 
	fd_t fd_out; 
	char templ[] = "/tmp/op_XXXXXX";
	char buf[4096]; 
	
	if ((fd_out = mkstemp(templ) == -1)) {
		stream.set(ios::failbit); 
		return;
	}

	ssize_t count;
	fcntl(fd, F_SETFL, O_NONBLOCK);
	while ((count = read(fd, buf, 4096)) != -1)
		write(fd_out, buf, count);

	close(fd_out);

	std::ifstream in(templ);
	std::string str;

	while (getline(in, str))
		stream << str;
 
	unlink(templ);
}

 
static int exec_command(std::string const & cmd, std::ostream & out, 
		 std::ostream & err, std::vector<std::string> args)
{
	int pstdout[2];
	int pstderr[2];

	if (pipe(pstdout) == -1 || pipe(pstderr) == -1) {
		err << "Couldn't create pipes !" << std::endl;
		return -1;
	}
 
	pid_t pid = fork();
	switch (pid) {
		case -1:
			err << "Couldn't fork !" << std::endl;
			return -1;
		 
		case 0: {
			char const * argv[args.size() + 2];
			uint i = 0;
			argv[i++] = cmd.c_str();
			for (std::vector<std::string>::const_iterator cit = args.begin();
				cit != args.end(); ++cit) {
				argv[i++] = cit->c_str();
			}
			argv[i] = 0;
		 
			// child
			dup2(pstdout[1], STDOUT_FILENO);
			dup2(pstderr[1], STDERR_FILENO);
 
			execvp(cmd.c_str(), (char * const *)argv);
 
			err << "Couldn't exec !" << std::endl;
			return -1;
		}

		default:;
	}

	// parent
 
	int ret;
	waitpid(pid, &ret, 0);
 
	fill_from_fd(out, pstdout[0]);
	fill_from_fd(err, pstderr[0]);
 
	return WEXITSTATUS(ret);
}

std::string const opd_read_link(std::string const & name)
{
	static char linkbuf[FILENAME_MAX]="";
	int c;
 
	c = readlink(name.c_str(), linkbuf, FILENAME_MAX);
 
	if (c == -1)
		return std::string();
 
	if (c == FILENAME_MAX)
		linkbuf[FILENAME_MAX-1] = '\0';
	else
		linkbuf[c] = '\0';
	return linkbuf;
}

std::string daemon_pid;

// hurrah ! Stupid interface
const int HZ = 100;
 
} // namespace anon 
 
daemon_status::daemon_status()
	: running(false)
{
	if (!daemon_pid.empty()) { 
		std::string exec = opd_read_link(std::string("/proc/") + daemon_pid + "/exe");
		if (exec.empty())
			daemon_pid.erase();
		else
			running = true;
	}
 
	if (daemon_pid.empty()) {
		DIR * dir;
		struct dirent * dirent;

		if (!(dir = opendir("/proc"))) {
			perror("oprofiled: /proc directory could not be opened. ");
			exit(1);
		}

		while ((dirent = readdir(dir))) {
			std::string const exec = opd_read_link(std::string("/proc/") + dirent->d_name + "/exe");
			std::string const name = basename(exec); 
			if (name != "oprofiled")
				continue;
 
			daemon_pid = dirent->d_name;
			running = true;
		}
	}
 
	runtime.erase();
	if (daemon_pid.empty())
		return;
 
	std::ifstream ifs((std::string("/proc/") + daemon_pid + "/stat").c_str());
	if (!ifs)
		return;

	long dummy;

	ifs >> dummy; // pid
	while (!isdigit(ifs.get()))
		;
	ifs.ignore(); // state
	for (uint i = 0; i < 17; ++i)
		ifs >> dummy;
 
	ulong starttime;

	ifs >> starttime;

	std::ifstream ifs2("/proc/uptime");
	if (!ifs2)
		return;
 
	double uptimef;
	ifs2 >> uptimef;
	int uptime = int(uptimef);
	 
	uint diff_mins = (uptime - starttime / HZ) / 60;
 
	std::ifstream ifs3("/proc/sys/dev/oprofile/nr_interrupts");
	if (!ifs3)
		return;
 
	ifs3 >> nr_interrupts;

	runtime = tostr(diff_mins / 60) + " hours, " +
		tostr(diff_mins % 60) + " mins";
}


/**
 * get_user_filename - get absoluate filename of file in user $HOME
 * @filename: the relative filename
 *
 * Get the absolute path of a file in a user's home directory.
 */
std::string const get_user_filename(std::string const & filename)
{
	return get_user_dir() + "/" + filename;
}

 
/**
 * check_and_create_config_dir - make sure config dir is accessible
 *
 * Returns %true if the dir is accessible.
 */
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

 
/**
 * format - re-format a string 
 * @orig: string to format
 * @maxlen: width of line
 *
 * Re-formats a string to fit into a certain width,
 * breaking lines at spaces between words.
 *
 * Returns the formatted string
 */
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


/**
 * do_exec_command - execute a command
 * @cmd: command name
 * @args: arguments to command
 *
 * Execute a command synchronously. An error message is shown
 * if the command returns a non-zero status, which is also returned.
 */
int do_exec_command(std::string const & cmd, std::vector<std::string> args)
{
	std::ostringstream out;
	std::ostringstream err;

	int ret = exec_command(cmd, out, err, args);

	if (ret) {
		std::string error = "Failed: \n" + err.str() + "\n";
		std::string cmdline = cmd;
		for (std::vector<std::string>::const_iterator cit = args.begin();
			cit != args.end(); ++cit)
			cmdline += " " + *cit + " ";
		error += "\n\nCommand was :\n\n" + cmdline + "\n";

		QMessageBox::warning(0, 0, format(error, 50).c_str());
	}

	return ret;
}

 
/**
 * do_open_file_or_dir - open file/directory
 * @base_dir: directory to start at
 * @dir_only: directory or filename to select
 *
 * Select a file or directory. The selection is returned;
 * an empty string if the selection was cancelled.
 */
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
 

/**
 * basename - get the basename of a path
 * @path_name: path
 *
 * Returns the basename of a path with trailing '/' removed.
 */
std::string const basename(std::string const & path_name)
{
	std::string result = path_name;

	while (result[result.size() - 1] == '/')
		result = result.substr(0, result.size() - 1);

	std::string::size_type slash = result.find_last_of('/');
	if (slash != std::string::npos)
		result.erase(0, slash + 1);

	return result;
}

 
/**
 * tostr - convert integer to str
 * i: the integer
 *
 * Returns the converted string
 */ 
std::string const tostr(unsigned int i)
{
        string str;
	std::ostringstream ss(str);
	ss << i;
	return ss.str(); 
}
