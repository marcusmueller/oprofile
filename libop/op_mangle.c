/**
 * @file op_mangle.c
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 * 
 * @author John Levon <moz@compsoc.man.ac.uk>
 * @author Philippe Elie <phil_el@wanadoo.fr>
 */

#include "op_mangle.h"
 
#include <string.h>
#include "op_libiberty.h"
 
#include "op_sample_file.h"
#include "op_config.h"
 
/**
 * op_mangle_filename - mangle a file filename
 * @param image_name  a path name to the image file
 * @param app_name  a path name for the owner image
 * of this image or %NULL if no owner exist
 *
 * Replace any path separator characters with %OPD_MANGLE_CHAR.
 *
 * Returns a char* pointer to the mangled string. Caller
 * is responsible for freeing this string.
 *
 */
char * op_mangle_filename(char const * image_name, char const * app_name)
{
	char * mangled;
	char * c;
	size_t len;

	len = strlen(OP_SAMPLES_DIR) + 2 + strlen(image_name) + 32;
	if (app_name) {
		len += strlen(app_name) + 2;
	}

	mangled = xmalloc(len);
	
	strcpy(mangled, OP_SAMPLES_DIR);
	strcat(mangled, "/");

	c = mangled + strlen(mangled);

	if (app_name) {
		strcat(mangled, app_name);
		/* a double OPD_MANGLE_CHAR used as marker ? */
		strcat(mangled, "//");
	}

	strcat(mangled, image_name);

	for ( ; *c != '\0'; ++c) {
		if (*c == '/')
			*c = OPD_MANGLE_CHAR;
	}

	return mangled;
}
