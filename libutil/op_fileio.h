/**
 * @file op_fileio.h
 * Reading from / writing to files
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author John Levon
 * @author Philippe Elie
 */

#ifndef OP_FILEIO_H
#define OP_FILEIO_H

#ifdef __cplusplus
extern "C" {
#endif

#include "op_types.h"

#include <stdio.h>

FILE * op_try_open_file(char const * name, char const * mode);
FILE * op_open_file(char const * name, char const * mode);
void op_close_file(FILE * fp);

void op_read_file(FILE * fp, void * buf, size_t size);
u32 op_read_int_from_file(char const * filename);
char * op_get_line(FILE * fp);

void op_write_file(FILE * fp, void const * buf, size_t size);
void op_write_u32(FILE * fp, u32 val);
void op_write_u64(FILE * fp, u64 val);
void op_write_u8(FILE * fp, u8 val);

/**
 * Follow exactly one level of symbolic link.
 * Returns NULL if it's not a symlink or on error,
 * or a string that caller must free.
 *
 * This does not re-seat any returned relative
 * symbolic links.
 */
char * op_get_link(char const * filename);

#ifdef __cplusplus
}
#endif

#endif /* OP_FILEIO_H */
