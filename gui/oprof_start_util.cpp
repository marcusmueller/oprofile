/**
 * @file oprof_start_util.cpp
 * Miscellaneous helpers for the GUI start
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author Philippe Elie
 * @author John Levon
 */

#include <dirent.h>
#include <unistd.h>

#include <cerrno>
#include <vector>
#include <cmath>
#include <sstream>
#include <iostream>
#include <fstream>

#include <qfiledialog.h>
#include <qmessagebox.h>

#include "op_file.h"
#include "string_manip.h"
#include "file_manip.h"
#include "child_reader.h"

#include "oprof_start_util.h"

using std::max;
using std::string;
using std::cout;
using std::cerr;
using std::endl;
using std::vector;
using std::ostream;

namespace {

// return the ~ expansion suffixed with a '/'
string const get_user_dir()
{
	static string user_dir;

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

string daemon_pid;

} // namespace anon

daemon_status::daemon_status()
	: running(false)
{
	int HZ;
	if (!daemon_pid.empty()) {
		string exec = op_read_link(string("/proc/") + daemon_pid + "/exe");
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
			exit(EXIT_FAILURE);
		}

		while ((dirent = readdir(dir))) {
			string const exec = op_read_link(string("/proc/") + dirent->d_name + "/exe");
			string const name = basename(exec);
			if (name != "oprofiled")
				continue;

			daemon_pid = dirent->d_name;
			running = true;
		}

		closedir(dir);
	}

	HZ = sysconf(_SC_CLK_TCK);
	if (HZ == -1) {
		perror("oprofiled: Unable to determine clock ticks per second. ");
		exit(EXIT_FAILURE);
	}

	runtime.erase();
	if (daemon_pid.empty())
		return;

	std::ifstream ifs((string("/proc/") + daemon_pid + "/stat").c_str());
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
 * get_cpu_speed - return CPU speed in MHz
 *
 */
unsigned long get_cpu_speed()
{
	unsigned long speed = 0;

	std::ifstream ifs("/proc/cpuinfo");
	if (!ifs)
		return speed;

	string str;

	while (getline(ifs, str)) {
		if (str.size() < 7)
			continue;

		if (str.substr(0, 7) == "cpu MHz") {
			string::const_iterator it = str.begin();
			uint i = 0;
			while (it != str.end() && !(isdigit(*it)))
				++i, ++it;
			if (it == str.end())
				break;
			std::istringstream ss(str.substr(i, string::npos));
			ss >> speed;
			break;
		}
	}
	return speed;
}


/**
 * get_user_filename - get absoluate filename of file in user $HOME
 * @param filename  the relative filename
 *
 * Get the absolute path of a file in a user's home directory.
 */
string const get_user_filename(string const & filename)
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
	string dir = get_user_filename(".oprofile");

	if (!create_dir(dir)) {
		std::ostringstream out;
		out << "unable to create " << dir << " directory: ";
		QMessageBox::warning(0, 0, out.str().c_str());

		return false;
	}
	return true;
}


/**
 * format - re-format a string
 * @param orig  string to format
 * @param maxlen  width of line
 *
 * Re-formats a string to fit into a certain width,
 * breaking lines at spaces between words.
 *
 * Returns the formatted string
 */
string const format(string const & orig, uint const maxlen)
{
	string text(orig);

	std::istringstream ss(text);
	vector<string> lines;

	string oline;
	string line;

	while (getline(ss, oline)) {
		if (line.size() + oline.size() < maxlen) {
			lines.push_back(line + oline);
			line.erase();
		} else {
			lines.push_back(line);
			line.erase();
			string s;
			string word;
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

	string ret;

	for(vector<string>::const_iterator it = lines.begin(); it != lines.end(); ++it)
		ret += *it + "\n";

	return ret;
}


/**
 * do_exec_command - execute a command
 * @param cmd  command name
 * @param args  arguments to command
 *
 * Execute a command synchronously. An error message is shown
 * if the command returns a non-zero status, which is also returned.
 *
 * The arguments are verified and will refuse to execute if they contain
 * shell metacharacters.
 */
int do_exec_command(string const & cmd, vector<string> const & args)
{
	std::ostringstream err;
	bool ok = true;

	// verify arguments
	for (vector<string>::const_iterator cit = args.begin();
		cit != args.end(); ++cit) {
		if (verify_argument(*cit))
			continue;

		QMessageBox::warning(0, 0,
			string(
			"Could not execute: Argument \"" + *cit +
			"\" contains shell metacharacters.\n").c_str());
		return EINVAL;
	}

	child_reader reader(cmd, args);
	if (reader.error())
		ok = false;

	if (ok)
		reader.get_data(cout, err);

	int ret = reader.terminate_process();
	if (ret) {
		string error = "Failed: \n" + err.str() + "\n";
		string cmdline = cmd;
		for (vector<string>::const_iterator cit = args.begin();
			cit != args.end(); ++cit) {
			cmdline += " " + *cit + " ";
		}
		error += "\n\nCommand was :\n\n" + cmdline + "\n";

		QMessageBox::warning(0, 0, format(error, 50).c_str());
	}

	return ret;
}


/**
 * do_open_file_or_dir - open file/directory
 * @param base_dir  directory to start at
 * @param dir_only  directory or filename to select
 *
 * Select a file or directory. The selection is returned;
 * an empty string if the selection was cancelled.
 */
string const do_open_file_or_dir(string const & base_dir, bool dir_only)
{
	QString result;

	if (dir_only) {
		result = QFileDialog::getExistingDirectory(base_dir.c_str(), 0,
			"open_file_or_dir", "Get directory name", true);
	} else {
		result = QFileDialog::getOpenFileName(base_dir.c_str(), 0, 0,
			"open_file_or_dir", "Get filename");
	}

	if (result.isNull())
		return string();
	else
		return result.latin1();
}

/**
 * verify_argument - check string for potentially dangerous characters
 *
 * This function returns false if the string contains dangerous shell
 * metacharacters.
 *
 * WWW Security FAQ dangerous chars:
 *
 * & ; ` ' \ " | * ? ~ < > ^ ( ) [ ] { } $ \n \r
 *
 * David Wheeler: ! #
 *
 * We allow '-' because we disallow whitespace. We allow ':' and '='
 */
bool verify_argument(string const & str)
{
	if (str.find_first_not_of(
		"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
		"abcdefghijklmnopqrstuvwxyz0123456789_:=-+%,./")
		!= string::npos)
		return false;
	return true;
}
