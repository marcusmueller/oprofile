#include <linux/config.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/init.h>
#include <linux/delay.h> 
#include <linux/sched.h> 
#include <linux/unistd.h>

#include <asm/io.h>
 
/* stuff we have to do ourselves */

#define APIC_DEFAULT_PHYS_BASE 0xfee00000
 
/* FIXME: not up to date */
static void set_pte_phys (unsigned long vaddr, unsigned long phys)
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

void my_set_fixmap (void)
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
	/* No MicroChannelA check */
 
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
		/* FIXME: couldn't we do something with irq_stat ?
		   we'd have to risk deadlock on console_lock ...
		   how does do_nmi() panic the machine with do_exit() -
		   neither !pid or in_interrupt() is necessarily true */ 
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

/* syscall intercepting */

static int (*old_sys_fork)(struct pt_regs);
static int (*old_sys_vfork)(struct pt_regs);
static int (*old_sys_clone)(struct pt_regs);
static int (*old_sys_execve)(struct pt_regs);
static long (*old_sys_mmap2)(unsigned long, unsigned long, unsigned long,
	unsigned long, unsigned long, unsigned long);
static long (*old_sys_init_module)(const char *, struct module *); 
static long (*old_sys_exit)(int);
 
static int my_sys_fork(struct pt_regs regs)
{
	return old_sys_fork(regs);
} 

static int my_sys_vfork(struct pt_regs regs)
{
	return old_sys_vfork(regs);
}

static int my_sys_clone(struct pt_regs regs)
{
	return old_sys_clone(regs);
}
 
static int my_sys_execve(struct pt_regs regs)
{
	return old_sys_execve(regs);
}

static int my_sys_mmap2(unsigned long addr, unsigned long len,
	unsigned long prot, unsigned long flags,
	unsigned long fd, unsigned long pgoff)
{
	return old_sys_mmap2(addr,len,prot,flags,fd,pgoff); 
}

static long my_sys_init_module(const char *name_user, struct module *mod_user)
{
	return old_sys_init_module(name_user, mod_user);
}

static long my_sys_exit(int error_code)
{
	return old_sys_exit(error_code);
}
 
extern void *sys_call_table[];
 
void __init op_intercept_syscalls(void)
{
	old_sys_fork = sys_call_table[__NR_fork]; 
	old_sys_vfork = sys_call_table[__NR_vfork];
	old_sys_clone = sys_call_table[__NR_clone];
	old_sys_execve = sys_call_table[__NR_execve];
	old_sys_mmap2 = sys_call_table[__NR_mmap2];
	old_sys_init_module = sys_call_table[__NR_init_module];
	old_sys_exit = sys_call_table[__NR_exit]; 

	sys_call_table[__NR_fork] = my_sys_fork;
	sys_call_table[__NR_vfork] = my_sys_vfork;
	sys_call_table[__NR_clone] = my_sys_clone;
	sys_call_table[__NR_execve] = my_sys_execve;
	sys_call_table[__NR_mmap2] = my_sys_mmap2; 
	sys_call_table[__NR_init_module] = my_sys_init_module;
	sys_call_table[__NR_exit] = my_sys_exit;
}

void __exit op_replace_syscalls(void)
{
	sys_call_table[__NR_fork] = old_sys_fork;
	sys_call_table[__NR_vfork] = old_sys_vfork;
	sys_call_table[__NR_clone] = old_sys_clone;
	sys_call_table[__NR_execve] = old_sys_execve;
	sys_call_table[__NR_mmap2] = old_sys_mmap2; 
	sys_call_table[__NR_init_module] = old_sys_init_module;
	sys_call_table[__NR_exit] = old_sys_exit;
}
