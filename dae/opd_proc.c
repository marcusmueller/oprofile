/* $Id: opd_proc.c,v 1.83 2001/12/07 22:13:54 phil_e Exp $ */
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

#include "oprofiled.h"

#define OPD_DEFAULT_IMAGES 32
#define OPD_IMAGE_INC 16
/* per-process */
#define OPD_DEFAULT_MAPS 16
#define OPD_MAP_INC 8

extern int kernel_only;
extern int verbose;
extern uint op_nr_counters; 
extern unsigned long opd_stats[];
extern char *vmlinux;
extern char *smpdir;
extern struct op_hash_index *hashmap;

/* hash of process lists */
static struct opd_proc *opd_procs[OPD_MAX_PROC_HASH];

u32 ctr_count[OP_MAX_COUNTERS];
u8 ctr_event[OP_MAX_COUNTERS];
u8 ctr_um[OP_MAX_COUNTERS];
extern u32 cpu_type;
double cpu_speed;

/* image structure */
static struct opd_image *opd_images;
static unsigned int nr_images=0;
static unsigned int max_nr_images=OPD_DEFAULT_IMAGES;

/* kernel and module support */
static u32 kernel_start;
static u32 kernel_end;
static char got_system_map=0;
static struct opd_module opd_modules[OPD_MAX_MODULES];
static unsigned int nr_modules=0;

static struct opd_proc *opd_add_proc(u16 pid);
static void opd_grow_images(void);
static void opd_open_sample_file(struct opd_image *image, int counter);
static void opd_open_image(struct opd_image *image, int hash, const char *name, int kernl);
static int opd_find_image(const char *name, int hash);
static int opd_add_image(const char *name, int hash, int kernel);
static int opd_get_image(const char *name, int hash, int kernel);
static int opd_get_image_by_hash(int hash);
static void opd_init_maps(struct opd_proc *proc);
static void opd_grow_maps(struct opd_proc *proc);
static void opd_kill_maps(struct opd_proc *proc);
static void opd_put_mapping(struct opd_proc *proc, int image_nr, u32 start, u32 offset, u32 end);
static struct opd_proc *opd_get_proc(u16 pid);
static void opd_delete_proc(struct opd_proc *proc);
static void opd_handle_old_sample_file(const char * mangled, time_t mtime);
static void opd_handle_old_sample_files(const char * mangled, time_t mtime);

/* every so many minutes, clean up old procs, msync mmaps, and
   report stats */
void opd_alarm(int val __attribute__((unused)))
{
	struct opd_proc *proc;
	struct opd_proc *next;
	uint i;
	uint j;

	for (i = 0; i < nr_images; i++) {
		struct opd_image* image = &opd_images[i];
		for (j = 0 ; j < op_nr_counters ; ++j) {
			if (image->sample_files[j].fd > 1)
				msync(image->sample_files[j].header, image->len + sizeof(struct opd_header), MS_ASYNC);
		}
	}

	j = 0;
	for (i=0; i < OPD_MAX_PROC_HASH; i++) {
		proc = opd_procs[i];

		while (proc) {
			++j;
			next = proc->next;
			if (proc->dead) {
				proc->dead += proc->accessed;
				proc->accessed = 0;
				if (--proc->dead == 0)
					opd_delete_proc(proc);
			}
			proc=next;
		}
	}

	printf("%s\n", opd_get_time());
	printf("Nr. proc struct: %d\n", j);
	printf("Nr. kernel samples: %lu\n", opd_stats[OPD_KERNEL]);
	printf("Nr. modules samples: %lu\n", opd_stats[OPD_MODULE]);
	printf("Nr. modules samples lost: %lu\n", opd_stats[OPD_LOST_MODULE]);
	printf("Nr. samples lost due to no process information: %lu\n", opd_stats[OPD_LOST_PROCESS]);
	printf("Nr. process samples in user-space: %lu\n", opd_stats[OPD_PROCESS]);
	printf("Nr. samples lost due to no map information: %lu\n", opd_stats[OPD_LOST_MAP_PROCESS]);
	if (opd_stats[OPD_PROC_QUEUE_ACCESS]) {
	printf("Average depth of search of proc queue: %f\n",
		(double)opd_stats[OPD_PROC_QUEUE_DEPTH] / (double)opd_stats[OPD_PROC_QUEUE_ACCESS]);
	}
	if (opd_stats[OPD_MAP_ARRAY_ACCESS]) {
	printf("Average depth of iteration through mapping array: %f\n",
		(double)opd_stats[OPD_MAP_ARRAY_DEPTH] / (double)opd_stats[OPD_MAP_ARRAY_ACCESS]);
	}
	printf("Nr. sample dumps: %lu\n", opd_stats[OPD_DUMP_COUNT]);
	printf("Nr. samples total: %lu\n", opd_stats[OPD_SAMPLES]);
	printf("Nr. notifications: %lu\n", opd_stats[OPD_NOTIFICATIONS]);
	fflush(stdout);

	alarm(60*10);
}

/**
 * opd_eip_is_kernel - is the sample from kernel/module space
 * @eip: EIP value
 *
 * Returns %TRUE if @eip is in the address space starting at
 * %KERNEL_VMA_OFFSET, %FALSE otherwise.
 */
inline static int opd_eip_is_kernel(u32 eip)
{
	return (eip >= KERNEL_VMA_OFFSET);
}

/**
 * opd_read_system_map - parse System.map file
 * @filename: file name of System.map
 *
 * Parse the kernel's System.map file. If the filename is
 * passed as "", a warning is produced and the function returns.
 *
 * If the file is parsed correctly, the global variables
 * kernel_start and kernel_end are set to the correct values for the
 * text section of the mainline kernel, and the global got_system_map
 * is set to %TRUE.
 *
 * Note that kernel modules will have EIP values above the value of
 * kernel_end.
 */
