/**
 * @file opd_image.c
 * Management of binary images
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author John Levon
 * @author Philippe Elie
 */

#include "opd_image.h"
#include "opd_kernel.h"
#include "opd_stats.h"
#include "opd_sample_files.h"
#include "opd_printf.h"
 
#include "op_types.h" 
#include "op_libiberty.h"
#include "op_file.h"
#include "op_interface_25.h"
 
#include <unistd.h>
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

extern uint op_nr_counters;
extern int separate_samples;

/* maintained for statistics purpose only */
unsigned int nr_images=0;

#define HASH_KERNEL 0
#define IMAGE_HASH_SIZE 2048
 
/* hashlist of images. opd_images[0] contains all kernel
 * images.
 */
static struct list_head opd_images[IMAGE_HASH_SIZE];

 
/** initialise image hash */
void opd_init_images(void)
{
	int i;
	for (i = 0; i < IMAGE_HASH_SIZE; ++i) {
		list_init(&opd_images[i]);
	}
}


/**
 * @param image  the image pointer
 *
 * free all memory belonging to this image -  This function does not close
 * nor flush the samples files
 */
static void opd_delete_image(struct opd_image * image)
{
	if (image->name)
		free(image->name);
	if (image->app_name)
		free(image->app_name);
	free(image);
}


/**
 * opd_image_cleanup - clean up images structures
 */
void opd_image_cleanup(void)
{
	/* to reuse opd_for_each_image we need to process images in two pass */
	opd_for_each_image(opd_close_image_samples_files);
	opd_for_each_image(opd_delete_image);
}


/**
 * @param image_cb callback to apply onto each existing image struct
 *
 * the callback receive a struct opd_image * (not a const struct) and is
 * allowed to freeze the image struct itself.
 */
void opd_for_each_image(opd_image_cb image_cb)
{
	struct list_head * pos;
	struct list_head * pos2;
	int i;

	for (i = 0; i < IMAGE_HASH_SIZE; ++i) {
		/* image callback is allowed to delete the current pointer so on
		 * use list_for_each_safe rather list_for_each */
		list_for_each_safe(pos, pos2, &opd_images[i]) {
			struct opd_image * image =
				list_entry(pos, struct opd_image, hash_list);

			image_cb(image);
		}
	}
}


static int opd_get_dcookie(unsigned long cookie, char * buf, size_t size)
{
	// FIXME
	return syscall(253, cookie, buf, size);
}
 

/**
 * opd_init_image - init an image sample file
 */
static void opd_init_image(struct opd_image * image, unsigned long cookie,
	char const * app_name)
{
	char buf[PATH_MAX + 1];
 
	if (opd_get_dcookie(cookie, buf, PATH_MAX))
		image->name = xstrdup("");
	else
		image->name = xstrdup(buf);

	image->cookie = cookie;
	image->kernel = 0;
	image->app_name = xstrdup(app_name);
}


/**
 * opd_open_image - open an image sample file
 * @param image  image to open file for
 *
 * image at function entry is partially initialized by opd_init_image()
 *
 * Initialise an opd_image struct for the image image
 * without opening the associated samples files. At return
 * the image is fully initialized.
 */
static void opd_open_image(struct opd_image * image)
{
	uint i;

	verbprintf("Opening image \"%s\" for app \"%s\"\n",
		   image->name, image->app_name ? image->app_name : "none");

	for (i = 0 ; i < op_nr_counters ; ++i) {
		memset(&image->sample_files[i], '\0', sizeof(db_tree_t));
	}

	image->mtime = op_get_mtime(image->name);

	opd_handle_old_sample_files(image);

	/* samples files are lazily opened */
}

#if 0 // FIXME: when can we check this !?

/**
 * opd_check_image_mtime - ensure samples file is up to date
 * @param image  image to check
 */
void opd_check_image_mtime(struct opd_image * image)
{
	uint i;
	char * mangled;
	uint len;
	time_t newmtime = op_get_mtime(image->name);
	char const * app_name;

	if (image->mtime == newmtime)
		return;

	verbprintf("Current mtime %lu differs from stored "
		"mtime %lu for %s\n", newmtime, image->mtime, image->name);

	app_name = separate_samples ? image->app_name : NULL;
	mangled = op_mangle_filename(image->name, app_name);

	len = strlen(mangled);

	for (i=0; i < op_nr_counters; i++) {
		db_tree_t * tree = &image->sample_files[i];
		if (tree->base_memory) {
			db_close(tree);
		}
		sprintf(mangled + len, "#%d", i);
		verbprintf("Deleting out of date \"%s\"\n", mangled);
		remove(mangled);
	}
	free(mangled);

	opd_open_image(image);
}

