/* $Id: opd_proc.c,v 1.92 2002/01/04 19:02:49 movement Exp $ */
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
extern op_cpu cpu_type;
double cpu_speed;

/* list of image */
static struct list_head opd_images = { &opd_images, &opd_images };
/* Images wich belongs to the same hash, more than one only if separate_samples
 *  == 1, are accessed by hash code and linked through the hash_next member
 * of opd_image. Hash-less image must be searched through opd_images list */
static struct opd_image * images_with_hash[OP_HASH_MAP_NR];
/* The kernel image is treated separately */
struct opd_image * kernel_image;
/* maintained for statistics purpose only */
static unsigned int nr_images=0;

/* reversed LRU of mapped samples files, unmapped samples are not lru'ed  */
static struct list_head opd_samples_files = { &opd_samples_files, &opd_samples_files };
 
void opd_handle_kernel_sample(u32 eip, u16 count);
struct opd_image * opd_get_image(const char *name, int hash, const char * app_name, int kernel);
int bstreq(const char *str1, const char *str2);
void opd_put_image_sample(struct opd_image *image, u32 offset, u16 count);
 
static struct opd_proc *opd_add_proc(u16 pid);
static void opd_open_sample_file(struct opd_image *image, int counter);
static void opd_open_image(struct opd_image *image);
static void opd_init_image(struct opd_image * image, const char * name, int hash, const char * app_name, int kernel);
static const char* opd_app_name(const struct opd_proc * proc);
static struct opd_image * opd_find_image(const char *name, int hash, const char* app_name);
static struct opd_image * opd_add_image(const char *name, int hash, const char * app_name, int kernel);
static struct opd_image * opd_get_image_by_hash(int hash, const char * app_name);
static void opd_init_maps(struct opd_proc *proc);
static void opd_grow_maps(struct opd_proc *proc);
static void opd_kill_maps(struct opd_proc *proc);
static void opd_put_mapping(struct opd_proc *proc, struct opd_image * image, u32 start, u32 offset, u32 end);
static struct opd_proc *opd_get_proc(u16 pid);
static void opd_delete_proc(struct opd_proc *proc);
static void opd_handle_old_sample_file(const char * mangled, time_t mtime);
static void opd_handle_old_sample_files(const struct opd_image * image);

void opd_print_stats(void)
{
	struct opd_proc *proc;
	int i,j = 0;
 
	for (i=0; i < OPD_MAX_PROC_HASH; i++) {
		proc = opd_procs[i];

		while (proc) {
			++j;
			proc = proc->next;
		}
	}
 
	printf("%s\n", opd_get_time());
	printf("Nr. proc struct: %d\n", j);
	printf("Nr. image struct: %d\n", nr_images);
	printf("Nr. kernel samples: %lu\n", opd_stats[OPD_KERNEL]);
	printf("Nr. modules samples: %lu\n", opd_stats[OPD_MODULE]);
	printf("Nr. modules samples lost: %lu\n", opd_stats[OPD_LOST_MODULE]);
	printf("Nr. samples lost due to no process information: %lu\n", opd_stats[OPD_LOST_PROCESS]);
	printf("Nr. process samples in user-space: %lu\n", opd_stats[OPD_PROCESS]);
	printf("Nr. samples lost due to no map information: %lu\n", opd_stats[OPD_LOST_MAP_PROCESS]);
	if (opd_stats[OPD_PROC_QUEUE_ACCESS]) {
		printf("Average depth of search of proc queue: %f\n",
			(double)opd_stats[OPD_PROC_QUEUE_DEPTH] 
			/ (double)opd_stats[OPD_PROC_QUEUE_ACCESS]);
	}
	if (opd_stats[OPD_MAP_ARRAY_ACCESS]) {
		printf("Average depth of iteration through mapping array: %f\n",
			(double)opd_stats[OPD_MAP_ARRAY_DEPTH] 
			/ (double)opd_stats[OPD_MAP_ARRAY_ACCESS]);
	}
	printf("Nr. mapping: %lu\n", opd_stats[OPD_MAPPING]);
	printf("Nr. mapping tried: %lu\n", opd_stats[OPD_TRY_MAPPING]);
	printf("Nr. unmapping due to mapping failure: %lu\n",
	       opd_stats[OPD_TRY_MAPPING] - opd_stats[OPD_MAPPING]);
	printf("Nr. sample dumps: %lu\n", opd_stats[OPD_DUMP_COUNT]);
	printf("Nr. samples total: %lu\n", opd_stats[OPD_SAMPLES]);
	printf("Nr. notifications: %lu\n", opd_stats[OPD_NOTIFICATIONS]);
	fflush(stdout);
}
 
