/* $Id: oprofile_k.c,v 1.34 2000/12/06 20:39:48 moz Exp $ */
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

#include <linux/sched.h>
#include <linux/unistd.h>
#include <linux/mman.h>
#include <linux/file.h>
#include <linux/wrapper.h>

#include <asm/io.h>

#include "oprofile.h"

/* stuff we have to do ourselves */

#define APIC_DEFAULT_PHYS_BASE 0xfee00000

extern u32 oprof_ready[NR_CPUS];
extern wait_queue_head_t oprof_wait;
extern pid_t pid_filter;
extern pid_t pgrp_filter;
extern u32 prof_on;

/* FIXME: this needs removing if UP oopser gets into mainline */
static void set_pte_phys(ulong vaddr, ulong phys)
{
	pgprot_t prot;
	pgd_t *pgd;
	pmd_t *pmd;
	pte_t *pte;

	pgd = pgd_offset_k(vaddr);
	pmd = pmd_offset(pgd, vaddr);
	pte = pte_offset(pmd, vaddr);
	prot = PAGE_KERNEL;
	if (boot_cpu_data.x86_capability & X86_FEATURE_PGE)
		pgprot_val(prot) |= _PAGE_GLOBAL;
	set_pte(pte, mk_pte_phys(phys, prot));
	__flush_tlb_one(vaddr);
}

void my_set_fixmap(void)
{
	ulong address = __fix_to_virt(FIX_APIC_BASE);

	set_pte_phys (address,APIC_DEFAULT_PHYS_BASE);
}

/* poor man's do_nmi() */

static void mem_parity_error(unsigned char reason, struct pt_regs * regs)
{
	printk("oprofile: Uhhuh. NMI received. Dazed and confused, but trying to continue\n");
	printk("You probably have a hardware problem with your RAM chips\n");

	/* Clear and disable the memory parity error line. */
	reason = (reason & 0xf) | 4;
	outb(reason, 0x61);
}

static void io_check_error(unsigned char reason, struct pt_regs * regs)
{
	ulong i;

	printk("oprofile: NMI: IOCK error (debug interrupt?)\n");
	/* Can't show registers */

	/* Re-enable the IOCK line, wait for a few seconds */
	reason = (reason & 0xf) | 8;
	outb(reason, 0x61);
	i = 2000;
	while (--i) udelay(1000);
	reason &= ~8;
	outb(reason, 0x61);
}

static void unknown_nmi_error(unsigned char reason, struct pt_regs * regs)
{
	/* No MCA check */

	printk("oprofile: Uhhuh. NMI received for unknown reason %02x.\n", reason);
	printk("Maybe you need to boot with nmi_watchdog=0\n");
	printk("Dazed and confused, but trying to continue\n");
	printk("Do you have a strange power saving mode enabled?\n");
}

asmlinkage void my_do_nmi(struct pt_regs * regs, long error_code)
{
	unsigned char reason = inb(0x61);

	/* can't count this NMI */

	if (!(reason & 0xc0)) {
		unknown_nmi_error(reason, regs);
		return;
	}
	if (reason & 0x80)
		mem_parity_error(reason, regs);
	if (reason & 0x40)
		io_check_error(reason, regs);
	/*
	 * Reassert NMI in case it became active meanwhile
	 * as it's edge-triggered.
	 */
	outb(0x8f, 0x70);
	inb(0x71);		/* dummy */
	outb(0x0f, 0x70);
	inb(0x71);		/* dummy */
}

/* --------- map reading stuff ----------- */

static u32 *map_buf;
static u32 nextmapbuf;
static uint map_open;
static uint hash_map_open;
static char *hash_map;

void oprof_put_note(struct op_sample *samp);

/* --------- device routines ------------- */

static u32 output_path_hash(const char *name, uint len);

int oprof_init_hashmap(void)
{
	struct page *page;

	map_buf = vmalloc(OP_MAX_MAP_BUF*sizeof(u32));
	if (!map_buf)
		return -EFAULT;

	hash_map = kmalloc(PAGE_ALIGN(OP_HASH_MAP_SIZE), GFP_KERNEL);
	if (!hash_map)
		return -EFAULT;

	for (page = virt_to_page(hash_map); page < virt_to_page(hash_map+PAGE_ALIGN(OP_HASH_MAP_SIZE)); page++)
		mem_map_reserve(page);

	memset(hash_map, 0, OP_HASH_MAP_SIZE);

	output_path_hash("lib",3);
	output_path_hash("bin",3);
	output_path_hash("usr",3);
	output_path_hash("sbin",4);
	output_path_hash("local",5);
	output_path_hash("X11R6",5);

	return 0;
}