#endif

/**
 * opd_put_image_sample - write sample to file
 * @param image  image for sample
 * @param offset  (file) offset to write to
 * @param counter counter number
 *
 * Add to the count stored at position @offset in the
 * image file. Overflow pins the count at the maximum
 * value.
 *
 * @count is the raw value passed from the kernel.
 */
void opd_put_image_sample(struct opd_image * image,
	unsigned long offset, int counter)
{
	db_tree_t * sample_file;

	sample_file = &image->sample_files[counter];
 
	if (!sample_file->base_memory) {
		opd_open_sample_file(image, counter);
		if (!sample_file->base_memory) {
			/* opd_open_sample_file output an error message */
			return;
		}
	}
 
	db_insert(sample_file, offset, 1);
}


/** return hash value for a cookie */
static unsigned long opd_hash_cookie(unsigned long cookie)
{
	return (cookie >> 2) & (IMAGE_HASH_SIZE - 1);
}
 

/**
 * opd_add_image - add an image to the image hashlist
 */
static struct opd_image * opd_add_image(unsigned long cookie, char const * app_name)
{
	struct opd_image * image = xmalloc(sizeof(struct opd_image));
	unsigned long hash = opd_hash_cookie(cookie);

	opd_init_image(image, cookie, app_name);
	list_add(&image->hash_list, &opd_images[hash]);
	nr_images++;
	opd_open_image(image);
	return image;
}


/**
 * opd_find_image - find an image
 */
static struct opd_image * opd_find_image(unsigned long cookie)
{
	unsigned long hash = opd_hash_cookie(cookie);
	struct opd_image * image = 0;
	struct list_head * pos;

	list_for_each(pos, &opd_images[hash]) {
		image = list_entry(pos, struct opd_image, hash_list);
		if (image->cookie == cookie)
			return image;
	}
	return NULL;
}

 
/**
 * opd_get_image - get an image from the image structure
 */
static struct opd_image * opd_get_image(unsigned long cookie, char const * app_name)
{
	struct opd_image * image;
	if ((image = opd_find_image(cookie)) == NULL)
		image = opd_add_image(cookie, app_name);

	return image;
}


static struct opd_image * opd_add_kernel_image(char const * name)
{
	struct opd_image * image = xmalloc(sizeof(struct opd_image));
	image->cookie = 0;
	image->name = xstrdup(name);
	image->app_name = NULL;
	image->kernel = 1;
	list_add(&image->hash_list, &opd_images[HASH_KERNEL]);
	nr_images++;
	opd_open_image(image);
	return image;
}

 
struct opd_image * opd_get_kernel_image(char const * name)
{
	struct list_head * pos;
	struct opd_image * image;
 
	list_for_each(pos, &opd_images[HASH_KERNEL]) {
		image = list_entry(pos, struct opd_image, hash_list);
		if (!strcmp(image->name, name))
			return image;
	}

	return opd_add_kernel_image(name);
}

 
// FIXME
static int opd_get_counter(unsigned long val)
{
	return val & (0x3);
}
 

#if 0 /* not used (yet) */
static int opd_get_cpu(unsigned long val)
{
	return val >> 2;
}
#endif


void opd_put_sample(struct op_sample const * sample)
{
	static char const * app_name = NULL;
 
	struct opd_image * image;
	int counter = opd_get_counter(sample->event);
 
	/* ctx switch pre-amble */
	if (sample->cookie == ~0UL) {
		struct opd_image * app_image = opd_find_image(sample->event);
		if (app_image) {
			app_name = app_image->name;
		} else {
			char buf[PATH_MAX + 1];
			if (opd_get_dcookie(sample->event, buf, PATH_MAX))
				app_name = NULL;
			else
				app_name = buf;
		}
		return;
	}

	/* kernel sample or no-mm */
	if (!sample->cookie) {
		if (opd_eip_is_kernel(sample->offset)) {
			opd_handle_kernel_sample(sample->offset, counter);
		} else {
			opd_stats[OPD_NO_MM]++;
		}
		return;
	}
 
	image =  opd_get_image(sample->cookie, app_name);
	opd_put_image_sample(image, sample->offset, counter);
}