void opd_read_system_map(const char *filename)
{
	FILE *fp;
	char *line;
	char *cp;

	fp = opd_open_file(filename, "r");

	do {
		line = opd_get_line(fp);
		if (streq(line, "")) {
			free(line);
			break;
		} else {
			if (strlen(line) < 11) {
				free(line);
				continue;
			}
			cp = line+11;
			if (streq("_text", cp))
				sscanf(line, "%x", &kernel_start);
			else if (streq("_end", cp))
				sscanf(line, "%x", &kernel_end);
			free(line);
		}
	} while (1);

	if (kernel_start && kernel_end)
		got_system_map = TRUE;

	opd_close_file(fp);
}

/**
 * opd_handle_old_sample_file - deal with old sample file
 * @mangled: the sample file name
 * @mtime: the new mtime of the binary
 *
 * If an old sample file exists, verify it is usable.
 * If not, move or delete it. Note than at startup the daemon
 * check than the last (session) events settings match the
 * currents
 */
static void opd_handle_old_sample_file(const char * mangled, time_t mtime)
{
	struct opd_header oldheader; 
	FILE * fp;

	fp = fopen(mangled, "r"); 
	if (!fp)
		goto del;

	if (fread(&oldheader, sizeof(struct opd_header), 1, fp) != 1)
		goto closedel;

	if (memcmp(&oldheader.magic, OPD_MAGIC, sizeof(oldheader.magic)) || oldheader.version != OPD_VERSION)
		goto closedel;

	if (difftime(mtime, oldheader.mtime))
		goto closedel;

	fclose(fp);
	verbprintf("Re-using old sample file \"%s\".\n", mangled);
	return;
 
closedel:
	fclose(fp);
del:
	verbprintf("Deleting old sample file \"%s\".\n", mangled);
	unlink(mangled);
}
  
 
/**
 * opd_handle_old_sample_files - deal with old sample files
 * @image_name: image to open files for
 * @mtime: the new mtime of the binary
 *
 * to simplify admin of sample file we rename or remove sample
 * files for each counter.
 *
 * If an old sample file exists, verify it is usable.
 * If not, move or delete it.
 */
static void opd_handle_old_sample_files(const char *image_name, time_t mtime)
{
	uint i;
	char *mangled;
	uint len;

	mangled = opd_mangle_filename(smpdir, image_name);

	len = strlen(mangled);
 
	for (i = 0 ; i < op_nr_counters ; ++i) {
		sprintf(mangled + len, "#%d", i);
		opd_handle_old_sample_file(mangled,  mtime);
	}

	free(mangled);
}
 
/**
 * opd_open_image - open an image sample file
 * @image: image to open file for
 * @hash: hash of image
 * @name: name of the image to add
 * @kernel: is the image a kernel/module image
 *
 * @image at funtion entry is uninitialised
 * @name is copied i.e. should be GC'd separately from the
 * image structure if appropriate.
 *
 * Initialise an opd_image struct for the image @image
 * without opening the associated samples files. At return
 * the @image is fully initialized.
 */
static void opd_open_image(struct opd_image *image, int hash, const char *name, int kernel)
{
	uint i;

	image->name = xstrdup(name);
	image->kernel = kernel;
	image->hash = hash; 
	image->len = 0;

	for (i = 0 ; i < op_nr_counters ; ++i) {
		image->sample_files[i].fd = -1;
		image->sample_files[i].start = (void *)-1;
		image->sample_files[i].header = (void *)-1;
	}

	verbprintf("Getting size of \"%s\"\n", name);

	/* for each byte in original one counter */
	image->len = opd_get_fsize(name, 0) * sizeof(struct opd_fentry);
	
	if (!image->len) {
		verbprintf("Size check failed for %s\n", name);
		return;
	}

	image->mtime = opd_get_mtime(name);
 
	/* give space for "negative" entries. This is because we
	 * don't know about kernel/module sections other than .text so
	 * a sample could be before our nominal start of image, or
	 * after the start */
	if (image->kernel)
		image->len += OPD_KERNEL_OFFSET * sizeof(struct opd_fentry);

	opd_handle_old_sample_files(name, image->mtime);

	/* samples files are lazily openeded */
}

/**
 * opd_get_count - retrieve counter value
 * @count: raw counter value
 *
 * Returns the counter value.
 */
inline static u16 opd_get_count(const u16 count)
{
	return (count & OP_COUNT_MASK);
}

/**
 * opd_get_counter - retrieve counter type
 * @count: raw counter value
 *
 * Returns positive for counter 1, zero for counter 0.
 */
inline static u16 opd_get_counter(const u16 count)
{
	return OP_COUNTER(count);
}

/*
 * opd_open_sample_file - open an image sample file
 * @image: image to open file for
 * @counter: counter number
 *
 * Open image sample file for the image @image, counter
 * @counter and set up memory mappings for it.
 * image->kernel and image->name must have meaningful
 * values.
 */
