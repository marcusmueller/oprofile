/* $Id: oprofiled.h,v 1.24 2001/06/22 01:17:39 movement Exp $ */
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

#include "opd_util.h"
#include "../op_user.h"

/* various defines */

//#define OPD_DEBUG

#ifdef OPD_DEBUG
#define dprintf(args...) printf(args)
#else
#define dprintf(args...)
#endif

#define verbprintf(args...) \
        do { \
		if (verbose) \
			printf(args); \
	} while (0)

#define streq(a,b) (!strcmp((a),(b)))
#define streqn(a,b,n) (!strncmp((a),(b),(n)))

/* this char replaces '/' in sample filenames */
#define OPD_MANGLE_CHAR '}'

/* maximum nr. of kernel modules */
#define OPD_MAX_MODULES 64

/* size of process hash table */
#define OPD_MAX_PROC_HASH 1024

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

/* FIXME : Carefull these are also present in pp/oprofpp.h */
#define OPD_MAGIC 0xdeb6
#define OPD_VERSION 0x3

/* at the end of the sample files */
struct opd_footer {
	u16 magic;
	u16 version;
	u8 is_kernel;
	u8 ctr0_type_val;
	u8 ctr1_type_val;
	u8 ctr0_um;
	u8 ctr1_um;
	char md5sum[16];
	u32 ctr0_count;
	u32 ctr1_count;
	/* Set to 0.0 if not available */
	double cpu_speed;
	/* binary compatibility reserve */
	u32  reserved[32];
};

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

int op_check_events(u8 ctr0_type, u8 ctr1_type, u8 ctr0_um, u8 ctr1_um, int proc);

void opd_get_ascii_procs(void);
void opd_init_images(void);
void opd_put_sample(const struct op_sample *sample);
void opd_read_system_map(const char *filename);
void opd_alarm(int val);

void opd_handle_fork(const struct op_sample *sample);
void opd_handle_exec(u16 pid);
void opd_handle_exit(const struct op_sample *sample);
void opd_handle_mapping(const struct op_mapping *mapping);
void opd_clear_module_info(void);

#endif /* OPROFILED_H */
