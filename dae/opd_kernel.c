/**
 * @file dae/opd_kernel.c
 * Dealing with the kernel and kernel module samples
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author John Levon
 * @author Philippe Elie
 */

#include "opd_kernel.h"
#include "opd_proc.h"
#include "opd_image.h"
#include "opd_mapping.h"
#include "opd_printf.h"
#include "opd_stats.h"

#include "op_fileio.h"
#include "op_config_24.h"
#include "op_libiberty.h"

#include "p_module.h"
#include <string.h>
#include <stdlib.h>
#include <errno.h>

/* kernel module */
struct opd_module {
	char * name;
	struct opd_image * image;
	unsigned long start;
	unsigned long end;
	struct list_head module_list;
};

extern char * vmlinux;
extern int verbose;
extern int no_vmlinux;
extern unsigned long opd_stats[];

static struct opd_image * kernel_image;

/* kernel and module support */
static unsigned long kernel_start;
static unsigned long kernel_end;
static struct list_head opd_modules = { &opd_modules, &opd_modules };
static unsigned int nr_modules=0;

/**
 * opd_init_kernel_image - initialise the kernel image
 */
void opd_init_kernel_image(void)
{
	/* for no vmlinux */
	if (!vmlinux)
		vmlinux = "no-vmlinux";
	kernel_image = opd_get_kernel_image(vmlinux, NULL);
}


/**
 * opd_parse_kernel_range - parse the kernel range values
 */
void opd_parse_kernel_range(char const * arg)
{
	sscanf(arg, "%lx,%lx", &kernel_start, &kernel_end);

	verbprintf("OPD_PARSE_KERNEL_RANGE: kernel_start = %lx, kernel_end = %lx\n",
		   kernel_start, kernel_end);

	if (kernel_start == 0x0 || kernel_end == 0x0) {
		fprintf(stderr,
			"Warning: mis-parsed kernel range: %lx-%lx\n",
			kernel_start, kernel_end);
		fprintf(stderr, "kernel profiles will be wrong.\n");
	}
}


/**
 * opd_create_module - allocate and initialise a module description
 * @param name module name
 * @param start start address
 * @param end end address
 */
static struct opd_module *
opd_create_module(char * name, unsigned long start, unsigned long end)
{
	struct opd_module * module = xmalloc(sizeof(struct opd_module));

	module->name = xstrdup(name);
	module->image = NULL;
	module->start = start;
	module->end = end;
	list_add(&module->module_list, &opd_modules);

	return module;
}


/**
 * opd_clear_module_info - clear kernel module information
 *
 * Clear and free all kernel module information and reset
 * values.
 */
void opd_clear_module_info(void)
{
	struct list_head * pos;
	struct list_head * pos2;
	struct opd_module * module;

	verbprintf("Removing module list\n");
	list_for_each_safe(pos, pos2, &opd_modules) {
		module = list_entry(pos, struct opd_module, module_list);
		free(module->name);
		free(module);
		list_del(pos);
	}

	opd_clear_kernel_mapping();
}


/**
 * opd_get_module_info - parse mapping information for kernel modules
 *
 * Parse the file /proc/ksyms to read in mapping information for
 * all kernel modules. The modutils package adds special symbols
 * to this file which allows determination of the module image
 * and mapping addresses of the form :
 *
 * __insmod_modulename_Oobjectfile_Mmtime_Vversion
 * __insmod_modulename_Ssectionname_Llength
 *
 * Currently the image file "objectfile" is stored, and details of
 * ".text" sections.
 *
 * There is no query_module API that allow to get directly the pathname
 * of a module so we need to parse all the /proc/ksyms.
 */
