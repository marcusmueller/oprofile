/* $Id: op_events.c,v 1.33 2001/10/22 16:14:16 davej Exp $ */
/* COPYRIGHT (C) 2000 THE VICTORIA UNIVERSITY OF MANCHESTER and John Levon
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59 Temple
 * Place - Suite 330, Boston, MA 02111-1307, USA.
 */

/* Adapted from libpperf 0.5 by M. Patrick Goda and Michael S. Warren */

/* See IA32 Vol. 3 Appendix A */

#ifdef __KERNEL__
#include <linux/string.h>
#define strcmp(a,b) strnicmp((a),(b),strlen((b)))
#else
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#endif

#include "op_user.h"

static const char* cpu_type_str[MAX_CPU_TYPE] = {
	"Pentium Pro",
	"PII",
	"PIII",
	"Athlon"
};

struct op_unit_mask op_unit_masks[] = {
	/* reserved empty entry */
	{ 0, utm_mandatory, 0x00, { 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 }, },
	/* MESI counters */
	{ 5, utm_bitmask, 0x0f, { 0x8, 0x4, 0x2, 0x1, 0xf, 0x0, 0x0 }, },
	/* EBL self/any default to any transitions */
	{ 2, utm_exclusive, 0x20, { 0x0, 0x20, 0x0, 0x0, 0x0, 0x0, 0x0 }, },
	/* MMX PII events */
	{ 1, utm_mandatory, 0xf, { 0xf, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 }, },
	{ 7, utm_bitmask, 0x3f, { 0x1, 0x2, 0x4, 0x8, 0x10, 0x20, 0x3f }, },
	{ 2, utm_exclusive, 0x0, { 0x0, 0x1, 0x0, 0x0, 0x0, 0x0, 0x0 }, },
	{ 5, utm_bitmask, 0x0f, { 0x1, 0x2, 0x4, 0x8, 0xf, 0x0, 0x0 }, },
	/* KNI PIII events */
	{ 4, utm_exclusive, 0x0, { 0x0, 0x1, 0x2, 0x3, 0x0, 0x0, 0x0 }, },
	{ 2, utm_bitmask, 0x1, { 0x0, 0x1, 0x0, 0x0, 0x0, 0x0, 0x0 }, },
	/* Athlon MOESI cache events */
	{ 6, utm_bitmask, 0x1f, { 0x10, 0x8, 0x4, 0x2, 0x1, 0x1f }, }
};

/* the following are just short cut for filling the table of event */
#define OP_ATHLON	(1 << CPU_ATHLON)
#define OP_PPRO		(1 << CPU_PPRO)
#define OP_PII		(1 << CPU_PII)
#define OP_PIII		(1 << CPU_PIII)
#define OP_PII_PIII	(OP_PII | OP_PIII)
#define OP_IA_ALL	(OP_PII_PIII | OP_PPRO)

#define CTR_ALL		(~0u)
#define CTR_0		(1 << 0)
#define CTR_1		(1 << 1)

