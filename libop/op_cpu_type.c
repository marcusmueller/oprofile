/**
 * @file op_cpu_type.c
 * CPU type determination
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author John Levon
 * @author Philippe Elie
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/utsname.h>
#include <ctype.h>
#include <errno.h>
#include <fnmatch.h>
#include <elf.h>
#include <link.h>

#include "config.h"
#include "op_cpu_type.h"
#include "op_hw_specific.h"

/* A macro to be used for ppc64 architecture-specific code.  The '__powerpc__' macro
 * is defined for both ppc64 and ppc32 architectures, so we must further qualify by
 * including the 'HAVE_LIBPFM' macro, since that macro will be defined only for ppc64.
 */
#define PPC64_ARCH (HAVE_LIBPFM) && ((defined(__powerpc__) || defined(__powerpc64__)))

struct cpu_descr {
	char const * pretty;
	char const * name;
	op_cpu cpu;
	unsigned int nr_counters;
};

static struct cpu_descr const cpu_descrs[MAX_CPU_TYPE] = {
	{ "Pentium Pro", "i386/ppro", CPU_PPRO, 2 },
	{ "PII", "i386/pii", CPU_PII, 2 },
	{ "PIII", "i386/piii", CPU_PIII, 2 },
	{ "Athlon", "i386/athlon", CPU_ATHLON, 4 },
        { "CPU with timer interrupt", "timer", CPU_TIMER_INT, 1 },
	{ "P4 / Xeon", "i386/p4", CPU_P4, 8 },
	{ "AMD64 processors", "x86-64/hammer", CPU_HAMMER, 4 },
	{ "P4 / Xeon with 2 hyper-threads", "i386/p4-ht", CPU_P4_HT2, 4 },
	{ "Alpha EV67", "alpha/ev67", CPU_AXP_EV67, 20 },
	{ "Pentium M (P6 core)", "i386/p6_mobile", CPU_P6_MOBILE, 2 },
	{ "ARM/XScale PMU1", "arm/xscale1", CPU_ARM_XSCALE1, 3 },
	{ "ARM/XScale PMU2", "arm/xscale2", CPU_ARM_XSCALE2, 5 },
	{ "ppc64 POWER4", "ppc64/power4", CPU_PPC64_POWER4, 8 },
	{ "ppc64 POWER5", "ppc64/power5", CPU_PPC64_POWER5, 6 },
	{ "ppc64 POWER5+", "ppc64/power5+", CPU_PPC64_POWER5p, 6 },
	{ "ppc64 970", "ppc64/970", CPU_PPC64_970, 8 },
	{ "MIPS 20K", "mips/20K", CPU_MIPS_20K, 1},
	{ "MIPS 24K", "mips/24K", CPU_MIPS_24K, 2},
	{ "MIPS 25K", "mips/25K", CPU_MIPS_25K, 2},
	{ "MIPS 34K", "mips/34K", CPU_MIPS_34K, 2},
	{ "MIPS 5K", "mips/5K", CPU_MIPS_5K, 2},
	{ "MIPS R10000", "mips/r10000", CPU_MIPS_R10000, 2 },
	{ "MIPS R12000", "mips/r12000", CPU_MIPS_R12000, 4 },
	{ "QED RM7000", "mips/rm7000", CPU_MIPS_RM7000, 1 },
	{ "PMC-Sierra RM9000", "mips/rm9000", CPU_MIPS_RM9000, 2 },
	{ "Sibyte SB1", "mips/sb1", CPU_MIPS_SB1, 4 },
	{ "NEC VR5432", "mips/vr5432", CPU_MIPS_VR5432, 2 },
	{ "NEC VR5500", "mips/vr5500", CPU_MIPS_VR5500, 2 },
	{ "e500", "ppc/e500", CPU_PPC_E500, 4 },
	{ "e500v2", "ppc/e500v2", CPU_PPC_E500_2, 4 },
	{ "Core Solo / Duo", "i386/core", CPU_CORE, 2 },
	{ "PowerPC G4", "ppc/7450",  CPU_PPC_7450, 6 },
	{ "Core 2", "i386/core_2", CPU_CORE_2, 2 },
	{ "ppc64 POWER6", "ppc64/power6", CPU_PPC64_POWER6, 4 },
	{ "ppc64 970MP", "ppc64/970MP", CPU_PPC64_970MP, 8 },
	{ "AMD64 family10", "x86-64/family10", CPU_FAMILY10, 4 },
	{ "ARM 11MPCore", "arm/mpcore", CPU_ARM_MPCORE, 2 },
	{ "ARM V6 PMU", "arm/armv6", CPU_ARM_V6, 3 },
	{ "ppc64 POWER5++", "ppc64/power5++", CPU_PPC64_POWER5pp, 6 },
	{ "e300", "ppc/e300", CPU_PPC_E300, 4 },
	{ "ARM Cortex-A8", "arm/armv7", CPU_ARM_V7, 5 },
 	{ "Intel Architectural Perfmon", "i386/arch_perfmon", CPU_ARCH_PERFMON, 0},
	{ "AMD64 family11h", "x86-64/family11h", CPU_FAMILY11H, 4 },
	{ "ppc64 POWER7", "ppc64/power7", CPU_PPC64_POWER7, 6 },
   	{ "Intel Core/i7", "i386/core_i7", CPU_CORE_I7, 4 },
   	{ "Intel Atom", "i386/atom", CPU_ATOM, 2 },
	{ "Loongson2", "mips/loongson2", CPU_MIPS_LOONGSON2, 2 },
	{ "Intel Nehalem microarchitecture", "i386/nehalem", CPU_NEHALEM, 4 },
	{ "ARM Cortex-A9", "arm/armv7-ca9", CPU_ARM_V7_CA9, 7 },
	{ "MIPS 74K", "mips/74K", CPU_MIPS_74K, 4},
	{ "MIPS 1004K", "mips/1004K", CPU_MIPS_1004K, 2},
	{ "AMD64 family12h", "x86-64/family12h", CPU_FAMILY12H, 4 },
	{ "AMD64 family14h", "x86-64/family14h", CPU_FAMILY14H, 4 },
	{ "AMD64 family15h", "x86-64/family15h", CPU_FAMILY15H, 6 },
	{ "Intel Westmere microarchitecture", "i386/westmere", CPU_WESTMERE, 4 },
	{ "ARMv7 Scorpion", "arm/armv7-scorpion", CPU_ARM_SCORPION, 5 },
	{ "ARMv7 ScorpionMP", "arm/armv7-scorpionmp", CPU_ARM_SCORPIONMP, 5 },
	{ "Intel Sandy Bridge microarchitecture", "i386/sandybridge", CPU_SANDYBRIDGE, 8 },
	{ "TILE64", "tile/tile64", CPU_TILE_TILE64, 2 },
	{ "TILEPro", "tile/tilepro", CPU_TILE_TILEPRO, 4 },
	{ "TILE-GX", "tile/tilegx", CPU_TILE_TILEGX, 4 },
	{ "IBM System z10", "s390/z10", CPU_S390_Z10, 1 },
	{ "IBM zEnterprise z196", "s390/z196", CPU_S390_Z196, 1 },
	{ "Intel Ivy Bridge microarchitecture", "i386/ivybridge", CPU_IVYBRIDGE, 8 },
	{ "ARM Cortex-A5", "arm/armv7-ca5", CPU_ARM_V7_CA5, 3 },
	{ "ARM Cortex-A7", "arm/armv7-ca7", CPU_ARM_V7_CA7, 5 },
	{ "ARM Cortex-A15", "arm/armv7-ca15", CPU_ARM_V7_CA15, 7 },
	{ "Intel Haswell microarchitecture", "i386/haswell", CPU_HASWELL, 4 },
	{ "IBM zEnterprise EC12", "s390/zEC12", CPU_S390_ZEC12, 1 },
	{ "AMD64 generic", "x86-64/generic", CPU_AMD64_GENERIC, 4 },
	{ "IBM Power Architected Events V1", "ppc64/architected_events_v1", CPU_PPC64_ARCH_V1, 6 },
	{ "ppc64 POWER8", "ppc64/power8", CPU_PPC64_POWER8, 6 },
	{ "e500mc", "ppc/e500mc", CPU_PPC_E500MC, 4 },
	{ "e6500", "ppc/e6500", CPU_PPC_E6500, 6 },
	{ "Intel Silvermont microarchitecture", "i386/silvermont", CPU_SILVERMONT, 2 },
	{ "ARMv7 Krait", "arm/armv7-krait", CPU_ARM_KRAIT, 5 },
	{ "APM X-Gene", "arm/armv8-xgene", CPU_ARM_V8_APM_XGENE, 6 },
	{ "Intel Broadwell microarchitecture", "i386/broadwell", CPU_BROADWELL, 4 },
	{ "ARM Cortex-A57", "arm/armv8-ca57", CPU_ARM_V8_CA57, 6},
	{ "ARM Cortex-A53", "arm/armv8-ca53", CPU_ARM_V8_CA53, 6},
};
 
