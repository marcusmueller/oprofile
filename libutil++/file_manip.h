/**
 * @file file_manip.h
 * Useful file management helpers
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author Philippe Elie
 * @author John Levon
 */

#ifndef FILE_MANIP_H
#define FILE_MANIP_H

#include <string>
#include <list>


/// return true if dir is an existing directory
bool is_directory(std::string const & dirname);

/**
 * is_file_identical - check for identical files
 * @param file1  first filename
 * @param file2  scond filename
 *
 * return true if the two filenames belong to the same file
 */
bool is_files_identical(std::string const & file1, std::string const & file2);

/**
 * op_follow_link - follow a symbolic link
 * @param name the file name
 *
 * Resolve a symbolic link as far as possible.
 * Returns the original string on failure.
 */
std::string const op_follow_link(std::string const & name);

/// return true if the given file is readable
bool op_file_readable(std::string const & file);

/**
 * @param file_list where to store result
 * @param base_dir directory from where lookup start
 * @param filter a filename filter
 * @param recursive if true lookup in sub-directory
 *
 * create a filelist under base_dir, filtered by filter and optionnaly
 * looking in sub-directory. If we look in sub-directory only sub-directory
 * which match filter are traversed.
 */
bool create_file_list(std::list<std::string> & file_list,
		      std::string const & base_dir,
		      std::string const & filter = "*",
		      bool recursive = false);

/**
 * @param path path name to translate
 * @param base_dir base directory from where the path name is relative
 * if base_dir is empty $PWD is used as base directory
 *
 * translate a relative path to an absolute path. If the path is
 * already absolute, no change is made.
 */
std::string relative_to_absolute_path(std::string const & path,
				std::string const & base_dir = std::string());

/**
 * dirname - get the path component of a filename
 * @param file_name  filename
 *
 * Returns the path name of a filename with trailing '/' removed.
 */
std::string dirname(std::string const & file_name);

/**
 * basename - get the basename of a path
 * @param path_name  path
 *
 * Returns the basename of a path with trailing '/' removed.
 */
std::string basename(std::string const & path_name);

#endif /* !FILE_MANIP_H */