static void opd_open_sample_file(struct opd_image *image, int counter)
{
	char* mangled;
	struct opd_sample_file *sample_file;

	sample_file = &image->sample_files[counter];

	/* avoid flood of error messages */
	if (sample_file->fd == -2)
		return;

	mangled = opd_mangle_filename(smpdir, image->name);

	sprintf(mangled + strlen(mangled), "#%d", counter);

	verbprintf("Opening \"%s\"\n", mangled);

	sample_file->fd = open(mangled, O_CREAT|O_RDWR, 0644);
	if (sample_file->fd == -1) {
		fprintf(stderr,"oprofiled: open of image sample file \"%s\" failed: %s\n", mangled, strerror(errno));
		goto err1;
	}

	/* truncate to grow the file is ok on linux */
	if (ftruncate(sample_file->fd, image->len + sizeof(struct opd_header)) == -1) {
		fprintf(stderr, "oprofiled: ftruncate failed for \"%s\". %s\n", mangled, strerror(errno));
		goto err2;
	}

	sample_file->header = mmap(0, image->len + sizeof(struct opd_header),
		PROT_READ|PROT_WRITE, MAP_SHARED, sample_file->fd, 0);

	if (sample_file->header == (void *)-1) {
		fprintf(stderr,"oprofiled: mmap of image sample file \"%s\" failed: %s\n", mangled, strerror(errno));
		goto err2;
	}

	sample_file->start = sample_file->header + 1;

	memset(sample_file->header, '\0', sizeof(struct opd_header));
	sample_file->header->version = OPD_VERSION;
	memcpy(&sample_file->header->magic, OPD_MAGIC, sizeof(sample_file->header->magic));
	sample_file->header->is_kernel = image->kernel;
	sample_file->header->ctr_event = ctr_event[counter];
	sample_file->header->ctr_um = ctr_um[counter];
	sample_file->header->ctr = counter;
	sample_file->header->cpu_type = cpu_type;
	sample_file->header->ctr_count = ctr_count[counter];
	sample_file->header->cpu_speed = cpu_speed;
	sample_file->header->mtime = image->mtime;
out:
	free(mangled);
	return;
err2:
	close(sample_file->fd);
err1:
	/* avoid flood of error messages */
	sample_file->fd = -2;
	goto out;
}

 
/**
 * opd_check_image_mtime - ensure samples file is up to date
 * @image: image to check
 */
static void opd_check_image_mtime(struct opd_image * image)
{
	uint i;
	char *mangled;
	uint len;
	char * tmp = image->name;
	time_t newmtime = opd_get_mtime(image->name);
 
	if (image->mtime == newmtime)
		return;
 
	verbprintf("Current mtime %lu differs from stored "
		"mtime %lu for %s\n", newmtime, image->mtime, image->name);

	mangled = opd_mangle_filename(smpdir, image->name);
	len = strlen(mangled);

	for (i=0; i < op_nr_counters; i++) {
		struct opd_sample_file * file = &image->sample_files[i]; 
		if (file->fd > 0) {
			close(file->fd);
			munmap(file->header, image->len + sizeof(struct opd_header));
		}
		sprintf(mangled + len, "#%d", i);
		verbprintf("Deleting out of date \"%s\"\n", mangled);
		unlink(mangled);
	}
	free(mangled);

	opd_open_image(image, image->hash, tmp, image->kernel);
	free(tmp);
}


/**
 * opd_put_image_sample - write sample to file
 * @image: image for sample
 * @offset: (file) offset to write to
 * @count: raw counter value
 *
 * Add to the count stored at position @offset in the
 * image file. Overflow pins the count at the maximum
 * value.
 *
 * If the image is a kernel or module image, the position
 * is further offset by %OPD_KERNEL_OFFSET.
 *
 * @count is the raw value passed from the kernel.
 */
inline static void opd_put_image_sample(struct opd_image *image, u32 offset, u16 count)
{
	struct opd_fentry *fentry;
	struct opd_sample_file* sample_file;
	int counter;

	counter = opd_get_counter(count);
	sample_file = &image->sample_files[counter];

	if (sample_file->fd < 1) {
		opd_open_sample_file(image, counter);
		if (sample_file->fd < 1) {
			/* opd_open_sample_file output an error message */
			return;
		}
	}

	fentry = sample_file->start + (offset*sizeof(struct opd_fentry));

	if (image->kernel)
		fentry += OPD_KERNEL_OFFSET;

	if (((u32)fentry) > ((u32)(sample_file->start + image->len))) {
		fprintf(stderr, "fentry %p out of bounds for \"%s\" (start %p, len 0x%.8lx, "
			"end %p, orig offset 0x%.8x, kernel %d)\n", fentry, image->name, sample_file->start,
			image->len, sample_file->start + image->len, offset, image->kernel);
		return;
	}

	if (fentry->count + opd_get_count(count) < fentry->count)
		fentry->count = (u32)-1;
	else
		fentry->count += opd_get_count(count);
}


/**
 * opd_init_images - initialise image structure
 *
 * Initialise the global image structure, reserving the
 * first entry for the kernel.
 */
void opd_init_images(void)
{
	/* 0 is reserved for the kernel image */
	opd_images = xcalloc(sizeof(struct opd_image), OPD_DEFAULT_IMAGES);

	opd_images[0].name = xstrdup(vmlinux);
	opd_images[0].kernel = 1;

	opd_open_image(&opd_images[0], -1, vmlinux, 1);
	nr_images = 1;
}

/**
 * opd_grow_images - grow the image structure
 *
 * Grow the global image structure by %OPD_IMAGE_INC entries.
 */
static void opd_grow_images(void)
{
	opd_images = xrealloc(opd_images, sizeof(struct opd_image)*(max_nr_images+OPD_IMAGE_INC));
	max_nr_images += OPD_IMAGE_INC;
}

/**
 * bstreq - reverse string comparison
 * @str1: first string
 * @str2: second string
 *
 * Compares two strings, starting at the end.
 * Returns %1 if they match, %0 otherwise.
 */
inline static int bstreq(const char *str1, const char *str2)
{
	char *a = (char *)str1;
	char *b = (char *)str2;

	while (*a && *b)
		a++,b++;

	if (*a || *b)
		return 0;

	while (a!=str1) {
		if (*b-- != *a--)
			return 0;
	}
	return 1;
}

