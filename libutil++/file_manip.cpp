/**
 * @file file_manip.cpp
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author Philippe Elie <phil_el@wanadoo.fr>
 * @author John Levon <moz@compsoc.man.ac.uk>
 */

// FIXME: still some work to do here ...
 
// FIXME: check these headers are needed 
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>	// for FILENAME_MAX
#include <dirent.h>
#include <errno.h>
#include <fnmatch.h>

#include <vector>
#include <iostream>
#include <string>
#include <algorithm>

#include "op_file.h"
 
#include "file_manip.h"
#include "string_manip.h"

// FIXME: nope !
#define OPD_MANGLE_CHAR '}'

using std::vector;
using std::string;
using std::list;
using std::find;
using std::cerr;
using std::endl;

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
 * create_dir - create a directory
 * @param dir  the directory name to create
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
 * @param dir  the path to create
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
 * op_read_link - read the contents of a symbolic link file
 * @param name  the file name
 *
 * return an empty string on failure
 */
string op_read_link(string const & name)
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
bool create_file_list(list<string>& file_list, const string & base_dir,
		      const string & filter, bool recursive)
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


std::string relative_to_absolute_path(const std::string & path,
				      const std::string & base_dir)
{
	const char * dir = base_dir.length() ? base_dir.c_str() : NULL;

	char * result = op_relative_to_absolute_path(path.c_str(), dir);

	std::string res(result);

	free(result);

	return res;
}

/**
 * dirname - get the path component of a filename
 * @param file_name  filename
 *
 * Returns the path name of a filename with trailing '/' removed.
 */
string dirname(string const & file_name)
{
	return erase_from_last_of(file_name, '/');
}

/**
 * basename - get the basename of a path
 * @param path_name  path
 *
 * Returns the basename of a path with trailing '/' removed.
 */
string basename(string const & path_name)
{
	string result = rtrim(path_name, '/');

	return erase_to_last_of(result, '/');
}

/**
 * extract_app_name - extract the mangled name of an application
 * @name the mangled name
 *
 * if @name is: }usr}sbin}syslogd}}}lib}libc-2.1.2.so (shared lib)
 * will return }usr}sbin}syslogd and }lib}libc-2.1.2.so in
 * @lib_name
 *
 * if @name is: }bin}bash (application)
 *  will return }bin}bash and an empty name in @lib_name
 */
string extract_app_name(const string & name, string & lib_name)
{
	string result(name);
	lib_name = string();

	size_t pos = result.find("}}");
	if (pos != string::npos) {
		result.erase(pos, result.length() - pos);
		lib_name = name.substr(pos + 2);
	}

	return result;
}

/**
 * strip_filename_suffix - strip the #nr suffix of a samples filename
 */
string strip_filename_suffix(const std::string & filename)
{
	std::string result(filename);

	size_t pos = result.find_last_of('#');
	if (pos != string::npos)
		result.erase(pos, result.length() - pos);

	return result;
}

// FIXME: a libop++ kind of thing
 
/**
 * get_sample_file_list - create a file list of base samples filename
 * @param file_list  where to put the results
 * @param base_dir  base directory
 * @param filter  a file filter name.
 *
 * fill @file_list with a list of base samples
 * filename where a base sample filename is a
 * samples filename without #nr suffix. Even if the call
 * pass "*" as filter only valid samples filename are
 * returned (ie string like base/dir/session-xxx are filtered)
 */
void get_sample_file_list(list<string> & file_list,
			  const std::string & base_dir,
			  const std::string & filter)
{
	file_list.clear();

	list <string> files;
	if (create_file_list(files, base_dir, filter) == false) {
		cerr << "Can't open directory \"" << base_dir << "\": "
		     << strerror(errno) << endl;
		exit(EXIT_FAILURE);
	}

	list<string>::iterator it;
	for (it = files.begin(); it != files.end(); ++it) {

		// even if caller specify "*" as filter we avoid to get
		// invalid filename
		if (it->find_first_of(OPD_MANGLE_CHAR) == string::npos)
			continue;

		string filename = strip_filename_suffix(*it);

		// After stripping the # suffix multiples identicals filenames
		// can exist.
		if (find(file_list.begin(), file_list.end(), filename) == 
		    file_list.end())
			file_list.push_back(filename);
	}
}
