/**
 * @file opd_parse_proc.c
 * Parsing of /proc/#pid
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author John Levon
 * @author Philippe Elie
 */

#include "op_libiberty.h"

#include "opd_parse_proc.h"
#include "opd_proc.h"
#include "opd_mapping.h"
#include "opd_image.h"
#include "opd_printf.h"

#include "op_file.h"
#include "op_fileio.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * opd_add_ascii_map - parse an ASCII map string for a process
 * @param proc  process to add map to
 * @param line  0-terminated ASCII string
 * @param image_name the binary application name
 *
 * Attempt to parse the string @line for map information
 * and add the info to the process @proc. Returns %1
 * on success, %0 otherwise.
 *
 * The parsing is based on Linux 2.4 format, which looks like this :
 *
 * 4001e000-400fc000 r-xp 00000000 03:04 31011      /lib/libc-2.1.2.so
 */
/* FIXME: handle (deleted) */
static int opd_add_ascii_map(struct opd_proc * proc, char const * line,
			     char * const image_name)
{
	struct opd_map * map = &proc->maps[proc->nr_maps];
	char const * cp = line;

	/* skip to protection field */
	while (*cp && *cp != ' ')
		cp++;

	/* handle rwx */
	if (!*cp || (!*(++cp)) || (!*(++cp)) || (*(++cp) != 'x'))
		return 0;

	/* get start and end from "40000000-4001f000" */
	if (sscanf(line, "%lx-%lx", &map->start, &map->end) != 2)
		return 0;

	/* "p " */
	cp += 2;

	/* read offset */
	if (sscanf(cp, "%lx", &map->offset) != 1)
		return 0;

	while (*cp && *cp != '/')
		cp++;

	if (!*cp)
		return 0;

	map->image = opd_get_image(cp, -1, image_name, 0);

	if (!map->image)
		return 0;

	if (++proc->nr_maps == proc->max_nr_maps)
		opd_grow_maps(proc);

	return 1;
}


/**
 * opd_get_ascii_maps - read all maps for a process
 * @param proc  process to work on
 *
 * Read the /proc/<pid>/maps file and add all
 * mapping information found to the process @proc.
 */
static void opd_get_ascii_maps(struct opd_proc * proc)
{
	FILE * fp;
	char mapsfile[20] = "/proc/";
	char * line;
	char exe_name[20];
	char * image_name;
	unsigned int map_nr;

	snprintf(mapsfile + 6, 6, "%hu", proc->pid);

	strcpy(exe_name, mapsfile);

	strcat(mapsfile,"/maps");

	fp = op_try_open_file(mapsfile, "r");
	if (!fp)
		return;

	strcat(exe_name, "/exe");
	image_name = op_get_link(exe_name);
	if (!image_name)
		/* FIXME likely to be kernel thread, actually we don't use them
		 * because samples go to vmlinux samples file but for
		 * completeness we record them in proc struct */
		image_name = xstrdup(exe_name);

	verbprintf("image name %s for pid %u\n", image_name, proc->pid);

	while (1) {
		line = op_get_line(fp);
		if (!strcmp(line, "") && feof(fp)) {
			free(line);
			break;
		} else {
			opd_add_ascii_map(proc, line, image_name);
			free(line);
		}
	}

	/* dae assume than maps[0] is the primary image name, this is always
	 * true at exec time but not for /proc/pid so reorder maps if necessary
	 */
	for (map_nr = 0 ; map_nr < proc->nr_maps ; ++map_nr) {
		if (!strcmp(proc->maps[map_nr].image->name, image_name)) {
			if (map_nr != 0) {
				struct opd_map temp = proc->maps[0];
				proc->maps[0] = proc->maps[map_nr];
				proc->maps[map_nr] = temp;

				fprintf(stderr, "swap map for image %s\n", image_name);
			}
			break;
		}
	}

	/* we can't free image_name because opd_add_ascii_map assume than
	 * image_name remains valid */

	op_close_file(fp);
}

/**
 * opd_get_ascii_procs - read process and mapping information from /proc
 *
 * Read information on each process and its mappings from the /proc
 * filesystem.
 */
void opd_get_ascii_procs(void)
{
	DIR * dir;
	struct dirent * dirent;
	struct opd_proc * proc;
	u16 pid;

	if (!(dir = opendir("/proc"))) {
		perror("oprofiled: /proc directory could not be opened. ");
		exit(EXIT_FAILURE);
	}

	while ((dirent = readdir(dir))) {
		if (sscanf(dirent->d_name, "%hu", &pid) == 1) {
			verbprintf("ASCII added %u\n", pid);
			proc = opd_add_proc(pid);
			opd_get_ascii_maps(proc);
		}
	}

	closedir(dir);
}
