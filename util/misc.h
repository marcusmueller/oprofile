/* COPYRIGHT (C) 2001 by various authors
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59 Temple
 * Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * Part written by John Levon and P. Elie
 */

#ifndef MISC_H
#define MISC_H

#include <stddef.h>

#include "../config.h"

#ifdef HAVE_LIBIBERTY_H
#include <libiberty.h>
#endif

#include "../op_user.h"

#ifdef MALLOC_OK
#define OP_ATTRIB_MALLOC	__attribute__((malloc))
#else
#define OP_ATTRIB_MALLOC
#endif

#ifdef __cplusplus
extern "C" {
#endif

char *opd_simplify_pathname(char *path);
char *opd_relative_to_absolute_path(const char *path, const char *base_dir);

#ifndef HAVE_LIBIBERTY_H
/* Set the program name used by xmalloc.  */
void xmalloc_set_program_name(const char *);

/* Allocate memory without fail.  If malloc fails, this will print a
   message to stderr (using the name set by xmalloc_set_program_name,
   if any) and then call xexit.  */
void * xmalloc(size_t) OP_ATTRIB_MALLOC;

/* Reallocate memory without fail.  This works like xmalloc.  Note,
   realloc type functions are not suitable for attribute malloc since
   they may return the same address across multiple calls. */
void * xrealloc(void *, size_t);

/* Allocate memory without fail and set it to zero.  This works like xmalloc */
void * xcalloc(size_t, size_t) OP_ATTRIB_MALLOC;

/* Copy a string into a memory buffer without fail.  */
char *xstrdup(const char *) OP_ATTRIB_MALLOC;
#endif	/* !LIBIBERTY_H */

char *opd_simplify_pathname(char *path);
char *opd_relative_to_absolute_path(const char *path, const char *base_dir);

#ifdef __cplusplus
}
#endif

#endif /* !MISC_H */
