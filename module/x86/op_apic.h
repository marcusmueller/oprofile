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

#define NMI_GATE_TYPE 14
#define NMI_VECTOR_NUM 2


#ifndef CONFIG_X86_64

#define NMI_DPL_LEVEL 0

/* copied from kernel 2.4.19 : arch/i386/traps.c */

struct gate_struct {
	u32 a;
	u32 b;
} __attribute__((packed));

#define _set_gate(gate_addr,type,dpl,addr) \
do { \
  int __d0, __d1; \
  __asm__ __volatile__ ("movw %%dx,%%ax\n\t" \
	"movw %4,%%dx\n\t" \
	"movl %%eax,%0\n\t" \
	"movl %%edx,%1" \
	:"=m" (*((long *) (gate_addr))), \
	 "=m" (*(1+(long *) (gate_addr))), "=&a" (__d0), "=&d" (__d1) \
	:"i" ((short) (0x8000+(dpl<<13)+(type<<8))), \
	 "3" ((char *) (addr)),"2" (__KERNEL_CS << 16)); \
} while (0)

#else /* CONFIG_X86_64 */

/* 
 * copied + modified slightly from x86-64.org kernel 
 * 2.1.19 : include/asm-x86_64/desc.h 
*/

#define NMI_DPL_LEVEL 3

struct gate_struct {          
	u16 offset_low;
	u16 segment; 
	unsigned ist : 3, zero0 : 5, type : 5, dpl : 2, p : 1;
	u16 offset_middle;
	u32 offset_high;
	u32 zero1; 
} __attribute__((packed));

#define PTR_LOW(x) ((unsigned long)(x) & 0xFFFF) 
#define PTR_MIDDLE(x) (((unsigned long)(x) >> 16) & 0xFFFF)
#define PTR_HIGH(x) ((unsigned long)(x) >> 32)

static inline void _set_gate(void * adr, unsigned type, unsigned dpl, void * fn)
{
	struct gate_struct s;
	unsigned long func = (unsigned long)fn;
	s.offset_low = PTR_LOW(func);
	s.segment = __KERNEL_CS;
	s.ist = 0;
	s.p = 1;
	s.dpl = dpl;
	s.zero0 = 0;
	s.zero1 = 0;
	s.type = type;
	s.offset_middle = PTR_MIDDLE(func);
	s.offset_high = PTR_HIGH(func);
	/* does not need to be atomic because it is only done once at setup time */ 
	memcpy(adr, &s, 16); 
} 

#endif /* CONFIG_X86_64 */
	
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
