/**
 * @file file_manip.cpp
 * Useful file management helpers
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author Philippe Elie
 * @author John Levon
 */

#include <unistd.h>
#include <sys/stat.h>
#include <dirent.h>
#include <fnmatch.h>

#include <cstdio>
#include <cerrno>
#include <iostream>
#include <vector>

#include "op_file.h"

#include "file_manip.h"
#include "string_manip.h"
#include "op_fileio.h"

using namespace std;

/**
 * is_file_identical - check for identical files
 * @param file1  first filename
 * @param file2  scond filename
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
 * op_read_link - read the contents of a symbolic link file
 * @param name  the file name
 *
 * return an empty string on failure
 */
string op_read_link(string const & name)
{
	char * linkbuff = op_get_link(name.c_str());
	if (linkbuff == NULL)
		return string();

	string result(linkbuff);
	free(linkbuff);
	return result;
}


bool op_file_readable(string const & file)
{
	return op_file_readable(file.c_str());
}

inline static bool is_directory_name(char const * name)
{
	return name[0] == '.' &&
		(name[1] == '\0' ||
		 (name[1] == '.' && name[2] == '\0'));
}


bool create_file_list(list<string> & file_list, string const & base_dir,
		      string const & filter, bool recursive)
{
	DIR *dir;
	struct dirent * ent;

	if (!(dir = opendir(base_dir.c_str())))
		return false;

	while ((ent = readdir(dir)) != 0) {
		if (!is_directory_name(ent->d_name) &&
		    fnmatch(filter.c_str(), ent->d_name, 0) != FNM_NOMATCH) {
			if (recursive) {
				struct stat stat_buffer;
				string name = base_dir + '/' + ent->d_name;
				if (stat(name.c_str(), &stat_buffer) == 0) {
					if (S_ISDIR(stat_buffer.st_mode) &&
					    !S_ISLNK(stat_buffer.st_mode)) {
						// recursive retrieve
						create_file_list(file_list,
								 name, filter,
								 recursive);
					} else {
						file_list.push_back(name);
					}
				}
			}
			else {
				file_list.push_back(ent->d_name);
			}
		}
	}

	closedir(dir);

	return true;
}


string relative_to_absolute_path(string const & path, string const & base_dir)
{
	char const * dir = 0;

	// don't screw up on already absolute paths
	if ((path.empty() || path[0] != '/') && !base_dir.empty())
		dir = base_dir.c_str();

	char * result = op_relative_to_absolute_path(path.c_str(), dir);

	string res(result);

	free(result);

	return res;
}


/**
 * @param path_name the path where we remove trailing '/'
 *
 * erase all trailing '/' in path_name except if the last '/' is at pos 0
 */
static string erase_trailing_path_separator(string const & path_name)
{
	string result(path_name);

	while (result.length() > 1) {
		if (result[result.length() - 1] != '/')
			break;
		result.erase(result.length() - 1, 1);
	}

	return result;
}


/**
 * dirname - get the path component of a filename
 * @param file_name  filename
 *
 * Returns the path name of a filename with trailing '/' removed.
 */
string dirname(string const & file_name)
{
	string result = erase_trailing_path_separator(file_name);

	if (result.find_first_of('/') == string::npos)
		return ".";

	if (result.length() == 1)
		// catch result == "/"
		return result;

	size_t pos = result.find_last_of('/');
	if (pos == 0)
		// "/usr" must return "/"
		pos = 1;

	result.erase(pos, result.length() - pos);

	// "////usr" must return "/"
	return erase_trailing_path_separator(result);
}


/**
 * basename - get the basename of a path
 * @param path_name  path
 *
 * Returns the basename of a path with trailing '/' removed.
 */
string basename(string const & path_name)
{
	string result = erase_trailing_path_separator(path_name);

	if (result.length() == 1)
		// catch result == "/"
		return result;

	return erase_to_last_of(result, '/');
}
