/**
 * @file op_fileio.h
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 * 
 * @author John Levon <moz@compsoc.man.ac.uk>
 * @author Philippe Elie <phil_el@wanadoo.fr>
 */

#ifndef OP_FILEIO_H
#define OP_FILEIO_H

#ifdef __cplusplus
extern "C" {
#endif

#include "op_types.h"
 
#include <stdio.h>
 
#define op_try_open_file(n,m) op_do_open_file((n), (m), 0)
#define op_open_file(n,m) op_do_open_file((n), (m), 1)
FILE * op_do_open_file(char const * name, char const * mode, int fatal);
void op_close_file(FILE * fp);
 
#define op_try_read_file(f,b,s) op_do_read_file((f), (b), (s), 0);
#define op_read_file(f,b,s) op_do_read_file((f), (b), (s), 1);
void op_do_read_file(FILE * fp, void * buf, size_t size, int fatal);
u8 op_read_u8(FILE * fp);
u16 op_read_u16_he(FILE * fp);
u32 op_read_u32_he(FILE * fp);
u32 op_read_int_from_file(char const * filename);
char * op_get_line(FILE * fp);
 
void op_write_file(FILE * fp, const void * buf, size_t size);
void op_write_u8(FILE * fp, u8 val);
void op_write_u16_he(FILE * fp, u16 val);
void op_write_u32_he(FILE * fp, u32 val);
 
#ifdef __cplusplus
}
#endif

#endif /* OP_FILEIO_H */
