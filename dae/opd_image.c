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
#include "opd_printf.h"
#include "opd_sample_files.h"

#include "op_file.h"
#include "op_config_24.h"
#include "op_mangle.h"
#include "op_libiberty.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

extern uint op_nr_counters;
extern int separate_samples;

/* The kernel image is treated separately */
struct opd_image * kernel_image;
/* maintained for statistics purpose only */
unsigned int nr_images=0;

/* list of images */
static struct list_head opd_images = { &opd_images, &opd_images };
/* Images which belong to the same hash, more than one only if separate_samples
 *  == 1, are accessed by hash code and linked through the hash_next member
 * of opd_image. Hash-less image must be searched through opd_images list */
static struct opd_image * images_with_hash[OP_HASH_MAP_NR];

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

	/* image callback is allowed to delete the current pointer so on
	 * use list_for_each_safe rather list_for_each */
	list_for_each_safe(pos, pos2, &opd_images) {
		struct opd_image * image =
			list_entry(pos, struct opd_image, list_node);

		image_cb(image);
	}

	image_cb(kernel_image);
}
 

/**
 * opd_init_image - init an image sample file
 * @param image  image to init file for
 * @param hash  hash of image
 * @param app_name  the application name where belongs this image
 * @param name  name of the image to add
 * @param kernel  is the image a kernel/module image
 *
 * image at funtion entry is uninitialised
 * name is copied i.e. should be GC'd separately from the
 * image structure if appropriate.
 *
 * Initialise an opd_image struct for the image image
 * without opening the associated samples files. At return
 * the image is partially initialized.
 */
static void opd_init_image(struct opd_image * image, char const * name,
			   int hash, char const * app_name, int kernel)
{
	list_init(&image->list_node);
	image->hash_next = NULL;
	image->name = xstrdup(name);
	image->kernel = kernel;
	image->hash = hash;
	image->app_name = app_name ? xstrdup(app_name) : NULL;
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

	for (i = 0; i < op_nr_counters; i++) {
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


/**
 * opd_create_image - create an image
 * @param name of image
 *
 * Create and initialise an image without adding it
 * to the image lists.
 */
struct opd_image * opd_create_image(char const * name)
{
	struct opd_image * image = xmalloc(sizeof(struct opd_image));
	opd_init_image(image, name, -1, NULL, 1);
	opd_open_image(image);
	return image;
}


/**
 * opd_add_image - add an image to the image structure
 * @param name  name of the image to add
 * @param hash  hash of image
 * @param app_name  the application name where belongs this image
 * @param kernel  is the image a kernel/module image
 *
 * Add an image to the image structure named name.
 *
 * name is copied i.e. should be GC'd separately from the
 * image structure if appropriate.
 *
 * The new image pointer is returned.
 */
static struct opd_image * opd_add_image(char const * name, int hash, char const * app_name, int kernel)
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
 * is_same_image - check for identical image
 * @param image  image to compare
 * @param name  name of image
 * @param app_name image must belong to this application name
 *
 * on entry caller have checked than strcmp(image->name, name) == 0
 * return 0 if the couple (name, app_name) refers to same image
 */
static int is_same_image(struct opd_image const * image, char const * app_name)
{
	/* correctness is really important here, if we fail to recognize
	 * identical image we will open/mmap multiple time the same samples
	 * files which is not supported by the kernel, strange assertion
	 * failure in libfd is a typical symptom of that */

	/* if separate_samples, the comparison made by caller is sufficient */
	if (!separate_samples)
		return 0;

	if (image->app_name == NULL && app_name == NULL)
		return 0;

	if (image->app_name != NULL && app_name != NULL &&
	    !strcmp(image->app_name, app_name))
		return 0;

	/* /proc parsed image come with a non null app_name but notification
	 * for application itself come with a null app_name, in this case
	 * the test above fail so check for this case. */
	if (image->app_name && !app_name && !strcmp(image->app_name, image->name))
		return 0;

	return 1;
}

/**
 * opd_find_image - find an image
 * @param name  name of image to find
 * @param hash  hash of image to find
 * @param app_name  the application name where belongs this image
 *
 * Returns the image pointer for the file specified by name, or %NULL.
 */
static struct opd_image * opd_find_image(char const * name, int hash, char const * app_name)
{
	struct opd_image * image = 0; /* supress warn non initialized use */
	struct list_head * pos;

	/* We make here a linear search through the whole image list. There is no need
	 * to improve performance, only /proc parsed app are hashless and when they
	 * are found one time by this function they receive a valid hash code.
	 */

	list_for_each(pos, &opd_images) {

		image = list_entry(pos, struct opd_image, list_node);

		if (!strcmp(image->name, name)) {
			if (!is_same_image(image, app_name))
				break;
		}
	}

	if (pos != &opd_images) {
		/* we can have hashless images from /proc/pid parsing, modules
		 * are handled in a separate list */
		if (hash != -1) {
			image->hash = hash;
			if (image->hash_next) {	/* paranoia check */
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
 * @param hash  hash of the image to get
 * @param app_name  the application name where belongs this image
 *
 * Get the image specified by hash and app_name
 * if present, else return %NULL
 */
struct opd_image * opd_get_image_by_hash(int hash, char const * app_name)
{
	struct opd_image * image;
	for (image = images_with_hash[hash]; image != NULL; image = image->hash_next) {
		if (!is_same_image(image, app_name))
			break;
	}

	return image;
}


/**
 * opd_get_image - get an image from the image structure
 * @param name  name of image
 * @param hash  hash of the image to get
 * @param app_name  the application name where belongs this image
 * @param kernel  is the image a kernel/module image
 *
 * Get the image specified by the file name name from the
 * image structure. If it is not present, the image is
 * added to the structure. In either case, the image number
 * is returned.
 */
struct opd_image * opd_get_image(char const * name, int hash, char const * app_name, int kernel)
{
	struct opd_image * image;
	if ((image = opd_find_image(name, hash, app_name)) == NULL)
		image = opd_add_image(name, hash, app_name, kernel);

	return image;
}
