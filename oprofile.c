/* $Id: oprofile.c,v 1.43 2000/12/05 01:18:28 moz Exp $ */

/* FIXME: data->next rotation ? */
/* FIXME: with generation numbers we can place mappings in
   every buffer. we still need one IPI, but we shouldn't need
   to wait for it (?) and it avoids mapping-in-other-CPU problem */

#include "oprofile.h"

EXPORT_NO_SYMBOLS;

static char *op_version = VERSION_STRING;
MODULE_AUTHOR("John Levon (moz@compsoc.man.ac.uk)");
MODULE_DESCRIPTION("Continuous Profiling Module");
static int op_hash_size;
static int op_buf_size;
static u8 op_ctr0_on[NR_CPUS];
static u8 op_ctr1_on[NR_CPUS];
static u8 op_ctr0_um[NR_CPUS];
static u8 op_ctr1_um[NR_CPUS];
static int op_ctr0_count[NR_CPUS];
static int op_ctr1_count[NR_CPUS];
static u8 op_ctr0_val[NR_CPUS];
static u8 op_ctr1_val[NR_CPUS];
static u8 op_ctr0_osusr[NR_CPUS];
static u8 op_ctr1_osusr[NR_CPUS];
pid_t pid_filter;
pid_t pgrp_filter;

u32 prof_on __cacheline_aligned;
 
static int op_major;
static int cpu_type;

static volatile uint oprof_opened __cacheline_aligned;
static DECLARE_WAIT_QUEUE_HEAD(oprof_wait);

u32 oprof_ready[NR_CPUS] __cacheline_aligned;
static struct _oprof_data oprof_data[NR_CPUS];

extern spinlock_t map_lock;
extern u32 nextmapbuf;

/* ---------------- NMI handler ------------------ */

/* FIXME: this whole handler would probably be better in straight asm */
static void evict_op_entry(struct _oprof_data *data, struct op_sample *ops)
{
	memcpy(&data->buffer[data->nextbuf],ops,sizeof(struct op_sample));
	if (++data->nextbuf!=(data->buf_size-OP_PRE_WATERMARK)) {
		if (data->nextbuf==data->buf_size)
			data->nextbuf=0;
		return;
	}
	oprof_ready[smp_processor_id()] = 1;
}

inline static void fill_op_entry(struct op_sample *ops, struct pt_regs *regs, int ctr)
{
	ops->eip = regs->eip;
	ops->pid = current->pid;
	ops->count = OP_COUNTER*ctr + 1;
}

#define op_full_count(c) (((c)&OP_MAX_COUNT)==OP_MAX_COUNT)

/* no check for ctr needed as one of the three will differ in the hash */
#define op_miss(ops)  \
	((ops).eip!=regs->eip || \
	(ops).pid!=current->pid || \
	op_full_count((ops).count))

/* the top half of pid is likely to remain static,
   so it's masked off. the ctr bit is used to separate
   the two counters */
#define op_hash(eip,pid,ctr) \
	((((((eip&0xff000)>>3)^eip)^(pid&0xff))^(eip<<9)) \
	^ (ctr<<8)) & (data->hash_size-1)

inline static void op_do_profile(struct _oprof_data *data, struct pt_regs *regs, int ctr)
{
	uint h = op_hash(regs->eip,current->pid,ctr);
	uint i;

	/* FIXME: can we remove new sample check by pretending to be full ? */
	for (i=0; i < OP_NR_ENTRY; i++) {
		if (!op_miss(data->entries[h].samples[i])) {
			data->entries[h].samples[i].count++;
			set_perfctr(data->ctr_count[ctr],ctr);
			return;
		} else if (op_full_count(data->entries[h].samples[i].count)) {
			goto full_entry;
		} else if (!data->entries[h].samples[i].count)
			goto new_entry;
	}

	evict_op_entry(data,&data->entries[h].samples[data->next]);
	fill_op_entry(&data->entries[h].samples[data->next],regs,ctr);
	data->next = (data->next+1) % OP_NR_ENTRY;
out:
	set_perfctr(data->ctr_count[ctr],ctr);
	return;
full_entry:
	evict_op_entry(data,&data->entries[h].samples[i]);
	data->entries[h].samples[i].count = OP_COUNTER*ctr + 1;
	goto out;
new_entry:
	fill_op_entry(&data->entries[h].samples[i],regs,ctr);
	goto out;
}

