/* COPYRIGHT (C) 2001 by ?
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
 *
 * first written by John Levon and P. Elie
 */

#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>	// for FILENAME_MAX
#include <dirent.h>

#include <vector>
#include <string>

#include "file_manip.h"

using std::vector;
using std::string;
using std::list;

/**
 * is_file_identical - check for identical files
 * @file1: first filename
 * @file2: scond filename
 *
 * return true if the two filenames belong to the same file
 */
bool is_files_identical(string const & file1, string const & file2)
{
	struct stat st1;
	struct stat st2;

	if (stat(file1.c_str(), &st1) == 0 && stat(file2.c_str(), &st2) == 0) {
		if (st1.st_dev == st2.st_dev && st1.st_ino == st2.st_ino)
			return true;
	}

	return false;
}

/**
 * create_dir - create a directory
 * @dir: the directory name to create
 *
 * return false if the directory @dir does not exist
 * and cannot be created
 */
bool create_dir(string const & dir)
{
	if (access(dir.c_str(), F_OK)) {
		if (mkdir(dir.c_str(), 0700))
			return false;
	}
	return true;
}

/**
 * create_path - create a path
 * @dir: the path to create
 *
 * create directory for each dir components in @path
 * return false if one of the path cannot be created.
 */
bool create_path(string const & path)
{
	vector<string> path_component;

	size_t slash = 0;
	while (slash < path.length()) {
		size_t new_pos = path.find_first_of('/', slash);
		if (new_pos == string::npos)
			new_pos = path.length();

		path_component.push_back(path.substr(slash, (new_pos - slash) + 1));
		slash = new_pos + 1;
	}

	string dir_name;
	for (size_t i = 0 ; i < path_component.size() ; ++i) {
		dir_name += '/' + path_component[i];
		if (!create_dir(dir_name))
			return false;
	}
	return true;
}

/**
 * opd_read_link - read the contents of a symbolic link file
 * @name: the file name
 *
 * return an empty string on failure
 */
string opd_read_link(string const & name)
{
	char linkbuf[FILENAME_MAX];
	int c;

	c = readlink(name.c_str(), linkbuf, FILENAME_MAX);
 
	if (c == -1)
		return string();
 
	if (c == FILENAME_MAX)
		linkbuf[FILENAME_MAX-1] = '\0';
	else
		linkbuf[c] = '\0';
	return linkbuf;
}

inline static bool is_directory_name(const char * name)
{
	return name[0] == '.' && 
		(name[1] == '\0' || 
		 (name[1] == '.' && name[2] == '\0'));
}

/// return false if base_dir can't be accessed.
bool create_file_list(list<string>& file_list, const string & base_dir)
{
	DIR *dir;
	struct dirent *dirent;

	if (!(dir = opendir(base_dir.c_str())))
		return false;

	while ((dirent = readdir(dir)) != 0) {
		if (!is_directory_name(dirent->d_name))
			file_list.push_back(dirent->d_name);
	}

	closedir(dir);

	return true;
}
