/**
 * @file op_file.h
 * Useful file management helpers
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 * 
 * @author John Levon <moz@compsoc.man.ac.uk>
 * @author Philippe Elie <phil_el@wanadoo.fr>
 */

#ifndef OP_FILE_H
#define OP_FILE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/types.h>
 
int op_file_readable(char const * file);
int op_get_fsize(char const * file, off_t * size);
time_t op_get_mtime(char const * file);
int op_move_regular_file(char const * new_dir, 
	char const * old_dir, char const * name);
char * op_relative_to_absolute_path(
	char const * path, char const * base_dir);

#ifdef __cplusplus
}
#endif

#endif /* OP_FILE_H */
