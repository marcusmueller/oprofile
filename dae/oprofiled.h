/**
 * @file oprofiled.h
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 * 
 * @author John Levon <moz@compsoc.man.ac.uk>
 * @author Philippe Elie <phil_el@wanadoo.fr>
 */

#ifndef OPROFILED_H
#define OPROFILED_H

/* See objdump --section-headers /usr/src/linux/vmlinux */
/* used to catch out kernel samples (and also compute
   text offset if no System.map or module info is available */
#define KERNEL_VMA_OFFSET           0xc0100000

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

#include "op_libiberty.h"
#include "op_list.h"
#include "op_interface.h"
#include "op_hw_config.h"
#include "db.h"

/* 1 if we separate samples for shared lib */
extern int separate_samples;

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

extern uint op_nr_counters;
extern int verbose;
extern int kernel_only;
extern int separate_samples;
extern char * vmlinux;
extern unsigned long opd_stats[];

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
	/** data are stored here */
	db_tree_t tree;
};

struct opd_image {
	/* all image image are linked in a list through this member */
	struct list_head list_node;
	/* used to link image with a valid hash, we never destroy image so a
	 * simple link is necessary */
	struct opd_image * hash_next;
	struct opd_sample_file sample_files[OP_MAX_COUNTERS];
	int hash;
	/* the application name where belongs this image, NULL if image has
	 * no owner (such as wmlinux or module) */
	const char * app_name;
	time_t mtime;	/* image file mtime */
	u8 kernel;
	char *name;
};

/* kernel module */
struct opd_module {
	char *name;
	struct opd_image * image;
	u32 start;
	u32 end;
};

struct opd_map {
	struct opd_image * image;
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
	u16 accessed;
	int dead;
	struct opd_proc *prev;
	struct opd_proc *next;
};

void opd_get_ascii_procs(void);
void opd_init_images(void);
void opd_put_sample(const struct op_sample *sample);
void opd_read_system_map(const char *filename);
void opd_alarm(int val);
void opd_print_stats(void);
void opd_proc_cleanup(void);

void opd_handle_fork(const struct op_note *note);
void opd_handle_exec(u16 pid);
void opd_handle_exit(const struct op_note *note);
void opd_handle_mapping(const struct op_note *note);
void opd_clear_module_info(void);

#endif /* OPROFILED_H */
