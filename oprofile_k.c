#include <linux/config.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/init.h>
#include <linux/delay.h> 
#include <linux/sched.h> 
#include <linux/unistd.h>
#include <linux/mman.h> 
#include <linux/file.h> 

#include <asm/io.h>
 
#include "oprofile.h"
 
/* stuff we have to do ourselves */

#define APIC_DEFAULT_PHYS_BASE 0xfee00000
 
/* FIXME: not up to date */
static void set_pte_phys(unsigned long vaddr, unsigned long phys)
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
	unsigned long address = __fix_to_virt(FIX_APIC_BASE);

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
	unsigned long i;

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

/* map reading stuff */

/* we're ok to fill up one entry (8 bytes) in the buffer.
 * Any more (i.e. all mapping info) goes in the map buffer;
 * the daemon knows how many entries to read depending on
 * the value in the main buffer entry. All entries go into 
 * CPU#0 buffer. Yes, this means we can have "broken" samples
 * in the other buffers. But we can also have them in the
 * hash table anyway, by similar mapping races involving
 * fork()/execve() and re-mapping. 
 */
    
/* each entry is 16-byte aligned */
static struct op_map map_buf[OP_MAX_MAP_BUF];
static unsigned long nextmapbuf; 
static unsigned int map_open; 
 
void oprof_out8(void *buf);
 
asmlinkage static int (*old_sys_fork)(struct pt_regs);
asmlinkage static int (*old_sys_vfork)(struct pt_regs);
asmlinkage static int (*old_sys_clone)(struct pt_regs);
#if 0 
asmlinkage static int (*old_sys_execve)(struct pt_regs);
#endif 
asmlinkage static long (*old_sys_mmap2)(unsigned long, unsigned long, unsigned long,
	unsigned long, unsigned long, unsigned long);
asmlinkage static long (*old_sys_init_module)(const char *, struct module *); 
asmlinkage static long (*old_sys_exit)(int);
 
