/**
 * @file op_libiberty.c
 * Wrapper for libiberty - always use this instead of
 * libiberty.h
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author John Levon
 * @author Philippe Elie
 */

#include <string.h>

#include "op_libiberty.h"

#ifndef HAVE_XCALLOC
/* some system have a valid libiberty without xcalloc */
void * xcalloc(size_t n_elem, size_t sz)
{
	void * ptr = xmalloc(n_elem * sz);

	memset(ptr, '\0', n_elem * sz);

	return ptr;
}
#endif

#ifndef HAVE_XMEMDUP
/**
 * Duplicates a region of memory without fail.  First, alloc_size bytes
 * are allocated, then copy_size bytes from input are copied into
 * it, and the new memory is returned.  If fewer bytes are copied than were
 * allocated, the remaining memory is zeroed.
 *
 */
void * xmemdup (void const * input, size_t copy_size, size_t alloc_size)
{
	void * output = xcalloc(1, alloc_size);

	memcpy(output, input, copy_size);

	return output;
}
#endif