static int op_check_ctr(struct _oprof_data *data, struct pt_regs *regs, int ctr)
{
	ulong l,h;
	get_perfctr(l,h,ctr);
	if (ctr_overflowed(l)) {
		op_do_profile(data,regs,ctr);
		return 1;
	}
	return 0;
}

asmlinkage void op_do_nmi(struct pt_regs * regs)
{
	struct _oprof_data *data = &oprof_data[smp_processor_id()];
	uint low,high;
	int overflowed=0;

#ifdef PID_FILTER
	if (pid_filter && current->pid!=pid_filter)
		return;
	if (pgrp_filter && current->pgrp!=pgrp_filter)
		return;
#endif

	/* disable counters */
	rdmsr(P6_MSR_EVNTSEL0,low,high);
	wrmsr(P6_MSR_EVNTSEL0,low&~(1<<22),high);

	if (data->ctrs&OP_CTR_0)
		overflowed = op_check_ctr(data,regs,0);
	if (data->ctrs&OP_CTR_1)
		overflowed |= op_check_ctr(data,regs,1);
		
	/* enable counters */
	rdmsr(P6_MSR_EVNTSEL0,low,high);
	wrmsr(P6_MSR_EVNTSEL0,low|(1<<22),high);

	if (!overflowed)
		DO_NMI(regs, 0);
}

/* ---------------- NMI handler setup ------------ */

void mask_LVT_NMIs(void)
{
	ulong v;

	/* LVT0,1,PC can generate NMIs on APIC */
	v = apic_read(APIC_LVT0);
	apic_write(APIC_LVT0, v | APIC_LVT_MASKED);
	v = apic_read(APIC_LVT1);
	apic_write(APIC_LVT1, v | APIC_LVT_MASKED);
        v = apic_read(APIC_LVTPC);
        apic_write(APIC_LVTPC, v | APIC_LVT_MASKED);
}

void unmask_LVT_NMIs(void)
{
	ulong v;

	v = apic_read(APIC_LVT0);
	apic_write(APIC_LVT0, v & ~APIC_LVT_MASKED);
	v = apic_read(APIC_LVT1);
	apic_write(APIC_LVT1, v & ~APIC_LVT_MASKED);
        v = apic_read(APIC_LVTPC);
        apic_write(APIC_LVTPC, v & ~APIC_LVT_MASKED);
}

static ulong idt_addr;
static ulong kernel_nmi;

static void install_nmi(void)
{
	volatile struct _descr descr = { 0, 0,};
	volatile struct _idt_descr *de;

	mask_LVT_NMIs();
	store_idt(descr);
	idt_addr = descr.base;
	de = (struct _idt_descr *)idt_addr;
	/* NMI handler is at idt_table[2] */
	de += 2;
	/* see Intel Vol.3 Figure 5-2, interrupt gate */
	kernel_nmi = (de->a & 0xffff) | (de->b & 0xffff0000);

	_set_gate(de,14,0,&op_nmi);
	unmask_LVT_NMIs();
}

static void restore_nmi(void)
{
	mask_LVT_NMIs();
	_set_gate(((char *)(idt_addr))+16,14,0,kernel_nmi);
	unmask_LVT_NMIs();
}

/* ---------------- APIC setup ------------------ */