/**
 * opd_find_image - find an image
 * @name: name of image to find
 * @hash: hash of image to find
 *
 * Returns the image number for the file specified by @name, or -1.
 */
static int opd_find_image(const char * name, int hash)
{
	unsigned int i;

	/* we can have hashless images from /proc/pid parsing */
	for (i=1; i < nr_images; i++) {
		if (bstreq(opd_images[i].name, name)) {
			if (hash != -1)
				opd_images[i].hash = hash;
			return i;
		}
	}

	return -1;
}

/**
 * opd_add_image - add an image to the image structure
 * @name: name of the image to add
 * @hash: hash of image
 * @kernel: is the image a kernel/module image
 *
 * Add an image to the image structure with name @name.
 *
 * @name is copied i.e. should be GC'd separately from the
 * image structure if appropriate.
 *
 * The image number is returned.
 */
static int opd_add_image(const char * name, int hash, int kernel)
{
	opd_open_image(&opd_images[nr_images], hash, name, kernel);
	nr_images++;

	if (nr_images == max_nr_images)
		opd_grow_images();

	return nr_images-1;
}

/**
 * opd_get_image_by_hash - get an image from the image structure by hash value
 * @hash: hash of the image to get
 *
 * Get the image specified by the hash @hash if present, else return -1
 */
static int opd_get_image_by_hash(int hash)
{
	unsigned int i;

	for (i=1; i < nr_images; i++) {
		if (opd_images[i].hash == hash)
			return i;
	}
	return -1;
}


/**
 * opd_get_image - get an image from the image structure
 * @name: name of image
 * @hash: hash of the image to get
 * @kernel: is the image a kernel/module image
 *
 * Get the image specified by the file name @name from the
 * image structure. If it is not present, the image is
 * added to the structure. In either case, the image number
 * is returned.
 */
static int opd_get_image(const char * name, int hash, int kernel)
{
	int image_nr;
	if ((image_nr = opd_find_image(name, hash)) == -1)
		image_nr = opd_add_image(name, hash, kernel);

	return image_nr;
}


/**
 * opd_new_proc - create a new process structure
 * @prev: previous list entry
 * @next: next list entry
 *
 * Allocate and initialise a process structure and insert
 * it into the the list point specified by @prev and @next.
 */
static struct opd_proc * opd_new_proc(struct opd_proc *prev, struct opd_proc *next)
{
	struct opd_proc *proc;

	proc = xmalloc(sizeof(struct opd_proc));
	proc->maps = NULL;
	proc->pid = 0;
	proc->nr_maps = 0;
	proc->max_nr_maps = 0;
	proc->last_map = 0;
	proc->dead = 0;
	proc->accessed = 0;
	proc->prev = prev;
	proc->next = next;
	return proc;
}

/**
 * proc_hash - hash pid value
 * @pid: pid value to hash
 *
 */
inline static uint proc_hash(u16 pid)
{
	return ((pid>>4) ^ (pid)) % OPD_MAX_PROC_HASH;
}

/**
 * opd_delete_proc - delete a process
 * @proc: process to delete
 *
 * Remove the process @proc from the process list and free
 * the associated structures.
 */
static void opd_delete_proc(struct opd_proc *proc)
{
	if (!proc->prev)
		opd_procs[proc_hash(proc->pid)] = proc->next;
	else
		proc->prev->next = proc->next;

	if (proc->next)
		proc->next->prev = proc->prev;
	
	if (proc->maps) free(proc->maps);
	free(proc);
}

/**
 * opd_init_maps - initialise map structure for a process
 * @proc: process to work on
 *
 * Initialise the mapping info structure for process @proc.
 */
static void opd_init_maps(struct opd_proc *proc)
{
	proc->maps = xcalloc(sizeof(struct opd_map), OPD_DEFAULT_MAPS);
	proc->max_nr_maps = OPD_DEFAULT_MAPS;
	proc->nr_maps = 0;
	proc->last_map = 0;
}

/**
 * opd_add_proc - add a process
 * @pid: process id
 *
 * Create a new process structure and add it
 * to the head of the process list. The process structure
 * is filled in as appropriate.
 *
 */
static struct opd_proc *opd_add_proc(u16 pid)
{
	struct opd_proc *proc;
	uint hash = proc_hash(pid);

	proc=opd_new_proc(NULL, opd_procs[hash]);
	if (opd_procs[hash])
		opd_procs[hash]->prev = proc;

	opd_procs[hash] = proc;

	opd_init_maps(proc);
	proc->pid = pid;

	return proc;
}

/**
 * opd_grow_maps - grow map structure for a process
 * @proc: process to work on
 *
 * Grow the map structure for a process by %OPD_MAP_INC
 * entries.
 */
static void opd_grow_maps(struct opd_proc *proc)
{
	proc->maps = xrealloc(proc->maps, sizeof(struct opd_map)*(proc->max_nr_maps+OPD_MAP_INC));
	proc->max_nr_maps += OPD_MAP_INC;
}

/**
 * opd_kill_maps - delete mapping information for a process
 * @proc: process to work on
 *
 * Frees structures holding mapping information and resets
 * the values, allocating a new map structure.
 */
static void opd_kill_maps(struct opd_proc *proc)
{
	if (proc->maps)
		free(proc->maps);
	proc->maps = NULL;
	proc->nr_maps = 0;
	proc->max_nr_maps = 0;
	proc->last_map = 0;
	opd_init_maps(proc);
}

/**
 * opd_do_proc_lru - rework process list
 * @head: head of process list
 * @proc: process to move
 *
 * Perform LRU on the process list by moving it to
 * the head of the process list.
 */
