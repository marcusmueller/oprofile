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
		vmlinux_image.name = "no-vmlinux";
		return;
	}

	vmlinux_image.name = xstrdup(name);

	sscanf(arg, "%llx,%llx", &vmlinux_image.start, &vmlinux_image.end);

	cverb << vmisc << "kernel_start = " << hex <<  vmlinux_image.start
	      << "; kernel_end = " << vmlinux_image.end << endl;

	if (!vmlinux_image.start && !vmlinux_image.end) {
		cerr << "error: mis-parsed kernel range: " << hex << vmlinux_image.start
		     << "; kernel_end = " << vmlinux_image.end << endl;
		exit(EXIT_FAILURE);
	}
}


/**
 * Allocate and initialise a kernel image description
 * @param name image name
 * @param start start address
 * @param end end address
 */
static struct operf_kernel_image *
operf_create_module(char const * name, vma_t start, vma_t end)
{
	struct operf_kernel_image * image =(struct operf_kernel_image *) xmalloc(sizeof(struct operf_kernel_image));

	image->name = xstrdup(name);
	image->start = start;
	image->end = end;
	list_add(&image->list, &modules);

	return image;
}


/**
 * Clear and free all kernel image information and reset
 * values.
 */
static void operf_clear_modules(void)
{
	struct list_head * pos;
	struct list_head * pos2;
	struct operf_kernel_image * image;

	list_for_each_safe(pos, pos2, &modules) {
		image = list_entry(pos, struct operf_kernel_image, list);
		if (image->name)
			free(image->name);
		free(image);
	}

	list_init(&modules);

	/* clear out lingering references */
	operf_sfile_clear_kernel();
}


/*
 * each line is in the format:
 *
 * module_name 16480 1 dependencies Live 0xe091e000
 *
 * without any blank space in each field
 */
void operf_reread_module_info(void)
{
	FILE * fp;
	char * line;
	struct operf_kernel_image * image;
	int module_size;
	char ref_count[32+1];
	int ret;
	char module_name[256+1];
	char live_info[32+1];
	char dependencies[4096+1];
	unsigned long long start_address;

	if (no_vmlinux)
		return;

	operf_clear_modules();

	printf("Reading module info.\n");

	fp = op_try_open_file("/proc/modules", "r");

	if (!fp) {
		printf("oprofiled: /proc/modules not readable, "
			"can't process module samples.\n");
		return;
	}

	while (1) {
		line = op_get_line(fp);

		if (!line)
			break;

		if (line[0] == '\0') {
			free(line);
			continue;
		}

		ret = sscanf(line, "%256s %u %32s %4096s %32s %llx",
			     module_name, &module_size, ref_count,
			     dependencies, live_info, &start_address);
		if (ret != 6) {
			printf("bad /proc/modules entry: %s\n", line);
			free(line);
			continue;
		}

		image = operf_create_module(module_name, start_address,
		                          start_address + module_size);

		cverb << vmisc << "module " << image->name << " start " << hex << image->start
		      << "; end " << image->end << endl;


		free(line);
	}

	op_close_file(fp);
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