static void disable_local_P6_APIC(void *dummy)
{
	ulong v;
	uint l,h;

	/* FIXME: maybe this should go at end of function ? */
	/* first disable via MSR */
	/* IA32 V3, 7.4.2 */
	rdmsr(MSR_APIC_BASE, l, h);
	wrmsr(MSR_APIC_BASE, l&~(1<<11), h);

	/*
	 * Careful: we have to set masks only first to deassert
	 * any level-triggered sources.
	 */
	v = apic_read(APIC_LVTT);
	apic_write(APIC_LVTT, v | APIC_LVT_MASKED);
	v = apic_read(APIC_LVT0);
	apic_write(APIC_LVT0, v | APIC_LVT_MASKED);
	v = apic_read(APIC_LVT1);
	apic_write(APIC_LVT1, v | APIC_LVT_MASKED);
	v = apic_read(APIC_LVTERR);
	apic_write(APIC_LVTERR, v | APIC_LVT_MASKED);
        v = apic_read(APIC_LVTPC);
        apic_write(APIC_LVTPC, v | APIC_LVT_MASKED);

	/*
	 * Clean APIC state for other OSs:
	 */
	apic_write(APIC_LVTT, APIC_LVT_MASKED);
	apic_write(APIC_LVT0, APIC_LVT_MASKED);
	apic_write(APIC_LVT1, APIC_LVT_MASKED);
	apic_write(APIC_LVTERR, APIC_LVT_MASKED);
        apic_write(APIC_LVTPC, APIC_LVT_MASKED);

	v = apic_read(APIC_SPIV);
	v &= ~(1<<8);
	apic_write(APIC_SPIV, v);

	printk(KERN_INFO "oprofile: disabled local APIC.\n");
}

static void __init smp_apic_setup(void *dummy)
{
	uint val;

	/* set up LVTPC as we need it */
	/* IA32 V3, Figure 7.8 */
	val = apic_read(APIC_LVTPC);
	/* allow PC overflow interrupts */
	val &= ~(1<<16);
	/* set delivery to NMI */
	val |= (1<<10);
	val &= ~(1<<9);
	val &= ~(1<<8);
	apic_write(APIC_LVTPC, val);
}

static int __init apic_setup(void)
{
	uint msr_low, msr_high;
	uint val;

	/* FIXME: davej says it might be possible to use PCI to find
	   SMP systems with one CPU */
	if (smp_num_cpus>1) {
		smp_apic_setup(NULL);
		return 0;
	}

	/* map the real local APIC back in */
	/* current kernels fake this on boot setup for UP */
	my_set_fixmap();

	/* FIXME: NMI delivery for SMP ? */

	/* enable local APIC via MSR. Forgetting this is a fun way to
	   lock the box */
	/* IA32 V3, 7.4.2 */
	rdmsr(MSR_APIC_BASE, msr_low, msr_high);
	wrmsr(MSR_APIC_BASE, msr_low|(1<<11), msr_high);

	/* check for a good APIC */
	/* IA32 V3, 7.4.15 */
	val = apic_read(APIC_LVR);
	if (!APIC_INTEGRATED(GET_APIC_VERSION(val)))	
		goto not_local_p6_apic;

	/* LVT0,LVT1,LVTT,LVTPC */
	if (GET_APIC_MAXLVT(apic_read(APIC_LVR))!=4)
		goto not_local_p6_apic;

	__cli();

	/* enable APIC locally */
	/* IA32 V3, 7.4.14.1 */
	val = apic_read(APIC_SPIV);
	apic_write(APIC_SPIV, val|(1<<8));

	/* FIXME: examine this stuff */
	val = 0x00008700;
	apic_write(APIC_LVT0, val);

	val = 0x00000400;
	apic_write(APIC_LVT1, val);

	/* clear error register */
	/* IA32 V3, 7.4.17 */
	apic_write(APIC_ESR, 0);

	/* mask error interrupt */
	/* IA32 V3, Figure 7.8 */
	val = apic_read(APIC_LVTERR);
	val |= 0x00010000;
	apic_write(APIC_LVTERR, val);

	/* setup timer vector */
	/* IA32 V3, 7.4.8 */
	apic_write(APIC_LVTT, 0x0001031);

	/* Divide configuration register */
	val = APIC_TDR_DIV_1;
	apic_write(APIC_TDCR, val);

	smp_apic_setup(NULL);

	__sti();

	printk(KERN_INFO "oprofile: enabled local APIC\n");
	return 0;

not_local_p6_apic:
	printk(KERN_ERR "oprofile: no local P6 APIC\n");
	/* IA32 V3, 7.4.2 */
	rdmsr(MSR_APIC_BASE, msr_low, msr_high);
	wrmsr(MSR_APIC_BASE, msr_low&~(1<<11), msr_high);
	return -ENODEV;
}