inline static void oprof_report_fork(u16 oldpid, pid_t newpid)
{
	struct op_sample samp; 

	samp.count = OP_FORK;
	samp.pid = oldpid;
	samp.eip = newpid;
	oprof_out8(&samp);
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
 
int oprof_map_open(void)
{
	if (map_open)
		return -EBUSY;

	map_open=1;
	return 0; 
}

int oprof_map_release(void)
{
	if (!map_open)
		return -EFAULT;

	map_open=0;
	return 0;
}

int oprof_map_read(char *buf, size_t count, loff_t *ppos)
{
	ssize_t max;
	char *data = (char *)map_buf; 

	max = sizeof(struct op_map)*OP_MAX_MAP_BUF;
 
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
 
inline static char *oprof_outstr(char *buf, char *str, int bytes)
{
	while (bytes-- && *str)
		*buf++ = *str++; 

	while (bytes--)
		*buf++ = '\0';

	return str; 
}

#define map_round(v) (((v)+(sizeof(struct op_map)/2))/sizeof(struct op_map)) 
 
/* buf must be PAGE_SIZE bytes large */
static int oprof_output_map(unsigned long addr, unsigned long len,
	unsigned long pgoff, struct file *file, char *buf)
{
	char *line;
	unsigned long size=16;
 
	if (!file)
		return 0;
 
	map_buf[nextmapbuf].addr = addr;
	map_buf[nextmapbuf].len = len;
	map_buf[nextmapbuf].offset = pgoff<<PAGE_SHIFT;
 
	line = d_path(file->f_dentry, file->f_vfsmnt, buf, PAGE_SIZE);
	printk("size %u\n",strlen(line));
	printk("line %s\n",line);

	line = oprof_outstr(map_buf[nextmapbuf].path,line,4);

	nextmapbuf++;
 
	/* output in 16-byte chunks, wrapping round */
	while (*line) {
		if (nextmapbuf==OP_MAX_MAP_BUF)
			nextmapbuf=0;

		line = oprof_outstr((char *)&map_buf[nextmapbuf], line, 16);
		size += 16; 
	}

	if (nextmapbuf==OP_MAX_MAP_BUF)
		nextmapbuf=0;

	return size;
}

#if 0
static int oprof_output_maps(struct task_struct *task)
{
	int size=0;
	char *buffer;
	struct mm_struct *mm;
	struct vm_area_struct *map;

	buffer = (char *) __get_free_page(GFP_KERNEL);
	if (!buffer)
		return 0;
 
	task_lock(task);
	if ((mm=task->mm))
		atomic_inc(&task->mm->mm_users);
	task_unlock(task);
 
	if (!mm)
		goto out; 

	down(&mm->mmap_sem);
 
	for (map=mm->mmap; map; map=map->vm_next) {
		if (!(map->vm_flags&VM_EXEC))
			continue;

		size += oprof_output_map(map->vm_start, map->vm_end-map->vm_start,
				map->vm_pgoff, map->vm_file, buffer);
	}

	up(&mm->mmap_sem);
 
	/* FIXME: we need to lock before so we don't have to GC the mm (exit_mmap not exported) */
	atomic_dec_and_test(&mm->mm_users);

out:
	free_page((unsigned long)buffer);
	return size;
}
 
asmlinkage static int my_sys_execve(struct pt_regs regs)
{
	struct task_struct *task = current;
	int ret;
 
	ret = old_sys_execve(regs);
	if (ret >= 0) {
		struct op_sample samp;

		samp.count = OP_DROP;
		samp.pid = task->pid;
		/* how many bytes to read from buffer */
		samp.eip = oprof_output_maps(task);
		oprof_out8(&samp);
	}
	return ret;
}
#endif 

asmlinkage static int my_sys_mmap2(unsigned long addr, unsigned long len,
	unsigned long prot, unsigned long flags,
	unsigned long fd, unsigned long pgoff)
{
	int ret;

 
	ret = old_sys_mmap2(addr,len,prot,flags,fd,pgoff);

	if ((prot&PROT_EXEC) && ret >= 0) {
		struct op_sample samp;
		struct file *file;
		char *buffer;

		buffer = (char *) __get_free_page(GFP_KERNEL);
		if (!buffer)
			return ret;

		file = fget(fd);
		if (!file) {
			free_page((unsigned long)buffer);
			return ret;
		}

		samp.count = OP_MAP;
		samp.pid = current->pid;
		/* how many bytes to read from buffer */
		samp.eip = oprof_output_map(ret,len,pgoff,file,buffer);
		fput(file);
		free_page((unsigned long)buffer);
		oprof_out8(&samp);
	}
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
		oprof_out8(&samp);
	}
	return ret;
}

asmlinkage static long my_sys_exit(int error_code)
{
	struct op_sample samp;

	samp.count = OP_EXIT;
	samp.pid = current->pid;
	samp.eip = 0;
	oprof_out8(&samp);
 
	return old_sys_exit(error_code);
}
 
extern void *sys_call_table[];
 
int oprof_binary(struct linux_binprm *binprm, struct pt_regs *regs)
{
	struct op_sample samp;

	printk("Called handler for pid %u\n",current->pid); 
	samp.count = OP_DROP;
	samp.pid = current->pid;
	samp.eip = 0;
	oprof_out8(&samp);
	return -ENOEXEC;
}
 
struct linux_binfmt oprof_binfmt = { NULL, THIS_MODULE, oprof_binary, NULL, NULL, 0 };
 
void __init op_intercept_syscalls(void)
{
	old_sys_fork = sys_call_table[__NR_fork]; 
	old_sys_vfork = sys_call_table[__NR_vfork];
	old_sys_clone = sys_call_table[__NR_clone];
	//old_sys_execve = sys_call_table[__NR_execve];
	old_sys_mmap2 = sys_call_table[__NR_mmap2];
	old_sys_init_module = sys_call_table[__NR_init_module];
	old_sys_exit = sys_call_table[__NR_exit]; 

	sys_call_table[__NR_fork] = my_sys_fork;
	sys_call_table[__NR_vfork] = my_sys_vfork;
	sys_call_table[__NR_clone] = my_sys_clone;
	//sys_call_table[__NR_execve] = my_sys_execve;
	sys_call_table[__NR_mmap2] = my_sys_mmap2; 
	sys_call_table[__NR_init_module] = my_sys_init_module;
	sys_call_table[__NR_exit] = my_sys_exit;

	register_binfmt(&oprof_binfmt); 
}

void __exit op_replace_syscalls(void)
{
	unregister_binfmt(&oprof_binfmt);

	sys_call_table[__NR_fork] = old_sys_fork;
	sys_call_table[__NR_vfork] = old_sys_vfork;
	sys_call_table[__NR_clone] = old_sys_clone;
	//sys_call_table[__NR_execve] = old_sys_execve;
	sys_call_table[__NR_mmap2] = old_sys_mmap2; 
	sys_call_table[__NR_init_module] = old_sys_init_module;
	sys_call_table[__NR_exit] = old_sys_exit;
}
