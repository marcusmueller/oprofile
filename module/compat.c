/* $Id: compat.c,v 1.1 2002/01/11 05:24:07 movement Exp $ */
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

#ifdef NEED_2_2_DENTRIES
 
int wind_dentries_2_2(struct dentry *dentry)
{
	struct dentry * root = current->fs->root;

	/* FIXME: correct ? */
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
 
/* called with map_lock held */
uint do_path_hash_2_2(struct dentry *dentry)
{
	uint value;
	struct dentry *root;

	lock_kernel();
	root = dget(current->fs->root);
	value = do_hash(dentry, 0, root, 0);
	dput(root);
	unlock_kernel();
	return value;
}

#endif /* NEED_2_2_DENTRIES */
