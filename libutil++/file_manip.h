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
 * \param file_list where to store result
 * \param base_dir directory from where lookup start
 * \param filter a filename filter
 * \param recursive if true lookup in sub-directory
 *
 * create a filelist under base_dir, filtered by filter and optionnaly
 * looking in sub-directory. If we look in sub-directory only sub-directory
 * which match filter are traversed.
 */
bool create_file_list(std::list<std::string>& file_list,
		      const std::string & base_dir,
		      const std::string & filter = "*",
		      bool recursive = false);

/** 
 * \param path path name to translate
 * \param base_dir base directory from where the path name is relative
 * if abse_dir is empty $PWD is used as base directory
 *
 * translate a relative path to an absolute path.
 */
std::string relative_to_absolute_path(const std::string & path,
				const std::string & base_dir = std::string());

/** return the base name of file_name as basename(1) */
std::string dirname(std::string const & file_name);
/** return the dir name of path_name as dirname(1) */
std::string basename(std::string const & path_name);

/**
 * extract_app_name - extract the mangled name of an application
 * \param name the mangled name
 * \param lib_name where to store the shared lib name if relevant
 *
 * if name is: }usr}sbin}syslogd}}}lib}libc-2.1.2.so (shared lib)
 * we return }usr}sbin}syslogd and }lib}libc-2.1.2.so in lib_name
 *
 * if name is: }bin}bash (application)
 *  we return }bin}bash and an empty name in lib_name
 */
// TODO: can we demangle filename before returning it to simplify caller code ?
std::string extract_app_name(const std::string & name, std::string & lib_name);

/**
 * get_sample_file_list - create a file list of base samples filename
 * \param file_list: where to store the results
 * \param base_dir: base directory
 * \param filter: a file filter name.
 *
 * fill file_list with a list of base samples filename where a base sample
 * filename is a samples filename without #nr suffix. Even if the call
 * pass "*" as filter only valid samples filename are returned (filename
 * containing at least on mangled char)
 *
 * Note than the returned list can contains filename where some samples
 * exist for one counter but does not exist for other counter. Caller must
 * handle this problems. e.g. if the samples dir contains foo#1 and bar#0
 * the list will contain { "foo", "bar" } and if the caller want to work
 * only on counter #0 it must refilter the filelist created
 */
void get_sample_file_list(std::list<std::string> & file_list,
			  const std::string & base_dir,
			  const std::string & filter);

/** strip the #nr suffix of a samples filename */
std::string strip_filename_suffix(const std::string & filename);

#endif /* !FILE_MANIP_H */
