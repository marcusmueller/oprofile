/*
 * op_apic.c
 *
 * APIC setup etc. routines
 */

#include <linux/mm.h>
#include <linux/init.h>
#include <linux/config.h>
#include <asm/io.h>

#include "oprofile.h"
 
static ulong idt_addr;
static ulong kernel_nmi;
static ulong lvtpc_masked;

/* this masking code is unsafe and nasty but might deal with the small
 * race when installing the NMI entry into the IDT
 */
static void mask_lvtpc(void * e)
{
	ulong v = apic_read(APIC_LVTPC);
	lvtpc_masked = v & APIC_LVT_MASKED;
	apic_write(APIC_LVTPC, v | APIC_LVT_MASKED);
}

static void unmask_lvtpc(void * e)
{
	if (!lvtpc_masked)
		apic_write(APIC_LVTPC, apic_read(APIC_LVTPC) & ~APIC_LVT_MASKED);
}
 
void install_nmi(void)
{
	volatile struct _descr descr = { 0, 0,};
	volatile struct _idt_descr *de;

	store_idt(descr);
	idt_addr = descr.base;
	de = (struct _idt_descr *)idt_addr;
	/* NMI handler is at idt_table[2] */
	de += 2;
	/* see Intel Vol.3 Figure 5-2, interrupt gate */
	kernel_nmi = (de->a & 0xffff) | (de->b & 0xffff0000);

	smp_call_function(mask_lvtpc, NULL, 0, 1);
	mask_lvtpc(NULL);
	_set_gate(de, 14, 0, &op_nmi);
	smp_call_function(unmask_lvtpc, NULL, 0, 1);
	unmask_lvtpc(NULL);
}

void restore_nmi(void)
{
	smp_call_function(mask_lvtpc, NULL, 0, 1);
	mask_lvtpc(NULL);
	_set_gate(((char *)(idt_addr)) + 16, 14, 0, kernel_nmi);
	smp_call_function(unmask_lvtpc, NULL, 0, 1);
	unmask_lvtpc(NULL);
}

/* ---------------- APIC setup ------------------ */

static uint lvtpc_old_mask[NR_CPUS];
static uint lvtpc_old_mode[NR_CPUS];

void __init lvtpc_apic_setup(void *dummy)
{
	uint val;

	/* set up LVTPC as we need it */
	/* IA32 V3, Figure 7.8 */
	val = apic_read(APIC_LVTPC);
	lvtpc_old_mask[op_cpu_id()] = val & APIC_LVT_MASKED;
	/* allow PC overflow interrupts */
	val &= ~APIC_LVT_MASKED;
	/* set delivery to NMI */
	lvtpc_old_mode[op_cpu_id()] = GET_APIC_DELIVERY_MODE(val);
	val = SET_APIC_DELIVERY_MODE(val, APIC_MODE_NMI);
	apic_write(APIC_LVTPC, val);
}

/* not safe to mark as __exit since used from __init code */
void lvtpc_apic_restore(void *dummy)
{
	uint val = apic_read(APIC_LVTPC);
	// FIXME: this gives APIC errors on SMP hardware.
	// val = SET_APIC_DELIVERY_MODE(val, lvtpc_old_mode[op_cpu_id()]);
	if (lvtpc_old_mask[op_cpu_id()])
		val |= APIC_LVT_MASKED;
	else
		val &= ~APIC_LVT_MASKED;
	apic_write(APIC_LVTPC, val);
}

static int __init enable_apic(void)
{
	uint msr_low, msr_high;
	uint val;

	/* enable local APIC via MSR. Forgetting this is a fun way to
	 * lock the box. But we have to hope this is allowed if the APIC
	 * has already been enabled.
	 *
	 * IA32 V3, 7.4.2 */
	rdmsr(MSR_IA32_APICBASE, msr_low, msr_high);
	if ((msr_low & (1 << 11)) == 0)
		wrmsr(MSR_IA32_APICBASE, msr_low | (1<<11), msr_high);

	/* even if the apic is up we must check for a good APIC */

	/* IA32 V3, 7.4.15 */
	val = apic_read(APIC_LVR);
	if (!APIC_INTEGRATED(GET_APIC_VERSION(val)))	
		goto not_local_p6_apic;

	/* LVT0,LVT1,LVTT,LVTPC */
	if (GET_APIC_MAXLVT(apic_read(APIC_LVR)) != 4)
		goto not_local_p6_apic;

	/* IA32 V3, 7.4.14.1 */
	val = apic_read(APIC_SPIV);
	if (!(val & APIC_SPIV_APIC_ENABLED))
		apic_write(APIC_SPIV, val | APIC_SPIV_APIC_ENABLED);

	return !!(val & APIC_SPIV_APIC_ENABLED);
 
not_local_p6_apic:
	rdmsr(MSR_IA32_APICBASE, msr_low, msr_high);
	/* disable the apic only if it was disabled */
	if ((msr_low & (1 << 11)) == 0)
		wrmsr(MSR_IA32_APICBASE, msr_low & ~(1<<11), msr_high);

	printk(KERN_ERR "oprofile: no local P6 APIC. Falling back to RTC mode.\n");
	return -ENODEV;
}

void __init do_apic_setup(void)
{
	uint val;
 
	__cli();
 
	val = APIC_LVT_LEVEL_TRIGGER;
	val = SET_APIC_DELIVERY_MODE(val, APIC_MODE_EXINT);
	apic_write(APIC_LVT0, val);

	/* edge triggered, IA 7.4.11 */
	val = SET_APIC_DELIVERY_MODE(0, APIC_MODE_NMI);
	apic_write(APIC_LVT1, val);

	/* clear error register */
	/* IA32 V3, 7.4.17 */
	/* PHE must be cleared after unmasking by a back-to-back write,
	 * but it is probably ok because we mask only, the ESR is not
	 * updated is this a real problem ? */
	apic_write(APIC_ESR, 0);

	/* mask error interrupt */
	/* IA32 V3, Figure 7.8 */
	val = apic_read(APIC_LVTERR);
	val |= APIC_LVT_MASKED;
	apic_write(APIC_LVTERR, val);

	/* setup timer vector */
	/* IA32 V3, 7.4.8 */
	apic_write(APIC_LVTT, APIC_SEND_PENDING | 0x31);

	/* Divide configuration register */
	/* PHE the apic clock is based on the FSB. This should only
	 * changed with a calibration method.  */
	val = APIC_TDR_DIV_1;
	apic_write(APIC_TDCR, val);

	__sti();
}
 
/* does the CPU have a local APIC ? */
static int __init check_cpu_ok(void)
{
	if (sysctl.cpu_type != CPU_PPRO &&
		sysctl.cpu_type != CPU_PII &&
		sysctl.cpu_type != CPU_PIII &&
		sysctl.cpu_type != CPU_ATHLON)
		return 0; 

	return 1;
}
 
int __init apic_setup(void)
{
	uint val;

	if (!check_cpu_ok())
		goto nodev;

	fixmap_setup();

	switch (enable_apic()) {
		case 0:
			do_apic_setup();
			val = apic_read(APIC_ESR);
			printk(KERN_INFO "oprofile: enabled local APIC. Err code %.08x\n", val);
			break;
		case 1:
			printk(KERN_INFO "oprofile: APIC was already enabled\n");
			break;
		default:
			goto nodev;
	}

	lvtpc_apic_setup(NULL);
	return 0;
nodev:
	printk(KERN_WARNING "Your CPU does not have a local APIC, e.g. "
	       "mobile P6. Falling back to RTC mode.\n");
	return -ENODEV;
}

void apic_restore(void)
{
	fixmap_restore();
}