/* ctr allowed, Event #, unit mask, name, minimum event value */
struct op_event op_events[] = {
  /* Clocks */
  { CTR_ALL, OP_IA_ALL, 0x79, 0, "CPU_CLK_UNHALTED", 6000 },
  /* Data Cache Unit (DCU) */
  { CTR_ALL, OP_IA_ALL, 0x43, 0, "DATA_MEM_REFS", 500 },
  { CTR_ALL, OP_IA_ALL, 0x45, 0, "DCU_LINES_IN", 500 },
  { CTR_ALL, OP_IA_ALL, 0x46, 0, "DCU_M_LINES_IN", 500 },
  { CTR_ALL, OP_IA_ALL, 0x47, 0, "DCU_M_LINES_OUT", 500},
  { CTR_ALL, OP_IA_ALL, 0x48, 0, "DCU_MISS_OUTSTANDING", 500 },
  /* Intruction Fetch Unit (IFU) */
  { CTR_ALL, OP_IA_ALL, 0x80, 0, "IFU_IFETCH", 500 },
  { CTR_ALL, OP_IA_ALL, 0x81, 0, "IFU_IFETCH_MISS", 500 },
  { CTR_ALL, OP_IA_ALL, 0x85, 0, "ITLB_MISS", 500},
  { CTR_ALL, OP_IA_ALL, 0x86, 0, "IFU_MEM_STALL", 500 },
  { CTR_ALL, OP_IA_ALL, 0x87, 0, "ILD_STALL", 500 },
  /* L2 Cache */
  { CTR_ALL, OP_IA_ALL, 0x28, 1, "L2_IFETCH", 500 },
  { CTR_ALL, OP_IA_ALL, 0x29, 1, "L2_LD", 500 },
  { CTR_ALL, OP_IA_ALL, 0x2a, 1, "L2_ST", 500 },
  { CTR_ALL, OP_IA_ALL, 0x24, 0, "L2_LINES_IN", 500 },
  { CTR_ALL, OP_IA_ALL, 0x26, 0, "L2_LINES_OUT", 500 },
  { CTR_ALL, OP_IA_ALL, 0x25, 0, "L2_M_LINES_INM", 500 },
  { CTR_ALL, OP_IA_ALL, 0x27, 0, "L2_M_LINES_OUTM", 500 },
  { CTR_ALL, OP_IA_ALL, 0x2e, 1, "L2_RQSTS", 500 },
  { CTR_ALL, OP_IA_ALL, 0x21, 0, "L2_ADS", 500 },
  { CTR_ALL, OP_IA_ALL, 0x22, 0, "L2_DBUS_BUSY", 500 },
  { CTR_ALL, OP_IA_ALL, 0x23, 0, "L2_DMUS_BUSY_RD", 500 },
  /* External Bus Logic (EBL) */
  { CTR_ALL, OP_IA_ALL, 0x62, 2, "BUS_DRDY_CLOCKS", 500 },
  { CTR_ALL, OP_IA_ALL, 0x63, 2, "BUS_LOCK_CLOCKS", 500 },
  { CTR_ALL, OP_IA_ALL, 0x60, 0, "BUS_REQ_OUTSTANDING", 500 },
  { CTR_ALL, OP_IA_ALL, 0x65, 2, "BUS_TRAN_BRD", 500 },
  { CTR_ALL, OP_IA_ALL, 0x66, 2, "BUS_TRAN_RFO", 500 },
  { CTR_ALL, OP_IA_ALL, 0x67, 2, "BUS_TRANS_WB", 500 },
  { CTR_ALL, OP_IA_ALL, 0x68, 2, "BUS_TRAN_IFETCH", 500 },
  { CTR_ALL, OP_IA_ALL, 0x69, 2, "BUS_TRAN_INVAL", 500 },
  { CTR_ALL, OP_IA_ALL, 0x6a, 2, "BUS_TRAN_PWR", 500 },
  { CTR_ALL, OP_IA_ALL, 0x6b, 2, "BUS_TRANS_P", 500 },
  { CTR_ALL, OP_IA_ALL, 0x6c, 2, "BUS_TRANS_IO", 500 },
  { CTR_ALL, OP_IA_ALL, 0x6d, 2, "BUS_TRANS_DEF", 500 },
  { CTR_ALL, OP_IA_ALL, 0x6e, 2, "BUS_TRAN_BURST", 500 },
  { CTR_ALL, OP_IA_ALL, 0x70, 2, "BUS_TRAN_ANY", 500 },
  { CTR_ALL, OP_IA_ALL, 0x6f, 2, "BUS_TRAN_MEM", 500 },
  { CTR_ALL, OP_IA_ALL, 0x64, 0, "BUS_DATA_RCV", 500 },
  { CTR_ALL, OP_IA_ALL, 0x61, 0, "BUS_BNR_DRV", 500 },
  { CTR_ALL, OP_IA_ALL, 0x7a, 0, "BUS_HIT_DRV", 500 },
  { CTR_ALL, OP_IA_ALL, 0x7b, 0, "BUS_HITM_DRV", 500 },
  { CTR_ALL, OP_IA_ALL, 0x7e, 0, "BUS_SNOOP_STALL", 500 },
  /* Floating Point Unit (FPU) */
  { CTR_0, OP_IA_ALL, 0xc1, 0, "COMP_FLOP_RET", 3000 },
  { CTR_0, OP_IA_ALL, 0x10, 0, "FLOPS", 3000 },
  { CTR_1, OP_IA_ALL, 0x11, 0, "FP_ASSIST", 500 },
  { CTR_1, OP_IA_ALL, 0x12, 0, "MUL", 1000 },
  { CTR_1, OP_IA_ALL, 0x13, 0, "DIV", 500 },
  { CTR_0, OP_IA_ALL, 0x14, 0, "CYCLES_DIV_BUSY", 1000 },
  /* Memory Ordering */
  { CTR_ALL, OP_IA_ALL, 0x03, 0, "LD_BLOCKS", 500 },
  { CTR_ALL, OP_IA_ALL, 0x04, 0, "SB_DRAINS", 500 },
  { CTR_ALL, OP_IA_ALL, 0x05, 0, "MISALIGN_MEM_REF", 500 },
  /* PIII KNI */
  { CTR_ALL, OP_PIII, 0x07, 7, "EMON_KNI_PREF_DISPATCHED", 500 },
  { CTR_ALL, OP_PIII, 0x4b, 7, "EMON_KNI_PREF_MISS", 500 },
  /* Instruction Decoding and Retirement */
  { CTR_ALL, OP_IA_ALL, 0xc0, 0, "INST_RETIRED", 6000 },
  { CTR_ALL, OP_IA_ALL, 0xc2, 0, "UOPS_RETIRED", 6000 },
  { CTR_ALL, OP_IA_ALL, 0xd0, 0, "INST_DECODED", 6000 },
  /* PIII KNI */
  { CTR_ALL, OP_PIII, 0xd8, 8, "EMON_KNI_INST_RETIRED", 3000 },
  { CTR_ALL, OP_PIII, 0xd9, 8, "EMON_KNI_COMP_INST_RET", 3000 },
  /* Interrupts */
  { CTR_ALL, OP_IA_ALL, 0xc8, 0, "HW_INT_RX", 500 },
  { CTR_ALL, OP_IA_ALL, 0xc6, 0, "CYCLES_INT_MASKED", 500 },
  { CTR_ALL, OP_IA_ALL, 0xc7, 0, "CYCLES_INT_PENDING_AND_MASKED", 500 },
  /* Branches */
  { CTR_ALL, OP_IA_ALL, 0xc4, 0, "BR_INST_RETIRED", 500 },
  { CTR_ALL, OP_IA_ALL, 0xc5, 0, "BR_MISS_PRED_RETIRED", 500 },
  { CTR_ALL, OP_IA_ALL, 0xc9, 0, "BR_TAKEN_RETIRED", 500 },
  { CTR_ALL, OP_IA_ALL, 0xca, 0, "BR_MISS_PRED_TAKEN_RET", 500 },
  { CTR_ALL, OP_IA_ALL, 0xe0, 0, "BR_INST_DECODED", 500 },
  { CTR_ALL, OP_IA_ALL, 0xe2, 0, "BTB_MISSES", 500 },
  { CTR_ALL, OP_IA_ALL, 0xe4, 0, "BR_BOGUS", 500 },
  { CTR_ALL, OP_IA_ALL, 0xe6, 0, "BACLEARS", 500 },
  /* Stalls */
  { CTR_ALL, OP_IA_ALL, 0xa2, 0, "RESOURCE_STALLS", 500 },
  { CTR_ALL, OP_IA_ALL, 0xd2, 0, "PARTIAL_RAT_STALLS", 500 },
  /* Segment Register Loads */
  { CTR_ALL, OP_IA_ALL, 0x06, 0, "SEGMENT_REG_LOADS", 500 },
  /* MMX (Pentium II only) */
  { CTR_ALL, OP_PII, 0xb0, 0, "MMX_INSTR_EXEC", 3000 },
  { CTR_ALL, OP_PII_PIII, 0xb1, 0, "MMX_SAT_INSTR_EXEC", 3000 },
  { CTR_ALL, OP_PII_PIII, 0xb2, 3, "MMX_UOPS_EXEC", 3000 },
  { CTR_ALL, OP_PII_PIII, 0xb3, 4, "MMX_INSTR_TYPE_EXEC", 3000 },
  { CTR_ALL, OP_PII_PIII, 0xcc, 5, "FP_MMX_TRANS", 3000 },
  { CTR_ALL, OP_PII_PIII, 0xcd, 0, "MMX_ASSIST", 500 },
  { CTR_ALL, OP_PII_PIII, 0xce, 0, "MMX_INSTR_RET", 3000 },
  /* segment renaming (Pentium II only) */
  { CTR_ALL, OP_PII, 0xd4, 6, "SEG_RENAME_STALLS", 500 },
  { CTR_ALL, OP_PII, 0xd5, 6, "SEG_REG_RENAMES", 500 },
  { CTR_ALL, OP_PII, 0xd6, 0, "RET_SEG_RENAMES", 500 },