static size_t const nr_cpu_descrs = sizeof(cpu_descrs) / sizeof(struct cpu_descr);

static char * _get_cpuinfo_cpu_type_line(char * buf, int len, const char * prefix, int token)
{
	char * ret = NULL;
	char * end = NULL;
	int prefix_len = strlen(prefix);
	FILE * fp = fopen("/proc/cpuinfo", "r");

	if (!fp) {
		perror("Unable to open /proc/cpuinfo\n");
		return ret;
	}

	memset(buf, 0, len);

	while (!ret) {
		if (fgets(buf, len, fp) == NULL) {
			fprintf(stderr, "Did not find processor type in /proc/cpuinfo.\n");
			break;
		}
		if (!strncmp(buf, prefix, prefix_len)) {
			ret = buf + prefix_len;
			/* Strip leading whitespace and ':' delimiter */
			while (*ret && (*ret == ':' || isspace(*ret)))
				++ret;
			buf = ret;
			/* if token param 0 then read the whole line else
			 * first token only. */
			if (token == 0) {
				/* Trim trailing whitespace */
				end = buf + strlen(buf) - 1;
				while (isspace(*end))
					--end;
				*(++end) = '\0';
				break;
			} else {
				/* Scan ahead to the end of the token */
				while (*buf && !isspace(*buf))
					++buf;
				/* Trim trailing whitespace */
				*buf = '\0';
				break;
			}
		}
	}

	fclose(fp);
	return ret;
}

