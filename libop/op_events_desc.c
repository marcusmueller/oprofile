/**
 * @file op_events_desc.c
 *
 * Adapted from libpperf 0.5 by M. Patrick Goda and Michael S. Warren
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 * 
 * @author John Levon <moz@compsoc.man.ac.uk>
 * @author Philippe Elie <phil_el@wanadoo.fr>
 */

/* See IA32 Vol. 3 Appendix A */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "op_events.h"
#include "op_events_desc.h"
 
/**
 * op_get_cpu_type - get from /proc/sys/dev/oprofile/cpu_type the cpu type
 *
 * returns %CPU_NO_GOOD if the CPU could not be identified
 */
op_cpu op_get_cpu_type(void)
{
	int cpu_type = CPU_NO_GOOD;
	char str[10];
 
	FILE * fp;

	fp = fopen("/proc/sys/dev/oprofile/cpu_type", "r");
	if (!fp) {
		fprintf(stderr, "Unable to open /proc/sys/dev/oprofile/cpu_type for reading\n");
		return cpu_type;
	}

	fgets(str, 9, fp);
 
	sscanf(str, "%d\n", &cpu_type);

	fclose(fp);

	return cpu_type;
}

static char const * cpu_names[MAX_CPU_TYPE] = {
	"Pentium Pro",
	"PII",
	"PIII",
	"Athlon",
	"CPU with RTC device"
};

/**
 * op_get_cpu_type_str - get the cpu string.
 * @param cpu_type  the cpu type identifier
 *
 * The function always return a valid const char*
 * the core cpu denomination or "invalid cpu type" if
 * @cpu_type is not valid.
 */
char const * op_get_cpu_type_str(op_cpu cpu_type)
{
	if (cpu_type < 0 || cpu_type > MAX_CPU_TYPE) {
		return "invalid cpu type";
	}

	return cpu_names[cpu_type];
}

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

char * op_event_descs[] = {
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
  "RTC interrupts/sec (rounded up to power of two)",
};

/**
 * op_get_um_desc - verify and get unit mask description
 * @param op_events_index  the index of the events in op_events array
 * @param um  unit mask
 *
 * Try to get the associated unit mask given the event index and unit
 * mask value. No error can occur.
 *
 * The function return the associated help string about this um or
 * NULL if um is invalid.
 * This string is in text section so should not be freed.
 */
static char * op_get_um_desc(u32 op_events_index, u8 um)
{
	struct op_unit_mask * op_um_mask;
	int um_mask_desc_index;
	u32 um_mask_index = op_events[op_events_index].unit;

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
 * @param cpu_type  the cpu_type
 * @param type  event value
 * @param um  unit mask
 * @param typenamep  returned event name string
 * @param typedescp  returned event description string
 * @param umdescp  returned unit mask description string
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
void op_get_event_desc(op_cpu cpu_type, u8 type, u8 um,
	char ** typenamep, char ** typedescp, char ** umdescp)
{
	u32 i;
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
		exit(EXIT_FAILURE);
	}
}
