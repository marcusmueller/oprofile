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
#include <string.h>
#include <sys/utsname.h>
#include <ctype.h>
#include <errno.h>
#include <fnmatch.h>

#include "op_cpu_type.h"
#include "op_hw_specific.h"

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
	{ "CPU with RTC device", "rtc", CPU_RTC, 1 },
	{ "P4 / Xeon", "i386/p4", CPU_P4, 8 },
	{ "IA64", "ia64/ia64", CPU_IA64, 4 },
	{ "Itanium", "ia64/itanium", CPU_IA64_1, 4 },
	{ "Itanium 2", "ia64/itanium2", CPU_IA64_2, 4 },
	{ "AMD64 processors", "x86-64/hammer", CPU_HAMMER, 4 },
	{ "P4 / Xeon with 2 hyper-threads", "i386/p4-ht", CPU_P4_HT2, 4 },
	{ "Alpha EV4", "alpha/ev4", CPU_AXP_EV4, 2 },
	{ "Alpha EV5", "alpha/ev5", CPU_AXP_EV5, 3 },
	{ "Alpha PCA56", "alpha/pca56", CPU_AXP_PCA56, 3 },
	{ "Alpha EV6", "alpha/ev6", CPU_AXP_EV6, 2 },
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
	{ "ppc64 Cell Broadband Engine", "ppc64/cell-be", CPU_PPC64_CELL, 8 },
	{ "AMD64 family10", "x86-64/family10", CPU_FAMILY10, 4 },
	{ "ppc64 PA6T", "ppc64/pa6t", CPU_PPC64_PA6T, 6 },
	{ "ARM 11MPCore", "arm/mpcore", CPU_ARM_MPCORE, 2 },
	{ "ARM V6 PMU", "arm/armv6", CPU_ARM_V6, 3 },
	{ "ppc64 POWER5++", "ppc64/power5++", CPU_PPC64_POWER5pp, 6 },
	{ "e300", "ppc/e300", CPU_PPC_E300, 4 },
	{ "AVR32", "avr32", CPU_AVR32, 3 },
	{ "ARM Cortex-A8", "arm/armv7", CPU_ARM_V7, 5 },
 	{ "Intel Architectural Perfmon", "i386/arch_perfmon", CPU_ARCH_PERFMON, 0},
	{ "AMD64 family11h", "x86-64/family11h", CPU_FAMILY11H, 4 },
	{ "ppc64 POWER7", "ppc64/power7", CPU_PPC64_POWER7, 6 },
	{ "ppc64 compat version 1", "ppc64/ibm-compat-v1", CPU_PPC64_IBM_COMPAT_V1, 4 },
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

static op_cpu _get_ppc64_cpu_type(void)
{
	int i;
	size_t len;
	char line[100], cpu_type_str[64], cpu_name_lowercase[64], * cpu_name;

	cpu_name = _get_cpuinfo_cpu_type(line, 100, "cpu");
	if (!cpu_name)
		return CPU_NO_GOOD;

	len = strlen(cpu_name);
	for (i = 0; i < (int)len ; i++)
		cpu_name_lowercase[i] = tolower(cpu_name[i]);

	cpu_type_str[0] = '\0';
	strcat(cpu_type_str, "ppc64/");
	strncat(cpu_type_str, cpu_name_lowercase, len);
	return op_get_cpu_number(cpu_type_str);
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

static op_cpu __get_cpu_type_alt_method(void)
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
	if (strncmp(uname_info.machine, "ppc64", 5) == 0) {
		return _get_ppc64_cpu_type();
	}
	if (strncmp(uname_info.machine, "arm", 3) == 0) {
		return _get_arm_cpu_type();
	}
	if (strncmp(uname_info.machine, "tile", 4) == 0) {
		return _get_tile_cpu_type();
	}
	if (strncmp(uname_info.machine, "mips", 4) == 0) {
		return _get_mips_cpu_type();
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
	char str[100];
	FILE * fp;

	fp = fopen("/proc/sys/dev/oprofile/cpu_type", "r");
	if (!fp) {
		/* Try 2.6's oprofilefs one instead. */
		fp = fopen("/dev/oprofile/cpu_type", "r");
		if (!fp) {
			if ((cpu_type = __get_cpu_type_alt_method()) == CPU_NO_GOOD) {
				fprintf(stderr, "Unable to open cpu_type file for reading\n");
				fprintf(stderr, "Make sure you have done opcontrol --init\n");
			}
			return cpu_type;
		}
	}

	if (!fgets(str, 99, fp)) {
		fprintf(stderr, "Could not read cpu type.\n");
		fclose(fp);
		return cpu_type;
	}

	cpu_type = op_get_cpu_number(str);

	if (op_cpu_variations(cpu_type))
		cpu_type = op_cpu_specific_type(cpu_type);

	fclose(fp);

	return cpu_type;
}


op_cpu op_get_cpu_number(char const * cpu_string)
{
	int cpu_type = CPU_NO_GOOD;
	size_t i;
	
	for (i = 0; i < nr_cpu_descrs; ++i) {
		if (!strcmp(cpu_descrs[i].name, cpu_string)) {
			cpu_type = cpu_descrs[i].cpu;
			break;
		}
	}

	/* Attempt to convert into a number */
	if (cpu_type == CPU_NO_GOOD)
		sscanf(cpu_string, "%d\n", &cpu_type);
	
	if (cpu_type <= CPU_NO_GOOD || cpu_type >= MAX_CPU_TYPE)
		cpu_type = CPU_NO_GOOD;

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
	if (cnt >= 0)
		return cnt;

	return op_cpu_has_timer_fs()
		? cpu_descrs[cpu_type].nr_counters + 1
		: cpu_descrs[cpu_type].nr_counters;
}

int op_cpu_has_timer_fs(void)
{
	static int cached_has_timer_fs_p = -1;
	FILE * fp;

	if (cached_has_timer_fs_p != -1)
		return cached_has_timer_fs_p;

	fp = fopen("/dev/oprofile/timer", "r");
	cached_has_timer_fs_p = !!fp;
	if (fp)
		fclose(fp);

	return cached_has_timer_fs_p;
}