static char * _get_cpuinfo_cpu_type(char * buf, int len, const char * prefix)
{
	return _get_cpuinfo_cpu_type_line(buf, len, prefix, 1);
}

#if PPC64_ARCH
// The aux vector stuff below is currently only used by ppc64 arch
static ElfW(auxv_t) * auxv_buf = NULL;

static ElfW(auxv_t) * _auxv_fetch()
{
	ElfW(auxv_t) * auxv_temp = (ElfW(auxv_t) *)auxv_buf;
	int auxv_f;
	size_t page_size = getpagesize();
	ssize_t bytes;


	if(auxv_temp == NULL) {
		auxv_f = open("/proc/self/auxv", O_RDONLY);

		if(auxv_f == -1) {
			perror("Cannot open /proc/self/auxv");
			fprintf(stderr, "Assuming native platform profiling is supported.\n");
			return NULL;
		}
		else {
			auxv_temp = (ElfW(auxv_t) *)malloc(page_size);
			if (!auxv_temp) {
				perror("Allocation of space for auxv failed.");
				close(auxv_f);
				return NULL;
			}
			bytes = read(auxv_f, (void *)auxv_temp, page_size);

			if (bytes <= 0) {
				free(auxv_temp);
				close(auxv_f);
				perror("Error /proc/self/auxv read failed");
				return NULL;
			}

			if (close(auxv_f)) {
				perror("Error close failed");
				fprintf(stderr, "Recoverable error. Continuing.\n");
			}
		}
		auxv_buf = auxv_temp;
	}
	return (ElfW(auxv_t) *)auxv_temp;
}