/* ---------------- PMC setup ------------------ */

static void pmc_fill_in(uint *val, u8 osusr, u8 event, u8 um)
{
	/* enable interrupt generation */
	*val |= (1<<20);
	/* enable chosen OS and USR counting */
	switch (osusr) {
		case 2: /* userspace only */
			*val |= (1<<16);
			break;
		default: /* O/S and userspace */
			*val |= (1<<16);
		case 1: /* O/S only */
			*val |= (1<<17);
			break;
	}
	/* what are we counting ? */
	*val |= event;
	*val |= (um<<8);
}

static void pmc_setup(void *dummy)
{
	uint low,high;
	uint cpu = smp_processor_id();

	/* IA Vol. 3 Figure 14-3 */

	rdmsr(P6_MSR_EVNTSEL0,low,high);
	/* clear */
	low &= (1<<21);

	if (op_ctr0_val[cpu]) {
		set_perfctr(op_ctr0_count[cpu],0);
		pmc_fill_in(&low, op_ctr0_osusr[cpu], op_ctr0_val[cpu], op_ctr0_um[cpu]);
	}

	wrmsr(P6_MSR_EVNTSEL0,low,0);

	rdmsr(P6_MSR_EVNTSEL1,low,high);
	/* clear */
	low &= (3<<21);

	if (op_ctr1_val[cpu]) {
		set_perfctr(op_ctr1_count[cpu],1);
		pmc_fill_in(&low, op_ctr1_osusr[cpu], op_ctr1_val[cpu], op_ctr1_um[cpu]);
	}

	wrmsr(P6_MSR_EVNTSEL1,low,high);
}

static void pmc_start(void *info)
{
	uint low,high;

	if (info && (*((uint *)info)!=smp_processor_id()))
		return;

 	/* enable counters */
	rdmsr(P6_MSR_EVNTSEL0,low,high);
	wrmsr(P6_MSR_EVNTSEL0,low|(1<<22),high);
}

static void pmc_stop(void *info)
{
	uint low,high;

	if (info && (*((uint *)info)!=smp_processor_id()))
		return;

	/* disable counters */
	rdmsr(P6_MSR_EVNTSEL0,low,high);
	wrmsr(P6_MSR_EVNTSEL0,low&~(1<<22),high);
}

inline static void pmc_select_start(uint cpu)
{
	if (cpu==smp_processor_id())
		pmc_start(NULL);
	else
		smp_call_function(pmc_start,&cpu,0,1);
}

inline static void pmc_select_stop(uint cpu)
{
	if (cpu==smp_processor_id())
		pmc_stop(NULL);
	else
		smp_call_function(pmc_stop,&cpu,0,1);
}

/* ---------------- driver routines ------------------ */

u32 diethreaddie;
pid_t threadpid;
DECLARE_MUTEX_LOCKED(threadstopsem);

/* we have to have another thread because we can't
 * do wake_up() from NMI due to no locking
 */
int oprof_thread(void *arg)
{
	int i;

	threadpid = current->pid;

	lock_kernel();
	exit_mm(current);
	current->session = 1;
	current->pgrp = 1;
	/* FIXME: daemonize() does this as of test11 */
	exit_files(current);
	exit_fs(current);
	sprintf(current->comm, "oprof-thread");
	siginitsetinv(&current->blocked, sigmask(SIGKILL));
        spin_lock(&current->sigmask_lock);
        flush_signals(current);
        spin_unlock(&current->sigmask_lock);
	current->policy = SCHED_OTHER;
	current->nice = -20;
	unlock_kernel();

	for (;;) {
		for (i=0; i<smp_num_cpus; i++) {
			if (oprof_ready[i])
				wake_up(&oprof_wait);
		}
		current->state = TASK_INTERRUPTIBLE;
		/* FIXME: determine best value here */
		schedule_timeout(HZ/20);

		if (diethreaddie)
			break;
	}

	up(&threadstopsem);
	return 0;
}