  /* athlon events */
  { CTR_ALL, OP_ATHLON, 0xc0, 0, "RETIRED_INSNS", 3000,},
  { CTR_ALL, OP_ATHLON, 0xc1, 0, "RETIRED_OPS", 500,},
  { CTR_ALL, OP_ATHLON, 0x80, 0, "ICACHE_FETCHES", 500,},
  { CTR_ALL, OP_ATHLON, 0x81, 0, "ICACHE_MISSES", 500,},
  { CTR_ALL, OP_ATHLON, 0x40, 0, "DATA_CACHE_ACCESSES", 500,},
  { CTR_ALL, OP_ATHLON, 0x41, 0, "DATA_CACHE_MISSES", 500,},
  { CTR_ALL, OP_ATHLON, 0x42, 9, "DATA_CACHE_REFILLS_FROM_L2", 500,},
  { CTR_ALL, OP_ATHLON, 0x43, 9, "DATA_CACHE_REFILLS_FROM_SYSTEM", 500,},
  { CTR_ALL, OP_ATHLON, 0x44, 9, "DATA_CACHE_WRITEBACKS", 500,},
  { CTR_ALL, OP_ATHLON, 0xc2, 0, "RETIRED_BRANCHES", 500,},
  { CTR_ALL, OP_ATHLON, 0xc3, 0, "RETIRED_BRANCHES_MISPREDICTED", 500,},
  { CTR_ALL, OP_ATHLON, 0xc4, 0, "RETIRED_TAKEN_BRANCHES", 500,},
  { CTR_ALL, OP_ATHLON, 0xc5, 0, "RETIRED_TAKEN_BRANCHES_MISPREDICTED", 500,},
  { CTR_ALL, OP_ATHLON, 0x45, 0, "L1_DTLB_MISSES_L2_DTLD_HITS", 500,},
  { CTR_ALL, OP_ATHLON, 0x46, 0, "L1_AND_L2_DTLB_MISSES", 500,},
  { CTR_ALL, OP_ATHLON, 0x47, 0, "MISALIGNED_DATA_REFS", 500,},
  { CTR_ALL, OP_ATHLON, 0x84, 0, "L1_ITLB_MISSES_L2_ITLB_HITS", 500,},
  { CTR_ALL, OP_ATHLON, 0x85, 0, "L1_AND_L2_ITLB_MISSES", 500,},
  { CTR_ALL, OP_ATHLON, 0xc6, 0, "RETIRED_FAR_CONTROL_TRANSFERS", 500,},
  { CTR_ALL, OP_ATHLON, 0xc7, 0, "RETIRED_RESYNC_BRANCHES", 500,},
  { CTR_ALL, OP_ATHLON, 0xcd, 0, "INTERRUPTS_MASKED", 500,},
  { CTR_ALL, OP_ATHLON, 0xce, 0, "INTERRUPTS_MASKED_PENDING", 500,},
  { CTR_ALL, OP_ATHLON, 0xcf, 0, "HARDWARE_INTERRUPTS", 10,},
  { CTR_ALL, OP_ATHLON, 0xd3, 0, "SERIALISE", 10,},
};