inline static void opd_do_proc_lru(struct opd_proc **head, struct opd_proc *proc)
{
	if (proc->prev) {
		proc->prev->next = proc->next;
		if (proc->next)
			proc->next->prev = proc->prev;
		(*head)->prev = proc;
		proc->prev = NULL;
		proc->next = *head;
		(*head) = proc;
	}
}

/**
 * opd_get_proc - get process from process list
 * @pid: pid to search for
 *
 * A process with pid @pid is searched on the process list,
 * maintaining LRU order. If it is not found, %NULL is returned,
 * otherwise the process structure is returned.
 */
static struct opd_proc *opd_get_proc(u16 pid)
{
	struct opd_proc *proc;

	proc = opd_procs[proc_hash(pid)];

	opd_stats[OPD_PROC_QUEUE_ACCESS]++;
	while (proc) {
		if (pid == proc->pid) {
			opd_do_proc_lru(&opd_procs[proc_hash(pid)],proc);
			return proc;
		}
		opd_stats[OPD_PROC_QUEUE_DEPTH]++;
		proc = proc->next;
	}

	return NULL;
}

/**
 * opd_is_in_map - check whether an EIP is within a mapping
 * @map: map to check
 * @eip: EIP value
 *
 * Return %TRUE if the EIP value @eip is within the boundaries
 * of the map @map, %FALSE otherwise.
 */
inline static int opd_is_in_map(struct opd_map *map, u32 eip)
{
	return (eip >= map->start && eip < map->end);
}

/**
 * opd_clear_module_info - clear kernel module information
 *
 * Clear and free all kernel module information and reset
 * values.
 */
void opd_clear_module_info(void)
{
	int i;

	for (i=0; i < OPD_MAX_MODULES; i++) {
		if (opd_modules[i].name)
			free(opd_modules[i].name);
		opd_modules[i].name = NULL;
		opd_modules[i].start = 0;
		opd_modules[i].end = 0;
	}
}

/**
 * opd_get_module - get module structure
 * @name: name of module image
 *
 * Find the module structure for module image @name.
 * If it could not be found, add the module to
 * the global module structure.
 *
 * If an existing module is found, @name is free()d.
 * Otherwise it must be freed when the module structure
 * is removed (i.e. in opd_clear_module_info()).
 */
static struct opd_module *opd_get_module(char *name)
{
	int i;

	for (i=0; i < OPD_MAX_MODULES; i++) {
		if (opd_modules[i].name && bstreq(name,opd_modules[i].name)) {
			/* free this copy */
			free(name);
			return &opd_modules[i];
		}
	}

	opd_modules[nr_modules].name = name;
	opd_modules[nr_modules].image = -1;
	opd_modules[nr_modules].start = 0;
	opd_modules[nr_modules].end = 0;
	nr_modules++;
	if (nr_modules == OPD_MAX_MODULES) {
		fprintf(stderr, "Exceeded %u kernel modules !\n", OPD_MAX_MODULES);
		exit(1);
	}

	return &opd_modules[nr_modules-1];
}

/**
 * opd_get_module_info - parse mapping information for kernel modules
 *
 * Parse the file /proc/ksyms to read in mapping information for
 * all kernel modules. The modutils package adds special symbols
 * to this file which allows determination of the module image
 * and mapping addresses of the form :
 *
 * __insmod_modulename_Oobjectfile_Mmtime_Vversion
 * __insmod_modulename_Ssectionname_Llength
 *
 * Currently the image file "objectfile" is stored, and details of
 * ".text" sections.
 *
 * There is no query_module API that allow to get directly the pathname
 * of a module so we need to parse all the /proc/ksyms.
 */
static void opd_get_module_info(void)
{
	char *line;
	char *cp, *cp2, *cp3;
	FILE *fp;
	struct opd_module *mod;
	char *modname;
	char *filename;

	nr_modules=0;

	fp = opd_try_open_file("/proc/ksyms", "r");

	if (!fp) {	
		printf("oprofiled: /proc/ksyms not readable, can't process module samples.\n");
		return;
	}

	do {
		line = opd_get_line(fp);
		if (streq("", line) && !feof(fp)) {
			free(line);
			continue;
		} else if (streq("",line))
			goto failure;

		if (strlen(line) < 9) {
			printf("oprofiled: corrupt /proc/ksyms line \"%s\"\n", line);
			goto failure;
		}

		if (strncmp("__insmod_", line+9, 9)) {
			free(line);
			continue;
		}

		cp = line + 18;
		cp2 = cp;
		while ((*cp2) && !streqn("_S", cp2+1, 2) && !streqn("_O", cp2+1, 2))
			cp2++;

		if (!*cp2) {
			printf("oprofiled: corrupt /proc/ksyms line \"%s\"\n", line);
			goto failure;
		}
	
		cp2++;
		/* freed by opd_clear_module_info() or opd_get_module() */
		modname = xmalloc((size_t)((cp2-cp) + 1));
		strncpy(modname, cp, (size_t)((cp2-cp)));
		modname[cp2-cp] = '\0';

		mod = opd_get_module(modname);

		switch (*(++cp2)) {
			case 'O':
				/* get filename */
				cp2++;
				cp3 = cp2;

				while ((*cp3) && !streqn("_M", cp3+1, 2))
					cp3++;

				if (!*cp3) {
					free(line);
					continue;
				}
				
				cp3++;
				filename = xmalloc((size_t)(cp3 - cp2 + 1));
				strncpy(filename, cp2, (size_t)(cp3 - cp2));
				filename[cp3-cp2] = '\0';

				mod->image = opd_get_image(filename, -1, 1);
				free(filename);
				break;

			case 'S':
				/* get extent of .text section */
				cp2++;
				if (strncmp(".text_L", cp2, 7)) {
					free(line);
					continue;
				}

				cp2 += 7;
				sscanf(line,"%x", &mod->start);
				sscanf(cp2,"%u", &mod->end);
				mod->end += mod->start;
				break;
		}

		free(line);
	} while (1);

failure:
	free(line);
	opd_close_file(fp);
}

