/*
 * op_x86.c
 *
 * A variety of Intel hardware grubbery.
 *
 * Based in part on arch/i386/kernel/mpparse.c
 */

#include <linux/mm.h>
#include <linux/init.h>
#include <linux/config.h>
#include <asm/io.h>

#include "oprofile.h"
 
/* ---------------- NMI handler setup ------------ */

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

static int smp_hardware;
 
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

/*
 * This logic is unfortunately horrendous. We are trying to
 * determine if the kernel has set up the APIC or not.
 *
 * It doesn't even work if there are BIOS problems or the like :(
 *
 * FIXME:
 * This just need to get the case where we really can't setup the apic.
 * On other case we rely on the robusteness of code.
 */
static int __init apic_needs_setup(void)
{
	/* FIXME: always safe to try to setup apic if !smp_hardware ? or
         * do you need to ponderate this with :/ #ifdef ... #endif */
	return !smp_hardware;
}

static int __init enable_apic(void)
{
	uint msr_low, msr_high;
	uint val;

	/* ugly hack */
	my_set_fixmap();

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

	return 1;
 
not_local_p6_apic:
	rdmsr(MSR_IA32_APICBASE, msr_low, msr_high);
	/* disable the apic only if it was disabled */
	if ((msr_low & (1 << 11)) == 0)
		wrmsr(MSR_IA32_APICBASE, msr_low & ~(1<<11), msr_high);

	printk(KERN_ERR "oprofile: no local P6 APIC. Your laptop doesn't have one !\n");
	return 0;
}

/* does the CPU have a local APIC ? */
static int __init check_p6_ok(void)
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
	int apic_was_enabled;

	/* paranoid checking */
	if (!check_p6_ok()) {
		printk("Your CPU does not have a local APIC, e.g. "
		       "mogile P6. No profiling can be done.\n");
		return -ENODEV;
	}

	if (!apic_needs_setup()) {
		printk(KERN_INFO "oprofile: no APIC setup needed.\n");
		lvtpc_apic_setup(NULL);
		return 0;
	}

	printk(KERN_INFO "oprofile: setting up APIC.\n");

	if (!enable_apic())
		return -ENODEV;
 
	/* enable APIC locally */
	/* IA32 V3, 7.4.14.1 */
	val = apic_read(APIC_SPIV);
	apic_was_enabled = val & APIC_SPIV_APIC_ENABLED;

	if (apic_was_enabled) {
		printk(KERN_INFO "oprofile: APIC was already enabled\n");
		lvtpc_apic_setup(NULL);
		return 0;
	}
 
	/* we blindly assume than if apic is up is state is ok */
 
	__cli();
 
	apic_write(APIC_SPIV, val | APIC_SPIV_APIC_ENABLED);

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

	val = apic_read(APIC_ESR);
	printk(KERN_INFO "oprofile: enabled local APIC err code %.08x\n", val);

	lvtpc_apic_setup(NULL);

	return 0;
}

/* ---------------- fixmap hack ------------------ */
 
#ifdef NEED_FIXMAP_HACK

/* TODO: this must be ruled in compat.h */
#ifndef APIC_DEFAULT_PHYS_BASE
#define APIC_DEFAULT_PHYS_BASE 0xfee00000
#endif
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
	/* when !CONFIG_X86_LOCAL_APIC we can't rely on no cache flag set */
	pgprot_val(prot) |= _PAGE_PCD;
	if (test_bit(X86_FEATURE_PGE, &boot_cpu_data.x86_capability))
		pgprot_val(prot) |= _PAGE_GLOBAL;
	set_pte(pte, mk_pte_phys(phys, prot));
	__flush_tlb_one(vaddr);
}

#ifndef CONFIG_X86_LOCAL_APIC
unsigned long virt_apic_base;
#endif

void my_set_fixmap(void)
{
#ifdef CONFIG_X86_LOCAL_APIC
	ulong address = __fix_to_virt(FIX_APIC_BASE);

	set_pte_phys (address, APIC_DEFAULT_PHYS_BASE);
#else
	/* dirty hack :/ and memory leak at unload ... */
	virt_apic_base = (unsigned long)vmalloc(4096);

	set_pte_phys(virt_apic_base, APIC_DEFAULT_PHYS_BASE);
#endif
}
#else /* NEED_FIXMAP_HACK */
void my_set_fixmap(void)
{
}
#endif /* NEED_FIXMAP_HACK */

/* ---------------- MP table code ------------------ */
 
#ifndef NO_MPTABLE_CHECK_NEEDED

static int __init mpf_checksum(unsigned char *mp, int len)
{
	int sum = 0;

	while (len--)
		sum += *mp++;

	return sum & 0xFF;
}

static int __init mpf_table_ok(struct intel_mp_floating * mpf, unsigned long *bp)
{
	if (*bp != SMP_MAGIC_IDENT)
		return 0;
	if (mpf->mpf_length != 1)
		return 0;
	if (mpf_checksum((unsigned char *)bp, 16))
		return 0;

	return (mpf->mpf_specification == 1 || mpf->mpf_specification == 4);
}

static int __init smp_scan_config (unsigned long base, unsigned long length)
{
	unsigned long *bp = phys_to_virt(base);
	struct intel_mp_floating *mpf;

	while (length > 0) {
		mpf = (struct intel_mp_floating *)bp;
		if (mpf_table_ok(mpf, bp))
			return 1;
		bp += 4;
		length -= 16;
	}
	return 0;
}
#else
static int __init smp_scan_config (unsigned long base, unsigned long length)
{
	return 0;
}
#endif /* !NO_MPTABLE_CHECK_NEEDED */

void __init find_intel_smp (void)
{
	unsigned int address;

	/*
	 * Linux assumes you have 640K of base ram..
	 * this continues the error...
	 *
	 * 1) Scan the bottom 1K for a signature
	 * 2) Scan the top 1K of base RAM
	 * 3) Scan the 64K of bios
	 */
	if (smp_scan_config(0x0,0x400) ||
		smp_scan_config(639*0x400,0x400) ||
		smp_scan_config(0xF0000,0x10000)) {
		smp_hardware = 1;
		return;
	}
	/*
	 * If it is an SMP machine we should know now, unless the
	 * configuration is in an EISA/MCA bus machine with an
	 * extended bios data area.
	 *
	 * there is a real-mode segmented pointer pointing to the
	 * 4K EBDA area at 0x40E, calculate and scan it here.
	 *
	 * NOTE! There are Linux loaders that will corrupt the EBDA
	 * area, and as such this kind of SMP config may be less
	 * trustworthy, simply because the SMP table may have been
	 * stomped on during early boot. These loaders are buggy and
	 * should be fixed.
	 */

	address = *(unsigned short *)phys_to_virt(0x40E);
	address <<= 4;
	smp_hardware = smp_scan_config(address, 0x1000);
}
