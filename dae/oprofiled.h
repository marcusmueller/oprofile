/* $Id: oprofiled.h,v 1.38 2001/12/22 18:01:52 phil_e Exp $ */
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

/* list manipulation: come from the linux header, with some macro removed */
/* There is no real need to put this in ../util/misc.c */

/*
 * Simple doubly linked list implementation.
 *
 * Some of the internal functions ("__xxx") are useful when
 * manipulating whole lists rather than single entries, as
 * sometimes we already know the next/prev entries and we can
 * generate better code by using them directly rather than
 * using the generic single-entry routines.
 */

struct list_head {
	struct list_head *next, *prev;
};

/**
 * list_init - init a new entry
 * @ptr: the list to init
 *
 * Init a list head to create an empty list from it
 */
static __inline__ void list_init(struct list_head * ptr)
{
	ptr->next = ptr;
	ptr->prev = ptr;
}

/*
 * Insert a new entry between two known consecutive entries. 
 *
 * This is only for internal list manipulation where we know
 * the prev/next entries already!
 */
static __inline__ void __list_add(struct list_head * new,
	struct list_head * prev,
	struct list_head * next)
{
	next->prev = new;
	new->next = next;
	new->prev = prev;
	prev->next = new;
}

/**
 * list_add - add a new entry
 * @new: new entry to be added
 * @head: list head to add it after
 *
 * Insert a new entry after the specified head.
 * This is good for implementing stacks.
 */
static __inline__ void list_add(struct list_head *new, struct list_head *head)
{
	__list_add(new, head, head->next);
}

/**
 * list_add_tail - add a new entry
 * @new: new entry to be added
 * @head: list head to add it before
 *
 * Insert a new entry before the specified head.
 * This is useful for implementing queues.
 */
static __inline__ void list_add_tail(struct list_head *new, struct list_head *head)
{
	__list_add(new, head->prev, head);
}

/*
 * Delete a list entry by making the prev/next entries
 * point to each other.
 *
 * This is only for internal list manipulation where we know
 * the prev/next entries already!
 */
static __inline__ void __list_del(struct list_head * prev,
				  struct list_head * next)
{
	next->prev = prev;
	prev->next = next;
}

/**
 * list_del - deletes entry from list.
 * @entry: the element to delete from the list.
 * Note: list_empty on entry does not return true after this, the entry is in an undefined state.
 */
static __inline__ void list_del(struct list_head *entry)
{
	__list_del(entry->prev, entry->next);
}

/**
 * list_del_init - deletes entry from list and reinitialize it.
 * @entry: the element to delete from the list.
 */
static __inline__ void list_del_init(struct list_head *entry)
{
	__list_del(entry->prev, entry->next);
	list_init(entry);
}

/**
 * list_empty - tests whether a list is empty
 * @head: the list to test.
 */
static __inline__ int list_empty(struct list_head *head)
{
	return head->next == head;
}

/**
 * list_splice - join two lists
 * @list: the new list to add.
 * @head: the place to add it in the first list.
 */
static __inline__ void list_splice(struct list_head *list, struct list_head *head)
{
	struct list_head *first = list->next;

	if (first != list) {
		struct list_head *last = list->prev;
		struct list_head *at = head->next;

		first->prev = head;
		head->next = first;

		last->next = at;
		at->prev = last;
	}
}

/**
 * list_entry - get the struct for this entry
 * @ptr:	the &struct list_head pointer.
 * @type:	the type of the struct this is embedded in.
 * @member:	the name of the list_struct within the struct.
 */
#define list_entry(ptr, type, member) \
	((type *)((char *)(ptr)-(unsigned long)(&((type *)0)->member)))

/**
 * list_for_each - iterate over a list
 * @pos:	the &struct list_head to use as a loop counter.
 * @head:	the head for your list.
 */
#define list_for_each(pos, head) \
	for (pos = (head)->next; pos != (head); pos = pos->next)
        	
/**
 * list_for_each_safe - iterate over a list safe against removal of list entry
 * @pos:	the &struct list_head to use as a loop counter.
 * @n:		another &struct list_head to use as temporary storage
 * @head:	the head for your list.
 */
#define list_for_each_safe(pos, n, head) \
	for (pos = (head)->next, n = pos->next; pos != (head); \
		pos = n, n = pos->next)

/* end of list manipulation */

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
	OPD_MAPPING, /* nr. mappings occured */
	OPD_TRY_MAPPING, /* nr. mappings attempted */
	OPD_MAX_STATS /* end of stats */
	};

struct opd_sample_file {
	fd_t fd;
	/* mapped memory begin here */
	struct opd_header *header;
	/* start + sizeof(header) ie. begin of map of samples */
	void *start;
	/* allow to differenciate a first open from a reopen */
	int opened;
	/* the lru of all sample file */
	struct list_head lru_node;
	/* NOT counted the size of header, to allow quick access check  */
	/* This field is also present in opd_image and duplicated here
	 * to allow umapping without knowing what is the image of this samples
	 * files (see opd_open_sample_file) */
	off_t len;
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
	/* NOT counted the size of header, to allow quick access check  */
	off_t len;
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

void opd_handle_fork(const struct op_note *note);
void opd_handle_exec(u16 pid);
void opd_handle_exit(const struct op_note *note);
void opd_handle_mapping(const struct op_note *note);
void opd_clear_module_info(void);

#endif /* OPROFILED_H */
