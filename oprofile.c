/* oprofile.c */
/* continuous profiling module for Linux 2.4 */
/* John Levon (moz@compsoc.man.ac.uk) */
/* May 2000 */

/* FIXME: when we springboard syscalls, we need to think about
   security DOS on fork failure, i.e. we should be very careful
   about not doing work on syscall that will fail */
/* FIXME: syscall stuff */

/* FIXME: data->next rotation ? */
/* FIXME: what about rdtsc timestamping ? */
 
#include "oprofile.h" 
 
static char *op_version = "oprofile 0.0.1";
MODULE_AUTHOR("John Levon (moz@compsoc.man.ac.uk)");
MODULE_DESCRIPTION("Continuous Profiling Module");
MODULE_PARM(op_hash_size,"i");
MODULE_PARM_DESC(op_hash_size,"Number of entries in hash table"); 
MODULE_PARM(op_buf_size,"i");
MODULE_PARM_DESC(op_buf_size,"Number of entries in eviction buffer"); 
MODULE_PARM(op_ctr0_on,"1-32b");
MODULE_PARM_DESC(op_ctr0_on,"Enable counter 0");
MODULE_PARM(op_ctr1_on,"1-32b");
MODULE_PARM_DESC(op_ctr1_on,"Enable counter 1");
MODULE_PARM(op_ctr0_type,"1-32s"); 
MODULE_PARM_DESC(op_ctr0_type,"Symbolic event name for counter 0"); 
MODULE_PARM(op_ctr1_type,"1-32s"); 
MODULE_PARM_DESC(op_ctr1_type,"Symbolic event name for counter 1"); 
MODULE_PARM(op_ctr0_um,"1-32s");
MODULE_PARM_DESC(op_ctr0_um,"Unit Mask for counter 0"); 
MODULE_PARM(op_ctr1_um,"1-32s");
MODULE_PARM_DESC(op_ctr1_um,"Unit Mask for counter 1");
MODULE_PARM(op_ctr0_count,"1-32i");
MODULE_PARM_DESC(op_ctr0_count,"Number of events between samples for counter 0 (decimal)"); 
MODULE_PARM(op_ctr1_count,"1-32i");
MODULE_PARM_DESC(op_ctr1_count,"Number of events between samples for counter 1 (decimal)"); 
MODULE_PARM(op_ctr0_osusr,"1-32b");
MODULE_PARM_DESC(op_ctr0_osusr,"All, O/S only, or userspace only counting for counter 0 (0/1/2)");
MODULE_PARM(op_ctr1_osusr,"1-32b");
MODULE_PARM_DESC(op_ctr1_osusr,"All, O/S only, or userspace only counting for counter 1 (0/1/2)");
static int op_hash_size=2048;
static int op_buf_size=2048;
static u8 op_ctr0_on[NR_CPUS];
static u8 op_ctr1_on[NR_CPUS]; 
static char *op_ctr0_type[NR_CPUS];
static char *op_ctr1_type[NR_CPUS];
static char *op_ctr0_um[NR_CPUS];
static char *op_ctr1_um[NR_CPUS]; 
static int op_ctr0_count[NR_CPUS];
static int op_ctr1_count[NR_CPUS];
static u8 op_ctr0_val[NR_CPUS];
static u8 op_ctr1_val[NR_CPUS]; 
static u8 op_ctr0_osusr[NR_CPUS];
static u8 op_ctr1_osusr[NR_CPUS]; 
 
static int op_major;
static int cpu_type;
 
static u32 oprof_opened; 
DECLARE_WAIT_QUEUE_HEAD(oprof_wait);
static u32 oprof_ready[NR_CPUS];
static struct _oprof_data oprof_data[NR_CPUS];
 
/* ---------------- NMI handler ------------------ */

/* can't convince gcc to not jump on the common path :( */ 
static void evict_op_entry(struct _oprof_data *data, struct op_sample *ops)
{
	memcpy(&data->buffer[data->nextbuf],ops,sizeof(struct op_sample));
	if (++data->nextbuf!=data->buf_size)
		return;

	data->nextbuf=0;
	oprof_ready[smp_processor_id()] = 1;
	wake_up(&oprof_wait);
}
 
static void fill_op_entry(struct op_sample *ops, struct pt_regs *regs, u8 ctr)
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
 
