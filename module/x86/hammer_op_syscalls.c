/**
 * @file hammer_op_syscalls.c
 * Tracing of system calls
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author John Levon
 * @author Philippe Elie
 * @author Dave Jones
 * @author Graydon Hoare
 */

#include <linux/sched.h>
#include <linux/unistd.h>
#include <linux/mman.h>
#include <linux/file.h>
#include <linux/mm.h>
#include <asm/ia32_unistd.h>

#include "oprofile.h"

void oprof_put_note(struct op_note *samp);
void __oprof_put_note(struct op_note *samp);

extern spinlock_t note_lock;

/* ------------ system calls --------------- */

struct mmap_arg_struct {
	unsigned int addr;
	unsigned int len;
	unsigned int prot;
	unsigned int flags;
	unsigned int fd;
	unsigned int offset;
};

asmlinkage static long (*old_sys_fork)(struct pt_regs);
asmlinkage static int (*old_sys32_fork)(struct pt_regs);
asmlinkage static long (*old_sys_vfork)(struct pt_regs);
asmlinkage static int (*old_sys32_vfork)(struct pt_regs);
asmlinkage static long (*old_sys_clone)(ulong, ulong, struct pt_regs);
asmlinkage static int (*old_sys32_clone)(uint, uint, struct pt_regs);
asmlinkage static long (*old_sys_execve)(char *, char **, char **, struct pt_regs);
asmlinkage static long (*old_sys32_execve)(char *, char **, char **, struct pt_regs);
asmlinkage static long (*old_old_mmap)(ulong, ulong, ulong, ulong, ulong, ulong);
asmlinkage static __u32 (*old_old32_mmap)(struct mmap_arg_struct *arg);
asmlinkage static int (*old_sys32_exit)(int);
asmlinkage long my_stub_fork(struct pt_regs);
asmlinkage long my_stub_vfork(struct pt_regs);
asmlinkage long my_stub_clone(ulong, ulong, struct pt_regs);
asmlinkage long my_stub_execve(char *, char **, char **, struct pt_regs);
asmlinkage int my_stub32_fork(struct pt_regs);
asmlinkage int my_stub32_vfork(struct pt_regs);
asmlinkage int my_stub32_clone(ulong, ulong, struct pt_regs);
asmlinkage int my_stub32_execve(char *, char **, char **, struct pt_regs);
asmlinkage static int (*old_sys32_mmap2)(uint, uint, uint, uint, uint, uint);

asmlinkage static long (*old_sys_init_module)(const char *, struct module *);
asmlinkage static long (*old_sys_exit)(int);