/* every so many minutes, clean up old procs, msync mmaps, and
   report stats */
void opd_alarm(int val __attribute__((unused)))
{
	struct opd_proc *proc;
	struct opd_proc *next;
	uint i;
	uint j;
	struct opd_image * image;
	struct list_head * pos;

	list_for_each(pos, &opd_images) {
		image = list_entry(pos, struct opd_image, list_node);
		for (j = 0 ; j < op_nr_counters ; ++j) {
			if (image->sample_files[j].fd > 1)
				msync(image->sample_files[j].header, 
					image->len + sizeof(struct opd_header), MS_ASYNC);
		}
	}

	j = 0;
	for (i=0; i < OPD_MAX_PROC_HASH; i++) {
		proc = opd_procs[i];

		while (proc) {
			++j;
			next = proc->next;
			// delay death whilst its still being accessed
			if (proc->dead) {
				proc->dead += proc->accessed;
				proc->accessed = 0;
				if (--proc->dead == 0)
					opd_delete_proc(proc);
			}
			proc=next;
		}
	}

	opd_print_stats();

	alarm(60*10);
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
/* TODO: opd_handle_old_sample_file and opd_handle_old_sample_files
 * can be simplified and improved */
static void opd_handle_old_sample_file(const char * mangled, time_t mtime)
{
	struct opd_header oldheader; 
	FILE * fp;

	fp = fopen(mangled, "r"); 
	if (!fp) {
		/* file might not be there, or it just might not be
		 * openable for some reason, so try to unlink anyway
		 */
		goto del;
	}

	if (fread(&oldheader, sizeof(struct opd_header), 1, fp) != 1) {
		verbprintf("Can't read %s\n", mangled);
		goto closedel;
	}

	if (memcmp(&oldheader.magic, OPD_MAGIC, sizeof(oldheader.magic)) || oldheader.version != OPD_VERSION) {
		verbprintf("Magic id check fail for %s\n", mangled);
		goto closedel;
	}

	if (difftime(mtime, oldheader.mtime)) {
		verbprintf("mtime differs for %s\n", mangled);
		goto closedel;
	}

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
 * @image: the image to check files for
 *
 * to simplify admin of sample file we rename or remove sample
 * files for each counter.
 *
 * If an old sample file exists, verify it is usable.
 * If not, delete it.
 */
static void opd_handle_old_sample_files(const struct opd_image * image)
{
	uint i;
	char *mangled;
	uint len;
	const char * app_name = separate_samples ? image->app_name : NULL;

	mangled = opd_mangle_filename(smpdir, image->name, app_name);

	len = strlen(mangled);
 
	for (i = 0 ; i < op_nr_counters ; ++i) {
		sprintf(mangled + len, "#%d", i);
		opd_handle_old_sample_file(mangled,  image->mtime);
	}

	free(mangled);
}

/**
 * opd_init_image - init an image sample file
 * @image: image to init file for
 * @hash: hash of image
 * @app_name: the application name where belongs this image
 * @name: name of the image to add
 * @kernel: is the image a kernel/module image
 *
 * @image at funtion entry is uninitialised
 * @name is copied i.e. should be GC'd separately from the
 * image structure if appropriate.
 *
 * Initialise an opd_image struct for the image @image
 * without opening the associated samples files. At return
 * the @image is partially initialized initialized.
 */
static void opd_init_image(struct opd_image * image, const char * name,
			   int hash, const char * app_name, int kernel)
{
	list_init(&image->list_node);
	image->hash_next = NULL;
	image->name = xstrdup(name);
	image->kernel = kernel;
	image->hash = hash;
	/* we do not duplicate this string! */
	image->app_name = app_name;
	image->len = 0;
}
 
/**
 * opd_open_image - open an image sample file
 * @image: image to open file for
 * @hash: hash of image
 * @app_name: the application name where belongs this image
 * @name: name of the image to add
 * @kernel: is the image a kernel/module image
 *
 * @image at funtion entry is partially initialized by opd_init_image()
 *
 * Initialise an opd_image struct for the image @image
 * without opening the associated samples files. At return
 * the @image is fully initialized.
 */
static void opd_open_image(struct opd_image *image)
{
	uint i;

	/* FIXME: image->app_name can be null. */
	verbprintf("Opening image \"%s\" for app \"%s\"\n",
		   image->name, image->app_name);

	for (i = 0 ; i < op_nr_counters ; ++i) {
		image->sample_files[i].fd = -1;
		image->sample_files[i].start = (void *)-1;
		image->sample_files[i].header = (void *)-1;
		image->sample_files[i].opened = 0;
	}

	verbprintf("Getting size of \"%s\"\n", image->name);

	/* for each byte in original one counter */
	image->len = opd_get_fsize(image->name, 0) * sizeof(struct opd_fentry);

	if (!image->len) {
		verbprintf("Size check failed for %s\n", image->name);
		return;
	}

	image->mtime = opd_get_mtime(image->name);
 
	/* give space for "negative" entries. This is because we
	 * don't know about kernel/module sections other than .text so
	 * a sample could be before our nominal start of image, or
	 * after the start */
	if (image->kernel)
		image->len += OPD_KERNEL_OFFSET * sizeof(struct opd_fentry);

	opd_handle_old_sample_files(image);

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
 * Returns the counter number (0-N)
 */
inline static u16 opd_get_counter(const u16 count)
{
	return OP_COUNTER(count);
}

/*
 * opd_try_mmap - try to mmap a sample file
 * @sample_file: the sample file to mmap
 *
 * try to mmap a sample file, return non-zero on failure
 */
static int opd_try_mmap(struct opd_sample_file * sample_file,
			u32 len, const char * name)
{
	opd_stats[OPD_TRY_MAPPING]++;

	sample_file->header = mmap(0, len + sizeof(struct opd_header),
		PROT_READ|PROT_WRITE, MAP_SHARED, sample_file->fd, 0);

	if (sample_file->header == (void *)-1) {
		verbprintf("oprofiled: mmap of image sample file \"%s\" failed: %s\n", 
			name, strerror(errno));
		return 1;
	}

	opd_stats[OPD_MAPPING]++;

	sample_file->start = sample_file->header + 1;
	sample_file->len = len;

	list_add_tail(&sample_file->lru_node, &opd_samples_files);

	return 0;
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
	const char * app_name;
	int open_flags;

	sample_file = &image->sample_files[counter];

	/* avoid flood of error messages */
	if (sample_file->fd == -2)
		return;

	app_name = separate_samples ? image->app_name : NULL;
	mangled = opd_mangle_filename(smpdir, image->name, app_name);

	sprintf(mangled + strlen(mangled), "#%d", counter);

	verbprintf("Opening \"%s\"\n", mangled);

	open_flags = sample_file->opened ? O_RDWR : O_CREAT|O_RDWR;
	sample_file->fd = open(mangled, open_flags, 0644);
	if (sample_file->fd == -1) {
		fprintf(stderr, 
			"oprofiled: open of image sample file \"%s\" (opened %d) failed: %s\n", 
			mangled, sample_file->opened, strerror(errno));
		goto err1;
	}

	if ( sample_file->opened == 0) {
		/* truncate to grow the file is ok on linux */
		int err = ftruncate(sample_file->fd, image->len + sizeof(struct opd_header));
		if (err == -1) {
			fprintf(stderr, 
				"oprofiled: ftruncate failed for \"%s\". %s\n",
				mangled, strerror(errno));
			goto err2;
		}
	}

	if (opd_try_mmap(sample_file, image->len, image->name)) {
		struct opd_sample_file * temp;
		struct list_head * tmp_pos, * pos;

		verbprintf("try mmap loose, for %s on len %lu must unmap things\n", 
			image->name, image->len);

		list_for_each_safe(pos, tmp_pos, &opd_samples_files) {
			temp = list_entry(pos, struct opd_sample_file, lru_node);

			if (temp == sample_file) {
				/* something feel very bad in the lru list */
				fprintf(stderr, "ARGH: temp == sample_file\n");
				exit(EXIT_FAILURE);
			}

			
			/* list_del is sufficient here but ... */
			list_del_init(pos);

			verbprintf("unmamp at %p for %lu byte\n", temp->header, temp->len);

			close(temp->fd);
			munmap(temp->header, temp->len + sizeof(struct opd_header));

			temp->fd = -1;
			temp->start = (void *)-1;
			temp->header = (void *)-1;

			if (!opd_try_mmap(sample_file, image->len, image->name))
				break;
		}

		if (pos == &opd_samples_files) {
			fprintf(stderr, "oprofiled: unable to map image \"%s\" even after whole unmaping/\n", mangled);
			/* not an exit just in case of failure due trying a
			 * very big mapping */
			goto err2;
		}

		verbprintf("mapping ok for %s at %p for len %lu (%lu)\n",
			   image->name, sample_file->start, sample_file->len,
			   image->len);
	}

	if (sample_file->opened == 0) {
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
		sample_file->header->separate_samples = separate_samples;
	}

	sample_file->opened = 1;

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
/* TODO: This is bugged we need to check mtime in opd_open_image: there is some
 * code that open image without checking for modified mtime ? */
static void opd_check_image_mtime(struct opd_image * image)
{
	uint i;
	char *mangled;
	uint len;
	time_t newmtime = opd_get_mtime(image->name);
	const char * app_name;
 
	if (image->mtime == newmtime)
		return;

	verbprintf("Current mtime %lu differs from stored "
		"mtime %lu for %s\n", newmtime, image->mtime, image->name);

	app_name = separate_samples ? image->app_name : NULL;
	mangled = opd_mangle_filename(smpdir, image->name, app_name);

	len = strlen(mangled);

	for (i=0; i < op_nr_counters; i++) {
		struct opd_sample_file * file = &image->sample_files[i]; 
		if (file->fd > 0) {
			list_del_init(&file->lru_node);
			close(file->fd);
			munmap(file->header, image->len + sizeof(struct opd_header));
		}
		sprintf(mangled + len, "#%d", i);
		verbprintf("Deleting out of date \"%s\"\n", mangled);
		unlink(mangled);
	}
	free(mangled);

	opd_open_image(image);
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
void opd_put_image_sample(struct opd_image *image, u32 offset, u16 count)
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

	/* simplify this ? t least list_del(); is sufficient. */
	list_del_init(&sample_file->lru_node);
	list_add_tail(&sample_file->lru_node, &opd_samples_files);

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
	/* the kernel image is treated a part the list of image */
	kernel_image = xmalloc(sizeof(struct opd_image));

	opd_init_image(kernel_image, vmlinux, -1, NULL, 1);

	opd_open_image(kernel_image);

#if 0
	/* TEST: map 1.75 Go just to see the effect of the lru */
	mmap(0, (1<<30) + (1<<29) + (1<<27), PROT_READ,
	     MAP_ANON | MAP_PRIVATE, -1, 0);
#endif
}

/**
 * opd_add_image - add an image to the image structure
 * @name: name of the image to add
 * @hash: hash of image
 * @app_name: the application name where belongs this image
 * @kernel: is the image a kernel/module image
 *
 * Add an image to the image structure with name @name.
 *
 * @name is copied i.e. should be GC'd separately from the
 * image structure if appropriate.
 *
 * The new image pointer is returned.
 */
static struct opd_image * opd_add_image(const char * name, int hash, const char * app_name, int kernel)
{
	struct opd_image * image = xmalloc(sizeof(struct opd_image));

	opd_init_image(image, name, hash, app_name, kernel);

	list_add(&image->list_node, &opd_images);

	/* image with hash -1 are lazilly put in the images_with_hash array */
	if (hash != -1) {
		image->hash_next = images_with_hash[hash];
		images_with_hash[hash] = image;
	}

	nr_images++;

	opd_open_image(image);

	return image;
}

/**
 * bstreq - reverse string comparison
 * @str1: first string
 * @str2: second string
 *
 * Compares two strings, starting at the end.
 * Returns %1 if they match, %0 otherwise.
 */
int bstreq(const char *str1, const char *str2)
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
 * opd_app_name - get the application name
 *  or %NULL if irrelevant
 *
 * Returns the app_name for the given @proc or %NULL if
 * it does not exist any mapping for this proc (which is
 * true for the first mapping at exec time)
 */
inline static const char * opd_app_name(const struct opd_proc * proc)
{
	const char * app_name = NULL;
	if (proc->nr_maps) 
		app_name = proc->maps[0].image->name;

	return app_name;
}

/**
 * opd_find_image - find an image
 * @name: name of image to find
 * @hash: hash of image to find
 * @app_name: the application name where belongs this image
 *
 * Returns the image pointer for the file specified by @name, or %NULL.
 */
/*
 * We make here a linear search through the whole image list. There is no need
 * to improve performance, only /proc parsed app are hashless and when they
 * are found one time by this function they receive a valid hash code. */
static struct opd_image * opd_find_image(const char * name, int hash, const char * app_name)
{
	struct opd_image * image = 0; /* supress warn non initialized use */
	struct list_head * pos;

	list_for_each(pos, &opd_images) {

		image = list_entry(pos, struct opd_image, list_node);

		if (bstreq(image->name, name)) {
			if (!separate_samples)
				break;

			if (image->app_name == NULL && app_name == NULL)
				break;

			if (image->app_name != NULL && app_name != NULL &&
			    bstreq(image->app_name, app_name))
				break;
		}
	}

	if (pos != &opd_images) {
		/* we can have hashless images from /proc/pid parsing, modules
		 * are handled in a separate list */
		if (hash != -1) {
			image->hash = hash;
			if (image->hash_next) {	/* parano check */
				printf("error: image->hash_next != NULL and image->hash == -1\n");
				exit(EXIT_FAILURE);
			}

			image->hash_next = images_with_hash[hash];
			images_with_hash[hash] = image;
		}

		/* The app_name field is always valid */
		return image;
	}

	return NULL;
}

/**
 * opd_get_image_by_hash - get an image from the image
 * structure by hash value
 * @hash: hash of the image to get
 * @app_name: the application name where belongs this image
 *
 * Get the image specified by @hash and @app_name
 * if present, else return %NULL
 */
static struct opd_image * opd_get_image_by_hash(int hash, const char * app_name)
{
	struct opd_image * image;
	for (image = images_with_hash[hash]; image != NULL; image = image->hash_next) {
		if (!separate_samples)
			break;

		if (image->app_name == NULL && app_name == NULL)
			break;

		if (image->app_name != NULL && app_name != NULL &&
		    bstreq(image->app_name, app_name))
			break;
	}

	return image;
}


/**
 * opd_get_image - get an image from the image structure
 * @name: name of image
 * @hash: hash of the image to get
 * @app_name: the application name where belongs this image
 * @kernel: is the image a kernel/module image
 *
 * Get the image specified by the file name @name from the
 * image structure. If it is not present, the image is
 * added to the structure. In either case, the image number
 * is returned.
 */
struct opd_image * opd_get_image(const char * name, int hash, const char * app_name, int kernel)
{
	struct opd_image * image;
	if ((image = opd_find_image(name, hash, app_name)) == NULL)
		image = opd_add_image(name, hash, app_name, kernel);

	return image;
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
 * verb_show_sample - print the sample out to the log
 * @offset: the offset value
 * @map: map to print
 */
inline static void verb_show_sample(u32 offset, struct opd_map *map, const char * last_map)
{
	verbprintf("DO_PUT_SAMPLE %s: calc offset 0x%.8x, map start 0x%.8x,"
		" end 0x%.8x, offset 0x%.8x, name \"%s\"\n",
		last_map, offset, map->start, map->end, map->offset, map->image->name);
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

	/* proc->last_map is always safe as mappings are never deleted except by
	 * opd_kill_maps, which resets last_map. But we need to check we have
	 * at least one map
	 */
	opd_stats[OPD_MAP_ARRAY_ACCESS]++;
	if (proc->nr_maps && opd_is_in_map(&proc->maps[proc->last_map], sample->eip)) {
		i = proc->last_map;
		if (proc->maps[i].image != NULL) {
			verb_show_sample(opd_map_offset(&proc->maps[i], sample->eip), 
				&proc->maps[i], "(LAST MAP)");
			opd_put_image_sample(proc->maps[i].image, 
				opd_map_offset(&proc->maps[i], sample->eip), sample->count);
		}

		opd_stats[OPD_PROCESS]++;
		return;
	}

	/* look for which map and find offset */
	for (i=0; i < proc->nr_maps; i++) {
		if (opd_is_in_map(&proc->maps[i], sample->eip)) {
			u32 offset = opd_map_offset(&proc->maps[i], sample->eip);
			if (proc->maps[i].image != NULL) {
				verb_show_sample(offset, &proc->maps[i], "");
				opd_put_image_sample(proc->maps[i].image, offset, sample->count);
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
 * @image: mapped image pointer
 * @start: start of mapping
 * @offset: file offset of mapping
 * @end: end of mapping
 *
 * Add the mapping specified to the process @proc.
 */
static void opd_put_mapping(struct opd_proc *proc, struct opd_image * image, u32 start, u32 offset, u32 end)
{
	verbprintf("Placing mapping for process %d: 0x%.8x-0x%.8x, off 0x%.8x, \"%s\" at maps pos %d\n",
		proc->pid, start, end, offset, image->name, proc->nr_maps);

	opd_check_image_mtime(image);
 
	proc->maps[proc->nr_maps].image = image;
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
	 * are racy. In particular, the fork notification is done on parent return (so we
	 * know the pid), but this will often be after the execve is done by the child.
	 *
	 * So we only create a new setup if it doesn't exist already, allowing
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
	} else {
		verbprintf("unknown proc %u just exited.\n", note->pid);
	}
}

 
/**
 * get_from_pool - retrieve string from hash map pool
 * @ind: index into pool
 */
inline static char * get_from_pool(uint ind)
{
	return ((char *)(hashmap + OP_HASH_MAP_NR) + ind);
}

 
/**
 * opd_handle_hashmap - parse image from kernel hash map
 * @hash: hash value
 * @app_name: the application name which belongs this image
 *
 * Finds an image from its name.
 */
static struct opd_image * opd_handle_hashmap(int hash, const char * app_name)
{
	char file[PATH_MAX];
	char *c = &file[PATH_MAX-1];
	int orighash = hash;

	*c = '\0'; 
	while (hash) {
		char * name = get_from_pool(hashmap[hash].name);
 
		if (strlen(name) + 1 + strlen(c) >= PATH_MAX) {
			fprintf(stderr,"String \"%s\" too large.\n", c);
			exit(1);
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
 * @mapping: mapping info
 *
 * Deal with one notification that a process has mapped
 * in a new executable file. The mapping information is
 * added to the process structure.
 */
void opd_handle_mapping(const struct op_note *note)
{
	struct opd_proc *proc;
	struct opd_image * image;
	int hash;
	const char * app_name;

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

	/* There is a race for samples received between fork/exec sequence.
	 * These samples belong to the old mapping but we can not say if
	 * samples has been received before the exec or after. This explain
	 * the message "Couldn't find map for ..." in verbose mode.
	 *
	 * Unhopefully it is difficult to get an estimation of these misplaced
	 * samples, the error message can count only out of mapping samples but
	 * not samples between the race and inside the mapping of the exec'ed
	 * process :/.
	 *
	 * Trying to save old mapping is not correct due the above reason. The
	 * only manner to handle this is to flush the module samples hash table
	 * after each fork which is unacceptable for performance reasons */
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

	/* FIXME: we should verify this is indeed the primary
	 * app image by readlinking /proc/pid/exe */
	map->image = opd_get_image(cp, -1, opd_app_name(proc), 0);

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