/* the total number of events for all processor type */
uint op_nr_events = (sizeof(op_events)/sizeof(op_events[0]));

/**
 * op_check_unit_mask - sanity check unit mask value
 * @allow: allowed unit mask array
 * @um: unit mask value to check
 *
 * Verify that a unit mask value is within the allowed array.
 *
 * The function returns:
 * -1  if the value is not allowed,
 * 0   if the value is allowed and represent multiple units,
 * > 0 otherwise, in this case allow->um[return value - 1] == um so the
 * caller can access to the description of the unit_mask.
 */
static int op_check_unit_mask(struct op_unit_mask *allow, u8 um)
{
	uint i, mask;

	switch (allow->unit_type_mask) {
		case utm_exclusive:
		case utm_mandatory:
			for (i=0; i < allow->num; i++) {
				if (allow->um[i] == um)
					return i + 1;
			}
			break;

		case utm_bitmask:
			/* Must reject 0 bit mask because it can count nothing */
			if (um == 0)
				break;
 
			mask = 0;
			for (i=0; i < allow->num; i++) {
				if (allow->um[i] == um)
					/* it is an exact match so return the index + 1 */
					return i + 1;

				mask |= allow->um[i];
			}

			if ((mask & um) == um)
				return 0;
			break;
	}

	return -1;
}

/**
 * op_min_count - get the minimum count value.
 * @ctr_type: event value
 * @cpu_type: cpu type
 *
 * 0 Pentium Pro
 *
 * 1 Pentium II
 *
 * 2 Pentium III
 *
 * 3 AMD Athlon
 *
 * The function returns > 0 if the event is found
 * 0 otherwise
 */
int op_min_count(u8 ctr_type, int cpu_type)
{
	int ret = 0;
	uint i;
	int cpu_mask = 1 << cpu_type;

	for (i = 0; i < op_nr_events; i++) {
		if (op_events[i].val == ctr_type && (op_events[i].cpu_mask & cpu_mask)) {
			ret = op_events[i].min_count;
			break;
		}
	}

	return ret;
}

