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

char * op_mangle_filename(struct mangle_values const * values)
{
	char * mangled;
	size_t len;
	char const * image_name = values->image_name;
	char const * dep_name = values->dep_name;

	len = strlen(OP_SAMPLES_CURRENT_DIR) + strlen(values->image_name)
	      + 1 + strlen(values->event_name) + 1;

	if (values->flags & MANGLE_DEP_NAME) {
		len += strlen(values->dep_name) + 1;

		/* PP:3 image_name and dep_name are reversed when
		 * profiling with --separate */
		image_name = values->dep_name;
		dep_name = values->image_name;
	}

	/* provision for tgid, tid, unit_mask, cpu and three {root}, {dep} or
	 * {kern} marker */
	len += 128;	/* FIXME: too ugly */

	mangled = xmalloc(len);

	strcpy(mangled, OP_SAMPLES_CURRENT_DIR);

	if ((values->flags & MANGLE_KERNEL) && !strchr(image_name, '/')) {
		strcat(mangled, "{kern}" "/");
	} else {
		strcat(mangled, "{root}" "/");
	}

	strcat(mangled, image_name);
	strcat(mangled, "/");

	if (values->flags & MANGLE_DEP_NAME) {
		strcat(mangled, "{dep}" "/");
		if ((values->flags & MANGLE_KERNEL)
		    && !strchr(image_name, '/')) {
			strcat(mangled, "{kern}" "/");
		} else {
			strcat(mangled, "{root}" "/");
		}
		strcat(mangled, dep_name);
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
