/* 
 * @file architecture specific interfaces
 * @remark Copyright 2008 Intel Corporation
 * @remark Read the file COPYING
 * @author Andi Kleen
 */

#if defined(__i386__) || defined(__x86_64__) 

/* Assume we run on the same host as the profilee */

#define num_to_mask(x) ((1U << (x)) - 1)

typedef struct {
	unsigned eax, ebx, ecx, edx;
} cpuid_data;

#if defined(__i386__)
static inline void cpuid(int func, cpuid_data * p)
{
	asm("push %%ebx; cpuid; mov %%ebx, %%esi; pop %%ebx"
	    : "=a" (p->eax), "=S" (p->ebx), "=c" (p->ecx), "=d" (p->edx)
	    : "0" (func));
}
#else
static inline void cpuid(int func, cpuid_data * p)
{
	asm("cpuid"
	    : "=a" (p->eax), "=b" (p->ebx), "=c" (p->ecx), "=d" (p->edx)
	    : "0" (func));
}
#endif

static inline int cpuid_vendor(char *vnd)
{
	union {
		struct {
			unsigned b,d,c;
		};
		char v[12];
	} v;
	cpuid_data data;
	cpuid(0, &data);
	v.b = data.ebx; v.c = data.ecx; v.d = data.edx;
	return !strncmp(v.v, vnd, 12);
}

static inline unsigned int cpuid_signature()
{
	cpuid_data data;
	cpuid(1, &data);
	return data.eax;
}

static inline unsigned int cpu_model(unsigned int eax)
{
	unsigned model = (eax & 0xf0) >> 4;
	unsigned ext_model = (eax & 0xf0000) >> 12;
	return  ext_model + model;
}

static inline unsigned int cpu_family(unsigned int eax)
{
	unsigned family =  (eax & 0xf00) >> 8;
	unsigned ext_family = (eax & 0xff00000) >> 20;
	return ext_family + family;
}

static inline unsigned int cpu_stepping(unsigned int eax)
{
	return (eax & 0xf);
}


/* Work around Nehalem spec update AAJ79: CPUID incorrectly indicates
   unhalted reference cycle architectural event is supported. We assume
   steppings after C0 report correct data in CPUID. */
static inline void workaround_nehalem_aaj79(unsigned *ebx)
{
	unsigned eax;

	if (!cpuid_vendor("GenuineIntel"))
		return;
	eax = cpuid_signature();
	if (cpu_family(eax) != 6 || cpu_model(eax) != 26
		|| cpu_stepping(eax) > 4)
		return;
	*ebx |= (1 << 2);	/* disable unsupported event */
}

static inline unsigned arch_get_filter(op_cpu cpu_type)
{
	if (op_cpu_base_type(cpu_type) == CPU_ARCH_PERFMON) { 
		cpuid_data data;
		cpuid(0xa, &data);
		workaround_nehalem_aaj79(&data.ebx);
		return data.ebx & num_to_mask(data.eax >> 24);
	}
	return -1U;
}

static inline int arch_num_counters(op_cpu cpu_type) 
{
	if (op_cpu_base_type(cpu_type) == CPU_ARCH_PERFMON) {
		cpuid_data data;
		cpuid(0xa, &data);
		return (data.eax >> 8) & 0xff;
	}
	return -1;
}

static inline unsigned arch_get_counter_mask(void)
{
	cpuid_data data;
	cpuid(0xa, &data);
	return num_to_mask((data.eax >> 8) & 0xff);
}

static inline op_cpu op_cpu_specific_type(op_cpu cpu_type)
{
	if (cpu_type == CPU_ARCH_PERFMON) {
		/* Already know is Intel family 6, so just check the model. */
		int model = cpu_model(cpuid_signature());
		switch(model) {
		case 0x0f:
		case 0x16:
		case 0x17:
		case 0x1d:
			return CPU_CORE_2;
		case 0x1a:
		case 0x1e:
		case 0x1f:
		case 0x2e:
			return CPU_CORE_I7;
		case 0x1c:
			return CPU_ATOM;
		case 0x25:  /* Westmere mobile/desktop/entry level server */
		case 0x2c:  /* Westmere-EP (Intel Xeon 5600 series) */
		case 0x2f:  /* Westmere-EX */
			return CPU_WESTMERE;
		case 0x2a:
		case 0x2d:
			return CPU_SANDYBRIDGE;
		case 0x3a:
		case 0x3e:
			return CPU_IVYBRIDGE;
		case 0x3c:
		case 0x3f:
		case 0x45:
		case 0x46:
			return CPU_HASWELL;
		case 0x3d:
		case 0x47:
		case 0x4f:
			return CPU_BROADWELL;
		case 0x37:
		case 0x4d:
			return CPU_SILVERMONT;
		}
	}
	return cpu_type;
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

static inline op_cpu op_cpu_specific_type(op_cpu cpu_type)
{
	return cpu_type;
}
#endif