static const char * fetch_at_hw_platform(ElfW(Addr) type)
{
	int i = 0;
	const char * platform = NULL;
	ElfW(auxv_t) * my_auxv = NULL;

	if ((my_auxv = (ElfW(auxv_t)*) _auxv_fetch()) == NULL)
		return NULL;

	do {
		if(my_auxv[i].a_type == type) {
			platform = (const char *)my_auxv[i].a_un.a_val;
			break;
		}
		i++;
	} while (my_auxv[i].a_type != AT_NULL);

	return platform;
}

static void release_at_hw_platform(void)
{
	if (auxv_buf) {
		free(auxv_buf);
		auxv_buf = NULL;
	}
}

static op_cpu _try_ppc64_arch_generic_cpu(void)
{
	const char * platform, * base_platform;
	op_cpu cpu_type = CPU_NO_GOOD;

	platform = fetch_at_hw_platform(AT_PLATFORM);
	base_platform = fetch_at_hw_platform(AT_BASE_PLATFORM);
	if (!platform || !base_platform) {
		fprintf(stderr, "NULL returned for one or both of AT_PLATFORM/AT_BASE_PLATFORM\n");
		fprintf(stderr, "AT_PLATFORM: %s; \tAT_BASE_PLATFORM: %s\n", platform, base_platform);
		release_at_hw_platform();
		return cpu_type;
	}
	// FIXME whenever a new IBM Power processor is added -- need to ensure
	// we're returning the correct version of the architected events file.
	if (strcmp(platform, base_platform)) {
		// If platform and base_platform differ by only a "+" at the end of the name, we
		// consider these equal.
		int platforms_are_equivalent = 0;
		size_t p1_len, p2_len;
		p1_len = strlen(platform);
		p2_len = strlen(base_platform);
		if (p2_len == (p1_len + 1)) {
			if ((strncmp(platform, base_platform, p1_len) == 0) &&
					(base_platform[p2_len - 1] == '+')) {
				platforms_are_equivalent = 1;
			}
		}
		if (!platforms_are_equivalent) {
			//  FIXME
			/* For POWER8 running in POWER7 compat mode (RHEL 6.5 and SLES 11 SP4),
			 * the kernel will have enough POWER8-specific PMU code so we can utilize
			 * all of the POWER8 events. In general, this is not necessarily the case
			 * when running in compat mode.  This code needs to be inspected for every
			 * new IBM Power processor released, but for now, we'll assume that for the
			 * next processor model (assuming there will be something like a POWER9?),
			 * we should use just the architected events when running POWER8 compat mode.
			 */
			if ((strcmp(platform, "power7") == 0) && (strcmp(base_platform, "power8") == 0))
				cpu_type = CPU_PPC64_POWER8;
			else
				cpu_type = CPU_PPC64_ARCH_V1;
		}
	}
	release_at_hw_platform();
	return cpu_type;
}

