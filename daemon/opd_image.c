/**
 * @file daemon/opd_image.c
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
#include "opd_cookie.h"
 
#include "op_types.h" 
#include "op_libiberty.h"
#include "op_file.h"
#include "op_interface_25.h"
#include "op_mangle.h"
 
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

extern uint op_nr_counters;
extern int separate_lib_samples;
extern int separate_kernel_samples;
extern size_t kernel_pointer_size;

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
	opd_delete_modules(image);
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


/**
 * opd_create_image - allocate and initialize an image struct
 * @param hash  hash entry number
 *
 */
static struct opd_image * opd_create_image(unsigned long hash)
{
	uint i;
	struct opd_image * image = xmalloc(sizeof(struct opd_image));

	image->name = image->app_name = NULL;
	image->cookie = image->app_cookie = 0;
	image->app_image = 0;
	image->mtime = 0;
	image->kernel = 0;

	for (i = 0 ; i < op_nr_counters ; ++i) {
		db_init(&image->sample_files[i]);
	}

	list_add(&image->hash_list, &opd_images[hash]);
	list_init(&image->module_list);

	nr_images++;

	return image;
}


/**
 * opd_init_image - init an image sample file
 */
static void opd_init_image(struct opd_image * image, cookie_t cookie,
	cookie_t app_cookie)
{
	char buf[PATH_MAX + 1];
 
	/* If dcookie lookup fails we will re open multiple time the
	 * same db which doesn't work */
	if (lookup_dcookie(cookie, buf, PATH_MAX) <= 0) {
		fprintf(stderr, "Lookup of cookie %llx failed, errno=%d\n",
		       cookie, errno); 
		exit(EXIT_FAILURE);
	}

	image->name = xstrdup(buf);
 
	if (lookup_dcookie(app_cookie, buf, PATH_MAX) <= 0) {
		fprintf(stderr, "Lookup of cookie %llx failed, errno=%d\n",
			cookie, errno); 
		exit(EXIT_FAILURE);
	}

	image->app_name = xstrdup(buf);

	image->cookie = cookie;
	image->app_cookie = app_cookie;
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
	verbprintf("Opening image \"%s\" for app \"%s\"\n",
		   image->name, image->app_name ? image->app_name : "none");

	image->mtime = op_get_mtime(image->name);

	opd_handle_old_sample_files(image);

	/* samples files are lazily opened */
}