/**
 * op_check_events - sanity check event values
 * @ctr: counter number
 * @ctr_type: event value for counter 0
 * @ctr_um: unit mask for counter 0
 * @cpu_type: processor type
 *
 * Check that the counter event and unit mask values
 * are allowed. @cpu_type should be set as follows :
 *
 * 0 Pentium Pro
 *
 * 1 Pentium II
 *
 * 2 Pentium III
 *
 * 3 AMD Athlon
 *
 * Don't fail if ctr_type == 0.
 *
 * The function returns bitmask of failure cause
 * 0 otherwise
 */
int op_check_events(int ctr, u8 ctr_type, u8 ctr_um, int cpu_type)
{
	int ret = 0x0;
	uint i = 0;
	uint cpu_mask = 1 << cpu_type;
	uint ctr_mask = 1 << ctr;

	if (ctr_type != 0) {
		for ( ; i < op_nr_events; i++) {
			if (op_events[i].val == ctr_type && (op_events[i].cpu_mask & cpu_mask)) {
				if ((op_events[i].counter_mask & ctr_mask) == 0)
					ret |= OP_EVT_CTR_NOT_ALLOWED;

				if (op_events[i].unit && 
				    op_check_unit_mask(&op_unit_masks[op_events[i].unit], ctr_um) < 0)
					ret |= OP_EVT_NO_UM;
				break;
			}
		}
	}

	if (i == op_nr_events)
		ret |= OP_EVT_NOT_FOUND;

	return ret;
}

/**
 * op_check_events_str - sanity check event strings and unit masks
 * @ctr: ctr number
 * @ctr_type: event name for counter
 * @ctr_um: unit mask for counter
 * @cpu_type: processor type
 * @ctr_t: event value for counter
 *
 * Check that the counter event and unit mask values
 * are allowed. @cpu_type should be set as follows :
 *
 * 0 Pentium Pro
 *
 * 1 Pentium II
 *
 * 2 Pentium III
 *
 * 3 AMD Athlon
 *
 * Use "" strings for @ctr_type if the counter is not used.
 *
 * On successful return, @ctr_t will contain
 * the event values for counters ctr
 *
 * The function returns 1 if the values are allowed,
 * 0 otherwise
 */
int op_check_events_str(int ctr, char *ctr_type, u8 ctr_um, int cpu_type, u8 *ctr_t)
{
	uint i;
	int ctr_found = 0;
	uint cpu_mask = 1 << cpu_type;

	if (!ctr_type)
		ctr_type="";

	if (strcmp(ctr_type,"")) {
		for (i=0; i < op_nr_events; i++) {
			if (!strcmp(ctr_type, op_events[i].name) && 
			    (op_events[i].cpu_mask & cpu_mask)) {
				*ctr_t = op_events[i].val;
				ctr_found = 1;
				break;
			}
		}
	}

	if (strcmp(ctr_type,"") && !ctr_found)
		return OP_EVT_NOT_FOUND;

	return op_check_events(ctr, *ctr_t, ctr_um, cpu_type);
}

/**
 * op_get_cpu_type_str - get the cpu string.
 * @cpu_type: the cpu type identifier
 *
 * The function always return a valid const char*
 * the core cpu denomination or "invalid cpu type" if
 * @cpu_type is not valid.
 */
const char* op_get_cpu_type_str(int cpu_type)
{
	if (cpu_type < 0 || cpu_type > MAX_CPU_TYPE) {
		return "invalid cpu type";
	}

	return cpu_type_str[cpu_type];
}


/* module have its own stuff to detect cpu type */
#ifndef MODULE

struct op_cpu_type {
	const char *cpu_name;
	int cpu_type;
};

/* be careful here, later entries will be override earlier ones */
static struct op_cpu_type op_cpu_types[] = {
	{ "PentiumPro",  CPU_PPRO },
	{ "Pentium II",  CPU_PII },
	{ "Pentium III", CPU_PIII },
	{ "Celeron",	 CPU_PII },
	{ "Coppermine",  CPU_PIII },
	{ "Athlon",	 CPU_ATHLON },
	{ "Duron",	 CPU_ATHLON },
	{ "K7",		 CPU_ATHLON },
};

#define OP_CPU_TYPES_NR (sizeof(op_cpu_types) / sizeof(op_cpu_types[0]))


static int op_type_from_name(char const * name)
{
	uint i;
	int cpu_type = CPU_NO_GOOD;

	for (i = 0; i < OP_CPU_TYPES_NR; i++) {
		if (strstr(name, op_cpu_types[i].cpu_name))
			cpu_type = op_cpu_types[i].cpu_type;
	}
	return cpu_type;
}

#define MODEL_PREFIX "model name\t: "

