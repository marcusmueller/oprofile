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
#include "op_config_24.h"
#include "op_types.h"
#include "odb_hash.h"

#include <time.h>

struct opd_24_sfile;

/**
 * A binary (library, application, kernel or module)
 * is represented by a struct opd_image.
 */
struct opd_image {
	/* all image image are linked in a list through this member */
	struct list_head hash_next;
	/* how many time this opd_image is referenced */
	int ref_count;
	struct opd_24_sfile * sfiles[OP_MAX_COUNTERS][NR_CPUS];
	/* name of this image */
	char * name;
	/* the application name where belongs this image, NULL if image has
	 * no owner (such as vmlinux or module) */
	char * app_name;
	/* thread id, on 2.2 kernel always == to tgid */
	pid_t tid;
	/* thread group id  */
	pid_t tgid;
	/* time of last modification */
	time_t mtime;
	/* kernel image or not */
	int kernel;
	/* non zero if this image must be profiled */
	int ignored;
};

typedef void (*opd_image_cb)(struct opd_image *);
void opd_for_each_image(opd_image_cb imagecb);

void opd_init_images(void);
void opd_delete_image(struct opd_image * image);

struct opd_image * opd_get_kernel_image(char const * name, char const * app_name, pid_t tid, pid_t tgid);

void opd_init_images(void);
struct opd_image * opd_get_image(char const * name, char const * app_name, int kernel, pid_t tid, pid_t tgid);

#endif /* OPD_IMAGE_H */
