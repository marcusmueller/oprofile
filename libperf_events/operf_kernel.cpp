/*
 * @file pe_profiling/operf_kernel.cpp
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

#include <stdio.h>
#include <iostream>
#include <sstream>
#include <unistd.h>
#include <stdlib.h>
#include "operf_kernel.h"
#include "operf_sfile.h"
#include "op_list.h"
#include "op_libiberty.h"
#include "cverb.h"
#include "op_fileio.h"


extern verbose vmisc;
extern bool no_vmlinux;

static LIST_HEAD(modules);

static struct operf_kernel_image vmlinux_image;

using namespace std;

void operf_create_vmlinux(char const * name, char const * arg)
{
	/* vmlinux is *not* on the list of modules */
	list_init(&vmlinux_image.list);

	/* for no vmlinux */
	if (no_vmlinux) {
		vmlinux_image.name = xstrdup("no-vmlinux");
		return;
	}

	vmlinux_image.name = xstrdup(name);

	sscanf(arg, "%llx,%llx", &vmlinux_image.start, &vmlinux_image.end);

	ostringstream message;
	message << "kernel_start = " << hex <<  vmlinux_image.start
	        << "; kernel_end = " << vmlinux_image.end << endl;
	cverb << vmisc << message.str();

	if (!vmlinux_image.start && !vmlinux_image.end) {
		ostringstream message;
		message << "error: mis-parsed kernel range: " << hex << vmlinux_image.start
		        << "; kernel_end = " << vmlinux_image.end << endl;
		cerr << message.str();
		exit(EXIT_FAILURE);
	}
}


/**
 * Allocate and initialise a kernel module image description.
 * @param name image name
 * @param start start address
 * @param end end address
 */
void operf_create_module(char const * name, vma_t start, vma_t end)
{
	struct operf_kernel_image * image =(struct operf_kernel_image *) xmalloc(sizeof(struct operf_kernel_image));

	image->name = xstrdup(name);
	image->start = start;
	image->end = end;
	list_add(&image->list, &modules);
}

void operf_free_modules_list(void)
{
	struct list_head * pos;
	struct list_head * pos2;
	struct operf_kernel_image * image;
	list_for_each_safe(pos, pos2, &modules) {
		image = list_entry(pos, struct operf_kernel_image, list);
		free(image->name);
		list_del(&image->list);
		free(image);
	}

}

/**
 * find a kernel image by PC value
 * @param trans holds PC value to look up
 *
 * find the kernel image which contains this PC.
 *
 * Return %NULL if not found.
 */
struct operf_kernel_image * operf_find_kernel_image(vma_t pc)
{
	struct list_head * pos;
	struct operf_kernel_image * image = &vmlinux_image;

	if (no_vmlinux)
		return image;

	if (image->start <= pc && image->end > pc)
		return image;

	list_for_each(pos, &modules) {
		image = list_entry(pos, struct operf_kernel_image, list);
		if (image->start <= pc && image->end > pc)
			return image;
	}

	return NULL;
}

const char * operf_get_vmlinux_name(void)
{
	return vmlinux_image.name;
}