/**
 * op_get_cpu_type - get from /proc/cpuinfo the cpu type
 *
 * returns CPU_NO_GOOD if the CPU could not be identified
 */
int op_get_cpu_type(void)
{
	int cpu_type = CPU_NO_GOOD;
	int cputmp;
	char line[256];
	FILE* fp;

	fp = fopen("/proc/cpuinfo", "r");
	if (!fp) {
		fprintf(stderr, "Unable to open /proc/cpuinfo for reading\n");
		return cpu_type;
	}

	while (fgets(line, sizeof(line) - 1, fp)) {
		if (strncmp(line, MODEL_PREFIX, strlen(MODEL_PREFIX)) == 0) {
			cputmp = op_type_from_name(line + strlen(MODEL_PREFIX));
			if (cputmp != CPU_NO_GOOD)
				cpu_type = cputmp;
		}
	}

	if (cpu_type == CPU_NO_GOOD)
		fprintf(stderr, "Unknown CPU type. Please send /proc/cpuinfo to oprofile-list@lists.sf.net\n");

	fclose(fp);
	return cpu_type;
}

#undef OP_CPU_TYPES_NR

#endif /* !defined(MODULE) */

#ifdef OP_EVENTS_DESC

struct op_unit_desc op_unit_descs[] = {
	{ { NULL, NULL, NULL, NULL, NULL, NULL, NULL, }, },
	{ { "(M)odified cache state",
	  "(E)xclusive cache state",
	  "(S)hared cache state",
	  "(I)nvalid cache state",
	  "all MESI cache state", NULL, NULL, }, },
	{ { "self-generated transactions",
	  "any transactions", NULL, NULL, NULL, NULL, NULL, }, },
	{ { "mandatory", NULL, NULL, NULL, NULL, NULL, NULL, }, },
	{ { "MMX packed multiplies",
	  "MMX packed shifts",
	  "MMX pack operations",
	  "MMX unpack operations",
	  "MMX packed logical",
	  "MMX packed arithmetic",
	  "All the above" }, },
	{ { "MMX->float transitions",
	  "float->MMX transitions",
	  NULL, NULL, NULL, NULL, NULL, }, },
	{ { "ES register",
	  "DS register",
	  "FS register",
	/* IA manual says this is actually FS again - no mention in errata */
	/* but test show that is really a typo error from IA manual */
	  "GS register",
	  "ES,DS,FS,GS registers", NULL, NULL }, },
	{ { "prefetch NTA",
	  "prefetch T1",
	  "prefetch T2",
	  "weakly ordered stores", NULL, NULL, NULL, }, },
	{ { "packed and scalar", "packed", NULL, NULL, NULL, NULL, NULL, }, },
	{ { "(M)odified cache state",
	  "(O)wner cache state",
	  "(E)xclusive cache state",
	  "(S)hared cache state",
	  "(I)nvalid cache state",
	  "all MOESI cache state", NULL, }, },
};

