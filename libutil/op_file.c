/**
 * @file op_file.c
 * Useful file management helpers
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author John Levon
 * @author Philippe Elie
 */

#include <sys/stat.h>
#include <unistd.h>

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <limits.h>

#include "op_file.h"
#include "op_libiberty.h"

/**
 * op_file_readable - is a file readable
 * @param file file name
 *
 * Return true if the given file is readable.
 *
 * Beware of race conditions !
 */
int op_file_readable(char const * file)
{
	return !access(file, R_OK);
}

/**
 * op_get_fsize - get size of file
 * @param file file name
 * @param size size parameter to fill
 *
 * Returns the size of the named file in bytes
 * into the pointer given. Returns non-zero
 * on failure, in which case size is not changed.
 */
int op_get_fsize(char const * file, off_t * size)
{
	struct stat st;

	int err = stat(file, &st);
	if (err)
		return err;

	*size = st.st_size;
	return 0;
}

/**
 * op_get_mtime - get mtime of file
 * @param file  file name
 *
 * Returns the mtime of the given file or 0 on failure
 */
time_t op_get_mtime(char const * file)
{
	struct stat st;

	if (stat(file, &st))
		return 0;

	return st.st_mtime;
}


/* remove_component_p() and op_simplify_pathname() comes from the gcc
 preprocessor */

/**
 * remove_component_p - check if it is safe to remove the final component
 * of a path.
 * @param path  string pointer
 *
 * Returns 1 if it is safe to remove the component
 * 0 otherwise
 */
static int remove_component_p(char const * path)
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
 * op_simplify_path_name - simplify a path name in place
 * @param path  string pointer to the path.
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
 *  of the string.  Returns path.  errno is 0 if no error occurred;
 *  nonzero if an error occurred when using stat().
 */
static char * op_simplify_pathname(char * path)
{
	char * from, * to;
	char * base, * orig_base;
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
		/* FIXME: original code wasn't stripping '//' nor '///', saying
		 * POSIX allows that, are we concerned ? */
		while (*++from == '/') {
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
					if (remove_component_p(path)) {
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
		while (*from != '\0' && *from != '/') {
			*to++ = *from++;
		}

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
 * op_relative_to_absolute_path - translate relative path to absolute path.
 * @param path  path name
 * @param base_dir  optionnal base directory, if %NULL getcwd() is used
 * to get the base directory.
 *
 * prepend base_dir or the result of getcwd if the path is not absolute.
 * The returned string is dynamic allocated, caller must free it. if
 * base_dir == NULL this function use getcwd to translate the path.
 *
 * Returns the translated path.
 */
char * op_relative_to_absolute_path(char const * path, char const * base_dir)
{
	char dir[PATH_MAX + 1];
	char * temp_path = NULL;

	if (path && path[0] != '/') {
		if (base_dir == NULL) {
			if (getcwd(dir, PATH_MAX) != NULL) {
				base_dir = dir;
			}

		}

		if (base_dir != NULL) {
			temp_path = xmalloc(strlen(path) + strlen(base_dir) + 2);
			strcpy(temp_path, base_dir);
			strcat(temp_path, "/");
			strcat(temp_path, path);
		}
	}

	/* absolute path or (base_dir == NULL && getcwd have lose) :
         * always return a value */
	if (temp_path == NULL)
		temp_path = xstrdup(path);

	return op_simplify_pathname(temp_path);
}

/**
 * create_dir - create a directory
 * @param dir  the directory name to create
 *
 * return false if the directory dir does not exist
 * and cannot be created
 */
int create_dir(char const * dir)
{
	if (access(dir, F_OK)) {
		if (mkdir(dir, 0755))
			return 1;
	}
	return 0;
}

/**
 * create_path - create a path
 * @param path  the path to create
 *
 * create directory for each dir components in path
 * return false if one of the path cannot be created.
 * the last path component is not considered as a directory
 * but as a filename
 */
int create_path(char const * path)
{
	int ret;

	char * str = xstrdup(path);

	char * pos = str[0] == '/' ? str + 1 : str;

	for ( ; (pos = strchr(pos, '/')) != NULL; ++pos) {
		*pos = '\0';
		ret = create_dir(str);
		*pos = '/';
		if (ret) {
			free(str);
			return ret;
		}
	}

	free(str);
	return 0;
}