static op_cpu _get_ppc64_cpu_type(void)
{
	int i;
	size_t len;
	char line[100], cpu_type_str[64], cpu_name_lowercase[64], * cpu_name;
	op_cpu cpu_type = CPU_NO_GOOD;

	cpu_type = _try_ppc64_arch_generic_cpu();
	if (cpu_type != CPU_NO_GOOD)
		return cpu_type;

	cpu_name = _get_cpuinfo_cpu_type(line, 100, "cpu");
	if (!cpu_name)
		return CPU_NO_GOOD;

	len = strlen(cpu_name);
	for (i = 0; i < (int)len ; i++)
		cpu_name_lowercase[i] = tolower(cpu_name[i]);

	if (strncmp(cpu_name_lowercase, "power7+", 7) == 0)
		cpu_name_lowercase[6] = '\0';
	if (strncmp(cpu_name_lowercase, "power8e", 7) == 0)
		cpu_name_lowercase[6] = '\0';

	cpu_type_str[0] = '\0';
	strcat(cpu_type_str, "ppc64/");
	strncat(cpu_type_str, cpu_name_lowercase, len);
	cpu_type = op_get_cpu_number(cpu_type_str);
	return cpu_type;
}
#else
static op_cpu _get_ppc64_cpu_type(void)
{
	return CPU_NO_GOOD;
}
#endif


static char *alpha_cpu_models[] = {
	"EV67", "EV68CB", "EV68AL", "EV68CX", "EV7", "EV79", "EV69", NULL
};


static op_cpu _get_alpha_cpu_type(void)
{
	char *cpu_model;
	char **p;
	char line[100];

	cpu_model = _get_cpuinfo_cpu_type(line, 100, "cpu model");
	if (!cpu_model)
		return CPU_NO_GOOD;

	for (p = alpha_cpu_models; *p; p++) {
		if (strcmp(cpu_model, *p) == 0)
			return CPU_AXP_EV67;
	}

	return CPU_NO_GOOD;
}


static op_cpu _get_arm_cpu_type(void)
{
	unsigned long cpuid, vendorid;
	char line[100];
	char * cpu_part, * cpu_implementer;

	cpu_implementer = _get_cpuinfo_cpu_type(line, 100, "CPU implementer");
	if (!cpu_implementer)
		return CPU_NO_GOOD;

	errno = 0;
	vendorid = strtoul(cpu_implementer, NULL, 16);
	if (errno) {
		fprintf(stderr, "Unable to parse CPU implementer %s\n", cpu_implementer);
		return CPU_NO_GOOD;
	}

	cpu_part = _get_cpuinfo_cpu_type(line, 100, "CPU part");
	if (!cpu_part)
		return CPU_NO_GOOD;

	errno = 0;
	cpuid = strtoul(cpu_part, NULL, 16);
	if (errno) {
		fprintf(stderr, "Unable to parse CPU part %s\n", cpu_part);
		return CPU_NO_GOOD;
	}

	if (vendorid == 0x41) {		/* ARM Ltd. */
		switch (cpuid) {
		case 0xb36:
		case 0xb56:
		case 0xb76:
			return op_get_cpu_number("arm/armv6");
		case 0xb02:
			return op_get_cpu_number("arm/mpcore");
		case 0xc05:
			return op_get_cpu_number("arm/armv7-ca5");
		case 0xc07:
			return op_get_cpu_number("arm/armv7-ca7");
		case 0xc08:
			return op_get_cpu_number("arm/armv7");
		case 0xc09:
			return op_get_cpu_number("arm/armv7-ca9");
		case 0xc0f:
			return op_get_cpu_number("arm/armv7-ca15");
		case 0xd07:
			return op_get_cpu_number("arm/armv8-ca57");
		case 0xd03:
			return op_get_cpu_number("arm/armv8-ca53");
		}
	} else if (vendorid == 0x42) {  /* Broadcom Corporation */
		switch (cpuid) {
		case 0x00f:
			return op_get_cpu_number("arm/armv7-ca15");
		}
	} else if (vendorid == 0x50) {	/* Applied Micro Circuits Corporation */
		switch (cpuid) {
		case 0x000:
			return op_get_cpu_number("arm/armv8-xgene");
		}
	} else if (vendorid == 0x69) {	/* Intel xscale */
		switch (cpuid >> 9) {
		case 1:
			return op_get_cpu_number("arm/xscale1");
		case 2:
			return op_get_cpu_number("arm/xscale2");
		}
	}

	return CPU_NO_GOOD;
}

