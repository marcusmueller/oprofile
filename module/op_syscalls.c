/**
 * @file op_syscalls.c
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

extern u32 prof_on;

uint dname_top;
struct qstr **dname_stack;
char * pool_pos;
char * pool_start;
char * pool_end;
 
static uint hash_map_open;
static struct op_hash_index *hash_map;

void oprof_put_note(struct op_note *samp);
void __oprof_put_note(struct op_note *samp);

extern spinlock_t note_lock;
 
/* --------- device routines ------------- */

int is_map_ready(void)
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

/* ------------ system calls --------------- */

struct mmap_arg_struct {
        unsigned long addr;
        unsigned long len;
        unsigned long prot;
        unsigned long flags;
        unsigned long fd;
        unsigned long offset;
};

asmlinkage static int (*old_sys_fork)(struct pt_regs);
asmlinkage static int (*old_sys_vfork)(struct pt_regs);
asmlinkage static int (*old_sys_clone)(struct pt_regs);
asmlinkage static int (*old_sys_execve)(struct pt_regs);
asmlinkage static int (*old_old_mmap)(struct mmap_arg_struct *);
#ifdef HAVE_MMAP2
asmlinkage static long (*old_sys_mmap2)(ulong, ulong, ulong, ulong, ulong, ulong);
#endif
asmlinkage static long (*old_sys_init_module)(const char *, struct module *);
asmlinkage static long (*old_sys_exit)(int);

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

/* called with note_lock held */
static void oprof_output_map(ulong addr, ulong len,
	ulong offset, struct file *file, int is_execve)
{
	struct op_note note;

	if (!prof_on)
		return;

	/* don't bother with /dev/zero mappings etc. */
	if (!len)
		return;

	note.pid = current->pid;
	note.addr = addr;
	note.len = len;
	note.offset = offset;
	note.type = is_execve ? OP_EXEC : OP_MAP;
	note.hash = hash_path(file);
	if (note.hash == -1)
		return;
	/* holding note lock */
	__oprof_put_note(&note);
}

static int oprof_output_maps(struct task_struct *task)
{
	int size=0;
	int is_execve = 1;
	struct mm_struct *mm;
	struct vm_area_struct *map;

	/* we don't need to worry about mm_users here, since there is at
	   least one user (current), and if there's other code using this
	   mm, then mm_users must be at least 2; we should never have to
	   mmput() here. */

	if (!(mm = task->mm))
		goto out;

	lock_mmap(mm);
	spin_lock(&note_lock);
	for (map = mm->mmap; map; map = map->vm_next) {
		if (!(map->vm_flags & VM_EXEC) || !map->vm_file)
			continue;

		oprof_output_map(map->vm_start, map->vm_end-map->vm_start,
			GET_VM_OFFSET(map), map->vm_file, is_execve);
		is_execve = 0;
	}
	spin_unlock(&note_lock);
	unlock_mmap(mm);

out:
	return size;
}

asmlinkage static int my_sys_execve(struct pt_regs regs)
{
	char *filename;
	int ret;

	MOD_INC_USE_COUNT;

	lock_execve();

	filename = getname((char *)regs.ebx);
	if (IS_ERR(filename)) {
		ret = PTR_ERR(filename);
		goto out;
	}
	ret = do_execve(filename, (char **)regs.ecx, (char **)regs.edx, &regs);

	if (!ret) {
		PTRACE_OFF(current);
		oprof_output_maps(current);
	}
 
	putname(filename);

out:
	unlock_execve();
	MOD_DEC_USE_COUNT;
        return ret;
}

static void out_mmap(ulong addr, ulong len, ulong prot, ulong flags,
	ulong fd, ulong offset)
{
	struct file *file;

	lock_out_mmap();
 
	file = fget(fd);
	if (!file)
		goto out;

	spin_lock(&note_lock);
	oprof_output_map(addr, len, offset, file, 0);
	spin_unlock(&note_lock);

	fput(file);

out:
	unlock_out_mmap();
}

#ifdef HAVE_MMAP2
asmlinkage static int my_sys_mmap2(ulong addr, ulong len,
	ulong prot, ulong flags, ulong fd, ulong pgoff)
{
	int ret;

	MOD_INC_USE_COUNT;

	ret = old_sys_mmap2(addr, len, prot, flags, fd, pgoff);

	if ((prot & PROT_EXEC) && ret >= 0)
		out_mmap(ret, len, prot, flags, fd, pgoff << PAGE_SHIFT);
 
	MOD_DEC_USE_COUNT;
	return ret;
}
#endif