void oprof_start_thread(void)
{
	if (kernel_thread(oprof_thread, NULL, CLONE_FS|CLONE_FILES|CLONE_SIGHAND)<0)
		printk(KERN_ERR "oprofile: couldn't spawn wakeup thread.\n");
}

void oprof_stop_thread(void)
{
	diethreaddie = 1;
	kill_proc(SIGKILL, threadpid, 1);
	down(&threadstopsem);
}

spinlock_t note_lock __cacheline_aligned = SPIN_LOCK_UNLOCKED;

void oprof_put_note(struct op_sample *samp)
{
	struct _oprof_data *data = &oprof_data[0];

	if (!prof_on)
		return;

	/* FIXME: IPIs are expensive */
	spin_lock(&note_lock);
	pmc_select_stop(0);

	memcpy(&data->buffer[data->nextbuf],samp,sizeof(struct op_sample));
	if (++data->nextbuf==(data->buf_size-OP_PRE_WATERMARK)) {
		oprof_ready[0] = 1;
		// FIXME: this ok under a spinlock ? 
		wake_up(&oprof_wait);
	} else if (data->nextbuf==data->buf_size)
		data->nextbuf=0;

	pmc_select_start(0);
	spin_unlock(&note_lock);
}

static int oprof_read(struct file *file, char *buf, size_t count, loff_t *ppos)
{
	struct op_sample *mybuf;
	uint i;
	uint num;
	ssize_t max;
	int forced_wakeup;

	if (!capable(CAP_SYS_PTRACE))
		return -EPERM;

	if (!prof_on) {
		kill_proc(SIGKILL, current->pid, 1);
		return -EINTR;
	}

	switch (MINOR(file->f_dentry->d_inode->i_rdev)) {
		case 2: return oprof_map_read(buf,count,ppos);
		case 1: return -EINVAL;
		default: 
	}

	max = sizeof(struct op_sample)*op_buf_size;

	if (*ppos || count!=sizeof(struct op_sample)+max)
		return -EINVAL;

	mybuf = vmalloc(sizeof(struct op_sample)+max);
	if (!mybuf)
		return -EFAULT;

again:
	for (i=0; i<smp_num_cpus; i++) {
		if (oprof_ready[i]) {
			forced_wakeup = oprof_ready[i]-1;
			oprof_ready[i]=0;
			goto doit;
		}
	}

	interruptible_sleep_on(&oprof_wait);
	if (signal_pending(current)) {
		vfree(mybuf);
		return -EINTR;
	}
	goto again;

	/* FIXME: what if a signal occurs now ? What is returned to
	 * the read() routine ?
	 */
doit:
	pmc_select_stop(i);
	spin_lock(&note_lock);

	num = oprof_data[i].nextbuf;
	/* might have overflowed from map buffer or ejection buffer */
	if (num < oprof_data[i].buf_size-OP_PRE_WATERMARK && !forced_wakeup) {
		printk(KERN_ERR "oprofile: Detected overflow of size %d. You must increase the "
				"hash table size or reduce the interrupt frequency\n", num);
		num = oprof_data[i].buf_size;
	} else
		oprof_data[i].nextbuf=0;

	count = num*sizeof(struct op_sample);

	if (count)
		memcpy(mybuf,oprof_data[i].buffer,count);

	spin_unlock(&note_lock);
	pmc_select_start(i);

	if (count && copy_to_user(buf, mybuf, count))
		count = -EFAULT;

	vfree(mybuf);
	return count;
}
 
static int oprof_open(struct inode *ino, struct file *file)
{
	if (!capable(CAP_SYS_PTRACE))
		return -EPERM;

	switch (MINOR(file->f_dentry->d_inode->i_rdev)) {
		case 2: return oprof_map_open();
		case 1: return oprof_hash_map_open();
		default:
	}

	if (test_and_set_bit(0,&oprof_opened))
		return -EBUSY;

	return 0;
}

static int oprof_stop(void);

