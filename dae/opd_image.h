/**
 * @file opd_image.h
 * Management of binary images
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author John Levon <moz@compsoc.man.ac.uk>
 * @author Philippe Elie <phil_el@wanadoo.fr>
 */

#ifndef OPD_IMAGE_H
#define OPD_IMAGE_H

#include "op_list.h"
#include "op_hw_config.h"
#include "op_types.h"
#include "db.h"

#include <time.h>

struct opd_image {
	/* all image image are linked in a list through this member */
	struct list_head list_node;
	/* used to link image with a valid hash, we never destroy image so a
	 * simple link is necessary */
	struct opd_image * hash_next;
	db_tree_t sample_files[OP_MAX_COUNTERS];
	int hash;
	/* the application name where belongs this image, NULL if image has
	 * no owner (such as vmlinux or module) */
	char const * app_name;
	time_t mtime;
	u8 kernel;
	char * name;
};

// FIXME: the wrong file, conceptually - need a for_each_image possibly
void opd_sync_sample_files(void);
void opd_reopen_sample_files(void);
void opd_image_cleanup(void);

struct opd_image * opd_create_image(char const * name);
void opd_init_images(void);
struct opd_image * opd_get_image(char const * name, int hash, char const * app_name, int kernel);
void opd_check_image_mtime(struct opd_image * image);
struct opd_image * opd_get_image_by_hash(int hash, char const * app_name);

#endif /* OPD_IMAGE_H */