static op_cpu _get_tile_cpu_type(void)
{
	int i;
	size_t len;
	char line[100], cpu_type_str[64], cpu_name_lowercase[64], * cpu_name;

	cpu_name = _get_cpuinfo_cpu_type(line, 100, "model name");
	if (!cpu_name)
		return CPU_NO_GOOD;

	len = strlen(cpu_name);
	for (i = 0; i < (int)len ; i++)
		cpu_name_lowercase[i] = tolower(cpu_name[i]);

	cpu_type_str[0] = '\0';
	strcat(cpu_type_str, "tile/");
	strncat(cpu_type_str, cpu_name_lowercase, len);
	return op_get_cpu_number(cpu_type_str);
}

#if defined(__x86_64__) || defined(__i386__)
int op_is_cpu_vendor(char * vendor)
{
	return cpuid_vendor(vendor);
}

static unsigned cpuid_eax(unsigned func)
{
	cpuid_data d;

	cpuid(func, &d);
	return d.eax;
}

static inline int perfmon_available(void)
{
        unsigned eax;
        if (cpuid_eax(0) < 10)
                return 0;
        eax = cpuid_eax(10);
        if ((eax & 0xff) == 0)
                return 0;
        return (eax >> 8) & 0xff;
}

static int cpu_info_number(char *name, unsigned long *number)
{
        char buf[100];
        char *end;

        if (!_get_cpuinfo_cpu_type(buf, sizeof buf, name))
                return 0;
        *number = strtoul(buf, &end, 0);
        return end > buf;
}

static op_cpu _get_intel_cpu_type(void)
{
	unsigned eax, family, model;

	if (perfmon_available())
		return op_cpu_specific_type(CPU_ARCH_PERFMON);

	/* Handle old non arch perfmon CPUs */
	eax = cpuid_signature();
	family = cpu_family(eax);
	model = cpu_model(eax);

	if (family == 6) {
		/* Reproduce kernel p6_init logic. Only for non arch perfmon cpus */
		switch (model) {
		case 0 ... 2:
			return op_get_cpu_number("i386/ppro");
		case 3 ... 5:
			return op_get_cpu_number("i386/pii");
		case 6 ... 8:
		case 10 ... 11:
			return op_get_cpu_number("i386/piii");
		case 9:
		case 13:
			return op_get_cpu_number("i386/p6_mobile");
		}
	} else if (family == 15) {
		unsigned long siblings;

		/* Reproduce kernel p4_init() logic */
		if (model > 6 || model == 5)
			return CPU_NO_GOOD;
		if (!cpu_info_number("siblings", &siblings) ||
		    siblings == 1)
			return op_get_cpu_number("i386/p4");
		if (siblings == 2)
			return op_get_cpu_number("i386/p4-ht");
	}
	return CPU_NO_GOOD;			
}

static op_cpu _get_amd_cpu_type(void)
{
	unsigned eax, family;
	op_cpu ret = CPU_NO_GOOD;
	char buf[20] = {'\0'};

	eax = cpuid_signature();
	family = cpu_family(eax);

	/* These family does not exist in the past.*/
	if (family < 0x0f || family == 0x13)
		return ret;

	switch (family) {
	case 0x0f:
		ret = op_get_cpu_number("x86-64/hammer");
		break;
	case 0x10:
		ret = op_get_cpu_number("x86-64/family10");
		break;
	case 0x11:
	case 0x12:
	case 0x14:
	case 0x15:
		/* From family11h and forward, we use the same naming scheme */
		snprintf(buf, 20, "x86-64/family%xh", family);
		ret = op_get_cpu_number(buf);
		break;
	default:
		/* Future processors */
		snprintf(buf, 20, "x86-64/generic");
		ret = op_get_cpu_number(buf);
		break;
	}

	return ret;			
}