static int oprof_release(struct inode *ino, struct file *file)
{
	switch (MINOR(file->f_dentry->d_inode->i_rdev)) {
		case 2: return oprof_map_release();
		case 1: return oprof_hash_map_release();
		default:
	}

	if (!oprof_opened)
		return -EFAULT;

	clear_bit(0,&oprof_opened);
	oprof_stop();

	return 0;
}

static int oprof_mmap(struct file *file, struct vm_area_struct *vma)
{
	if (MINOR(file->f_dentry->d_inode->i_rdev)==1)
		return oprof_hash_map_mmap(file,vma);
	return -EINVAL;
}

/* called under spinlock, cannot sleep */
static void oprof_free_mem(uint num)
{
	uint i;
	for (i=0; i < num; i++) {
		if (oprof_data[i].entries) 
			vfree(oprof_data[i].entries);
		if (oprof_data[i].buffer) 
			vfree(oprof_data[i].buffer);
		oprof_data[i].entries = NULL;
		oprof_data[i].buffer = NULL;
	}
}

static int oprof_init_data(void)
{
	uint i;
	ulong hash_size,buf_size;
	struct _oprof_data *data;

	for (i=0; i < smp_num_cpus; i++) {
		data = &oprof_data[i];
		hash_size = (sizeof(struct op_entry)*op_hash_size);
		buf_size = (sizeof(struct op_sample)*op_buf_size);

		data->entries = vmalloc(hash_size);
		if (!data->entries) {
			printk("oprofile: failed to allocate hash table of %lu bytes\n",hash_size);
			oprof_free_mem(i);
			return -EFAULT;
		}

		data->buffer = vmalloc(buf_size);
		if (!data->buffer) {
			printk("oprofile: failed to allocate eviction buffer of %lu bytes\n",buf_size);
			vfree(data->entries);
			oprof_free_mem(i);
			return -EFAULT;
		}

		memset(data->entries,0,hash_size);
		memset(data->buffer,0,buf_size);

		data->hash_size = op_hash_size;
		data->buf_size = op_buf_size;
	}

	return 0;
}

static int parms_ok(void)
{
	int ret;
	uint cpu;
	struct _oprof_data *data;

	op_check_range(op_hash_size,256,262144,"op_hash_size value %d not in range\n");
	op_check_range(op_buf_size,512,262144,"op_buf_size value %d not in range\n");

	for (cpu=0; cpu < smp_num_cpus; cpu++) {
		data = &oprof_data[cpu];
		data->ctrs |= OP_CTR_0*(!!op_ctr0_on[cpu]);
		data->ctrs |= OP_CTR_1*(!!op_ctr1_on[cpu]);

		/* FIXME: maybe we should allow no set on a CPU ? */
		if (!data->ctrs) {
			printk("oprofile: neither counter enabled for CPU%d\n",cpu);
			return 0;
		}

		/* make sure the buffer and hash table have been set up */
		if (!data->buffer || !data->entries)
			return 0;

		if (data->ctrs&OP_CTR_0) {
			op_check_range(op_ctr0_count[cpu],500,OP_MAX_PERF_COUNT,"ctr0 count value %d not in range\n");
			data->ctr_count[0]=op_ctr0_count[cpu];
		}
		if (data->ctrs&OP_CTR_1) {
			op_check_range(op_ctr1_count[cpu],500,OP_MAX_PERF_COUNT,"ctr1 count value %d not in range\n");
			data->ctr_count[1]=op_ctr1_count[cpu];
		}

		/* hw_ok() has set cpu_type */
		ret = op_check_events(op_ctr0_val[cpu], op_ctr1_val[cpu], op_ctr0_um[cpu], op_ctr1_um[cpu], cpu_type);

		if (ret&OP_CTR0_NOT_FOUND) printk("oprofile: ctr0: no such event\n");
		if (ret&OP_CTR1_NOT_FOUND) printk("oprofile: ctr1: no such event\n");
		if (ret&OP_CTR0_NO_UM) printk("oprofile: ctr0: invalid unit mask\n");
		if (ret&OP_CTR1_NO_UM) printk("oprofile: ctr1: invalid unit mask\n");
		if (ret&OP_CTR0_NOT_ALLOWED) printk("oprofile: ctr0 can't count this event\n");
		if (ret&OP_CTR1_NOT_ALLOWED) printk("oprofile: ctr1 can't count this event\n");
		if (ret&OP_CTR0_PII_EVENT) printk("oprofile: ctr0: event only available on PII\n");
		if (ret&OP_CTR1_PII_EVENT) printk("oprofile: ctr1: event only available on PII\n");
		if (ret&OP_CTR0_PIII_EVENT) printk("oprofile: ctr0: event only available on PIII\n");
		if (ret&OP_CTR1_PIII_EVENT) printk("oprofile: ctr1: event only available on PIII\n");

		if (ret)
			return 0;
	}

	return 1;
}

