/**
 * @file op_mangling.h
 * Mangling and unmangling of sample filenames
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author John Levon
 * @author Philippe Elie
 */

#ifndef OP_MANGLING_H
#define OP_MANGLING_H

#include <string>
#include <list>

/// remove a counter suffix, if any (e.g. #0)
void strip_counter_suffix(std::string & name);

/**
 * remangle - convert a filename into the related sample file name
 * @param filename the filename string
 *
 * The returned filename is still relative to the samples directory.
 */
std::string remangle_filename(std::string const & filename);

/**
 * demangle_filename - convert a sample filenames into the related
 * image file name
 * @param samples_filename the samples image filename
 *
 * if samples_filename does not contain any %OPD_MANGLE_CHAR
 * the string samples_filename itself is returned.
 */
std::string demangle_filename(std::string const & samples_filename);

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
std::string extract_app_name(std::string const & name, std::string & lib_name);

/**
 * get_sample_file_list - create a file list of base samples filename
 * @param file_list: where to store the results
 * @param base_dir: base directory
 * @param filter: a file filter name.
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
 *
 * The returned filenames are relative to the base_dir
 */
void get_sample_file_list(std::list<std::string> & file_list,
			  std::string const & base_dir,
			  std::string const & filter);

#endif // OP_MANGLING_H
