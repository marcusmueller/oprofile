/**
 * @file op_msr.h
 * x86-specific MSR stuff
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author John Levon
 * @author Philippe Elie
 */

#ifndef OP_MSR_H
#define OP_MSR_H
 
/* work-around compiler bug in gcc 2.91.66, just mark all input register as
 * magically cloberred by wrmsr */
#if __GNUC__ == 2 && __GNUC_MINOR__ == 91
	#undef wrmsr
	#define wrmsr(msr,val1,val2)					\
	     __asm__ __volatile__("wrmsr"				\
				  : /* no outputs */			\
				  : "c" (msr), "a" (val1), "d" (val2)	\
				  : "ecx", "eax", "edx")
#endif

/* MSRs */
#ifndef MSR_P6_PERFCTR0
#define MSR_P6_PERFCTR0 0xc1
#endif
#ifndef MSR_P6_PERFCTR1
#define MSR_P6_PERFCTR1 0xc2
#endif
#ifndef MSR_P6_EVNTSEL0
#define MSR_P6_EVNTSEL0 0x186
#endif
#ifndef MSR_P6_EVNTSEL1
#define MSR_P6_EVNTSEL1 0x187
#endif
#ifndef MSR_K7_PERFCTL0
#define MSR_K7_PERFCTL0 0xc0010000
#endif
#ifndef MSR_K7_PERFCTL1
#define MSR_K7_PERFCTL1 0xc0010001
#endif
#ifndef MSR_K7_PERFCTL2
#define MSR_K7_PERFCTL2 0xc0010002
#endif
#ifndef MSR_K7_PERFCTL3
#define MSR_K7_PERFCTL3 0xc0010003
#endif
#ifndef MSR_K7_PERFCTR0
#define MSR_K7_PERFCTR0 0xc0010004
#endif
#ifndef MSR_K7_PERFCTR1
#define MSR_K7_PERFCTR1 0xc0010005
#endif
#ifndef MSR_K7_PERFCTR2
#define MSR_K7_PERFCTR2 0xc0010006
#endif
#ifndef MSR_K7_PERFCTR3
#define MSR_K7_PERFCTR3 0xc0010007
#endif

#endif /* OP_MSR_H */
