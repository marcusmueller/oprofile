/* $Id: oprofiled.h,v 1.5 2000/08/01 23:00:17 moz Exp $ */

#ifndef OPROFILED_H
#define OPROFILED_H

/* See objdump --section-headers /usr/src/linux/vmlinux */
/* used to catch out kernel samples (and also compute 
   text offset if no System.map or module info is available */ 
#define KERNEL_VMA_OFFSET           0xc0100000
 
#include <popt.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <dirent.h> 
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/poll.h>
#include <sys/mman.h> 

#include "opd_util.h"
 
/* various defines */
 
#define OPD_DEBUG
 
#define streq(a,b) (!strcmp((a),(b))) 
#define streqn(a,b,n) (!strncmp((a),(b),(n))) 
 
/* this char replaces '/' in sample filenames */
#define OPD_MANGLE_CHAR '}'
 
/* maximum nr. of kernel modules */
#define OPD_MAX_MODULES 64
 
#define NR_CPUS 32
 
/* stats for sample collection */
#define OPD_MAX_STATS 9

/* nr. kernel samples */
#define OPD_KERNEL 0
/* nr. samples for which process info couldn't be accessed */ 
#define OPD_LOST_PROCESS 1
/* nr. userspace samples */
#define OPD_PROCESS 2
/* nr. samples for which map info couldn't be accessed */
#define OPD_LOST_MAP_PROCESS 3
/* nr. accesses of proc queue */
#define OPD_PROC_QUEUE_ACCESS 4
/* cumulative depth of proc queue accesses */
#define OPD_PROC_QUEUE_DEPTH 5
/* nr. of times buffer is read */
#define OPD_DUMP_COUNT 6
/* nr. accesses of map array */
#define OPD_MAP_ARRAY_ACCESS 7
/* cumulative depth of map array accesses */
#define OPD_MAP_ARRAY_DEPTH 8 
 
#define OPD_DEFAULT_BUF_SIZE 2048
 
#define OP_BITS 2

/* top OP_BITS bits of count are used as follows: */
/* is this actually a mapping notification ? */
#define OP_MAPPING (1U<<15) 
/* which perf counter the sample is from */
#define OP_COUNTER (1U<<14)

#define OP_COUNT_MASK ((1U<<(16-OP_BITS))-1U)

/* mapping notification types */ 
/* fork(),vfork(),clone() */
#define OP_FORK ((1U<<15)|(1U<<0))
/* execve() */
#define OP_DROP ((1U<<15)|(1U<<1))
/* mapping */
#define OP_MAP ((1U<<15)|(1U<<2))
/* init_module() */
#define OP_DROP_MODULES ((1U<<15)|(1U<<3))
/* exit() */
#define OP_EXIT ((1U<<15)|(1U<<4))
 
/* event check returns */
#define OP_EVENTS_OK            0x0
#define OP_CTR0_NOT_FOUND       0x1
#define OP_CTR1_NOT_FOUND       0x2
#define OP_CTR0_NO_UM           0x4
#define OP_CTR1_NO_UM           0x8
#define OP_CTR0_NOT_ALLOWED     0x10
#define OP_CTR1_NOT_ALLOWED     0x20
#define OP_CTR0_PII_EVENT       0x40
#define OP_CTR1_PII_EVENT       0x80
#define OP_CTR0_PIII_EVENT     0x100
#define OP_CTR1_PIII_EVENT     0x200
 
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
 
/* note that pid_t is 32 bits, but only 16 are used
   currently, so to save cache, we use u16 */
struct op_sample {
        u32 eip;
        u16 pid;
        u16 count;
} __attribute__((__packed__,__aligned__(8)));

struct opd_image {
	fd_t fd;
	void *start;
	off_t len; 
	u8 kernel; 
	char *name;
};
 
/* kernel module */
struct opd_module {
	struct opd_image *image;
	char *name;
	u32 start;
	u32 end;
};
 
struct opd_map {
	struct opd_image *image;
	u32 start;
	u32 offset;
	u32 end;
};

struct opd_proc {
	struct opd_map *maps;
	unsigned int nr_maps;
	unsigned int max_nr_maps;
	u16 pid;
	int dead; 
	struct opd_proc *prev;
	struct opd_proc *next;
};
 
int op_check_events_str(char *ctr0_type, char *ctr1_type, u8 ctr0_um, u8 ctr1_um, int p2, u8 *ctr0_t, u8 *ctr1_t);
 
void opd_get_ascii_procs(void); 
void opd_init_images(void);
void opd_put_sample(const struct op_sample *sample);
void opd_read_system_map(const char *filename); 
void opd_alarm(int val); 
 
void opd_handle_fork(const struct op_sample *sample);
void opd_handle_exit(const struct op_sample *sample);
void opd_handle_mapping(const struct op_sample *sample);
void opd_handle_drop_mappings(const struct op_sample *sample);
void opd_clear_module_info(void);
 
#endif /* OPROFILED_H */