static void opd_get_module_info(void)
{
	char * line;
	char * cp, * cp2, * cp3;
	FILE * fp;
	struct opd_module * mod;
	char * modname;
	char * filename;

	nr_modules=0;

	fp = op_try_open_file("/proc/ksyms", "r");

	if (!fp) {
		printf("oprofiled: /proc/ksyms not readable, can't process module samples.\n");
		return;
	}

	verbprintf("Read module info.\n");

	while (1) {
		line = op_get_line(fp);

		if (!line)
			break;

		if (!strcmp("", line)) {
			free(line);
			continue;
		}

		if (strlen(line) < 9) {
			printf("oprofiled: corrupt /proc/ksyms line \"%s\"\n", line);
			goto failure;
		}

		if (strncmp("__insmod_", line + 9, 9)) {
			free(line);
			continue;
		}

		cp = line + 18;
		cp2 = cp;
		while ((*cp2) && !!strncmp("_S", cp2+1, 2) && !!strncmp("_O", cp2+1, 2))
			cp2++;

		if (!*cp2) {
			printf("oprofiled: corrupt /proc/ksyms line \"%s\"\n", line);
			goto failure;
		}

		cp2++;
		/* freed by opd_clear_module_info() or opd_get_module() */
		modname = xmalloc((size_t)((cp2-cp) + 1));
		strncpy(modname, cp, (size_t)((cp2-cp)));
		modname[cp2-cp] = '\0';

		mod = opd_create_module(modname, 0, 0);

		switch (*(++cp2)) {
			case 'O':
				/* get filename */
				cp2++;
				cp3 = cp2;

				while ((*cp3) && !!strncmp("_M", cp3+1, 2))
					cp3++;

				if (!*cp3) {
					free(line);
					continue;
				}

				cp3++;
				filename = xmalloc((size_t)(cp3 - cp2 + 1));
				strncpy(filename, cp2, (size_t)(cp3 - cp2));
				filename[cp3-cp2] = '\0';

				mod->image = opd_get_kernel_image(filename, NULL);
				free(filename);
				break;

			case 'S':
				/* get extent of .text section */
				cp2++;
				if (strncmp(".text_L", cp2, 7)) {
					free(line);
					continue;
				}

				cp2 += 7;
				sscanf(line,"%lx", &mod->start);
				sscanf(cp2,"%lu", &mod->end);
				mod->end += mod->start;
				break;
		}

		free(line);
	}

failure:
	if (line)
		free(line);
	op_close_file(fp);
}
 

/**
 * opd_drop_module_sample - drop a module sample efficiently
 * @param eip  eip of sample
 *
 * This function is called to recover from failing to put a samples even
 * after re-reading /proc/ksyms. It's either a rogue sample, or from a module
 * that didn't create symbols (like in some initrd setups). So we check with
 * query_module() if we can place it in a symbol-less module, and if so create
 * a negative entry for it, to quickly ignore future samples.
 *
 * Problem uncovered by Bob Montgomery <bobm@fc.hp.com>
 *
 */
static void opd_drop_module_sample(unsigned long eip)
{
	char * module_names;
	char * name;
	size_t size = 1024;
	size_t ret;
	uint nr_mods;
	uint mod = 0;

	opd_stats[OPD_LOST_MODULE]++;

	module_names = xmalloc(size);
	while (query_module(NULL, QM_MODULES, module_names, size, &ret)) {
		if (errno != ENOSPC) {
			verbprintf("query_module failed: %s\n", strerror(errno));
			return;
		}
		size = ret;
		module_names = xrealloc(module_names, size);
	}

	nr_mods = ret;
	name = module_names;

	while (mod < nr_mods) {
		struct module_info info;
		if (!query_module(name, QM_INFO, &info, sizeof(info), &ret)) {
			if (eip >= info.addr && eip < info.addr + info.size) {
				verbprintf("Sample from unprofilable module %s\n", name);
				opd_create_module(name, info.addr, info.addr + info.size);
				goto out;
			}
		}
		mod++;
		name += strlen(name) + 1;
	}
out:
	if (module_names)
		free(module_names);
}


/**
 * opd_find_module_by_eip - find a module by its eip
 * @param eip  EIP value
 *
 * find in the modules container the module which
 * contain this eip return %NULL if not found.
 * caller must check than the module image is valid
 */
static struct opd_module * opd_find_module_by_eip(unsigned long eip)
{
	struct list_head * pos;
	struct opd_module * module;

