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

#ifndef FILE_MANIP_H
#define FILE_MANIP_H

#include <string>
#include <list>

bool is_files_identical(std::string const & file1, std::string const & file2);
bool create_dir(std::string const & dir);
bool create_path(std::string const & path);
std::string opd_read_link(std::string const & name);

/// return false if base_dir is not a valid directory.
bool create_file_list(std::list<std::string>& file_list,
		      const std::string & base_dir);

#endif /* !FILE_MANIP_H */