#if 0 /* not easy to re-enable ... */
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

	app_name = separate_lib_samples ? image->app_name : NULL;
	mangled = op_mangle_filename(image->name, app_name);

	len = strlen(mangled);

	for (i = 0; i < op_nr_counters; i++) {
		samples_db_t * db = &image->sample_files[i];
		if (db->base_memory) {
			db_close(db);
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
 * Add to the count stored at position offset in the
 * image file. Overflow pins the count at the maximum
 * value.
 *
 * count is the raw value passed from the kernel.
 */
void opd_put_image_sample(struct opd_image * image,
	vma_t offset, int counter)
{
	char * err_msg;
	samples_db_t * sample_file;

	sample_file = &image->sample_files[counter];
 
	if (!sample_file->base_memory) {
		opd_open_sample_file(image, counter);
		if (!sample_file->base_memory) {
			/* opd_open_sample_file output an error message */
			return;
		}
	}
 
	/* Possible narrowing to 32-bit value only. */
	if (db_insert(sample_file, (unsigned long)offset, 1, &err_msg) != EXIT_SUCCESS) {
		fprintf(stderr, "db_insert() %s\n", err_msg);
		free(err_msg);
		exit(EXIT_FAILURE);
	}
}


/** return hash value for a cookie */
static unsigned long opd_hash_cookie(cookie_t cookie)
{
	return (cookie >> DCOOKIE_SHIFT) & (IMAGE_HASH_SIZE - 1);
}
 

/**
 * opd_find_image - find an image
 */
static struct opd_image * opd_find_image(cookie_t cookie, cookie_t app_cookie)
{
	unsigned long hash = opd_hash_cookie(cookie);
	struct opd_image * image = 0;
	struct list_head * pos;

	list_for_each(pos, &opd_images[hash]) {
		image = list_entry(pos, struct opd_image, hash_list);

		/* without this check !separate_lib_samples will open the
		 * same sample file multiple times
		 */
		if (separate_lib_samples && image->app_cookie != app_cookie)
			continue;
 
		if (image->cookie == cookie)
			return image;
	}
	return NULL;
}


/**
 * opd_add_image - add an image to the image hashlist
 */
static struct opd_image * opd_add_image(cookie_t cookie, cookie_t app_cookie)
{
	unsigned long hash = opd_hash_cookie(cookie);
	struct opd_image * image = opd_create_image(hash);

	opd_init_image(image, cookie, app_cookie);

	if (separate_lib_samples) {
		image->app_image = opd_find_image(app_cookie, app_cookie);
		if (!image->app_image) {
			verbprintf("image->app_image %p for cookie %llx\n",
				   image->app_image, app_cookie);
		}
	}

	opd_open_image(image);

	return image;
}

 
/**
 * opd_get_image - get an image from the image structure
 */
static struct opd_image * opd_get_image(cookie_t cookie, cookie_t app_cookie)
{
	struct opd_image * image;
	if ((image = opd_find_image(cookie, app_cookie)) == NULL)
		image = opd_add_image(cookie, app_cookie);

	return image;
}


struct opd_image * opd_add_kernel_image(char const * name, char const * app_name)
{
	struct opd_image * image = opd_create_image(HASH_KERNEL);

	image->name = xstrdup(name);
	image->app_name = app_name ? xstrdup(app_name) : NULL;
	image->kernel = 1;
	opd_open_image(image);

	return image;
}

 
struct opd_image * opd_get_kernel_image(char const * name,
					char const * app_name)
{
	struct list_head * pos;
	struct opd_image * image;
 
	list_for_each(pos, &opd_images[HASH_KERNEL]) {
		image = list_entry(pos, struct opd_image, hash_list);
		if (strcmp(image->name, name))
			continue;
		if (!separate_kernel_samples)
			return image;
		if (!app_name && !image->app_name)
			return image;
		if (app_name && image->app_name &&
		    !strcmp(app_name, image->app_name))
			return image;
	}

	return opd_add_kernel_image(name, app_name);
}


static inline int is_escape_code(uint64_t code)
{
	return kernel_pointer_size == 4 ? code == ~0LU : code == ~0LLU;
}


static uint64_t get_buffer_value(void const * buffer, size_t index)
{
	if (kernel_pointer_size == 4) {
		uint32_t const * lbuf = buffer;
		return lbuf[index];
	} else {
		uint64_t const * lbuf = buffer;
		return lbuf[index];
	}
}


static void opd_put_sample(struct opd_image * image, int in_kernel, 
	char const * buffer, size_t index)
{
	vma_t eip = get_buffer_value(buffer, index);
	unsigned long event = get_buffer_value(buffer, index + 1);

	opd_stats[OPD_SAMPLES]++;

	if (in_kernel > 0) {
		struct opd_image * app_image = 0;

		/* We can get a NULL image if it's a kernel thread */
		if (separate_kernel_samples && image)
			app_image = image->app_image;
		verbprintf("Putting kernel sample 0x%llx, counter %lu - application %s\n",
			eip, event, app_image ? app_image->name : "kernel");
		opd_handle_kernel_sample(eip, event, app_image);
	} else if (in_kernel == 0) {
		if (image != NULL) {
			verbprintf("Putting image sample (%s) offset 0x%llx, counter %lu\n",
				image->name, eip, event);
			opd_put_image_sample(image, eip, event);
		} else {
			verbprintf("opd_put_sample() nil image, sample lost\n");
			opd_stats[OPD_NIL_IMAGE]++;
		}
	} else {
		fprintf(stderr, "Cannot determine if we are in kernel mode or not\n");
		exit(EXIT_FAILURE);
	}
}


// FIXME: pid/pgrp filter ?
void opd_process_samples(char const * buffer, size_t count)
{
	unsigned long i = 0;
	unsigned long cpu = 0;
	unsigned long code, pid;
	cookie_t cookie, app_cookie = 0;
	struct opd_image * image = NULL;
	static int in_kernel = -1;

	printf("Reading sample buffer.\n");

	while (i < count) {
		if (!is_escape_code(get_buffer_value(buffer, i))) {
			if (i + 1 == count)
				return;

			opd_put_sample(image, in_kernel, buffer, i);
			i += 2;
			continue;
		}

		// skip ESCAPE_CODE
		if (++i == count)
			return;

		code = get_buffer_value(buffer, i);

		// skip code
		if (++i == count)
			return;
 
		switch (code) {
			case CPU_SWITCH_CODE:
				cpu = get_buffer_value(buffer, i);
				verbprintf("CPU_SWITCH to %lu\n", cpu);
				++i;
				break;

			case COOKIE_SWITCH_CODE:
				cookie = get_buffer_value(buffer, i);
				image = opd_get_image(cookie, app_cookie);
				verbprintf("COOKIE_SWITCH to cookie %llx (%s)\n", cookie, image->name); 
				++i;
				break;
 
			case CTX_SWITCH_CODE:
				pid = get_buffer_value(buffer, i);
				// skip pid
				if (++i == count)
					break;
				app_cookie = get_buffer_value(buffer, i);
				/* This is a corner case - if a kernel sample follows this,
				 * we need to make sure that it is attributed to the
				 * right application, namely the one we just switched into.
				 */
				if (app_cookie)
					image = opd_get_image(app_cookie, app_cookie);
				else
					image = 0;

				verbprintf("CTX_SWITCH to pid %lu, cookie %llx, app %s\n",
					pid, app_cookie, image ? image->name : "kernel");
				++i;
				break;

			case KERNEL_ENTER_SWITCH_CODE:
				verbprintf("KERNEL_ENTER_SWITCH to kernel\n");
				in_kernel = 1;
				break;

			case KERNEL_EXIT_SWITCH_CODE:
				verbprintf("KERNEL_EXIT_SWITCH to user-space\n");
				in_kernel = 0;
				break;

			case MODULE_LOADED_CODE:
				verbprintf("MODULE_LOADED_CODE\n");
				opd_reread_module_info();
				break;

			default:
				verbprintf("Unknown code %lx\n", code);
				exit(EXIT_FAILURE);
				break;
		}
	}
}
