/**
 * @file op_mangle.c
 * Mangling of sample file names
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author John Levon
 * @author Philippe Elie
 */

#include "op_mangle.h"

#include <string.h>
#include <stdio.h>

#include "op_libiberty.h"

#include "op_sample_file.h"
#include "op_config.h"

static void append_separator(char * dest, int flags, char const * image_name)
{
	if ((flags & MANGLE_KERNEL) && !strchr(image_name, '/')) {
		strcat(dest, "{kern}" "/");
	} else {
		strcat(dest, "{root}" "/");
	}
}

char * op_mangle_filename(struct mangle_values const * values)
{
	char * mangled;
	size_t len;
	/* if dep_name != image_name we need to revert them (and so revert them
	 * unconditionnaly because if they are equal it doesn't hurt to invert
	 * them), see P:3, FIXME: this is a bit weirds, we prolly need to
	 * reword pp_interface */
	char const * image_name = values->dep_name;
	char const * dep_name = values->image_name;

	len = strlen(OP_SAMPLES_CURRENT_DIR) + strlen(values->image_name)
		+ 1 + strlen(values->event_name) 
		+ 1 + strlen(values->dep_name) + 1;

	if (values->flags & MANGLE_CALLGRAPH)
		len += strlen(values->cg_image_name) + 1;

	/* provision for tgid, tid, unit_mask, cpu and some {root}, {dep},
	 * {kern} and {cg} marker */
	/* FIXME: too ugly */
	len += 256;

	mangled = xmalloc(len);

	strcpy(mangled, OP_SAMPLES_CURRENT_DIR);
	append_separator(mangled, values->flags, image_name);
	strcat(mangled, image_name);
	strcat(mangled, "/");

	strcat(mangled, "{dep}" "/");
	append_separator(mangled, values->flags, image_name);
	strcat(mangled, dep_name);
	strcat(mangled, "/");

	if (values->flags & MANGLE_CALLGRAPH) {
		strcat(mangled, "{cg}" "/");
		append_separator(mangled, values->flags, values->cg_image_name);
		strcat(mangled, values->cg_image_name);
		strcat(mangled, "/");
	}

	strcat(mangled, values->event_name);
	sprintf(mangled + strlen(mangled), ".%d.%d.",
	        values->count, values->unit_mask);

	if (values->flags & MANGLE_TGID) {
		sprintf(mangled + strlen(mangled), "%d.", values->tgid);
	} else {
		sprintf(mangled + strlen(mangled), "%s.", "all");
	}

	if (values->flags & MANGLE_TID) {
		sprintf(mangled + strlen(mangled), "%d.", values->tid);
	} else {
		sprintf(mangled + strlen(mangled), "%s.", "all");
	}

	if (values->flags & MANGLE_CPU) {
		sprintf(mangled + strlen(mangled), "%d", values->cpu);
	} else {
		sprintf(mangled + strlen(mangled), "%s", "all");
	}

	return mangled;
}