char *op_event_descs[] = {
  "clocks processor is not halted",
  /* Data Cache Unit (DCU) */
  "all memory references, cachable and non",
  "total lines allocated in the DCU",
  "number of M state lines allocated in DCU",
  "number of M lines evicted from the DCU",
  "number of cycles while DCU miss outstanding",
  /* Intruction Fetch Unit (IFU) */
  "number of non/cachable instruction fetches",
  "number of instruction fetch misses",
  "number of ITLB misses",
  "cycles instruction fetch pipe is stalled",
  "cycles instruction length decoder is stalled",
  /* L2 Cache */
  "number of L2 instruction fetches",
  "number of L2 data loads",
  "number of L2 data stores",
  "number of allocated lines in L2",
  "number of recovered lines from L2",
  "number of modified lines allocated in L2",
  "number of modified lines removed from L2",
  "number of L2 requests",
  "number of L2 address strobes",
  "number of cycles data bus was busy",
  "cycles data bus was busy in xfer from L2 to CPU",
  /* External Bus Logic (EBL) */
  "number of clocks DRDY is asserted",
  "number of clocks LOCK is asserted",
  "number of outstanding bus requests",
  "number of burst read transactions",
  "number of read for ownership transactions",
  "number of write back transactions",
  "number of instruction fetch transactions",
  "number of invalidate transactions",
  "number of partial write transactions",
  "number of partial transactions",
  "number of I/O transactions",
  "number of deferred transactions",
  "number of burst transactions",
  "number of all transactions",
  "number of memory transactions",
  "bus cycles this processor is receiving data",
  "bus cycles this processor is driving BNR pin",
  "bus cycles this processor is driving HIT pin",
  "bus cycles this processor is driving HITM pin",
  "cycles during bus snoop stall",
  /* Floating Point Unit (FPU) */
  "number of computational FP operations retired",
  "number of computational FP operations executed",
  "number of FP exceptions handled by microcode",
  "number of multiplies",
  "number of divides",
  "cycles divider is busy",
  /* Memory Ordering */
  "number of store buffer blocks",
  "number of store buffer drain cycles",
  "number of misaligned data memory references",
  /* PIII KNI */
  "number of KNI pre-fetch/weakly ordered insns dispatched",
  "number of KNI pre-fetch/weakly ordered insns that miss all caches",
  /* Instruction Decoding and Retirement */
  "number of instructions retired",
  "number of UOPs retired",
  "number of instructions decoded",
  /* PIII KNI */
  "number of KNI instructions retired",
  "number of KNI computation instructions retired",
  /* Interrupts */
  "number of hardware interrupts received",
  "cycles interrupts are disabled",
  "cycles interrupts are disabled with pending interrupts",
  /* Branches */
  "number of branch instructions retired",
  "number of mispredicted bracnhes retired",
  "number of taken branches retired",
  "number of taken mispredictions branches retired",
  "number of branch instructions decoded",
  "number of branches that miss the BTB",
  "number of bogus branches",
  "number of times BACLEAR is asserted",
  /* Stalls */
  "cycles during resource related stalls",
  "cycles or events for partial stalls",
  /* Segment Register Loads */
  "number of segment register loads",
  /* MMX (Pentium II only) */
  "number of MMX instructions executed",
  "number of MMX saturating instructions executed",
  "number of MMX UOPS executed",
  "number of MMX packing instructions",
  "MMX-floating point transitions",
  "number of EMMS instructions executed",
  "number of MMX instructions retired",
  /* segment renaming (Pentium II only) */
  "number of segment register renaming stalls",
  "number of segment register renames",
  "number of segment register rename events retired",
  /* Athlon/Duron */
  "Retired instructions (includes exceptions, interrupts, resyncs)",
  "Retired Ops",
  "Instruction cache fetches)",
  "Instruction cache misses)",
  "Data cache accesses",
  "Data cache misses",
  "Data cache refills from L2",
  "Data cache refills from system",
  "Data cache write backs",
  "Retired branches (conditional, unconditional, exceptions, interrupts)",
  "Retired branches mispredicted",
  "Retired taken branches",
  "Retired taken branches mispredicted",
  "L1 DTLB misses and L2 DTLB hits",
  "L1 and L2 DTLB misses",
  "Misaligned data references",
  "L1 ITLB misses (and L2 ITLB hits)",
  "L1 and L2 ITLB misses",
  "Retired far control transfers",
  "Retired resync branches (only non-control transfer branches counted)",
  "Interrupts masked cycles (IF=0)",
  "Interrupts masked while pending cycles (INTR while IF=0)",
  "Number of taken hardware interrupts",
  "Serialise",
};

/**
 * op_get_um_desc - verify and get unit mask description
 * @op_events_index: the index of the events in op_events array
 * @um: unit mask
 *
 * Try to get the associated unit mask given the event index and unit
 * mask value. No error can occur.
 *
 * The function return the associated help string about this um or
 * NULL if um is invalid.
 * This string is in text section so should not be freed.
 */
static char *op_get_um_desc(uint op_events_index, u8 um)
{
	struct op_unit_mask *op_um_mask;
	int um_mask_desc_index;
	uint um_mask_index = op_events[op_events_index].unit;

	if (!um_mask_index)
		return NULL;

	op_um_mask = &op_unit_masks[um_mask_index];
	um_mask_desc_index = op_check_unit_mask(op_um_mask, um);

	if (um_mask_desc_index == -1)
		return NULL;
	else if (um_mask_desc_index == 0) {
		/* avoid dynamic alloc to simplify caller's life */
		return "set with multiple units, check the documentation";
	}

	return op_unit_descs[um_mask_index].desc[um_mask_desc_index-1];
}

/**
 * op_get_event_desc - get event name and description
 * @cpu_type: the cpu_type
 * @type: event value
 * @um: unit mask
 * @typenamep: returned event name string
 * @typedescp: returned event description string
 * @umdescp: returned unit mask description string
 *
 * Get the associated event name and descriptions given
 * the cpu type, event value and unit mask value. It is a fatal error
 * to supply a non-valid @type value, but an invalid @um
 * will not exit.
 *
 * @typenamep, @typedescp, @umdescp are filled in with pointers
 * to the relevant name and descriptions. @umdescp can be set to
 * NULL when @um is invalid for the given @type value.
 * These strings are in text section so should not be freed.
 */
