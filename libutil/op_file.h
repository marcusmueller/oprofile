/**
 * @file op_file.h
 * Useful file management helpers
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author John Levon
 * @author Philippe Elie
 */

#ifndef OP_FILE_H
#define OP_FILE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/types.h>

/**
 * op_file_readable - is a file readable
 * @param file file name
 *
 * Return true if the given file is readable.
 *
 * Beware of race conditions !
 */
int op_file_readable(char const * file);

/**
 * op_get_fsize - get size of file
 * @param file file name
 * @param size size parameter to fill
 *
 * Returns the size of the named file in bytes
 * into the pointer given. Returns non-zero
 * on failure, in which case size is not changed.
 */
int op_get_fsize(char const * file, off_t * size);

/**
 * op_get_mtime - get mtime of file
 * @param file  file name
 *
 * Returns the mtime of the given file or 0 on failure
 */
time_t op_get_mtime(char const * file);

/**
 * Follow exactly one level of symbolic link.
 * Returns NULL if it's not a symlink or on error,
 * or a string that caller must free.
 *
 * This does not re-seat any returned relative
 * symbolic links.
 */
char * op_get_link(char const * filename);

/**
 * op_relative_to_absolute_path - translate relative path to absolute path.
 * @param path  path name
 * @param base_dir  optional base directory, if %NULL getcwd() is used
 * to get the base directory.
 *
 * prepend base_dir or the result of getcwd if the path is not absolute.
 * The returned string is dynamic allocated, caller must free it. if
 * base_dir == NULL this function use getcwd to translate the path.
 *
 * Returns the translated path.
 */
char * op_relative_to_absolute_path(
	char const * path, char const * base_dir);

/**
 * create_dir - create a directory
 * @param dir  the directory name to create
 *
 * Returns 0 on success.
 */
int create_dir(char const * dir);


/**
 * create_path - create a path
 * @param path  the path to create
 *
 * create directory for each dir components in path
 * the last path component is not considered as a directory
 * but as a filename
 *
 * Returns 0 on success.
 */
int create_path(char const * path);

#ifdef __cplusplus
}
#endif

#endif /* OP_FILE_H */
