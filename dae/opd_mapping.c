/**
 * @file opd_mapping.c
 * Management of process mappings
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author John Levon
 * @author Philippe Elie
 */

#include "opd_mapping.h"
#include "opd_proc.h"
#include "opd_image.h"
#include "opd_printf.h"

#include "op_interface.h"
#include "op_config.h"
#include "op_libiberty.h"

#include <sys/mman.h>
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/* per-process */
#define OPD_DEFAULT_MAPS 16
#define OPD_MAP_INC 8

/* hash map device mmap */
static struct op_hash_index * hashmap;

/**
 * opd_init_hash_map - initialise the hashmap
 */
void opd_init_hash_map(void)
{
	extern fd_t hashmapdevfd;

	hashmap = mmap(0, OP_HASH_MAP_SIZE, PROT_READ, MAP_SHARED, hashmapdevfd, 0);
	if ((long)hashmap == -1) {
		perror("oprofiled: couldn't mmap hash map");
		exit(EXIT_FAILURE);
	}

}


/**
 * opd_init_maps - initialise map structure for a process
 * @param proc  process to work on
 *
 * Initialise the mapping info structure for process @proc.
 */
void opd_init_maps(struct opd_proc * proc)
{
	proc->maps = xcalloc(sizeof(struct opd_map), OPD_DEFAULT_MAPS);
	proc->max_nr_maps = OPD_DEFAULT_MAPS;
	proc->nr_maps = 0;
	proc->last_map = 0;
}


/**
 * opd_grow_maps - grow map structure for a process
 * @param proc  process to work on
 *
 * Grow the map structure for a process by %OPD_MAP_INC
 * entries. (FIXME: should be static)
 */
void opd_grow_maps(struct opd_proc * proc)
{
	proc->maps = xrealloc(proc->maps, sizeof(struct opd_map)*(proc->max_nr_maps+OPD_MAP_INC));
	proc->max_nr_maps += OPD_MAP_INC;
}


/**
 * opd_kill_maps - delete mapping information for a process
 * @param proc  process to work on
 *
 * Frees structures holding mapping information and resets
 * the values, allocating a new map structure.
 */
void opd_kill_maps(struct opd_proc * proc)
{
	if (proc->maps)
		free(proc->maps);
	opd_init_maps(proc);
}


/**
 * opd_put_mapping - add a mapping to a process
 * @param proc  process to add map to
 * @param image  mapped image pointer
 * @param start  start of mapping
 * @param offset  file offset of mapping
 * @param end  end of mapping
 *
 * Add the mapping specified to the process @proc.
 */
static void opd_put_mapping(struct opd_proc * proc, struct opd_image * image,
	unsigned long start, unsigned long offset, unsigned long end)
{
	verbprintf("Placing mapping for process %d: 0x%.8lx-0x%.8lx, off 0x%.8lx, \"%s\" at maps pos %d\n",
		proc->pid, start, end, offset, image->name, proc->nr_maps);

	opd_check_image_mtime(image);

	proc->maps[proc->nr_maps].image = image;
	proc->maps[proc->nr_maps].start = start;
	proc->maps[proc->nr_maps].offset = offset;
	proc->maps[proc->nr_maps].end = end;

	if (++proc->nr_maps == proc->max_nr_maps)
		opd_grow_maps(proc);

	/* we reset last map here to force searching backwards */
	proc->last_map = 0;
}


/**
 * get_from_pool - retrieve string from hash map pool
 * @param ind index into pool
 */
inline static char * get_from_pool(uint ind)
{
	return ((char *)(hashmap + OP_HASH_MAP_NR) + ind);
}


/**
 * opd_handle_hashmap - parse image from kernel hash map
 * @param hash hash value
 * @param app_name the application name which belongs this image
 *
 * Finds an image from its name.
 */
static struct opd_image * opd_handle_hashmap(int hash, char const * app_name)
{
	char file[PATH_MAX];
	char * c = &file[PATH_MAX-1];
	int orighash = hash;

	*c = '\0';
	while (hash) {
		char * name = get_from_pool(hashmap[hash].name);

		if (strlen(name) + 1 + strlen(c) >= PATH_MAX) {
			fprintf(stderr,"String \"%s\" too large.\n", c);
			exit(EXIT_FAILURE);
		}

		c -= strlen(name) + 1;
		*c = '/';
		strncpy(c + 1, name, strlen(name));

		/* move onto parent */
		hash = hashmap[hash].parent;
	}
	return opd_get_image(c, orighash, app_name, 0);
}


/**
 * opd_handle_mapping - deal with mapping notification
 * @param note  mapping notification
 *
 * Deal with one notification that a process has mapped
 * in a new executable file. The mapping information is
 * added to the process structure.
 */
void opd_handle_mapping(struct op_note const * note)
{
	struct opd_proc * proc;
	struct opd_image * image;
	int hash;
	char const * app_name;

	proc = opd_get_proc(note->pid);

	if (!proc) {
		verbprintf("Told about mapping for non-existent process %u.\n", note->pid);
		proc = opd_add_proc(note->pid);
	}

	hash = note->hash;

	if (hash == -1) {
		/* possibly deleted file */
		return;
	}

	if (hash < 0 || hash >= OP_HASH_MAP_NR) {
		fprintf(stderr,"hash value %u out of range.\n",hash);
		return;
	}

	app_name = opd_app_name(proc);

	image = opd_get_image_by_hash(hash, app_name);
	if (image == NULL)
		image = opd_handle_hashmap(hash, app_name);

	opd_put_mapping(proc, image, note->addr, note->offset, note->addr + note->len);
}