static int oprof_start(void)
{
	if (!parms_ok())
		return -EINVAL;
		 
	if ((smp_call_function(pmc_setup,NULL,0,1)))
		return -EFAULT;
	pmc_setup(NULL);
 
	install_nmi();

	op_intercept_syscalls();

	oprof_start_thread();
	smp_call_function(pmc_start,NULL,0,1);
	pmc_start(NULL);
	prof_on = 1;
	return 0;
}

static int oprof_stop(void)
{
	uint i;

	if (!prof_on)
		return -EINVAL;

	/* here we need to :
	 * bring back the old system calls
	 * stop the wake-up thread
	 * stop the perf counter
	 * bring back the old NMI handler
	 * remove any sleeping readers of the eviction buffer
	 * reset the map buffer stuff and ready values
	 *
	 * Nothing will be able to write into the map buffer because
	 * we check explicitly for prof_on and synchronise via the spinlocks
	 */

	op_replace_syscalls();

	oprof_stop_thread();

	prof_on = 0;

	smp_call_function(pmc_stop,NULL,0,1);
	pmc_stop(NULL);
	restore_nmi();

	wake_up(&oprof_wait);
	/* the daemon will eventually die when it tries to read without prof_on */
	while (test_bit(0,&oprof_opened)) {
		set_current_state(TASK_UNINTERRUPTIBLE);
		schedule_timeout(HZ/2);
	}

	spin_lock(&map_lock);
	spin_lock(&note_lock);

	nextmapbuf = 0;

	for (i=0; i < smp_num_cpus; i++) {
		struct _oprof_data *data = &oprof_data[i];
		oprof_ready[i] = 0; 
		data->nextbuf = data->next = 0;
		oprof_free_mem(smp_num_cpus);
	}

	spin_unlock(&note_lock);
	spin_unlock(&map_lock);

	return 0;
}