void oprof_free_hashmap(void)
{
	vfree(map_buf);
	kfree(hash_map);
}

int oprof_hash_map_open(void)
{
	if (test_and_set_bit(0,&hash_map_open))
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
	ulong size = (ulong)(vma->vm_end-vma->vm_start);

	if (size > PAGE_ALIGN(OP_HASH_MAP_SIZE) || vma->vm_flags&VM_WRITE || vma->vm_pgoff)
		return -EINVAL;

	if (remap_page_range(vma->vm_start, virt_to_phys(hash_map),
		size, vma->vm_page_prot))
		return -EAGAIN;

	return 0;
}

int oprof_map_open(void)
{
	if (test_and_set_bit(0,&map_open))
		return -EBUSY;

	return 0;
}

int oprof_map_release(void)
{
	if (!map_open)
		return -EFAULT;

	clear_bit(0,&map_open);
	return 0;
}

int oprof_map_read(char *buf, size_t count, loff_t *ppos)
{
	ssize_t max;
	char *data = (char *)map_buf;

	max = OP_MAX_MAP_BUF*sizeof(u32);

	if (!count)
		return 0;

	if (*ppos >= max)
		return -EINVAL;

	if (*ppos + count > max) {
		size_t firstcount = max - *ppos;

		if (copy_to_user(buf, data+*ppos, firstcount))
			return -EFAULT;

		count -= firstcount;
		*ppos = 0;
	}

	if (count > max)
		return -EINVAL;

	data += *ppos;

	if (copy_to_user(buf,data,count))
		return -EFAULT;

	*ppos += count;

	/* wrap around */
	if (*ppos==max)
		*ppos = 0;

	return count;
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
asmlinkage static long (*old_sys_mmap2)(ulong, ulong, ulong, ulong, ulong, ulong);
asmlinkage static long (*old_sys_init_module)(const char *, struct module *);
asmlinkage static long (*old_sys_exit)(int);

inline static void oprof_report_fork(u16 old, u32 new)
{
	struct op_sample samp;

#ifdef PID_FILTER
	if (pgrp_filter && pgrp_filter!=current->pgrp)
		return;
#endif

	samp.count = OP_FORK;
	samp.pid = old;
	samp.eip = new;
	//printk("FORK ");
	oprof_put_note(&samp);
}

asmlinkage static int my_sys_fork(struct pt_regs regs)
{
	u16 pid = (u16)current->pid;
	int ret;

	ret = old_sys_fork(regs);
	if (ret)
		oprof_report_fork(pid,ret);
	return ret;
}

asmlinkage static int my_sys_vfork(struct pt_regs regs)
{
	u16 pid = (u16)current->pid;
	int ret;

	ret = old_sys_vfork(regs);
	if (ret)
		oprof_report_fork(pid,ret);
	return ret;
}

asmlinkage static int my_sys_clone(struct pt_regs regs)
{
	u16 pid = (u16)current->pid;
	int ret;

	ret = old_sys_clone(regs);
	if (ret)
		oprof_report_fork(pid,ret);
	return ret;
}

spinlock_t map_lock = SPIN_LOCK_UNLOCKED;

inline static uint name_hash(const char *name, uint len)
{
	uint hash=0;

	while (len--)
		hash = ((hash << 4) | (hash >> 28)) ^ name[len];

	return hash % OP_HASH_MAP_NR;
}

#define hash_access(hash) (&hash_map[hash*OP_HASH_LINE])

/* called with map_lock, VFS locks as below */
static u32 output_path_hash(const char *name, uint len)
{
	uint firsthash;
	u32 hash;
	uint probe=1;

	if (len>=OP_HASH_LINE) {
		printk(KERN_ERR "Hash component \"%s\" is too long.\n",name);
		return -1;
	}

	hash = firsthash = name_hash(name,len);
	do {
		if (!*hash_access(hash)) {
			strncpy(hash_access(hash), name, len);
			return hash;
		}

		if (!strcmp(hash_access(hash),name))
			return hash;

		hash = (hash+probe) % OP_HASH_MAP_NR;
		probe *= probe;
	} while (hash!=firsthash);

	printk(KERN_ERR "oprofile: hash map is full !\n");
	return -1;
}

/* called with map_lock held */
inline static u32 *map_out32(u32 val)
{
	static u32 count=0;
	u32 * pos=&map_buf[nextmapbuf];
 
	map_buf[nextmapbuf++] = val;
	if (!(++count % OP_MAP_BUF_WATERMARK)) {
		oprof_ready[0] = 2;
		// FIXME: ok under spinlock ? 
		wake_up(&oprof_wait);
	}
	if (nextmapbuf==OP_MAX_MAP_BUF)
		nextmapbuf=0;

	return pos;
}

/* called with map_lock held */
static void do_d_path(struct dentry *dentry, struct vfsmount *vfsmnt, char *buf, u32 *count)
{
	struct vfsmount *rootmnt;
	struct dentry *root;

	/* we do same as d_path, but we only care about the components */

	read_lock(&current->fs->lock);
	rootmnt = mntget(current->fs->rootmnt);
	root = dget(current->fs->root);
	read_unlock(&current->fs->lock);
	spin_lock(&dcache_lock);

	/* has it been deleted */
        if (!IS_ROOT(dentry) && list_empty(&dentry->d_hash))
		return;

        for (;;) {
                struct dentry * parent;

		if (dentry==root && vfsmnt==rootmnt)
			break;

		if (dentry==vfsmnt->mnt_root || IS_ROOT(dentry)) {
			/* Global root? */
			if (vfsmnt->mnt_parent==vfsmnt)
				goto global_root;
			dentry = vfsmnt->mnt_mountpoint;
			vfsmnt = vfsmnt->mnt_parent;
			continue;
		}

		(*count)++;
		map_out32(output_path_hash(dentry->d_name.name, dentry->d_name.len));
		parent = dentry->d_parent;
		dentry = parent;
	}
out:
	spin_unlock(&dcache_lock);
	dput(root);
	mntput(rootmnt);
	return;

global_root:
	/* FIXME: do we want this ? check chroot()ed */
	printk("Global root: %s\n",dentry->d_name.name);
	(*count)++;
	map_out32(output_path_hash(dentry->d_name.name, dentry->d_name.len));
	goto out;
}

/* buf must be PAGE_SIZE bytes large */
/* called with map_lock held */
static int oprof_output_map(ulong addr, ulong len,
	ulong offset, struct file *file, char *buf)
{
	u32 *tot;

	if (!prof_on)
		return 0;

	map_out32(addr);
	map_out32(len);
	map_out32(offset);
	tot = map_out32(0);
	do_d_path(file->f_dentry, file->f_vfsmnt, buf, tot);

	//printk("Map proc %d, 0x%.8lx-0x%.8lx,off0x%lx, bytes %u, tot %u\n",current->pid,addr,addr+len,offset,sizeof(u32)*(4+*tot),*tot);
	return sizeof(u32)*(4+*tot);
}

static int oprof_output_maps(struct task_struct *task)
{
	int size=0;
	char *buffer;
	struct mm_struct *mm;
	struct vm_area_struct *map;
	struct op_sample samp = {0,0,0,};

	buffer = (char *) __get_free_page(GFP_KERNEL);
	if (!buffer)
		return 0;

	/* we don't need to worry about mm_users here, since there is at
	   least one user (current), and if there's other code using this
	   mm, then mm_users must be at least 2; we should never have to
	   mmput() here. */

	if (!(mm=task->mm))
		goto out;

	down(&mm->mmap_sem);
	spin_lock(&map_lock);
	for (map=mm->mmap; map; map=map->vm_next) {
		if (!(map->vm_flags&VM_EXEC) || !map->vm_file)
			continue;

		samp.eip += oprof_output_map(map->vm_start, map->vm_end-map->vm_start,
				map->vm_pgoff<<PAGE_SHIFT, map->vm_file, buffer);
	}
	samp.count = OP_EXEC;
	samp.pid = current->pid;
	//printk("EXECVE bytes %d ",samp.eip);
	oprof_put_note(&samp);
	spin_unlock(&map_lock);
	up(&mm->mmap_sem);

out:
	free_page((ulong)buffer);
	return size;
}

asmlinkage static int my_sys_execve(struct pt_regs regs)
{
	char *filename;
	int ret;

	filename = getname((char *)regs.ebx);
	if (IS_ERR(filename))
		return PTR_ERR(filename);
	ret = do_execve(filename, (char **)regs.ecx, (char **)regs.edx, &regs);

	if (!ret) {
		current->ptrace &= ~PT_DTRACE;

#ifdef PID_FILTER
		if ((!pid_filter || pid_filter==current->pid) &&
		    (!pgrp_filter || pgrp_filter==current->pgrp))
			oprof_output_maps(current);
#else
		oprof_output_maps(current);
#endif 
	}
	putname(filename);
        return ret;
}

static void out_mmap(ulong addr, ulong len, ulong prot, ulong flags,
	ulong fd, ulong offset)
{
	struct op_sample samp;
	struct file *file;
	char *buffer;

	buffer = (char *) __get_free_page(GFP_KERNEL);
	if (!buffer)
		return;

	file = fget(fd);
	if (!file) {
		free_page((ulong)buffer);
		return;
	}

	samp.count = OP_MAP;
	samp.pid = current->pid;
	/* how many bytes to read from map buffer */
	spin_lock(&map_lock);
	samp.eip = oprof_output_map(addr,len,offset,file,buffer);
	//printk("MMAP bytes %d \n",samp.eip);
	oprof_put_note(&samp);
	spin_unlock(&map_lock);

	fput(file);
	free_page((ulong)buffer);
}

asmlinkage static int my_sys_mmap2(ulong addr, ulong len,
	ulong prot, ulong flags, ulong fd, ulong pgoff)
{
	int ret;

	ret = old_sys_mmap2(addr,len,prot,flags,fd,pgoff);

#ifdef PID_FILTER
	if ((pid_filter && current->pid!=pid_filter) ||
	    (pgrp_filter && current->pgrp!=pgrp_filter))
		return ret;
#endif

	if ((prot&PROT_EXEC) && ret >= 0)
		out_mmap(ret,len,prot,flags,fd,pgoff<<PAGE_SHIFT);
	return ret;
}

asmlinkage static int my_old_mmap(struct mmap_arg_struct *arg)
{
	int ret;

	ret = old_old_mmap(arg);

#ifdef PID_FILTER
	if ((pid_filter && current->pid!=pid_filter) ||
	    (pgrp_filter && current->pgrp!=pgrp_filter))
		return ret;
#endif

	if (ret>=0) {
		struct mmap_arg_struct a;

		if (copy_from_user(&a, arg, sizeof(a)))
			goto out;

		if (a.prot&PROT_EXEC)
			out_mmap(ret, a.len, a.prot, a.flags, a.fd, a.offset);
	}
out:
	return ret;
}

asmlinkage static long my_sys_init_module(const char *name_user, struct module *mod_user)
{
	long ret;
	ret = old_sys_init_module(name_user, mod_user);

	if (ret >= 0) {
		struct op_sample samp;

		samp.count = OP_DROP_MODULES;
		samp.pid = 0;
		samp.eip = 0;
		//printk("INIT_MODULE ");
		oprof_put_note(&samp);
	}
	return ret;
}

asmlinkage static long my_sys_exit(int error_code)
{
	struct op_sample samp;

#ifdef PID_FILTER
	if ((pid_filter && current->pid!=pid_filter) ||
	    (pgrp_filter && current->pgrp!=pgrp_filter))
		return old_sys_exit(error_code);
#endif

	samp.count = OP_EXIT;
	samp.pid = current->pid;
	samp.eip = 0;
	//printk("EXIT ");
	oprof_put_note(&samp);

	return old_sys_exit(error_code);
}

extern void *sys_call_table[];

void op_intercept_syscalls(void)
{
	old_sys_fork = sys_call_table[__NR_fork];
	old_sys_vfork = sys_call_table[__NR_vfork];
	old_sys_clone = sys_call_table[__NR_clone];
	old_sys_execve = sys_call_table[__NR_execve];
	old_old_mmap = sys_call_table[__NR_mmap];
	old_sys_mmap2 = sys_call_table[__NR_mmap2];
	old_sys_init_module = sys_call_table[__NR_init_module];
	old_sys_exit = sys_call_table[__NR_exit];

	sys_call_table[__NR_fork] = my_sys_fork;
	sys_call_table[__NR_vfork] = my_sys_vfork;
	sys_call_table[__NR_clone] = my_sys_clone;
	sys_call_table[__NR_execve] = my_sys_execve;
	sys_call_table[__NR_mmap] = my_old_mmap;
	sys_call_table[__NR_mmap2] = my_sys_mmap2;
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
	sys_call_table[__NR_mmap2] = old_sys_mmap2;
	sys_call_table[__NR_init_module] = old_sys_init_module;
	sys_call_table[__NR_exit] = old_sys_exit;
}
