/* $Id: op_events_desc.c,v 1.1 2001/11/12 14:05:34 phil_e Exp $ */
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#include "../op_user.h"

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