inline static void op_do_profile(struct _oprof_data *data, struct pt_regs *regs, u8 ctr)
{
	unsigned int h = op_hash(regs->eip,current->pid,ctr);
	unsigned int i; 

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
 
static u8 op_check_ctr(struct _oprof_data *data, struct pt_regs *regs, u8 ctr) 
{ 
	unsigned long l,h;
	get_perfctr(l,h,ctr);
	/* FIXME: is using ? : better assem ? */
	if (ctr_overflowed(l)) {
		op_do_profile(data,regs,ctr);
		return 1;
	}
	return 0;
}
 
asmlinkage void op_do_nmi(struct pt_regs * regs)
{
	struct _oprof_data *data = &oprof_data[smp_processor_id()]; 
	unsigned int low,high;
	u8 overflowed=0;

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
	unsigned long v;
 
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
	unsigned long v;

	v = apic_read(APIC_LVT0);
	apic_write(APIC_LVT0, v & ~APIC_LVT_MASKED);
	v = apic_read(APIC_LVT1);
	apic_write(APIC_LVT1, v & ~APIC_LVT_MASKED);
        v = apic_read(APIC_LVTPC);
        apic_write(APIC_LVTPC, v & ~APIC_LVT_MASKED);
}
 
#define store_idt(addr) \
	do { \
	__asm__ __volatile__ ( "sidt %0" \
		: "=m" (addr) \
		: : "memory" ); } while (0)
 
#define _set_gate(gate_addr,type,dpl,addr) \
	do { \
	  int __d0, __d1; \
	  __asm__ __volatile__ ( "movw %%dx,%%ax\n\t" \
		"movw %4,%%dx\n\t" \
		"movl %%eax,%0\n\t" \
		"movl %%edx,%1" \
		:"=m" (*((long *) (gate_addr))), \
		 "=m" (*(1+(long *) (gate_addr))), "=&a" (__d0), "=&d" (__d1) \
		:"i" ((short) (0x8000+(dpl<<13)+(type<<8))), \
		 "3" ((char *) (addr)),"2" (__KERNEL_CS << 16)); \
	} while (0)
 

struct _descr { u16 limit; u32 base; } __attribute__((__packed__));
struct _idt_descr { u32 a; u32 b; } __attribute__((__packed__));
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
	unsigned long v;
	unsigned int l,h;
 
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
 
static void smp_apic_setup(void *dummy)
{
	unsigned int val;
 
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

/* FIXME: understand, credit this 
 * Enable the local APIC on a P6, and put it in "through local" mode.
 * These are the steps to perform:
 * 0. Set up the APIC memory mapping as the kernel doesn't on UP hardware.
 * 1. Set the enable bit in MSR(APIC_BASE).
 * 2. Set the enable bit in APIC(SPIV).
 * 3. Program APIC(LINT0) in ExtINT mode:
 *    - clear masked flag and interrupt vector field
 *    - set delivery mode ExtINT and trigger mode level
 * 4. Program APIC(LINT1) in NMI mode.
 *    - clear masked flag, and interrupt vector field
 *    - set delivery mode NMI and trigger mode edge
 * 5. Mask error and timer interrupts.
 * 6. Enable NMI-delivery PMC interrupts
 * 
 * Thanks to Mikael Pettersson for most of the code.
 */
static int apic_setup(void)
{
	unsigned int msr_low, msr_high;
	unsigned int val;
  
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
 
	/* enable local APIC via MSR */ 
	/* we have to do this before checking for P6 APIC */
	/* IA32 V3, 7.4.2 */
	rdmsr(MSR_APIC_BASE, msr_low, msr_high);
	wrmsr(MSR_APIC_BASE, msr_low|(1<<11), msr_high);
  
	/* check for a good APIC */
	/* IA32 V3, 7.4.15 */ 
	val = apic_read(APIC_LVR);
	if (!APIC_INTEGRATED(GET_APIC_VERSION(val)))	
		goto not_local_p6_apic;
 
	/* LVT0,LVT1,LVTT,LVTPC FIXME: what about LVTERR ?*/ 
	if (GET_APIC_MAXLVT(apic_read(APIC_LVR))!=4)
		goto not_local_p6_apic;
 
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
	/* FIXME: ref */
	val = APIC_TDR_DIV_1;
	apic_write(APIC_TDCR, val);

	smp_apic_setup(NULL);
 
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
 
static void pmc_setup(void *dummy)
{
	unsigned int low,high;
	unsigned int cpu = smp_processor_id(); 
	u8 ctr0_um,ctr1_um; 
 
	/* being careful to skirt round reserved bits */
 
	/* IA Vol. 3 Figure 14-3 */
	if (op_ctr0_val[cpu]) {
		set_perfctr(op_ctr0_count[cpu],0);
		rdmsr(P6_MSR_EVNTSEL0,low,high);
		/* clear */
		low &= (1<<21);
		/* enable interrupt generation */
		low |= (1<<20);
		/* enable chosen OS and USR counting */
		switch(op_ctr0_osusr[cpu]) {
			case 2: /* userspace only */
				low |= (1<<16);
				break;
			default: /* O/S and userspace */
				low |= (1<<16);
			case 1: /* O/S only */
				low |= (1<<17);
				break;
		}
		/* what are we counting ? */
		low |= op_ctr0_val[cpu];
		/* unit mask if any */
		if (op_ctr0_um[cpu])
			ctr0_um = (u8) simple_strtoul(op_ctr0_um[cpu],NULL,0);
		else
			ctr0_um = 0x0;
		low |= (ctr0_um<<8);
		wrmsr(P6_MSR_EVNTSEL0,low,high);
	} else {
		rdmsr(P6_MSR_EVNTSEL0,low,high);
		/* clear */
		low &= (1<<21);
		wrmsr(P6_MSR_EVNTSEL0,low,0);
	}

	if (op_ctr1_val[cpu]) {
		set_perfctr(op_ctr1_count[cpu],1);
		rdmsr(P6_MSR_EVNTSEL1,low,high); 
		low &= (3<<21); 
		/* enable interrupt generation */
		low |= (1<<20);
		/* enable chosen OS and USR counting */
		switch(op_ctr1_osusr[cpu]) {
			case 2: /* userspace only */
				low |= (1<<16);
				break;
			default: /* O/S and userspace */
				low |= (1<<16);
			case 1: /* O/S only */
				low |= (1<<17);
				break; 
		} 
		/* what are we counting ? */
		low |= op_ctr1_val[cpu]; 
		/* unit mask if any */
		if (op_ctr1_um[cpu]) 
			ctr1_um = (u8) simple_strtoul(op_ctr1_um[cpu],NULL,0);
		else
			ctr1_um = 0x0; 
		low |= (ctr1_um<<8); 
		wrmsr(P6_MSR_EVNTSEL1,low,high);
	} else {
		rdmsr(P6_MSR_EVNTSEL1,low,high); 
		/* clear */ 
		low &= (3<<21); 
		wrmsr(P6_MSR_EVNTSEL1,low,0);
	}
}

static void pmc_start(void *info)
{
	unsigned int low,high; 

	if (info && (*((unsigned int *)info)!=smp_processor_id()))
		return; 

 	/* enable counters */
	rdmsr(P6_MSR_EVNTSEL0,low,high);
	wrmsr(P6_MSR_EVNTSEL0,low|(1<<22),high); 
}
 
static void pmc_stop(void *info)
{
	unsigned int low,high; 

	if (info && (*((unsigned int *)info)!=smp_processor_id()))
		return; 

	/* disable counters */ 
	rdmsr(P6_MSR_EVNTSEL0,low,high);
	wrmsr(P6_MSR_EVNTSEL0,low&~(1<<22),high); 
} 
 
inline static void pmc_select_start(unsigned int cpu) 
{
	if (cpu==smp_processor_id())
		pmc_start(NULL);
	else
		smp_call_function(pmc_start,&cpu,0,1);
}

inline static void pmc_select_stop(unsigned int cpu) 
{
	if (cpu==smp_processor_id())
		pmc_stop(NULL);
	else
		smp_call_function(pmc_stop,&cpu,0,1);
}

/* ---------------- driver routines ------------------ */ 
 
static int oprof_open(struct inode *ino, struct file *filp)
{
	if (oprof_opened)
		return -EBUSY;
 
	oprof_opened=1; 
	smp_call_function(pmc_start,NULL,0,1);
	pmc_start(NULL);
	return 0;
}
 
static int oprof_release(struct inode *ino, struct file *filp)
{
	if (!oprof_opened)
		return -EFAULT;
 
	smp_call_function(pmc_stop,NULL,0,1);
	pmc_stop(NULL);
	oprof_opened=0;
	return 0;
}

/* FIXME: additional info struct on end, CPU# etc. ? */ 
static int oprof_read(struct file *file, char *buf, size_t count, loff_t *ppos)
{
	struct op_sample *mybuf;
	unsigned int i; 
	ssize_t max;
 
	max = sizeof(struct op_sample)*op_buf_size;

	if (*ppos || count!=max)
		return -EINVAL;

	/* FIXME: vmalloc */ 
	mybuf = kmalloc(max,GFP_KERNEL);
	if (!mybuf)
		return -EFAULT;
 
again:
	for (i=0; i<smp_num_cpus; i++) {
		if (oprof_ready[i]) {
			oprof_ready[i]=0;
			goto doit;
		}
	}

	interruptible_sleep_on(&oprof_wait);
	if (signal_pending(current)) {
		kfree(mybuf);
		return -EAGAIN;
	} 
	goto again;
 
doit:
	pmc_select_stop(i);
	memcpy(mybuf,oprof_data[i].buffer,max);
	pmc_select_start(i);
 
	if (copy_to_user(buf,mybuf, count)) {
		kfree(mybuf);
		return -EFAULT;
	}

	kfree(mybuf);
	return count;
}

static struct file_operations oprof_fops = {
	owner: THIS_MODULE,
	open: oprof_open,
	release: oprof_release,
	read: oprof_read,
};
 
/* ---------------- general setup ------------------------- */ 
 
#define OP_EVENTS_OK            0x0
#define OP_CTR0_NOT_FOUND       0x1
#define OP_CTR1_NOT_FOUND       0x2
#define OP_CTR0_NO_UM           0x4
#define OP_CTR1_NO_UM           0x8
#define OP_CTR0_NOT_ALLOWED     0x10
#define OP_CTR1_NOT_ALLOWED     0x20
#define OP_CTR0_PII_EVENT       0x40
#define OP_CTR1_PII_EVENT       0x80
#define OP_CTR0_PIII_EVENT      0x100
#define OP_CTR1_PIII_EVENT      0x200
 
#define op_check_range(val,l,h,str) do { \
	if ((val) < (l) || (val) > (h)) { \
		printk(str,(val)); \
		return 0; \
	} } while (0);  
 
static __init int parms_ok(void)
{
	int ret;
	unsigned int cpu; 
	struct _oprof_data *data; 
	u8 ctr0_um, ctr1_um; 
 
	/* FIXME: change max when vmalloc */ 
	op_check_range(op_hash_size,256,4096,"op_hash_size value %d not in range\n");
	op_check_range(op_buf_size,512,16384,"op_buf_size value %d not in range\n");
 
	for (cpu=0; cpu < smp_num_cpus; cpu++) {
		data = &oprof_data[cpu]; 
		data->ctrs |= OP_CTR_0*(!!op_ctr0_on[cpu]);
		data->ctrs |= OP_CTR_1*(!!op_ctr1_on[cpu]);

		/* FIXME: maybe we should allow no set on a CPU ? */
		if (!data->ctrs) {
			printk("oprofile: neither counter enabled for CPU%d\n",cpu);
			return 0;
		}

		if (data->ctrs&OP_CTR_0) {
			op_check_range(op_ctr0_count[cpu],500,OP_MAX_PERF_COUNT,"ctr0 count value %d not in range\n");
			data->ctr_count[0]=op_ctr0_count[cpu]; 
		}
		if (data->ctrs&OP_CTR_1) {
			op_check_range(op_ctr1_count[cpu],500,OP_MAX_PERF_COUNT,"ctr1 count value %d not in range\n");
			data->ctr_count[1]=op_ctr1_count[cpu]; 
		}
 
		if (op_ctr0_um[cpu]) 
			ctr0_um = (u8) simple_strtoul(op_ctr0_um[cpu],NULL,0);
		else
			ctr0_um = 0x0; 
 
		if (op_ctr1_um[cpu]) 
			ctr1_um = (u8) simple_strtoul(op_ctr1_um[cpu],NULL,0);
		else
			ctr1_um = 0x0; 
 
		ret = op_check_events_str(op_ctr0_type[cpu], op_ctr1_type[cpu], ctr0_um, ctr1_um, 
			cpu_type, &op_ctr0_val[cpu], &op_ctr1_val[cpu]);
 
		if (ret&OP_CTR0_NOT_FOUND) printk("oprofile: ctr0: no such event\n");
		if (ret&OP_CTR1_NOT_FOUND) printk("oprofile: ctr1: no such event\n");
		if (ret&OP_CTR0_NO_UM) printk("oprofile: ctr0: invalid unit mask\n");
		if (ret&OP_CTR1_NO_UM) printk("oprofile: ctr1: invalid unit mask\n");
		if (ret&OP_CTR0_NOT_ALLOWED) printk("oprofile: ctr0: can't count event\n");
		if (ret&OP_CTR1_NOT_ALLOWED) printk("oprofile: ctr1: can't count event\n");
		if (ret&OP_CTR0_PII_EVENT) printk("oprofile: ctr0: event only available on PII\n");
		if (ret&OP_CTR1_PII_EVENT) printk("oprofile: ctr1: event only available on PII\n");
		if (ret&OP_CTR0_PIII_EVENT) printk("oprofile: ctr0: event only available on PIII\n");
		if (ret&OP_CTR1_PIII_EVENT) printk("oprofile: ctr1: event only available on PIII\n");

		if (ret)
			return 0;
	}
 
	return 1; 
}

static void oprof_free_mem(unsigned int num)
{
	unsigned int i;
	for (i=0; i < num; i++) {
		kfree(oprof_data[i].entries);
		kfree(oprof_data[i].buffer);
	}
}
 
static int __init oprof_init_data(void)
{
	unsigned int i;
	unsigned long hash_size,buf_size; 
	struct _oprof_data *data; 

	init_waitqueue_head(&oprof_wait); 
 
	for (i=0; i < smp_num_cpus; i++) {
		data = &oprof_data[i]; 
		hash_size = (sizeof(struct op_entry)*op_hash_size);
		buf_size = (sizeof(struct op_sample)*op_buf_size);
 
		/* FIXME: use vmalloc not kmalloc to allow sizes bigger than 4096*32==131072 */ 
		data->entries = kmalloc(hash_size,GFP_KERNEL);
		if (!data->entries) {
			printk("oprofile: failed to allocate hash table of %lu bytes\n",hash_size);
			oprof_free_mem(i); 
			return -EFAULT;
		}
 
		data->buffer = kmalloc(buf_size,GFP_KERNEL);
		if (!data->buffer) {
			printk("oprofile: failed to allocate eviction buffer of %lu bytes\n",buf_size);
			kfree(data->entries); 
			oprof_free_mem(i); 
			return -EFAULT;
		}
 
		memset(data->entries,0,hash_size);
		memset(data->buffer,0,buf_size);

		data->hash_size = op_hash_size;
		data->buf_size = op_buf_size;
		/* the rest are set in parms_ok() */ 
	}
 
	return 0; 
}
 
int __init hw_ok(void)
{
	if (current_cpu_data.x86_vendor != X86_VENDOR_INTEL || 
	    current_cpu_data.x86 < 6) {
		printk("oprofile: not an Intel P6 processor. Sorry.\n");
		return 0;
	}

	/* FIXME: is this correct ? */
	/* 0 if PPro, 1 if PII, 2 if PIII */
	cpu_type = (current_cpu_data.x86_model > 2);
	cpu_type += !!(current_cpu_data.x86_model > 5);
	printk("CPU type %u\n",cpu_type);
	return 1; 
}
 
int __init oprof_init(void)
{
	int err;
 
	if (!parms_ok() || !hw_ok())
		return -EINVAL;
 
	printk("%s\n",op_version);

	if ((err=oprof_init_data()))
		return err;
	install_nmi();
 
	if ((err=apic_setup())) {
		restore_nmi();
		oprof_free_mem(smp_num_cpus); 
		return err;
	}
 
	if ((err=smp_call_function(smp_apic_setup,NULL,0,1)))
		goto out_err;
 
	if ((err=smp_call_function(pmc_setup,NULL,0,1)))
		goto out_err; 
	pmc_setup(NULL);

 	err = op_major = register_chrdev(0,"oprof",&oprof_fops);    
	if (err<0)
		goto out_err;
 
	op_intercept_syscalls();
 
	printk("oprofile: /dev/oprofile enabled, major %u\n",op_major); 
	return 0;
 
out_err:
	smp_call_function(disable_local_P6_APIC,NULL,0,1);
	disable_local_P6_APIC(NULL);
	restore_nmi();
	oprof_free_mem(smp_num_cpus); 
	return err; 
}

void __exit oprof_exit(void)
{
	op_replace_syscalls();
 
	unregister_chrdev(op_major,"oprof");

	if (smp_call_function(disable_local_P6_APIC,NULL,0,1))
		return;
	disable_local_P6_APIC(NULL);

	restore_nmi();
 
	/* give time for in-flight NMIs on other CPUs
	   to finish (?) */ 
	udelay(1000000);
 
	oprof_free_mem(smp_num_cpus);
	return;
}
 
module_init(oprof_init);
module_exit(oprof_exit);