static op_cpu _get_x86_64_cpu_type(void)
{
	op_cpu ret = CPU_NO_GOOD;

	if (cpuid_vendor("GenuineIntel")) {
		ret = _get_intel_cpu_type();
	} else if (cpuid_vendor("AuthenticAMD")) {
		ret = _get_amd_cpu_type();
	}

	return ret;
}

#else
static op_cpu _get_x86_64_cpu_type(void)
{
	return CPU_NO_GOOD;
}
#endif

struct mips_cpu_descr
{
	const char * key;
	const char * value;
};

static struct mips_cpu_descr mips_cpu_descrs[] = {
	{ .key = "MIPS 5Kc", .value = "mips/5K" },		/* CPU_5KC */
	{ .key = "MIPS 20Kc", .value = "mips/20K" },		/* CPU_20KC */
	{ .key = "MIPS 24Kc", .value = "mips/24K" },		/* CPU_24K */
	{ .key = "MIPS 25Kc", .value = "mips/25K" },		/* CPU_25KF */
	{ .key = "MIPS 34Kc", .value = "mips/34K" },		/* CPU_34K */
	{ .key = "MIPS 74Kc", .value = "mips/74K" },		/* CPU_74K */
	{ .key = "MIPS M14Kc", .value = "mips/M14Kc" },		/* CPU_M14KC */
	{ .key = "RM9000", .value = "mips/rm9000" },		/* CPU_RM9000 */
	{ .key = "R10000", .value = "mips/r10000" }, 		/* CPU_R10000 */
	{ .key = "R12000", .value = "mips/r12000" },		/* CPU_R12000 */
	{ .key = "R14000", .value = "mips/r12000" },		/* CPU_R14000 */
	{ .key = "ICT Loongson-2", .value = "mips/loongson2" },	/* CPU_LOONGSON2 */
	{ .key = NULL, .value = NULL }
};

static const char * _get_mips_op_name(const char * key)
{
	struct mips_cpu_descr * p_it = mips_cpu_descrs;
	size_t len;


	while (p_it->key != NULL) {
		len = strlen(p_it->key);
		if (0 == strncmp(key, p_it->key, len))
			return p_it->value;
		++p_it;
	}
	return NULL;
}

static op_cpu _get_mips_cpu_type(void)
{
	char line[100];
	char * cpu_model;
	const char * op_name = NULL;

	cpu_model = _get_cpuinfo_cpu_type_line(line, 100, "cpu model", 0);
	if (!cpu_model)
		return CPU_NO_GOOD;

	op_name = _get_mips_op_name(cpu_model);

	if (op_name)
		return op_get_cpu_number(op_name);
	return CPU_NO_GOOD;
}

static op_cpu _get_s390_cpu_type(void)
{
	char line[100];
	char *ptr;
	const char prefix[] = "machine = ";
	unsigned model;

	ptr = _get_cpuinfo_cpu_type_line(line, sizeof(line), "processor", 0);
	if (!ptr)
		return CPU_NO_GOOD;

	ptr = strstr(ptr, prefix);
	if (!ptr)
		return CPU_NO_GOOD;

	ptr += sizeof(prefix) - 1;
	if (sscanf(ptr, "%u", &model) != 1)
		return CPU_NO_GOOD;

	switch (model) {
	case 2097:
	case 2098:
		return CPU_S390_Z10;
	case 2817:
	case 2818:
		return CPU_S390_Z196;
	case 2827:
	case 2828:
		return CPU_S390_ZEC12;
	}
	return CPU_NO_GOOD;
}

