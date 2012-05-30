/*
 * @file pe_profiling/operf_kernel.h
 * This file is based on daemon/opd_kernel and is used for
 * dealing with the kernel and kernel module samples.
 *
 * @remark Copyright 2011 OProfile authors
 * @remark Read the file COPYING
 *
 * Created on: Dec 12, 2011
 * @author Maynard Johnson
 * (C) Copyright IBM Corp. 2011
 */

#ifndef OPERF_KERNEL_H_
#define OPERF_KERNEL_H_

#include "op_types.h"
#include "op_list.h"


/** create the kernel image */
void operf_create_vmlinux(char const * name, char const * arg);

/** Describes a kernel module or vmlinux itself */
struct operf_kernel_image {
	char * name;
	vma_t start;
	vma_t end;
	struct list_head list;
};

/** Find a kernel_image based upon the given pc address. */
struct operf_kernel_image *
operf_find_kernel_image(vma_t pc);

/** Return the name field of the stored vmlinux_image. */
const char * operf_get_vmlinux_name(void);

/** Create a kernel image for a kernel module and place it on the
 * module_list.
 */
void operf_create_module(char const * name, vma_t start, vma_t end);

/** Free resources in modules list.
 *
 */
void operf_free_modules_list(void);



#endif /* OPERF_KERNEL_H_ */
