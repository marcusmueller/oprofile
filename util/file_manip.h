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

bool is_files_identical(std::string const & file1, std::string const & file2);
bool create_dir(std::string const & dir);
bool create_path(std::string const & path);
std::string opd_read_link(std::string const & name);

/* return false if base_dir is not a valid directory. */
bool create_file_list(std::list<std::string>& file_list,
		      const std::string & base_dir,
		      const std::string & filter = "*");

std::string relative_to_absolute_path(const std::string & path,
				const std::string & base_dir = std::string());

// filename handling.
std::string dirname(std::string const & file_name);
std::string basename(std::string const & path_name);
// extract the mangled name of an application and the shared lib name
std::string extract_app_name(const string & name, string & lib_name);
// get a file list of valid sample filename from base_dir filtered by
// by filter. The #counter_nr suffix are stripepd before creating the
// list
void get_sample_file_list(list<string> & file_list,
			  const std::string & base_dir,
			  const std::string & filter);
// return filename stripped of the #counter_nr suffix
string strip_filename_suffix(const std::string & filename);

#endif /* !FILE_MANIP_H */
