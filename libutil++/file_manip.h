/**
 * @file file_manip.h
 * Useful file management helpers
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author Philippe Elie <phil_el@wanadoo.fr>
 * @author John Levon <moz@compsoc.man.ac.uk>
 */

#ifndef FILE_MANIP_H
#define FILE_MANIP_H

#include <string>
#include <list>

/** return true if the two files are identical */
bool is_files_identical(std::string const & file1, std::string const & file2);
/** create a directory, return false on failure */
bool create_dir(std::string const & dir);
/** create each component of the path, return false o, failure */
bool create_path(std::string const & path);
/** return the contents of a symbolic link or an empty string on failure */
std::string op_read_link(std::string const & name);

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
 * if abse_dir is empty $PWD is used as base directory
 *
 * translate a relative path to an absolute path. If the path is
 * already absolute, no change is made.
 */
std::string relative_to_absolute_path(std::string const & path,
				std::string const & base_dir = std::string());

/** return the base name of file_name as basename(1) */
std::string dirname(std::string const & file_name);
/** return the dir name of path_name as dirname(1) */
std::string basename(std::string const & path_name);

#endif /* !FILE_MANIP_H */
