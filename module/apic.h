#ifndef __ASM_APIC_H
#define __ASM_APIC_H

#include <linux/config.h>
#include <asm/system.h>
#include "apicdef.h"

#ifdef CONFIG_X86_LOCAL_APIC

#define APIC_DEBUG 0

#if APIC_DEBUG
#define Dprintk(x...) printk(x)
#else
#define Dprintk(x...)
#endif

/* FIXME defined as extern inline in 2.2, compiler is not happy, that's
 * perhaps only a problem of configure. Check if function are generated
 * inline or not */
#if 0
/*
 * Basic functions accessing APICs.
 */

static __inline void apic_write(unsigned long reg, unsigned long v)
{
	*((volatile unsigned long *)(APIC_BASE+reg)) = v;
}

static __inline void apic_write_atomic(unsigned long reg, unsigned long v)
{
	/* phe: FIXME commented, strange compiler warning about the typeof
         * which convert volatile xxx * to xxx *, that's perhaps a problem
	 * of typeof which remove cv-qualifier from a type. Is it a problem
	 * or not depend on the exact meaning of volatile in gcc and the APIC
	 * behavior, unhopefully some apic register behavior rely on not
	 * unnecessary read (error register)
	 * look like a long discussion in gcc mail list about volatile about
	 * volatile int p;
	 * p;
	 * don't remember the subject, started in december 2001. Take a bunch
	 * of aspirine before reading it :/
	 */
	xchg((/*volatile*/ unsigned long *)(APIC_BASE+reg), v);
}

static __inline unsigned long apic_read(unsigned long reg)
{
	return *((volatile unsigned long *)(APIC_BASE+reg));
}

static __inline__ void apic_wait_icr_idle(void)
{
	do { } while ( apic_read( APIC_ICR ) & APIC_ICR_BUSY );
}
#endif

#ifdef CONFIG_X86_GOOD_APIC
# define FORCE_READ_AROUND_WRITE 0
# define apic_read_around(x)
# define apic_write_around(x,y) apic_write((x),(y))
#else
# define FORCE_READ_AROUND_WRITE 1
# define apic_read_around(x) apic_read(x)
# define apic_write_around(x,y) apic_write_atomic((x),(y))
#endif

static inline void ack_APIC_irq(void)
{
	/*
	 * ack_APIC_irq() actually gets compiled as a single instruction:
	 * - a single rmw on Pentium/82489DX
	 * - a single write on P6+ cores (CONFIG_X86_GOOD_APIC)
	 * ... yummie.
	 */

	/* Docs say use 0 for future compatibility */
	apic_write_around(APIC_EOI, 0);
}

extern int get_maxlvt(void);
extern void clear_local_APIC(void);
extern void connect_bsp_APIC (void);
extern void disconnect_bsp_APIC (void);
extern void disable_local_APIC (void);
extern int verify_local_APIC (void);
extern void cache_APIC_registers (void);
extern void sync_Arb_IDs (void);
extern void init_bsp_APIC (void);
extern void setup_local_APIC (void);
extern void init_apic_mappings (void);
extern void smp_local_timer_interrupt (struct pt_regs * regs);
extern void setup_APIC_clocks (void);
extern void setup_apic_nmi_watchdog (void);
extern inline void nmi_watchdog_tick (struct pt_regs * regs);
extern int APIC_init_uniprocessor (void);

extern unsigned int apic_timer_irqs [NR_CPUS];
extern int check_nmi_watchdog (void);

extern unsigned int nmi_watchdog;
#define NMI_NONE	0
#define NMI_IO_APIC	1
#define NMI_LOCAL_APIC	2
#define NMI_INVALID	3

#endif /* CONFIG_X86_LOCAL_APIC */

#endif /* __ASM_APIC_H */