/**
 * opd_enter_invalid_module - create a negative module entry
 * @name: name of module
 * @info: module info
 */
static void opd_enter_invalid_module(char const * name, struct module_info * info)
{
	opd_modules[nr_modules].name = xstrdup(name);
	opd_modules[nr_modules].image = -1;
	opd_modules[nr_modules].start = info->addr;
	opd_modules[nr_modules].end = info->addr + info->size;
	nr_modules++;
	if (nr_modules == OPD_MAX_MODULES) {
		fprintf(stderr, "Exceeded %u kernel modules !\n", OPD_MAX_MODULES);
		exit(1);
	}
}

/**
 * opd_drop_module_sample - drop a module sample efficiently
 * @eip: eip of sample
 */
static void opd_drop_module_sample(u32 eip)
{
	char * module_names;
	char * name;
	size_t size = 1024;
	size_t ret;
	uint nr_mods;
	uint mod = 0;
	
	module_names = xmalloc(size);
	while (query_module(NULL, QM_MODULES, module_names, size, &ret)) {
		if (errno != ENOSPC) {
			verbprintf("query_module failed: %s\n", strerror(errno)); 
			return;
		}
		size = ret;
		module_names = xrealloc(module_names, size);
	}

	nr_mods = ret;
	name = module_names;

	while (mod < nr_mods) {
		struct module_info info;
		if (!query_module(name, QM_INFO, &info, sizeof(info), &ret)) {
			if (eip >= info.addr && eip < info.addr + info.size) {
				verbprintf("Sample from unprofilable module %s\n", name);
				opd_enter_invalid_module(name, &info);
				goto out;
			}
		}
		mod++;
		name += strlen(name) + 1;
	}
out:
	if (module_names) free(module_names);
}

/**
 * opd_find_module_by_eip - find a module by its eip
 * @eip: EIP value
 *
 * find in the modules container the module which
 * contain this @eip return %NULL if not found.
 * caller must check than the module image is valid
 */
static struct opd_module * opd_find_module_by_eip(u32 eip)
{
	uint i;
	for (i = 0; i < nr_modules; i++) {
		if (opd_modules[i].start && opd_modules[i].end &&
		    opd_modules[i].start <= eip && opd_modules[i].end > eip)
			return &opd_modules[i];
	}

	return NULL;
}

/**
 * opd_handle_module_sample - process a module sample
 * @eip: EIP value
 * @count: count value of sample
 *
 * Process a sample in module address space. The sample @eip
 * is matched against module information. If the search was
 * successful, the sample is output to the relevant file.
 *
 * Note that for modules and the kernel, the offset will be
 * wrong in the file, as it is not a file offset, but the offset
 * from the text section. This is fixed up in pp.
 *
 * If the sample could not be located in a module, it is treated
 * as a kernel sample.
 */
static void opd_handle_module_sample(u32 eip, u16 count)
{
	struct opd_module * module;

	module = opd_find_module_by_eip(eip);
	if (!module) {
		/* not found in known modules, re-read our info and retry */
		opd_clear_module_info();
		opd_get_module_info();

		module = opd_find_module_by_eip(eip);
	}

	if (module) {
		if (module->image != -1) {
			opd_stats[OPD_MODULE]++;
			opd_put_image_sample(&opd_images[module->image],
					     eip - module->start, count);
		}
		else {
			opd_stats[OPD_LOST_MODULE]++;
			verbprintf("No image for sampled module %s\n",
				   module->name);
		}
	} else {
		/* ok, we failed to place the sample even after re-reading
		 * /proc/ksyms. It's either a rogue sample, or from a module
		 * that didn't create symbols (like in some initrd setups).
		 * So we check with query_module() if we can place it in a
		 * symbol-less module, and if so create a negative entry for
		 * it, to quickly ignore future samples.
		 *
		 * Problem uncovered by Bob Montgomery <bobm@fc.hp.com>
		 */
		opd_stats[OPD_LOST_MODULE]++;
		opd_drop_module_sample(eip);
	}
}

/**
 * opd_handle_kernel_sample - process a kernel sample
 * @eip: EIP value of sample
 * @count: count value of sample
 *
 * Handle a sample in kernel address space or in a module. The sample is
 * output to the relevant image file.
 *
 * This function requires the global variable
 * got_system_map to be %TRUE to handle module samples.
 */
static void opd_handle_kernel_sample(u32 eip, u16 count)
{
	if (got_system_map) {
		if (eip < kernel_end) {
			opd_stats[OPD_KERNEL]++;
			opd_put_image_sample(&opd_images[0], eip - kernel_start, count);
			return;
		}

		/* in a module */
		opd_handle_module_sample(eip, count);
		return;
	}

	opd_stats[OPD_KERNEL]++;
	opd_put_image_sample(&opd_images[0], eip - KERNEL_VMA_OFFSET, count);
	return;
}

/**
 * verb_show_sample - print the sample out to the log
 * @offset: the offset value
 * @map: map to print
 */
inline static void verb_show_sample(u32 offset, struct opd_map *map)
{
	if (!verbose) 
		return;

	printf("DO_PUT_SAMPLE (LAST_MAP): calc offset 0x%.8x, map start 0x%.8x,"
		" end 0x%.8x, offset 0x%.8x, name \"%s\"\n",
		offset, map->start, map->end, map->offset, opd_images[map->image].name);
}

