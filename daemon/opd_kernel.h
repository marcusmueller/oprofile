/**
 * @file daemon/opd_kernel.h
 * Dealing with the kernel and kernel module samples
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author John Levon
 * @author Philippe Elie
 */

#ifndef OPD_KERNEL_H
#define OPD_KERNEL_H

#include "op_types.h"

struct opd_image;

void opd_init_kernel_image(void);
void opd_parse_kernel_range(char const * arg);
void opd_reread_module_info(void);
void opd_delete_modules(struct opd_image * image);

struct opd_image *
opd_find_kernel_image(vma_t * eip, struct opd_image * app_image);

#endif /* OPD_KERNEL_H */
