/* COPYRIGHT (C) 2001 by various authors
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
 * Part written by John Levon and P. Elie
 */

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

#include "file_manip.h"
#include "string_manip.h"

#define OPD_MANGLE_CHAR '}'

using std::vector;
using std::string;
using std::list;
using std::find;
using std::cerr;
using std::endl;

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
bool create_file_list(list<string>& file_list, const string & base_dir,
		      const string & filter, bool recursive)
{
	DIR *dir;
	struct dirent *dirent;

	if (!(dir = opendir(base_dir.c_str())))
		return false;

	while ((dirent = readdir(dir)) != 0) {
		if (!is_directory_name(dirent->d_name) &&
		    fnmatch(filter.c_str(), dirent->d_name, 0) != FNM_NOMATCH) {
			if (recursive) {
				struct stat stat_buffer;
				string name = base_dir + '/' + dirent->d_name;
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
				file_list.push_back(dirent->d_name);
			}
		}
	}

	closedir(dir);

	return true;
}


/* remove_component_p() and op_simlify_pathname() comes from the gcc
 preprocessor */

/**
 * remove_component_p - check if it is safe to remove the final component
 * of a path.
 * @path: string pointer
 *
 * Returns 1 if it is safe to remove the component
 * 0 otherwise
 */ 
static int remove_component_p (const char *path)
{
	struct stat s;
	int result;

	result = lstat (path, &s);

	/* There's no guarantee that errno will be unchanged, even on
	   success.  Cygwin's lstat(), for example, will often set errno to
	   ENOSYS.  In case of success, reset errno to zero.  */
	if (result == 0)
		errno = 0;

	return result == 0 && S_ISDIR (s.st_mode);
}

/**
 * opd_simplify_path_name - simplify a path name in place
 * @path: string pointer to the path.
 *
 *  Simplify a path name in place, deleting redundant components.  This
 *  reduces OS overhead and guarantees that equivalent paths compare
 *  the same (modulo symlinks).
 *
 *  Transforms made:
 *  foo/bar/../quux	foo/quux
 *  foo/./bar		foo/bar
 *  foo//bar		foo/bar
 *  /../quux		/quux
 *  //quux		//quux  (POSIX allows leading // as a namespace escape)
 *
 *  Guarantees no trailing slashes.  All transforms reduce the length
 *  of the string.  Returns @path.  errno is 0 if no error occurred;
 *  nonzero if an error occurred when using stat().
 */
char *opd_simplify_pathname (char *path)
{
	char *from, *to;
	char *base, *orig_base;
	int absolute = 0;

	errno = 0;
	/* Don't overflow the empty path by putting a '.' in it below.  */
	if (*path == '\0')
		return path;

	from = to = path;
    
	/* Remove redundant leading /s.  */
	if (*from == '/') {
		absolute = 1;
		to++;
		from++;
		if (*from == '/') {
			if (*++from == '/')
				/* 3 or more initial /s are equivalent to 1 /.  */
				while (*++from == '/');
			else
				/* On some hosts // differs from /; Posix allows this.  */
				to++;
		}
	}

	base = orig_base = to;
	for (;;) {
		int move_base = 0;

		while (*from == '/')
			from++;

		if (*from == '\0')
			break;

		if (*from == '.') {
			if (from[1] == '\0')
				break;
			if (from[1] == '/') {
				from += 2;
				continue;
			}
			else if (from[1] == '.' && (from[2] == '/' || from[2] == '\0')) {
				/* Don't simplify if there was no previous component.  */
				if (absolute && orig_base == to) {
					from += 2;
					continue;
				}
				/* Don't simplify if the previous component 
				 * was "../", or if an error has already
				 * occurred with (l)stat.  */
				if (base != to && errno == 0) {
					/* We don't back up if it's a symlink.  */
					*to = '\0';
					if (remove_component_p (path)) {
						while (to > base && *to != '/')
							to--;
						from += 2;
						continue;
					}
				}
				move_base = 1;
			}
		}

		/* Add the component separator.  */
		if (to > orig_base)
			*to++ = '/';

		/* Copy this component until the trailing null or '/'.  */
		while (*from != '\0' && *from != '/')
			*to++ = *from++;

		if (move_base)
			base = to;
	}
    
	/* Change the empty string to "." so that it is not treated as stdin.
	   Null terminate.  */
	if (to == path)
		*to++ = '.';
	*to = '\0';

	return path;
}

/**
 * opd_relative_to_absolute_path - translate relative path to absolute path.
 * @path: path name
 * @base_dir: optionnal base directory, if %NULL getcwd() is used
 * to get the base directory.
 *
 * prepend @base_dir or the result of getcwd if the path is not absolute.
 * The returned string is dynamic allocated, caller must free it. if 
 * base_dir == NULL this function use getcwd to translate the path.
 *
 * Returns the translated path.
 */
/* FIXME: from libiberty: a hack until we need a C version of this or opd_malloc 
 * and related are put in the util dir */
extern "C" char * xstrdup(const char *);
 
char *opd_relative_to_absolute_path(const char *path, const char *base_dir)
{
	char dir[PATH_MAX + 1];
	char *temp_path = NULL;

	if (path && path[0] != '/') {
		if (base_dir == NULL) {
			if (getcwd(dir, PATH_MAX) != NULL) {
				base_dir = dir;
			}

		}

		if (base_dir != NULL) {
			temp_path = new char [strlen(path) + strlen(base_dir) + 2];
			strcpy(temp_path, base_dir);
			strcat(temp_path, "/");
			strcat(temp_path, path);
		}
	}

	/* absolute path or (base_dir == NULL && getcwd have lose) : 
         * always return a value */
	if (temp_path == NULL)
		temp_path = xstrdup(path);

	return opd_simplify_pathname(temp_path);
}


std::string relative_to_absolute_path(const std::string & path,
				      const std::string & base_dir)
{
	const char * dir = base_dir.length() ? base_dir.c_str() : NULL;

	char * result = opd_relative_to_absolute_path(path.c_str(), dir);

	std::string res(result);

	free(result);

	return res;
}

/**
 * dirname - get the path component of a filename
 * @file_name: filename
 *
 * Returns the path name of a filename with trailing '/' removed.
 */
string dirname(string const & file_name)
{
	return erase_from_last_of(file_name, '/');
}

/**
 * basename - get the basename of a path
 * @path_name: path
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

/**
 * get_sample_file_list - create a file list of base samples filename
 * @file_list: where to put the results
 * @base_dir: base directory
 * @filter: a file filter name.
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
