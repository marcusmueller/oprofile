/**
 * @file op_dname.c
 * dentry stack walking
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 * 
 * @author John Levon <moz@compsoc.man.ac.uk>
 * @author Philippe Elie <phil_el@wanadoo.fr>
 */

#include <linux/sched.h>
#include <linux/unistd.h>
#include <linux/mman.h>
#include <linux/file.h>

#include "oprofile.h"
#include "op_dcache.h"

/* --------- device routines ------------- */

static uint dname_top;

static struct qstr **dname_stack;
static char * pool_pos;
static char * pool_start;
static char * pool_end;

static ulong hash_map_open;
static struct op_hash_index *hash_map;

unsigned long is_map_ready(void)
{
	return hash_map_open;
}

int oprof_init_hashmap(void)
{
	uint i;

	dname_stack = kmalloc(DNAME_STACK_MAX * sizeof(struct qstr *), GFP_KERNEL);
	if (!dname_stack)
		return -EFAULT;
	dname_top = 0;
	memset(dname_stack, 0, DNAME_STACK_MAX * sizeof(struct qstr *));

	hash_map = rvmalloc(PAGE_ALIGN(OP_HASH_MAP_SIZE));
	if (!hash_map)
		return -EFAULT;

	for (i = 0; i < OP_HASH_MAP_NR; ++i) {
		hash_map[i].name = 0;
		hash_map[i].parent = -1;
	}

	pool_start = (char *)(hash_map + OP_HASH_MAP_NR);
	pool_end = pool_start + POOL_SIZE;
	pool_pos = pool_start;

	/* Ensure than the zero hash map entry is never used, we use this
	 * value as end of path terminator */
	hash_map[0].name = alloc_in_pool("/", 1);
	hash_map[0].parent = 0;

	return 0;
}

void oprof_free_hashmap(void)
{
	kfree(dname_stack);
	rvfree(hash_map, PAGE_ALIGN(OP_HASH_MAP_SIZE));
}

int oprof_hash_map_open(void)
{
	if (test_and_set_bit(0, &hash_map_open))
		return -EBUSY;

	return 0;
}

int oprof_hash_map_release(void)
{
	if (!hash_map_open)
		return -EFAULT;

	clear_bit(0,&hash_map_open);
	return 0;
}

int oprof_hash_map_mmap(struct file *file, struct vm_area_struct *vma)
{
	ulong start = (ulong)vma->vm_start;
	ulong page, pos;
	ulong size = (ulong)(vma->vm_end-vma->vm_start);

	if (size > PAGE_ALIGN(OP_HASH_MAP_SIZE) || (vma->vm_flags & VM_WRITE) || GET_VM_OFFSET(vma))
		return -EINVAL;

	pos = (ulong)hash_map;
	while (size > 0) {
		page = kvirt_to_pa(pos);
		if (REMAP_PAGE_RANGE(vma, start, page, PAGE_SIZE, PAGE_SHARED))
			return -EAGAIN;
		start += PAGE_SIZE;
		pos += PAGE_SIZE;
		size -= PAGE_SIZE;
	}
	return 0;
}


#ifndef NEED_2_2_DENTRIES
int wind_dentries_2_4(struct dentry *dentry, struct vfsmount *vfsmnt, struct dentry *root, struct vfsmount *rootmnt)
{
	struct dentry *d = dentry;
	struct vfsmount *v = vfsmnt;

	/* wind the dentries onto the stack pages */
	for (;;) {
		/* deleted ? */
		if (!IS_ROOT(d) && list_empty(&d->d_hash))
			return 0;

		/* the root */
		if (d == root && v == rootmnt)
			break;

		if (d == v->mnt_root || IS_ROOT(d)) {
			if (v->mnt_parent == v)
				break;
			/* cross the mount point */
			d = v->mnt_mountpoint;
			v = v->mnt_parent;
		}

		push_dname(&d->d_name);

		d = d->d_parent;
	}

	return 1;
}

/* called with note_lock held */
static uint do_path_hash_2_4(struct dentry *dentry, struct vfsmount *vfsmnt)
{
	uint value;
	struct vfsmount *rootmnt;
	struct dentry *root;

	read_lock(&current->fs->lock);
	rootmnt = mntget(current->fs->rootmnt);
	root = dget(current->fs->root);
	read_unlock(&current->fs->lock);

	spin_lock(&dcache_lock);

	value = do_hash(dentry, vfsmnt, root, rootmnt);

	spin_unlock(&dcache_lock);
	dput(root);
	mntput(rootmnt);
	return value;
}
#endif /* NEED_2_2_DENTRIES */

/* called with note_lock held */
uint do_hash(struct dentry *dentry, struct vfsmount *vfsmnt, struct dentry *root, struct vfsmount *rootmnt)
{
	struct qstr *dname;
	uint value = -1;
	uint firsthash;
	uint incr;
	uint parent = 0;
	struct op_hash_index *entry;

	if (!wind_dentries(dentry, vfsmnt, root, rootmnt))
		goto out;

	/* unwind and hash */

	while ((dname = pop_dname())) {
		/* if N is prime, value in [0-N[ and incr = max(1, value) then
		 * iteration: value = (value + incr) % N covers the range [0-N[
		 * in N iterations */
		incr = firsthash = value = name_hash(dname->name, dname->len, parent);
		if (incr == 0)
			incr = 1;

	retry:
		entry = &hash_map[value];
		/* existing entry ? */
		if (streq(get_from_pool(entry->name), dname->name)
			&& entry->parent == parent)
			goto next;

		/* new entry ? */
		if (entry->parent == -1) {
			if (add_hash_entry(entry, parent, dname->name, dname->len))
				goto fullpool;
			goto next;
		}

		/* nope, find another place in the table */
		value = (value + incr) % OP_HASH_MAP_NR;

		if (value == firsthash)
			goto fulltable;

		goto retry;
	next:
		parent = value;
	}

out:
	dname_top = 0;
	return value;
fullpool:
	printk(KERN_ERR "oprofile: string pool exhausted.\n");
	value = -1;
	goto out;
fulltable:
	printk(KERN_ERR "oprofile: component hash table full :(\n");
	value = -1;
	goto out;
}
