/* $Id: oprofpp.h,v 1.4 2000/09/28 00:39:17 moz Exp $ */

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
#define DMGL_PARAMS     (1 << 0)        /* Include function args */
#define DMGL_ANSI       (1 << 1)        /* Include const, volatile, etc */
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

 