asmlinkage static int my_old_mmap(struct mmap_arg_struct *arg)
{
	int ret;

	MOD_INC_USE_COUNT;

	ret = old_old_mmap(arg);

	if (ret >= 0) {
		struct mmap_arg_struct a;

		if (copy_from_user(&a, arg, sizeof(a))) {
			ret = -EFAULT;
			goto out;
		}

		if (a.prot&PROT_EXEC)
			out_mmap(ret, a.len, a.prot, a.flags, a.fd, a.offset);
	}

out:
	MOD_DEC_USE_COUNT;
	return ret;
}

inline static void oprof_report_fork(u16 old, u32 new)
{
	struct op_note note;

	note.type = OP_FORK;
	note.pid = old;
	note.addr = new;
	oprof_put_note(&note);
}

asmlinkage static int my_sys_fork(struct pt_regs regs)
{
	u16 pid = (u16)current->pid;
	int ret;

	MOD_INC_USE_COUNT;

	ret = old_sys_fork(regs);
	if (ret)
		oprof_report_fork(pid,ret);
	MOD_DEC_USE_COUNT;
	return ret;
}

asmlinkage static int my_sys_vfork(struct pt_regs regs)
{
	u16 pid = (u16)current->pid;
	int ret;

	MOD_INC_USE_COUNT;
	ret = old_sys_vfork(regs);
	if (ret)
		oprof_report_fork(pid,ret);
	MOD_DEC_USE_COUNT;
	return ret;
}

asmlinkage static int my_sys_clone(struct pt_regs regs)
{
	u16 pid = (u16)current->pid;
	int ret;

	MOD_INC_USE_COUNT;
	ret = old_sys_clone(regs);
	if (ret)
		oprof_report_fork(pid,ret);
	MOD_DEC_USE_COUNT;
	return ret;
}

asmlinkage static long my_sys_init_module(const char *name_user, struct module *mod_user)
{
	long ret;

	MOD_INC_USE_COUNT;

	ret = old_sys_init_module(name_user, mod_user);

	if (ret >= 0) {
		struct op_note note;

		note.type = OP_DROP_MODULES;
		oprof_put_note(&note);
	}
	MOD_DEC_USE_COUNT;
	return ret;
}

asmlinkage static long my_sys_exit(int error_code)
{
	struct op_note note;

	MOD_INC_USE_COUNT;

	note.type = OP_EXIT;
	note.pid = current->pid;
	oprof_put_note(&note);

	/* this looks UP-dangerous, as the exit sleeps and we don't
	 * have a use count, but in fact its ok as sys_exit is noreturn,
	 * so we can never come back to this non-existent exec page
	 */
	MOD_DEC_USE_COUNT;
	return old_sys_exit(error_code);
}

extern void *sys_call_table[];

void op_save_syscalls(void)
{
	old_sys_fork = sys_call_table[__NR_fork];
	old_sys_vfork = sys_call_table[__NR_vfork];
	old_sys_clone = sys_call_table[__NR_clone];
	old_sys_execve = sys_call_table[__NR_execve];
	old_old_mmap = sys_call_table[__NR_mmap];
#ifdef HAVE_MMAP2
	old_sys_mmap2 = sys_call_table[__NR_mmap2];
#endif
	old_sys_init_module = sys_call_table[__NR_init_module];
	old_sys_exit = sys_call_table[__NR_exit];
}

void op_intercept_syscalls(void)
{
	sys_call_table[__NR_fork] = my_sys_fork;
	sys_call_table[__NR_vfork] = my_sys_vfork;
	sys_call_table[__NR_clone] = my_sys_clone;
	sys_call_table[__NR_execve] = my_sys_execve;
	sys_call_table[__NR_mmap] = my_old_mmap;
#ifdef HAVE_MMAP2
	sys_call_table[__NR_mmap2] = my_sys_mmap2;
#endif
	sys_call_table[__NR_init_module] = my_sys_init_module;
	sys_call_table[__NR_exit] = my_sys_exit;
}

void op_replace_syscalls(void)
{
	sys_call_table[__NR_fork] = old_sys_fork;
	sys_call_table[__NR_vfork] = old_sys_vfork;
	sys_call_table[__NR_clone] = old_sys_clone;
	sys_call_table[__NR_execve] = old_sys_execve;
	sys_call_table[__NR_mmap] = old_old_mmap;
#ifdef HAVE_MMAP2
	sys_call_table[__NR_mmap2] = old_sys_mmap2;
#endif
	sys_call_table[__NR_init_module] = old_sys_init_module;
	sys_call_table[__NR_exit] = old_sys_exit;
}
