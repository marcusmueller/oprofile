/* $Id: oprofiled.h,v 1.18 2000/12/06 20:39:50 moz Exp $ */
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
#include <sys/ioctl.h>

#include "opd_util.h"
#include "../version.h"

/* various defines */

//#define OPD_DEBUG

#ifdef OPD_DEBUG
#define dprintf(args...) printf(args)
#else
#define dprintf(args...)
#endif

#define streq(a,b) (!strcmp((a),(b)))
#define streqn(a,b,n) (!strncmp((a),(b),(n)))

/* this char replaces '/' in sample filenames */
#define OPD_MANGLE_CHAR '}'

/* maximum nr. of kernel modules */
#define OPD_MAX_MODULES 64

/* size of process hash table */
#define OPD_MAX_PROC_HASH 1024

#define NR_CPUS 32

enum {  OPD_KERNEL, /* nr. kernel samples */
	OPD_LOST_PROCESS, /* nr. samples for which process info couldn't be accessed */
	OPD_PROCESS, /* nr. userspace samples */
	OPD_LOST_MAP_PROCESS, /* nr. samples for which map info couldn't be accessed */
	OPD_PROC_QUEUE_ACCESS, /* nr. accesses of proc queue */
	OPD_PROC_QUEUE_DEPTH, /* cumulative depth of proc queue accesses */
	OPD_DUMP_COUNT, /* nr. of times buffer is read */
	OPD_MAP_ARRAY_ACCESS, /* nr. accesses of map array */
	OPD_MAP_ARRAY_DEPTH, /* cumulative depth of map array accesses */
	OPD_SAMPLES, /* nr. samples */
	OPD_NOTIFICATIONS, /* nr. notifications */
	OPD_MAX_STATS /* end of stats */
	};

#define OPD_DEFAULT_BUF_SIZE 2048

#define OP_BITS 2

/* top OP_BITS bits of count are used as follows: */
/* is this actually a mapping notification ? */
#define OP_MAPPING (1U<<15)
/* which perf counter the sample is from */
#define OP_COUNTER (1U<<14)

#define OP_COUNT_MASK ((1U<<(16-OP_BITS))-1U)

/* nr. entries in hash map, prime */
#define OP_HASH_MAP_NR 1023

/* size of hash map entries */
#define OP_HASH_LINE 128

/* size of hash map in bytes */
#define OP_HASH_MAP_SIZE OP_HASH_LINE*OP_HASH_MAP_NR

/* mapping notification types */
/* fork(),vfork(),clone() */
#define OP_FORK ((1U<<15)|(1U<<0))
/* execve() */
#define OP_EXEC ((1U<<15)|(1U<<1))
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
        u16 count;
        u16 pid;
        u32 eip;
} __attribute__((__packed__));

struct opd_image {
	fd_t fd;
	void *start;
	off_t len;
	u8 kernel;
	char *name;
};

/* kernel module */
struct opd_module {
	char *name;
	int image;
	u32 start;
	u32 end;
};

struct opd_map {
	int image;
	u32 start;
	u32 offset;
	u32 end;
};

struct opd_proc {
	struct opd_map *maps;
	unsigned int nr_maps;
	unsigned int max_nr_maps;
	unsigned int last_map;
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
void opd_handle_exec(const struct op_sample *sample);
void opd_handle_exit(const struct op_sample *sample);
void opd_handle_mapping(const struct op_sample *sample);
void opd_clear_module_info(void);

#endif /* OPROFILED_H */
