/* 
 * @file architecture specific interfaces
 * @remark Copyright 2008 Intel Corporation
 * @remark Read the file COPYING
 * @author Andi Kleen
 */

#if defined(__i386__) || defined(__x86_64__) 

/* Assume we run on the same host as the profilee */

#define num_to_mask(x) ((1U << (x)) - 1)

static inline unsigned arch_get_filter(op_cpu cpu_type)
{
	if (cpu_type == CPU_ARCH_PERFMON) { 
		unsigned ebx, eax;
		asm("cpuid" : "=a" (eax), "=b" (ebx) : "0" (0xa) : "ecx","edx");
		return ebx & num_to_mask(eax >> 24);
	}
	return 0;
}

static inline int arch_num_counters(op_cpu cpu_type) 
{
	if (cpu_type == CPU_ARCH_PERFMON) {
		unsigned v;
		asm("cpuid" : "=a" (v) : "0" (0xa) : "ebx","ecx","edx");
		return (v >> 8) & 0xff;
	} 
	return -1;
}

static inline unsigned arch_get_counter_mask(void)
{
	unsigned v;
	asm("cpuid" : "=a" (v) : "0" (0xa) : "ebx","ecx","edx");
	return num_to_mask((v >> 8) & 0xff);	
}

#else

static inline unsigned arch_get_filter(op_cpu cpu_type)
{
	/* Do something with passed arg to shut up the compiler warning */
	if (cpu_type != CPU_NO_GOOD)
		return 0;
	return 0;
}

static inline int arch_num_counters(op_cpu cpu_type) 
{
	/* Do something with passed arg to shut up the compiler warning */
	if (cpu_type != CPU_NO_GOOD)
		return -1;
	return -1;
}

static inline unsigned arch_get_counter_mask(void)
{
	return 0;
}

#endif
