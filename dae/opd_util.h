/* $Id: opd_util.h,v 1.7 2001/06/22 01:17:39 movement Exp $ */
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
 
#ifdef OPD_GZIP 
#include <zlib.h>
#endif /* OPD_GZIP */ 
#include <sys/types.h>
 
#define FALSE 0
#define TRUE 1
 
#define u8  unsigned char
#define u16 u_int16_t
#define u32 u_int32_t
#define fd_t int
#define streq(a,b) (!strcmp((a),(b)))   
 
/* utility functions */
#define opd_calloc(memb, size) opd_malloc(memb*size)
#define opd_calloc0(memb, size) opd_malloc0(memb*size)
#define opd_crealloc(memb, buf, size) opd_realloc((buf), memb*size)
void *opd_malloc(size_t size);
void *opd_malloc0(size_t size);
void *opd_realloc(void *buf, size_t size);
void opd_free(void *p);
#define opd_try_open_file(n,m) opd_do_open_file((n),(m),0)
#define opd_open_file(n,m) opd_do_open_file((n),(m),1)
FILE *opd_do_open_file(const char *name, const char *mode, int fatal);
void opd_close_file(FILE *fp);
char *opd_read_link(const char *name);
#define opd_try_read_file(f,b,s) opd_do_read_file((f),(b),(s),0);
#define opd_read_file(f,b,s) opd_do_read_file((f),(b),(s),1); 
void opd_do_read_file(FILE *fp, void *buf, size_t size, int fatal);
u8 opd_read_u8(FILE *fp);
u16 opd_read_u16_he(FILE *fp);
u32 opd_read_u32_he(FILE *fp);
u16 opd_read_u16_ne(FILE *fp);
u32 opd_read_u32_ne(FILE *fp);
u32 opd_read_int_from_file(const char *filename);
#ifdef OPD_GZIP
#define opd_try_open_file_z(n,m) opd_do_open_file_z((n),(m),0)
#define opd_open_file_z(n,m) opd_do_open_file_z((n),(m),1)
gzFile opd_do_open_file_z(const char *name, const char *mode, int fatal);
#define opd_try_read_file_z(f,b,s) opd_do_read_file_z((f),(b),(s),0);
#define opd_read_file_z(f,b,s) opd_do_read_file_z((f),(b),(s),1); 
void opd_do_read_file_z(gzFile fp, void *buf, size_t size, int fatal);
u8 opd_read_u8_z(gzFile fp);
u16 opd_read_u16_he_z(gzFile fp);
u32 opd_read_u32_he_z(gzFile fp);
u16 opd_read_u16_ne_z(gzFile fp);
#endif /* OPD_GZIP */ 
void opd_write_file(FILE *fp, const void *buf, size_t size);
void opd_write_u8(FILE *fp, u8 val);
void opd_write_u16_he(FILE *fp, u16 val);
void opd_write_u32_he(FILE *fp, u32 val);
void opd_write_u16_ne(FILE *fp, u16 val);
void opd_write_u32_ne(FILE *fp, u32 val);
#define opd_try_open_device(n) opd_open_device((n),0)
fd_t opd_open_device(const char *name, int fatal);
void opd_close_device(fd_t devfd);
size_t opd_read_device(fd_t devfd, void *buf, size_t size, int seek);
off_t opd_get_fsize(const char *file, int fatal);
 
char *opd_get_time(void);
char *opd_get_line(FILE *fp);

#endif /* OPD_UTIL_H */
