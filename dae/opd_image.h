/**
 * @file dae/opd_image.h
 * Management of binary images
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author John Levon
 * @author Philippe Elie
 */

#ifndef OPD_IMAGE_H
#define OPD_IMAGE_H

#include "op_list.h"
#include "op_hw_config.h"
#include "op_types.h"
#include "odb_hash.h"

#include <time.h>

/**
 * A binary (library, application, kernel or module)
 * is represented by a struct opd_image.
 */
struct opd_image {
	/* all image image are linked in a list through this member */
	struct list_head list_node;
	/* used to link image with a valid hash, we never destroy image so a
	 * simple link is necessary */
	struct opd_image * hash_next;
	samples_odb_t sample_files[OP_MAX_COUNTERS];
	int hash;
	/* name of this image */
	char * name;
	/* the application name where belongs this image, NULL if image has
	 * no owner (such as vmlinux or module) */
	char * app_name;
	/* time of last modification */
	time_t mtime;
	/* kernel image or not */
	int kernel;
};

typedef void (*opd_image_cb)(struct opd_image *);
void opd_for_each_image(opd_image_cb imagecb);

void opd_image_cleanup(void);

struct opd_image * opd_get_kernel_image(char const * name, char const * app_name);

void opd_init_images(void);
struct opd_image * opd_get_image(char const * name, int hash, char const * app_name, int kernel);
void opd_check_image_mtime(struct opd_image * image);
struct opd_image * opd_get_image_by_hash(int hash, char const * app_name);

#endif /* OPD_IMAGE_H */
