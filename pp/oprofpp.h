/* $Id: oprofpp.h,v 1.6 2000/12/06 20:39:58 moz Exp $ */
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

#include <libiberty.h>
#include <bfd.h>
#include <popt.h>
 
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>  
#include <fcntl.h> 
#include <errno.h> 

#include <sys/types.h> 
#include <sys/stat.h> 
#include <sys/mman.h>

#include "../dae/opd_util.h"
#include "../version.h"

/* missing from libiberty.h */
#ifndef DMGL_PARAMS
# define DMGL_PARAMS     (1 << 0)        /* Include function args */
#endif 
#ifndef DMGL_ANSI 
# define DMGL_ANSI       (1 << 1)        /* Include const, volatile, etc */
#endif
char *cplus_demangle (const char *mangled, int options);
 
void op_get_event_desc(u8 type, u8 um, char **typenamep, char **typedescp, char **umdescp);
 

#define FALSE 0
#define TRUE 1
#define uint unsigned int
#define ulong unsigned long 
#define u8  unsigned char
#define u16 u_int16_t
#define u32 u_int32_t
#define fd_t int
#define streq(a,b) (!strcmp((a),(b)))

/* kernel image entries are offset by this many entries */
#define OPD_KERNEL_OFFSET 524288
 
/* this char replaces '/' in sample filenames */
#define OPD_MANGLE_CHAR '}'
 
struct opd_fentry {
        u32 count0;
        u32 count1;
};

#define OPD_MAGIC 0xdeb6
#define OPD_VERSION 0x1

/* at the end of the sample files */
struct opd_footer {
        u16 magic;
        u16 version;
        u8 is_kernel;
        u8 ctr0_type_val;
        u8 ctr1_type_val;
        u8 ctr0_um;
        u8 ctr1_um;
};