	list_for_each(pos, &opd_modules) {
		module = list_entry(pos, struct opd_module, module_list);
		if (module->start && module->end &&
		    module->start <= eip && module->end > eip)
			return module;
	}

	return NULL;
}



/**
 * opd_handle_module_sample - process a module sample
 * @param eip  EIP value
 * @param counter  counter number
 *
 * Process a sample in module address space. The sample eip
 * is matched against module information. If the search was
 * successful, the sample is output to the relevant file.
 *
 * Note that for modules and the kernel, the offset will be
 * wrong in the file, as it is not a file offset, but the offset
 * from the text section. This is fixed up in pp.
 *
 * If the sample could not be located in a module, it is treated
 * as a kernel sample.
 */
static void opd_handle_module_sample(unsigned long eip, u32 counter)
{
	struct opd_module * module;

	module = opd_find_module_by_eip(eip);
	if (!module) {
		/* not found in known modules, re-read our info and retry */
		opd_clear_module_info();
		opd_get_module_info();

		module = opd_find_module_by_eip(eip);
	}

	if (module) {
		if (module->image != NULL) {
			opd_stats[OPD_MODULE]++;
			opd_put_image_sample(module->image,
					     eip - module->start, counter);
		} else {
			opd_stats[OPD_LOST_MODULE]++;
			verbprintf("No image for sampled module %s\n",
				   module->name);
		}
	} else {
		opd_drop_module_sample(eip);
	}
}

/**
 * opd_handle_kernel_sample - process a kernel sample
 * @param eip  EIP value of sample
 * @param counter  counter number
 *
 * Handle a sample in kernel address space or in a module. The sample is
 * output to the relevant image file.
 */
void opd_handle_kernel_sample(unsigned long eip, u32 counter)
{
	if (no_vmlinux || eip < kernel_end) {
		opd_stats[OPD_KERNEL]++;
		opd_put_image_sample(kernel_image, eip - kernel_start, counter);
		return;
	}

	/* in a module */
	opd_handle_module_sample(eip, counter);
}
 
/**
 * opd_eip_is_kernel - is the sample from kernel/module space
 * @param eip  EIP value
 *
 * Returns %1 if eip is in the address space starting at
 * kernel_start, %0 otherwise.
 */
int opd_eip_is_kernel(unsigned long eip)
{
	/* kernel_start == 0 when vm_nolinux != 0 */
	return kernel_start && eip >= kernel_start;
}

/**
 * opd_add_kernel_map - add a module or kernel maps to a proc struct
 *
 * @param proc owning proc of the new mapping
 * @param eip eip inside the new mapping
 *
 * We assume than eip >= kernel_start
 *
 */
void opd_add_kernel_map(struct opd_proc * proc, unsigned long eip)
{
	struct opd_module * module;
	struct opd_image * image;
	char const * app_name;

	app_name = opd_app_name(proc);
	if (!app_name) {
		verbprintf("un-named proc for pid %d\n", proc->pid);
		return;
	}


	if (eip < kernel_end) {
		image = opd_get_kernel_image(vmlinux, app_name);
		if (!image) {
			verbprintf("Can't create image for %s %s\n", vmlinux, app_name);
			return;
		}

		opd_add_mapping(proc, image, kernel_start, 0, kernel_end);
		return;
	}

	module = opd_find_module_by_eip(eip);
	if (!module) {
		/* not found in known modules, re-read our info and retry */
		opd_clear_module_info();
		opd_get_module_info();

		module = opd_find_module_by_eip(eip);
	}

	if (module) {
		/* module->name is only the module name not the full path */
		char const * module_name = 0;
		if (module->image)
			module_name = module->image->name;
		if (!module_name) {
			verbprintf("unable to get path name for module %s\n",
			       module->name);
			module_name = module->name;
		}
		image = opd_get_kernel_image(module_name, app_name);
		if (!image) {
			verbprintf("Can't create image for %s %s\n",
			       module->name, app_name);
			return;
		}
		opd_add_mapping(proc, image, module->start, 0, module->end);
	} else {
		opd_drop_module_sample(eip);
	}
}