static op_cpu __get_cpu_type(void)
{
	struct utsname uname_info;
	if (uname(&uname_info) < 0) {
		perror("uname failed");
		return CPU_NO_GOOD;
	}
	if (strncmp(uname_info.machine, "x86_64", 6) == 0 || 
	    fnmatch("i?86", uname_info.machine, 0) == 0) {
		return _get_x86_64_cpu_type();
	}
	if ((strncmp(uname_info.machine, "ppc64", 5) == 0) ||
			(strncmp(uname_info.machine, "ppc64le", 7) == 0)) {
		return _get_ppc64_cpu_type();
	}
	if (strncmp(uname_info.machine, "alpha", 5) == 0) {
		return _get_alpha_cpu_type();
	}
	if (strncmp(uname_info.machine, "arm", 3) == 0 ||
	    strncmp(uname_info.machine, "aarch64", 7) == 0) {
		return _get_arm_cpu_type();
	}
	if (strncmp(uname_info.machine, "tile", 4) == 0) {
		return _get_tile_cpu_type();
	}
	if (strncmp(uname_info.machine, "mips", 4) == 0) {
		return _get_mips_cpu_type();
	}
	if (strncmp(uname_info.machine, "s390", 4) == 0) {
		return _get_s390_cpu_type();
	}
	return CPU_NO_GOOD;
}

int op_cpu_variations(op_cpu cpu_type)
{
	switch (cpu_type) {
	case  CPU_ARCH_PERFMON:
		return 1;
	default:
		return 0;
	}
}


op_cpu op_cpu_base_type(op_cpu cpu_type)
{
	/* All the processors that support CPU_ARCH_PERFMON */
	switch (cpu_type) {
	case CPU_CORE_2:
	case CPU_CORE_I7:
	case CPU_ATOM:
	case CPU_NEHALEM:
	case CPU_HASWELL:
	case CPU_BROADWELL:
	case CPU_SILVERMONT:
	case CPU_WESTMERE:
	case CPU_SANDYBRIDGE:
	case CPU_IVYBRIDGE:
		return CPU_ARCH_PERFMON;
	default:
		/* assume processor in a class by itself */
		return cpu_type;
	}
}

op_cpu op_get_cpu_type(void)
{
	int cpu_type = CPU_NO_GOOD;

	if ((cpu_type = __get_cpu_type()) == CPU_NO_GOOD) {
		fprintf(stderr, "Unable to obtain cpu_type\n");
		fprintf(stderr, "Verify that a pre-1.0 version of OProfile is not in use.\n"
		        "If the /dev/oprofile/cpu_type file exists, locate the pre-1.0 OProfile\n"
		        "installation, and use its 'opcontrol' command, passing the --deinit option.\n");
	}
	return cpu_type;
}


op_cpu op_get_cpu_number(char const * cpu_string)
{
	int cpu_type = CPU_NO_GOOD;
	int scan_matches = 0;
	size_t i;
	
	for (i = 0; i < nr_cpu_descrs; ++i) {
		if (!strcmp(cpu_descrs[i].name, cpu_string)) {
			cpu_type = cpu_descrs[i].cpu;
			break;
		}
	}

	/* Attempt to convert into a number */
	if (cpu_type == CPU_NO_GOOD) {
		scan_matches = sscanf(cpu_string, "%d\n", &cpu_type);
		if (scan_matches && (cpu_type <= CPU_NO_GOOD || cpu_type >= MAX_CPU_TYPE))
			cpu_type = CPU_NO_GOOD;
	}
	return cpu_type;
}


char const * op_get_cpu_type_str(op_cpu cpu_type)
{
	if (cpu_type <= CPU_NO_GOOD || cpu_type >= MAX_CPU_TYPE)
		return "invalid cpu type";

	return cpu_descrs[cpu_type].pretty;
}


char const * op_get_cpu_name(op_cpu cpu_type)
{
	if (cpu_type <= CPU_NO_GOOD || cpu_type >= MAX_CPU_TYPE)
		return "invalid cpu type";

	return cpu_descrs[cpu_type].name;
}


int op_get_nr_counters(op_cpu cpu_type)
{
	int cnt;

	if (cpu_type <= CPU_NO_GOOD || cpu_type >= MAX_CPU_TYPE)
		return 0;

	cnt = arch_num_counters(cpu_type);
	if (cnt < 0)
		cnt = cpu_descrs[cpu_type].nr_counters;
	return cnt;
}

