/* $Id: compat.c,v 1.7 2002/03/01 19:23:20 movement Exp $ */
/* COPYRIGHT (C) 2000 THE VICTORIA UNIVERSITY OF MANCHESTER and John Levon
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
 */

#include "op_dcache.h"
#include <linux/ioport.h>

#ifdef NEED_2_2_DENTRIES
 
/* note - assumes you only test for NULL, and not
 * actually care about the return value */
void *compat_request_region (unsigned long start, unsigned long n, const char *name)
{
        if (check_region (start, n) != 0)
                return NULL;
        request_region (start, n, name);
        return (void *) 1;
}
 
int wind_dentries_2_2(struct dentry *dentry)
{
	struct dentry * root = current->fs->root;

	if (dentry->d_parent != dentry && list_empty(&dentry->d_hash))
		return 0;

	for (;;) {
		struct dentry * parent;

		if (dentry == root)
			break;
 
		dentry = dentry->d_covers;
		parent = dentry->d_parent;
 
		if (dentry == parent)
			break;
 
		push_dname(&dentry->d_name);
 
		dentry = parent;
	}

	return 1;
}
 
/* called with note_lock held */
uint do_path_hash_2_2(struct dentry *dentry)
{
	/* BKL is already taken */

	return do_hash(dentry, 0, 0, 0);
}

#endif /* NEED_2_2_DENTRIES */
