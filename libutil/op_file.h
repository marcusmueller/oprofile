/**
 * \file op_file.h
 * Copyright 2002 OProfile authors
 * Read the file COPYING
 *
 * \author John Levon <moz@compsoc.man.ac.uk>
 * \author Philippe Elie <phil_el@wanadoo.fr>
 */

#ifndef OP_FILE_H
#define OP_FILE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/types.h>
 
#include <stdio.h>
#include <stdlib.h>
 
off_t op_get_fsize(char const * file, int fatal);
time_t op_get_mtime(const char * file);
int op_move_regular_file(const char * new_dir, 
	const char * old_dir, const char * name);
char * op_relative_to_absolute_path(
	char const * path, char const * base_dir);

#ifdef __cplusplus
}
#endif

#endif /* OP_FILE_H */
