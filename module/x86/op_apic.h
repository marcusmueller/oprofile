/**
 * @file op_apic.h
 * x86 apic, nmi, perf counter declaration
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author John Levon
 * @author Philippe Elie
 * @author Dave Jones
 * @author Graydon Hoare
 */

#ifndef OP_APIC_H
#define OP_APIC_H

#include "apic_compat.h"

/* see sect 5.10, intel x86 system programming manual */
/* and sect 4.8.4, pg 112, amd x86-64 system programming manual */

#define NMI_GATE_TYPE 14
#define NMI_VECTOR_NUM 2

struct gate_struct {
	u32 a;
	u32 b;
#ifdef CONFIG_X86_64
	u32 c;
	u32 d;
#endif /* CONFIG_X86_64 */
} __attribute__((__packed__));

#define SET_BITS(dest,val,mask) ((dest) = ((dest) & ~(mask)) | ((val) & (mask)))

#define SET_GATE_SELECTOR(gate,sel)     SET_BITS((gate).a, ((sel) << 16), 0xffff0000)
#define SET_GATE_TARGET_LOW(gate,targ)  SET_BITS((gate).a, (targ), 0x0000ffff)
#define SET_GATE_TARGET_MID(gate,targ)  SET_BITS((gate).b, (targ), 0xffff0000)
#define SET_GATE_ZERO1(gate,val)        SET_BITS((gate).b, ((val) << 5), 0xe0)
#define SET_GATE_S(gate,s)              SET_BITS((gate).b, ((s) << 12), 0x1000)
#define SET_GATE_DPL(gate,dpl)          SET_BITS((gate).b, ((dpl) << 13), 0x6000)
#define SET_GATE_P(gate,p)              SET_BITS((gate).b, ((p) << 15), 0x8000)
#define SET_GATE_TYPE(gate,type)        SET_BITS((gate).b, ((type) << 8), 0xf00)
#define SET_GATE_TARGET_HIGH(gate,targ) SET_BITS((gate).c, ((targ) >> 32), 0xffffffff)
#define SET_GATE_CONSTANT_BITS(gate)    SET_BITS((gate).d, 0, 0x1f00) 

static inline void _set_gate(struct gate_struct * adr, void * fnptr)
{
	unsigned long fn = (unsigned long)fnptr;
	struct gate_struct tmp = *adr;

#ifndef CONFIG_X86_64
	SET_GATE_DPL(tmp, 0);
#else /* CONFIG_X86_64 */
	SET_GATE_DPL(tmp, 3);
	SET_GATE_TARGET_HIGH(tmp, fn);
	SET_GATE_CONSTANT_BITS(tmp);
#endif /* CONFIG_X86_64 */

	SET_GATE_TARGET_LOW(tmp, fn);
	SET_GATE_TARGET_MID(tmp, fn);
	SET_GATE_ZERO1(tmp, 0);
	SET_GATE_S(tmp, 0);
	SET_GATE_TYPE(tmp, NMI_GATE_TYPE);
	SET_GATE_P(tmp, 1);
	SET_GATE_SELECTOR(tmp, __KERNEL_CS);

	*adr = tmp;
};
	
#define store_idt(addr) \
	do { \
		__asm__ __volatile__ ( "sidt %0" \
			: "=m" (addr) \
			: : "memory" ); \
	} while (0)
 
struct _descr { 
	u16 limit; 
	struct gate_struct * base; 
} __attribute__((__packed__));

/* read/write of perf counters */
#define get_perfctr(l,h,c) do { rdmsr(perfctr_msr[(c)], (l), (h)); } while (0)
#define set_perfctr(l,c) do { wrmsr(perfctr_msr[(c)], -(u32)(l), -1); } while (0)
#define ctr_overflowed(n) (!((n) & (1U<<31)))

void lvtpc_apic_setup(void *dummy);
void lvtpc_apic_restore(void *dummy);
int apic_setup(void);
void apic_restore(void);
void install_nmi(void);
void restore_nmi(void);

void fixmap_setup(void);
void fixmap_restore(void);

asmlinkage void op_nmi(void);

#endif /* OP_APIC_H */
