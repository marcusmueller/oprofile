/**
 * @file daemon/opd_image.h
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
#include "db_hash.h"

#include <time.h>

/**
 * A binary (library, application, kernel or module)
 * is represented by a struct opd_image.
 */
struct opd_image {
	/* name of this image */
	char * name;
	/* name of the owning app or "" */
	char * app_name;
	/* cookie value for this image if any */
	cookie_t cookie;
	/* cookie value of the owning app or 0 */
	cookie_t app_cookie;
	/* hash table link */
	struct list_head hash_list;
	/* opened sample files */
	samples_db_t sample_files[OP_MAX_COUNTERS];
	/* time of last modification */
	time_t mtime;
	/* kernel image or not */
	int kernel;
};

typedef void (*opd_image_cb)(struct opd_image *);
void opd_for_each_image(opd_image_cb imagecb);

void opd_put_image_sample(struct opd_image * image, vma_t offset, int counter);
 
void opd_image_cleanup(void);

void opd_init_images(void);
 
struct opd_image * opd_get_kernel_image(char const * name);
 
struct op_sample;
 
void opd_process_samples(char const * buffer, size_t count);

#endif /* OPD_IMAGE_H */
