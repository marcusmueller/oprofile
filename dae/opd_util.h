/* $Id: opd_util.h,v 1.21 2001/12/04 21:16:11 phil_e Exp $ */
/* COPYRIGHT (C) 2000 THE VICTORIA UNIVERSITY OF MANCHESTER and John Levon
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
 */

#ifndef OPD_UTIL_H
#define OPD_UTIL_H

#include <stdio.h>
#include <string.h> 
#include <sys/stat.h>
#include <unistd.h> 
#include <sys/types.h>
#include <errno.h> 
#include <time.h> 
#include <fcntl.h> 
#include <popt.h>

#include "../op_user.h"

/* this char replaces '/' in sample filenames */
#define OPD_MANGLE_CHAR '}'

#define FALSE 0
#define TRUE 1

#define streq(a,b) (!strcmp((a), (b)))

#define OPD_MAGIC "DAE\n"
#define OPD_VERSION 0x5

/* header of the sample files */
struct opd_header {
	u8  magic[4];
	u32 version;
	u8 is_kernel;
	u32 ctr_event;
	u32 ctr_um;
	/* ctr number, used for sanity checking */
	u32 ctr;
	u32 cpu_type;
	u32 ctr_count;
	double cpu_speed;
	time_t mtime;
	/* binary compatibility reserve */
	u32  reserved2[21];
};

struct opd_fentry {
	u32 count;
};

#ifdef __cplusplus
extern "C" {
#endif

#ifdef MALLOC_OK
#define OP_ATTRIB_MALLOC	__attribute__((malloc))
#else
#define OP_ATTRIB_MALLOC
#endif

/* utility functions */
#define opd_calloc(memb, size) opd_malloc(memb*size)
#define opd_calloc0(memb, size) opd_malloc0(memb*size)
#define opd_crealloc(memb, buf, size) opd_realloc((buf), memb*size)

void *opd_malloc(size_t size) OP_ATTRIB_MALLOC;
void *opd_malloc0(size_t size) OP_ATTRIB_MALLOC;
char *opd_strdup(const char* str) OP_ATTRIB_MALLOC;
void *opd_realloc(void *buf, size_t size);
void opd_free(void *p);
char* opd_mangle_filename(const char *smpdir, const char* filename);

#define opd_try_open_file(n,m) opd_do_open_file((n), (m), 0)
#define opd_open_file(n,m) opd_do_open_file((n), (m), 1)
FILE *opd_do_open_file(const char *name, const char *mode, int fatal);
void opd_close_file(FILE *fp);
#define opd_try_read_file(f,b,s) opd_do_read_file((f), (b), (s), 0);
#define opd_read_file(f,b,s) opd_do_read_file((f), (b), (s), 1);
void opd_do_read_file(FILE *fp, void *buf, size_t size, int fatal);
u8 opd_read_u8(FILE *fp);
u16 opd_read_u16_he(FILE *fp);
u32 opd_read_u32_he(FILE *fp);
u32 opd_read_int_from_file(const char *filename);
 
void opd_write_file(FILE *fp, const void *buf, size_t size);
void opd_write_u8(FILE *fp, u8 val);
void opd_write_u16_he(FILE *fp, u16 val);
void opd_write_u32_he(FILE *fp, u32 val);
 
#define opd_try_open_device(n) opd_open_device((n), 0)
fd_t opd_open_device(const char *name, int fatal);
void opd_close_device(fd_t devfd);
size_t opd_read_device(fd_t devfd, void *buf, size_t size, int seek);
off_t opd_get_fsize(const char *file, int fatal);
time_t opd_get_mtime(const char *file);

char *opd_get_time(void);
char *opd_get_line(FILE *fp);

int opd_move_regular_file(const char *new_dir, const char *old_dir, 
			  const char *name);

#ifdef __cplusplus
}
#endif

#endif /* OPD_UTIL_H */