void op_get_event_desc(int cpu_type, u8 type, u8 um, char **typenamep, char **typedescp, char **umdescp)
{
	uint i;
	int cpu_mask = 1 << cpu_type;

	*typenamep = *typedescp = *umdescp = NULL;

	for (i=0; i < op_nr_events; i++) {
		if (op_events[i].val == type && (op_events[i].cpu_mask & cpu_mask)) {
			*typenamep = (char *)op_events[i].name;
			*typedescp = op_event_descs[i];
			
			*umdescp = op_get_um_desc(i, um);
			break;
		}
	}

	if (!*typenamep) {
		fprintf(stderr,"op_get_event_desc: no such event 0x%.2x\n",type);
		exit(1);
	}
}

#endif /* OP_EVENTS_DESC */

#ifdef OP_EVENTS_MAIN

#include "version.h"

static int cpu_type = CPU_NO_GOOD;

/**
 * help_for_event - output event name and description
 * @i: event number
 *
 * output an help string for the event @i
 */
static void help_for_event(int i)
{
	uint k, j;
	uint mask;

	printf("%s", op_events[i].name);

	printf(": (counter: ");
	if (op_events[i].counter_mask == CTR_ALL) {
		printf("all");
	} else {
		mask = op_events[i].counter_mask;
		for (k = 0; k < CHAR_BIT * sizeof(op_events[i].counter_mask); ++k) {
			if (mask & (1 << k)) {
				printf("%d", k);
				mask &= ~(1 << k);
				if (mask)
					printf(", ");
			}
		}
	}
	printf(")");

	printf(" (supported cpu: ");
	mask = op_events[i].cpu_mask;
	for (k = 0; k < MAX_CPU_TYPE; ++k) {
		if (mask & (1 << k)) {
			printf("%s", cpu_type_str[k]);
			mask &= ~(1 << k);
			if (mask)
				printf(", ");
		}
	}

	printf(")\n\t%s (min count: %d)\n", op_event_descs[i], op_events[i].min_count);

	if (op_events[i].unit) {
		int unit_idx = op_events[i].unit;

		printf("\tUnit masks\n");
		printf("\t----------\n");

		for (j=0; j < op_unit_masks[unit_idx].num; j++) {
			printf("\t%.2x: %s\n",
			       op_unit_masks[unit_idx].um[j],
			       op_unit_descs[unit_idx].desc[j]);
		}
	}
}

int main(int argc, char *argv[])
{
	int i;
	uint j;
	int cpu_type_mask;
	int for_gui;

	cpu_type = op_get_cpu_type();

	for_gui = 0;
	for (i = 1 ; i < argc ; ++i) {
		if (!strcmp(argv[i], "--version")) {
			printf(VERSION_STRING " compiled on " __DATE__ " " __TIME__ "\n");
			return 0;
		} else if (!strcmp(argv[i], "--help")) {
			printf("op_help [--version|--cpu-type] event_name\n");
			return 0;
		} else if (!strncmp(argv[i], "--cpu-type=", 11)) {
			sscanf(argv[i] + 11, "%d", &cpu_type);
			if (cpu_type < 0 || cpu_type >= MAX_CPU_TYPE) {
				fprintf(stderr, "invalid cpu type %d !\n", cpu_type);
				exit(EXIT_FAILURE);
			}
		} else if (!strncmp(argv[i], "--get-cpu-type", 11)) {
			printf("%d\n", cpu_type);
			exit(EXIT_SUCCESS);
		} else {
			cpu_type_mask = 1 << cpu_type;
			for (j=0; j < op_nr_events; j++) {
				if (!strcmp(op_events[j].name, argv[i]) && 
				    (op_events[j].cpu_mask & cpu_type_mask)) {
					printf("%d\n", op_events[j].val); 
					return 0;
				}
			}
			fprintf(stderr, "No such event \"%s\"\n", argv[i]);
			return 1;
		return 0;
		}
	}

	printf("oprofile: available events\n");
	printf("--------------------------\n\n");
	if (cpu_type == CPU_ATHLON)
		printf ("See AMD document x86 optimisation guide (22007.pdf), Appendix D\n\n");
	else
		printf("See Intel Architecture Developer's Manual\nVol. 3, Appendix A\n\n");

	cpu_type_mask = 1 << cpu_type;
	for (j=0; j < op_nr_events; j++) {
		if ((op_events[j].cpu_mask & cpu_type_mask) != 0) {
			help_for_event(j);
		}
	}

	return 0;
}
#endif /* OP_EVENTS_MAIN */
