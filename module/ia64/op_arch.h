/**
 * @file op_arch.h
 * defines machine specific values for ia64
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author Will Cohen
 */

#ifndef OP_ARCH_H
#define OP_ARCH_H

/* How to access the processor's instruction pointer */
#define INST_PTR(regs) ((regs)->cr_iip)

/* How to access the processor's status register */
#define STATUS(regs) ((regs)->cr_ipsr)

/* Bit in processor's status register for interrupt masking */
/* This is always okay on the ia64 because the perfmon interrupts
   on the ia64 Linux are just regular irqs. Thus, there is no
   chance that the performance monitoring hardware interrupted
   code with the interrupts enabled, e.g. wake_up */
#define IRQ_ENABLED(eflags)	(1)

/* Valid bits in PMD registers */
#define IA64_1_PMD_MASK_VAL	((1UL << 32) - 1)
#define IA64_2_PMD_MASK_VAL	((1UL << 47) - 1)

#endif /* OP_ARCH_H */