/* called with note_lock held */
static void oprof_output_map(ulong addr, ulong len,
	ulong offset, struct file *file, int is_execve)
{
	struct op_note note;

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

inline static void oprof_report_fork(u32 old, u32 new)
{
	struct op_note note;

	note.type = OP_FORK;
	note.pid = old;
	note.addr = new;
	oprof_put_note(&note);
}


asmlinkage long my_sys_execve(char *name, char **argv,char **envp, struct pt_regs regs)
{
	char *filename;
	long ret;

	MOD_INC_USE_COUNT;

	lock_execve();

	filename = getname(name);
	if (IS_ERR(filename)) {
		ret = PTR_ERR(filename);
		goto out;
	}
	ret = do_execve(filename, argv, envp, &regs);

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

static int nargs(u32 src, char **dst) 
{ 
	int cnt;
	u32 val; 

	cnt = 0; 
	do { 		
		int ret = get_user(val, (__u32 *)(u64)src); 
		if (ret)
			return ret;
		if (dst)
			dst[cnt] = (char *)(u64)val; 
		cnt++;
		src += 4;
		if (cnt >= (MAX_ARG_PAGES*PAGE_SIZE)/sizeof(void*))
			return -E2BIG; 
	} while(val); 
	if (dst)
		dst[cnt-1] = 0; 
	return cnt; 
} 

int my_sys32_execve(char *name, u32 argv, u32 envp, struct pt_regs regs)
{ 
	mm_segment_t oldseg; 
	char **buf; 
	int na,ne;
	int ret;
	unsigned sz; 

	MOD_INC_USE_COUNT;
	
	na = nargs(argv, NULL); 
	if (na < 0) 
		return -EFAULT; 
	ne = nargs(envp, NULL); 
	if (ne < 0) 
		return -EFAULT; 

	sz = (na+ne)*sizeof(void *); 
	if (sz > PAGE_SIZE) 
		buf = vmalloc(sz); 
	else
		buf = kmalloc(sz, GFP_KERNEL); 
	if (!buf)
		return -ENOMEM; 
	
	ret = nargs(argv, buf);
	if (ret < 0)
		goto free;

	ret = nargs(envp, buf + na); 
	if (ret < 0)
		goto free; 

	name = getname(name); 
	ret = PTR_ERR(name); 
	if (IS_ERR(name))
		goto free; 

	oldseg = get_fs(); 
	set_fs(KERNEL_DS);
	ret = do_execve(name, buf, buf+na, &regs);  
	if (!ret) {
		PTRACE_OFF(current);
		oprof_output_maps(current);
	}
	set_fs(oldseg); 

	if (ret == 0)
		current->ptrace &= ~PT_DTRACE;

	putname(name);
 
free:
	if (sz > PAGE_SIZE)
		vfree(buf); 
	else
		kfree(buf);
	MOD_DEC_USE_COUNT;
	return ret; 
} 


static long my_old_mmap(unsigned long addr, unsigned long len,
			unsigned long prot, unsigned long flags,
			unsigned long fd, unsigned long off)
{
	long ret;

	MOD_INC_USE_COUNT;

	ret = old_old_mmap(addr, len, prot, flags, fd, off);

	if (ret >= 0) {
		if (prot&PROT_EXEC)
			out_mmap(ret, len, prot, flags, fd, off);
	}

	MOD_DEC_USE_COUNT;
	return ret;
}


asmlinkage __u32
my_sys32_mmap(struct mmap_arg_struct *arg)
{
	int ret;

	MOD_INC_USE_COUNT;

	ret = old_old32_mmap(arg);

	if (ret >= 0) {
		struct mmap_arg_struct a;

		if (copy_from_user(&a, arg, sizeof(a))) {
			ret = -EFAULT;
			goto out;
		}

		if (a.prot&PROT_EXEC)
			out_mmap(ret, a.len, a.prot, a.flags | MAP_32BIT, a.fd, a.offset);
	}

out:
	MOD_DEC_USE_COUNT;
	return ret;
}

asmlinkage static int my_sys32_mmap2(ulong addr, ulong len,
	ulong prot, ulong flags, ulong fd, ulong pgoff)
{
	int ret;

	MOD_INC_USE_COUNT;

	ret = old_sys32_mmap2(addr, len, prot, flags, fd, pgoff);

	if ((prot & PROT_EXEC) && ret >= 0)
		out_mmap(ret, len, prot, flags | MAP_32BIT, fd, pgoff << PAGE_SHIFT);

	MOD_DEC_USE_COUNT;
	return ret;
}

asmlinkage long my_sys_fork(struct pt_regs regs)
{
	u32 pid = current->pid;
	long ret;

	MOD_INC_USE_COUNT;

	ret = do_fork(SIGCHLD, regs.rsp, &regs, 0);
	if (ret)
		oprof_report_fork(pid,ret);
	MOD_DEC_USE_COUNT;
	return ret;
}

asmlinkage long my_sys_vfork(struct pt_regs regs)
{
	u32 pid = current->pid;
	long ret;

	MOD_INC_USE_COUNT;
	ret = do_fork(CLONE_VFORK | CLONE_VM | SIGCHLD, regs.rsp, &regs, 0);
	if (ret)
		oprof_report_fork(pid,ret);
	MOD_DEC_USE_COUNT;
	return ret;
}

asmlinkage long my_sys_clone(unsigned long clone_flags, unsigned long newsp, struct pt_regs regs)
{
	u32 pid = current->pid;
	long ret;

	MOD_INC_USE_COUNT;
	ret = do_fork(clone_flags, newsp, &regs, 0);
	if (ret)
		oprof_report_fork(pid,ret);
	MOD_DEC_USE_COUNT;
	return ret;
}

asmlinkage int my_sys32_fork(struct pt_regs regs)
{
	u32 pid = current->pid;
	long ret;

	MOD_INC_USE_COUNT;

	ret = do_fork(SIGCHLD, regs.rsp, &regs, 0);

	if (ret)
		oprof_report_fork(pid,ret);
	MOD_DEC_USE_COUNT;
	return ret;
}

asmlinkage int my_sys32_clone(unsigned int clone_flags, unsigned int newsp, struct pt_regs regs)
{
	u32 pid = current->pid;
	long ret;

	MOD_INC_USE_COUNT;
	ret = do_fork(clone_flags, newsp, &regs, 0);
	if (ret)
		oprof_report_fork(pid,ret);
	MOD_DEC_USE_COUNT;
	return ret;
}
asmlinkage int my_sys32_vfork(struct pt_regs regs)
{
	u32 pid = current->pid;
	long ret;

	MOD_INC_USE_COUNT;
	ret = do_fork(CLONE_VFORK | CLONE_VM | SIGCHLD, regs.rsp, &regs, 0);
	if (ret)
		oprof_report_fork(pid,ret);
	MOD_DEC_USE_COUNT;
	return ret;
}


asmlinkage static long my_sys_init_module(char const * name_user, struct module * mod_user)
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

/* used from do_nmi */
asmlinkage long my_sys_exit(int error_code)
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
extern void *ia32_sys_call_table[];

void op_save_syscalls(void)
{
	old_sys_fork = sys_call_table[__NR_fork];
	old_sys_vfork = sys_call_table[__NR_vfork];
	old_sys_clone = sys_call_table[__NR_clone];
	old_sys_execve = sys_call_table[__NR_execve];
	old_old_mmap = sys_call_table[__NR_mmap];
	old_sys_init_module = sys_call_table[__NR_init_module];
	old_sys_exit = sys_call_table[__NR_exit];
	old_sys32_fork = ia32_sys_call_table[__NR_ia32_fork];
	old_sys32_vfork = ia32_sys_call_table[__NR_ia32_vfork];
	old_sys32_clone = ia32_sys_call_table[__NR_ia32_clone];
	old_sys32_execve = ia32_sys_call_table[__NR_ia32_execve];
	old_old32_mmap = ia32_sys_call_table[__NR_ia32_mmap];
	old_sys32_exit = ia32_sys_call_table[__NR_ia32_exit];
	old_sys32_mmap2 = ia32_sys_call_table[__NR_ia32_mmap2];
}

void op_intercept_syscalls(void)
{
	sys_call_table[__NR_mmap] = my_old_mmap;
	sys_call_table[__NR_init_module] = my_sys_init_module;
	sys_call_table[__NR_exit] = my_sys_exit;
	sys_call_table[__NR_fork] = my_stub_fork;
	sys_call_table[__NR_vfork] = my_stub_vfork;
	sys_call_table[__NR_clone] = my_stub_clone;
	sys_call_table[__NR_execve] = my_stub_execve;
	ia32_sys_call_table[__NR_ia32_fork] = my_stub32_fork;
	ia32_sys_call_table[__NR_ia32_vfork] = my_stub32_vfork;
	ia32_sys_call_table[__NR_ia32_clone] = my_stub32_clone;
	ia32_sys_call_table[__NR_ia32_execve] = my_stub32_execve;
	ia32_sys_call_table[__NR_ia32_mmap] = my_sys32_mmap;
	ia32_sys_call_table[__NR_ia32_exit] = my_sys_exit;
	ia32_sys_call_table[__NR_ia32_mmap2] = my_sys32_mmap2;
}

void op_restore_syscalls(void)
{
	sys_call_table[__NR_fork] = old_sys_fork;
	sys_call_table[__NR_vfork] = old_sys_vfork;
	sys_call_table[__NR_clone] = old_sys_clone;
	sys_call_table[__NR_execve] = old_sys_execve;
	sys_call_table[__NR_mmap] = old_old_mmap;
	sys_call_table[__NR_init_module] = old_sys_init_module;
	sys_call_table[__NR_exit] = old_sys_exit;
	ia32_sys_call_table[__NR_ia32_fork] = old_sys32_fork;
	ia32_sys_call_table[__NR_ia32_vfork] = old_sys32_vfork;
	ia32_sys_call_table[__NR_ia32_clone] = old_sys32_clone;
	ia32_sys_call_table[__NR_ia32_execve] = old_sys32_execve;
	ia32_sys_call_table[__NR_ia32_mmap] = old_old32_mmap;
	ia32_sys_call_table[__NR_ia32_exit] = old_sys32_exit;
	ia32_sys_call_table[__NR_ia32_mmap2] = old_sys32_mmap2;
}