/**
 * opd_map_offset - return offset of sample against map
 * @map: map to use
 * @eip: EIP value to use
 *
 * Returns the offset of the EIP value @eip into
 * the map @map, which is the same as the file offset
 * for the relevant binary image.
 */
inline static u32 opd_map_offset(struct opd_map *map, u32 eip)
{
	return (eip - map->start) + map->offset;
}

/**
 * opd_put_sample - process a sample
 * @sample: sample to process
 *
 * Write out the sample to the appropriate sample file. This
 * routine handles kernel and module samples as well as ordinary ones.
 */
void opd_put_sample(const struct op_sample *sample)
{
	unsigned int i;
	struct opd_proc *proc;

	opd_stats[OPD_SAMPLES]++;

	verbprintf("DO_PUT_SAMPLE: c%d, EIP 0x%.8x, pid %.6d, count %.6d\n", 
		opd_get_counter(sample->count), sample->eip, sample->pid, sample->count);

	if (opd_eip_is_kernel(sample->eip)) {
		opd_handle_kernel_sample(sample->eip, sample->count);
		return;
	}

	if (kernel_only)
		return;

	/* here we don't want to add the new process because we don't know if it
	 * was execve()d or a thread
	 */
	if (!(proc = opd_get_proc(sample->pid))) {
		verbprintf("No proc info for pid %.6d.\n", sample->pid);
		opd_stats[OPD_LOST_PROCESS]++;
		return;
	}

	proc->accessed = 1;

	opd_stats[OPD_MAP_ARRAY_ACCESS]++;
	if (opd_is_in_map(&proc->maps[proc->last_map], sample->eip)) {
		i = proc->last_map;
		if (proc->maps[i].image != -1) {
			verb_show_sample(opd_map_offset(&proc->maps[i], sample->eip), &proc->maps[i]); 
			opd_put_image_sample(&opd_images[proc->maps[i].image], opd_map_offset(&proc->maps[i], sample->eip), sample->count);
		}
		opd_stats[OPD_PROCESS]++;
		return;
	}

	/* look for which map and find offset */
	for (i=0; i < proc->nr_maps; i++) {
		if (opd_is_in_map(&proc->maps[i], sample->eip)) {
			u32 offset = opd_map_offset(&proc->maps[i], sample->eip);
			if (proc->maps[i].image != -1) {
				verb_show_sample(offset, &proc->maps[i]); 
				opd_put_image_sample(&opd_images[proc->maps[i].image], offset, sample->count);
			}
			proc->last_map = i;
			opd_stats[OPD_PROCESS]++;
			return;
		}
		opd_stats[OPD_MAP_ARRAY_DEPTH]++;
	}

	/* couldn't locate it */
	verbprintf("Couldn't find map for pid %.6d, EIP 0x%.8x.\n", sample->pid, sample->eip);
	opd_stats[OPD_LOST_MAP_PROCESS]++;
	return;
}

/**
 * opd_put_mapping - add a mapping to a process
 * @proc: process to add map to
 * @image_nr: mapped image number
 * @start: start of mapping
 * @offset: file offset of mapping
 * @end: end of mapping
 *
 * Add the mapping specified to the process @proc.
 */
void opd_put_mapping(struct opd_proc *proc, int image_nr, u32 start, u32 offset, u32 end)
{
	verbprintf("Placing mapping for process %d: 0x%.8x-0x%.8x, off 0x%.8x, \"%s\"\n",
		proc->pid, start, end, offset, opd_images[image_nr].name);

	opd_check_image_mtime(&opd_images[image_nr]);
 
	proc->maps[proc->nr_maps].image = image_nr;
	proc->maps[proc->nr_maps].start = start;
	proc->maps[proc->nr_maps].offset = offset;
	proc->maps[proc->nr_maps].end = end;
	
	if (++proc->nr_maps == proc->max_nr_maps)
		opd_grow_maps(proc);
}

/**
 * opd_handle_fork - deal with fork notification
 * @note: note to handle
 *
 * Deal with a fork() notification by creating a new process
 * structure, and copying mapping information from the old process.
 *
 * sample->pid contains the process id of the old process.
 * sample->eip contains the process id of the new process.
 */
void opd_handle_fork(const struct op_note *note)
{
	struct opd_proc *old;
	struct opd_proc *proc;

	verbprintf("DO_FORK: from %d to %d\n", note->pid, note->addr);

	old = opd_get_proc(note->pid);

	/* we can quite easily get a fork() after the execve() because the notifications
	 * are racy. So we only create a new setup if it doesn't exist already, allowing
	 * both the clone() and the execve() cases to work.
	 */
	if (opd_get_proc((u16)note->addr))
		return;

	/* eip is actually pid of new process */
	proc = opd_add_proc((u16)note->addr);

	if (!old)
		return;

	/* remove the kernel map and copy over */

	if (proc->maps) free(proc->maps);
	proc->maps = xmalloc(sizeof(struct opd_map) * old->max_nr_maps);
	memcpy(proc->maps,old->maps,sizeof(struct opd_map) * old->nr_maps);
	proc->nr_maps = old->nr_maps;
	proc->max_nr_maps = old->max_nr_maps;
}

/**
 * opd_handle_exit - deal with exit notification
 * @note: note to handle
 *
 * Deal with an exit() notification by setting the flag "dead"
 * on a process. These will be later cleaned up by the %SIGALRM
 * handler.
 *
 * sample->pid contains the process id of the exited process.
 */
