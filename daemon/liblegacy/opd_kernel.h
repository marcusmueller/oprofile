/**
 * @file dae/opd_kernel.h
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

struct opd_proc;

void opd_init_kernel_image(void);
void opd_parse_kernel_range(char const * arg);
void opd_clear_module_info(void);
void opd_handle_kernel_sample(unsigned long eip, u32 counter);
int opd_eip_is_kernel(unsigned long eip);
void opd_add_kernel_map(struct opd_proc * proc, unsigned long eip);

#endif /* OPD_KERNEL_H */
