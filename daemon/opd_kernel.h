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

void opd_init_kernel_image(void);
void opd_parse_kernel_range(char const * arg);
void opd_clear_module_info(void);
void opd_handle_kernel_sample(unsigned long eip, u32 counter);
int opd_eip_is_kernel(unsigned long eip);

#endif /* OPD_KERNEL_H */
