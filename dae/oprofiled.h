/* $Id: oprofiled.h,v 1.36 2001/12/06 21:17:51 phil_e Exp $ */
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
#include "p_module.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/poll.h>
#include <sys/mman.h>

#include "opd_util.h"
#include "../op_user.h"

/* various defines */

/*#define OPD_DEBUG*/

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

#define streq(a,b) (!strcmp((a), (b)))
#define streqn(a,b,n) (!strncmp((a), (b), (n)))

/* maximum nr. of kernel modules */
#define OPD_MAX_MODULES 64

/* size of process hash table */
#define OPD_MAX_PROC_HASH 1024

enum {  OPD_KERNEL, /* nr. kernel samples */
	OPD_MODULE, /* nr. module samples */
	OPD_LOST_MODULE, /* nr. samples in module for which modules can not be located */
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

struct opd_sample_file {
	fd_t fd;
	/* mapped memory begin here */
	struct opd_header *header;
	/* start + sizeof(header) ie. begin of map of samples */
	void *start;
	/* the size of mapped memory comes from the opd_image */
};

struct opd_image {
	struct opd_sample_file sample_files[OP_MAX_COUNTERS];
	int hash;
	/* NOT counted the size of header, to allow quick access check  */
	off_t len;
	time_t mtime;	/* image file mtime */
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

void opd_get_ascii_procs(void);
void opd_init_images(void);
void opd_put_sample(const struct op_sample *sample);
void opd_read_system_map(const char *filename);
void opd_alarm(int val);

void opd_handle_fork(const struct op_note *note);
void opd_handle_exec(u16 pid);
void opd_handle_exit(const struct op_note *note);
void opd_handle_mapping(const struct op_note *note);
void opd_clear_module_info(void);

#endif /* OPROFILED_H */