void opd_handle_exit(const struct op_note *note)
{
	struct opd_proc *proc;

	verbprintf("DO_EXIT: process %d\n", note->pid);

	proc = opd_get_proc(note->pid);
	if (proc) {
		proc->dead = 1;
		proc->accessed = 1;
	}
	else {
		verbprintf("unknown proc %u just exited.\n", note->pid);
	}
}

 
/**
 * get_from_pool - retrieve string from hash map pool
 * @ind: index into pool
 */
inline static char * get_from_pool(uint ind)
{
	return ((char *)(hashmap+OP_HASH_MAP_NR) + ind);
}

 
/**
 * opd_handle_hashmap - parse image from kernel hash map
 * @hash: hash value
 * @c: string to fill
 *
 * Finds an image from a hashmap hash value
 */
static int opd_handle_hashmap(int hash, char **c)
{
	int orighash = hash;
	char * name;
 
	while (hash) {
		name = get_from_pool(hashmap[hash].name);
 
		if (strlen(name) + 1 + strlen(*c) >= PATH_MAX) {
			fprintf(stderr,"String \"%s\" too large.\n", *c);
			exit(1);
		}

		*c -= strlen(name) + 1;
		**c = '/';
		strncpy(*c + 1, name, strlen(name));
		
		/* move onto parent */
		hash = hashmap[hash].parent;
	}
	return opd_get_image(*c, orighash, 0);
}
 

/**
 * opd_handle_mapping - deal with mapping notification
 * @mapping: mapping info
 *
 * Deal with one or more notifications that a process has mapped
 * in a new executable file. The mapping information is
 * added to the process structure.
 */
void opd_handle_mapping(const struct op_note *note)
{
	static char file[PATH_MAX];
	char *c=&file[PATH_MAX-1];
	struct opd_proc *proc;
	int hash;
	int im_nr;

	proc = opd_get_proc(note->pid);

	if (!proc) {
		verbprintf("Told about mapping for non-existent process %u.\n", note->pid);
		proc = opd_add_proc(note->pid);
	}

	*c = '\0';

	hash = note->hash;

	if (hash == -1) {
		/* possibly deleted file */
		return;
	}

	if (hash < 0 || hash >= OP_HASH_MAP_NR) {
		fprintf(stderr,"hash value %u out of range.\n",hash);
		return;
	}

	im_nr = opd_get_image_by_hash(hash); 
	if (im_nr == -1)
		im_nr = opd_handle_hashmap(hash, &c);

	opd_put_mapping(proc, im_nr, note->addr, note->offset, note->addr + note->len);
}

/**
 * opd_handle_exec - deal with notification of execve()
 * @pid: pid of execve()d process
 *
 * Drop all mapping information for the process.
 */
void opd_handle_exec(u16 pid)
{
	struct opd_proc *proc;

	verbprintf("DO_EXEC: pid %u\n", pid);

	/* FIXME: we should save old maps into ->old_exec()
	 * to reduce loss of samples in fork()/exec() window
	 */
	proc = opd_get_proc(pid);
	if (proc)
		opd_kill_maps(proc);
	else
		opd_add_proc(pid);
}

/**
 * opd_add_ascii_map - parse an ASCII map string for a process
 * @proc: process to add map to
 * @line: 0-terminated ASCII string
 *
 * Attempt to parse the string @line for map information
 * and add the info to the process @proc. Returns %TRUE
 * on success, %FALSE otherwise.
 *
 * The parsing is based on Linux 2.4 format, which looks like this :
 *
 * 4001e000-400fc000 r-xp 00000000 03:04 31011      /lib/libc-2.1.2.so
 */
static int opd_add_ascii_map(struct opd_proc *proc, const char *line)
{
	struct opd_map *map = &proc->maps[proc->nr_maps];
	const char *cp = line;

	/* skip to protection field */
	while (*cp && *cp != ' ')
		cp++;

	/* handle rwx */
	if (!*cp || (!*(++cp)) || (!*(++cp)) || (*(++cp) != 'x'))
		return FALSE;

	/* get start and end from "40000000-4001f000" */
	if (sscanf(line,"%x-%x", &map->start, &map->end) != 2)
		return FALSE;

	/* "p " */
	cp += 2;

	/* read offset */
	if (sscanf(cp,"%x", &map->offset) != 1)
		return FALSE;

	while (*cp && *cp != '/')
		cp++;

	if (!*cp)
		return FALSE;

	map->image = opd_get_image(cp, -1, 0);

	if (!map->image)
		return FALSE;

	if (++proc->nr_maps == proc->max_nr_maps)
		opd_grow_maps(proc);

	return TRUE;
}

/**
 * opd_get_ascii_maps - read all maps for a process
 * @proc: process to work on
 *
 * Read the /proc/<pid>/maps file and add all
 * mapping information found to the process @proc.
 */
static void opd_get_ascii_maps(struct opd_proc *proc)
{
	FILE *fp;
	char mapsfile[20] = "/proc/";
	char *line;

	snprintf(mapsfile + 6, 6, "%hu", proc->pid);
	strcat(mapsfile,"/maps");

	fp = opd_try_open_file(mapsfile, "r");
	if (!fp)
		return;

	do {
		line = opd_get_line(fp);
		if (streq(line, "") && feof(fp)) {
			free(line);
			break;
		} else {
			opd_add_ascii_map(proc, line);
			free(line);
		}
	} while (1);

	opd_close_file(fp);
}

/**
 * opd_get_ascii_procs - read process and mapping information from /proc
 *
 * Read information on each process and its mappings from the /proc
 * filesystem.
 */
void opd_get_ascii_procs(void)
{
	DIR *dir;
	struct dirent *dirent;
	struct opd_proc *proc;
	u16 pid;

	if (!(dir = opendir("/proc"))) {
		perror("oprofiled: /proc directory could not be opened. ");
		exit(1);
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