/* this is already losing, maybe sysctls for the config bits would be better. */
static int oprof_ioctl(struct inode *ino, struct file *file, uint cmd, ulong arg)
{
	uint cpu;

	if (prof_on && cmd!=OPROF_DUMP && cmd!=OPROF_STOP)
		return -EBUSY;

	switch (cmd) {
		case OPROF_DUMP:
			for (cpu=0; cpu < smp_num_cpus; cpu++)
				oprof_ready[cpu]=2;
			wake_up(&oprof_wait);
			return 0; break;

		case OPROF_START:
			return oprof_start();
			break;

		case OPROF_STOP:
			return oprof_stop(); 
			break;

		case OPROF_SET_HASH_SIZE:
			op_hash_size=arg;
			oprof_free_mem(smp_num_cpus);
			return oprof_init_data(); 
			break;
		
		case OPROF_SET_BUF_SIZE:
			op_buf_size=arg;
			oprof_free_mem(smp_num_cpus);
			return oprof_init_data(); 
			break;
		
		case OPROF_SET_CTR0:
			if ((arg & ~(1<<31)) > smp_num_cpus)
				return -EINVAL;
			op_ctr0_on[(arg & ~(1<<31))] = (arg & (1<<31));
			return 0; break;

		case OPROF_SET_CTR1:
			if ((arg & ~(1<<31)) > smp_num_cpus)
				return -EINVAL;
			op_ctr1_on[(arg & ~(1<<31))] = (arg & (1<<31));
			return 0; break;

		case OPROF_SET_PID_FILTER:
#ifndef PID_FILTER
			return -EINVAL;
#endif
			pid_filter = (pid_t)arg;
			return 0; break;

		case OPROF_SET_PGRP_FILTER:
#ifndef PID_FILTER
			return -EINVAL;
#endif
			pgrp_filter = (pid_t)arg;
			return 0; break;
	}
	
	/* top two bytes are which cpu for the cpu-specific 
	 * options below. Validation is done in parms_ok().
	 */
	cpu = arg>>16;

	if (cpu >= smp_num_cpus) 
		return -EINVAL;

	switch (cmd) {
		case OPROF_SET_CTR0_VAL:
			op_ctr0_val[cpu] = (arg & 0xff);
			break;
		
		case OPROF_SET_CTR1_VAL:
			op_ctr1_val[cpu] = (arg & 0xff);
			break;
			 
		case OPROF_SET_CTR0_UM:
			op_ctr0_um[cpu] = (arg & 0xff);
			break;
			 
		case OPROF_SET_CTR1_UM:
			op_ctr1_um[cpu] = (arg & 0xff);
			break;
		
		case OPROF_SET_CTR0_COUNT:
			op_ctr0_count[cpu] = (arg & 0xffff);
			break;
		
		case OPROF_SET_CTR1_COUNT:
			op_ctr1_count[cpu] = (arg & 0xffff);
			break;
		
		case OPROF_SET_CTR0_OS_USR:
			if ((arg & 0xf) > 2)
				return -EINVAL; 
			op_ctr0_osusr[cpu] = (arg & 0xf);
			break;
			 
		case OPROF_SET_CTR1_OS_USR:
			if ((arg & 0xf) > 2)
				return -EINVAL;
			op_ctr1_osusr[cpu] = (arg & 0xf);
			break;
	}
	return 0;
}

static struct file_operations oprof_fops = {
	owner: THIS_MODULE,
	open: oprof_open,
	release: oprof_release,
	read: oprof_read,
	mmap: oprof_mmap,
	ioctl: oprof_ioctl,
};

static int can_unload(void)
{
	return -EBUSY; /* nope */
}
 
static int __init hw_ok(void)
{
	if (current_cpu_data.x86_vendor != X86_VENDOR_INTEL ||
	    current_cpu_data.x86 < 6) {
		printk("oprofile: not an Intel P6 processor. Sorry.\n");
		return 0;
	}

	/* 0 if PPro, 1 if PII, 2 if PIII */
	cpu_type = (current_cpu_data.x86_model > 5) ? 2 :
		(current_cpu_data.x86_model > 2);
	return 1;
}

int __init oprof_init(void)
{
	int err;

	if (!hw_ok())
		return -EINVAL;

	printk(KERN_INFO "%s\n",op_version);

	if ((err=apic_setup()))
		return err;

	if ((err=smp_call_function(smp_apic_setup,NULL,0,1)))
		goto out_err;

 	err = op_major = register_chrdev(0,"oprof",&oprof_fops);
	if (err<0)
		goto out_err;

	err = oprof_init_hashmap();
	if (err<0) {
		unregister_chrdev(op_major,"oprof");
		goto out_err;
	}

	/* module is not unloadable */
	THIS_MODULE->can_unload = can_unload;
		
	printk("oprofile: /dev/oprofile enabled, major %u\n",op_major);
	return 0;

out_err:
	smp_call_function(disable_local_P6_APIC,NULL,0,1);
	disable_local_P6_APIC(NULL);
	return err;
}

void __exit oprof_exit(void)
{
	/* FIXME: what to do here ? will this ever happen ? */

	oprof_free_hashmap();

	op_replace_syscalls();

	unregister_chrdev(op_major,"oprof");

	if (smp_call_function(disable_local_P6_APIC,NULL,0,1))
		return;
	disable_local_P6_APIC(NULL);

	return;
}

module_init(oprof_init);
module_exit(oprof_exit);
