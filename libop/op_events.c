/**
 * @file op_events.c
 * Details of PMC profiling events
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author John Levon
 * @author Philippe Elie
 */

/* Adapted from libpperf 0.5 by M. Patrick Goda and Michael S. Warren */

/* See IA32 Vol. 3 Appendix A + Athlon optimization manual */

/*
 * WARNING: this code is also included in the kernel module,
 * so no silliness
 */ 
 
#include "op_events.h"

/* Special empty unit mask, globally exposed */
static struct op_unit_mask const um_empty = 
	{ 0, utm_mandatory, 0x00, 
	  { {0x0, 0x0} } };

/* Modified/Exclusive/Shared/Invalid (MESI) counters */
static struct op_unit_mask const um_mesi = 
	{ 5, utm_bitmask, 0x0f, 
	  { {0x8, "(M)odified cache state"},
	    {0x4, "(E)xclusive cache state"}, 
	    {0x2, "(S)hared cache state"},
	    {0x1, "(I)nvalid cache state"},
	    {0xf, "all MESI cache state"} } };

/* External Bus Logic (EBL) self/any default to any transitions */
static struct op_unit_mask const um_ebl = 
	{ 2, utm_exclusive, 0x20, 
	  { {0x0, "self-generated transactions"},
	    {0x20, "any transactions"} } };

/* MMX PII events */
static struct op_unit_mask const um_mmx_uops = 
	{ 1, utm_mandatory, 0xf, 
	  { {0xf, "mandatory"} } };

static struct op_unit_mask const um_mmx_instr_type_exec = 
	{ 7, utm_bitmask, 0x3f, 
	  { {0x1,  "MMX packed multiplies"},
	    {0x2,  "MMX packed shifts"},
	    {0x4,  "MMX pack operations"},
	    {0x8,  "MMX unpack operations"},
	    {0x10, "MMX packed logical"},
	    {0x20, "MMX packed arithmetic"},
	    {0x3f, "All the above"} } };

static struct op_unit_mask const um_mmx_trans = 
	{ 2, utm_exclusive, 0x0, 
	  { {0x0, "MMX->float transitions"},
	    {0x1, "float->MMX transitions"} } };

static struct op_unit_mask const um_seg_rename = 
	{ 5, utm_bitmask, 0x0f, 
	  { {0x1, "ES register"},
	    {0x2, "DS register"},
	    {0x4, "FS register"},
	    /* IA manual says this is actually FS again - no mention in errata */
	    /* but test show that is really a typo error from IA manual */
	    {0x8, "GS register"},
	    {0xf, "ES,DS,FS,GS registers"} } };

/* KNI PIII events */
static struct op_unit_mask const um_kni_prefetch = 
	{ 4, utm_exclusive, 0x0, 
	  { {0x0, "prefetch NTA"},
	    {0x1, "prefetch T1"},
	    {0x2, "prefetch T2"},
	    {0x3, "weakly ordered stores"} } };

static struct op_unit_mask const um_kni_inst_retired = 
	{ 2, utm_bitmask, 0x1, 
	  { {0x0, "packed and scalar"},
	    {0x1, "packed"} } };

/* Athlon MOESI cache events */
static struct op_unit_mask const um_moesi = 
	{ 6, utm_bitmask, 0x1f, 
	  { {0x10, "(M)odified cache state"},
	    {0x8, "(O)wner cache state"},
	    {0x4, "(E)xclusive cache state"},
	    {0x2, "(S)hared cache state"},
	    {0x1, "(I)nvalid cache state"},
	    {0x1f, "all MOESI cache state"} } };

/* the following are just short cut for filling the table of event */
#define OP_RTC		(1 << CPU_RTC)
#define OP_ATHLON	(1 << CPU_ATHLON)
#define OP_PPRO		(1 << CPU_PPRO)
#define OP_PII		(1 << CPU_PII)
#define OP_PIII		(1 << CPU_PIII)
#define OP_PII_PIII	(OP_PII | OP_PIII)
#define OP_IA_ALL	(OP_PII_PIII | OP_PPRO)

#define CTR_0		(1 << 0)
#define CTR_1		(1 << 1)

/* ctr allowed, allowed cpus, Event #, unit mask, name, min event value */

/* event name must be in one word */
struct op_event const op_events[] = {

  /* Clocks */
  { CTR_ALL, OP_IA_ALL, 0x79, &um_empty, "CPU_CLK_UNHALTED", 
      "clocks processor is not halted", 6000 },

  /* Data Cache Unit (DCU) */
  { CTR_ALL, OP_IA_ALL, 0x43, &um_empty, "DATA_MEM_REFS", 
    "all memory references, cachable and non", 500 },
  { CTR_ALL, OP_IA_ALL, 0x45, &um_empty, "DCU_LINES_IN", 
    "total lines allocated in the DCU", 500 },
  { CTR_ALL, OP_IA_ALL, 0x46, &um_empty, "DCU_M_LINES_IN", 
    "number of M state lines allocated in DCU", 500 },
  { CTR_ALL, OP_IA_ALL, 0x47, &um_empty, "DCU_M_LINES_OUT", 
    "number of M lines evicted from the DCU", 500},
  { CTR_ALL, OP_IA_ALL, 0x48, &um_empty, "DCU_MISS_OUTSTANDING", 
    "number of cycles while DCU miss outstanding", 500 },
  /* Intruction Fetch Unit (IFU) */
  { CTR_ALL, OP_IA_ALL, 0x80, &um_empty, "IFU_IFETCH", 
    "number of non/cachable instruction fetches", 500 },
  { CTR_ALL, OP_IA_ALL, 0x81, &um_empty, "IFU_IFETCH_MISS", 
    "number of instruction fetch misses", 500 },
  { CTR_ALL, OP_IA_ALL, 0x85, &um_empty, "ITLB_MISS", 
    "number of ITLB misses" ,500},
  { CTR_ALL, OP_IA_ALL, 0x86, &um_empty, "IFU_MEM_STALL", 
    "cycles instruction fetch pipe is stalled", 500 },
  { CTR_ALL, OP_IA_ALL, 0x87, &um_empty, "ILD_STALL", 
    "cycles instruction length decoder is stalled", 500 },
  /* L2 Cache */
  { CTR_ALL, OP_IA_ALL, 0x28, &um_mesi, "L2_IFETCH", 
    "number of L2 instruction fetches", 500 },
  { CTR_ALL, OP_IA_ALL, 0x29, &um_mesi, "L2_LD", 
    "number of L2 data loads", 500 },
  { CTR_ALL, OP_IA_ALL, 0x2a, &um_mesi, "L2_ST", 
    "number of L2 data stores", 500 },
  { CTR_ALL, OP_IA_ALL, 0x24, &um_empty, "L2_LINES_IN", 
    "number of allocated lines in L2", 500 },
  { CTR_ALL, OP_IA_ALL, 0x26, &um_empty, "L2_LINES_OUT", 
    "number of recovered lines from L2", 500 },
  { CTR_ALL, OP_IA_ALL, 0x25, &um_empty, "L2_M_LINES_INM", 
    "number of modified lines allocated in L2", 500 },
  { CTR_ALL, OP_IA_ALL, 0x27, &um_empty, "L2_M_LINES_OUTM", 
    "number of modified lines removed from L2", 500 },
  { CTR_ALL, OP_IA_ALL, 0x2e, &um_mesi, "L2_RQSTS", 
    "number of L2 requests", 500 },
  { CTR_ALL, OP_IA_ALL, 0x21, &um_empty, "L2_ADS", 
    "number of L2 address strobes", 500 },
  { CTR_ALL, OP_IA_ALL, 0x22, &um_empty, "L2_DBUS_BUSY", 
    "number of cycles data bus was busy", 500 },
  { CTR_ALL, OP_IA_ALL, 0x23, &um_empty, "L2_DMUS_BUSY_RD", 
    "cycles data bus was busy in xfer from L2 to CPU", 500 },
  /* External Bus Logic (EBL) */
  { CTR_ALL, OP_IA_ALL, 0x62, &um_ebl, "BUS_DRDY_CLOCKS", 
    "number of clocks DRDY is asserted", 500 },
  { CTR_ALL, OP_IA_ALL, 0x63, &um_ebl, "BUS_LOCK_CLOCKS", 
    "number of clocks LOCK is asserted", 500 },
  { CTR_ALL, OP_IA_ALL, 0x60, &um_empty, "BUS_REQ_OUTSTANDING", 
    "number of outstanding bus requests", 500 },
  { CTR_ALL, OP_IA_ALL, 0x65, &um_ebl, "BUS_TRAN_BRD", 
    "number of burst read transactions", 500 },
  { CTR_ALL, OP_IA_ALL, 0x66, &um_ebl, "BUS_TRAN_RFO", 
    "number of read for ownership transactions", 500 },
  { CTR_ALL, OP_IA_ALL, 0x67, &um_ebl, "BUS_TRANS_WB", 
    "number of write back transactions", 500 },
  { CTR_ALL, OP_IA_ALL, 0x68, &um_ebl, "BUS_TRAN_IFETCH", 
    "number of instruction fetch transactions", 500 },
  { CTR_ALL, OP_IA_ALL, 0x69, &um_ebl, "BUS_TRAN_INVAL", 
    "number of invalidate transactions", 500 },
  { CTR_ALL, OP_IA_ALL, 0x6a, &um_ebl, "BUS_TRAN_PWR", 
    "number of partial write transactions", 500 },
  { CTR_ALL, OP_IA_ALL, 0x6b, &um_ebl, "BUS_TRANS_P", 
    "number of partial transactions", 500 },
  { CTR_ALL, OP_IA_ALL, 0x6c, &um_ebl, "BUS_TRANS_IO", 
    "number of I/O transactions", 500 },
  { CTR_ALL, OP_IA_ALL, 0x6d, &um_ebl, "BUS_TRANS_DEF", 
    "number of deferred transactions", 500 },
  { CTR_ALL, OP_IA_ALL, 0x6e, &um_ebl, "BUS_TRAN_BURST", 
    "number of burst transactions", 500 },
  { CTR_ALL, OP_IA_ALL, 0x70, &um_ebl, "BUS_TRAN_ANY", 
    "number of all transactions", 500 },
  { CTR_ALL, OP_IA_ALL, 0x6f, &um_ebl, "BUS_TRAN_MEM", 
    "number of memory transactions", 500 },
  { CTR_ALL, OP_IA_ALL, 0x64, &um_empty, "BUS_DATA_RCV", 
    "bus cycles this processor is receiving data", 500 },
  { CTR_ALL, OP_IA_ALL, 0x61, &um_empty, "BUS_BNR_DRV", 
    "bus cycles this processor is driving BNR pin", 500 },
  { CTR_ALL, OP_IA_ALL, 0x7a, &um_empty, "BUS_HIT_DRV", 
    "bus cycles this processor is driving HIT pin", 500 },
  { CTR_ALL, OP_IA_ALL, 0x7b, &um_empty, "BUS_HITM_DRV", 
    "bus cycles this processor is driving HITM pin", 500 },
  { CTR_ALL, OP_IA_ALL, 0x7e, &um_empty, "BUS_SNOOP_STALL", 
    "cycles during bus snoop stall", 500 },
  /* Floating Point Unit (FPU) */
  { CTR_0, OP_IA_ALL, 0xc1, &um_empty, "COMP_FLOP_RET", 
    "number of computational FP operations retired", 3000 },
  { CTR_0, OP_IA_ALL, 0x10, &um_empty, "FLOPS", 
    "number of computational FP operations executed", 3000 },
  { CTR_1, OP_IA_ALL, 0x11, &um_empty, "FP_ASSIST", 
    "number of FP exceptions handled by microcode", 500 },
  { CTR_1, OP_IA_ALL, 0x12, &um_empty, "MUL", 
    "number of multiplies", 1000 },
  { CTR_1, OP_IA_ALL, 0x13, &um_empty, "DIV", 
    "number of divides", 500 },
  { CTR_0, OP_IA_ALL, 0x14, &um_empty, "CYCLES_DIV_BUSY", 
    "cycles divider is busy", 1000 },
  /* Memory Ordering */
  { CTR_ALL, OP_IA_ALL, 0x03, &um_empty, "LD_BLOCKS", 
    "number of store buffer blocks", 500 },
  { CTR_ALL, OP_IA_ALL, 0x04, &um_empty, "SB_DRAINS", 
    "number of store buffer drain cycles", 500 },
  { CTR_ALL, OP_IA_ALL, 0x05, &um_empty, "MISALIGN_MEM_REF", 
    "number of misaligned data memory references", 500 },
  /* PIII KNI */
  { CTR_ALL, OP_PIII, 0x07, &um_kni_prefetch, "EMON_KNI_PREF_DISPATCHED", 
    "number of KNI pre-fetch/weakly ordered insns dispatched", 500 },
  { CTR_ALL, OP_PIII, 0x4b, &um_kni_prefetch, "EMON_KNI_PREF_MISS", 
    "number of KNI pre-fetch/weakly ordered insns that miss all caches", 500 },
  /* Instruction Decoding and Retirement */
  { CTR_ALL, OP_IA_ALL, 0xc0, &um_empty, "INST_RETIRED", 
    "number of instructions retired", 6000 },
  { CTR_ALL, OP_IA_ALL, 0xc2, &um_empty, "UOPS_RETIRED", 
    "number of UOPs retired", 6000 },
  { CTR_ALL, OP_IA_ALL, 0xd0, &um_empty, "INST_DECODED", 
    "number of instructions decoded", 6000 },
  /* PIII KNI */
  { CTR_ALL, OP_PIII, 0xd8, &um_kni_inst_retired, "EMON_KNI_INST_RETIRED", 
    "number of KNI instructions retired", 3000 },
  { CTR_ALL, OP_PIII, 0xd9, &um_kni_inst_retired, "EMON_KNI_COMP_INST_RET", 
    "number of KNI computation instructions retired", 3000 },
  /* Interrupts */
  { CTR_ALL, OP_IA_ALL, 0xc8, &um_empty, "HW_INT_RX", 
    "number of hardware interrupts received", 500 },
  { CTR_ALL, OP_IA_ALL, 0xc6, &um_empty, "CYCLES_INT_MASKED", 
    "cycles interrupts are disabled", 500 },
  { CTR_ALL, OP_IA_ALL, 0xc7, &um_empty, "CYCLES_INT_PENDING_AND_MASKED", 
    "cycles interrupts are disabled with pending interrupts", 500 },
  /* Branches */
  { CTR_ALL, OP_IA_ALL, 0xc4, &um_empty, "BR_INST_RETIRED", 
    "number of branch instructions retired", 500 },
  { CTR_ALL, OP_IA_ALL, 0xc5, &um_empty, "BR_MISS_PRED_RETIRED", 
    "number of mispredicted branches retired", 500 },
  { CTR_ALL, OP_IA_ALL, 0xc9, &um_empty, "BR_TAKEN_RETIRED", 
    "number of taken branches retired", 500 },
  { CTR_ALL, OP_IA_ALL, 0xca, &um_empty, "BR_MISS_PRED_TAKEN_RET", 
    "number of taken mispredictions branches retired", 500 },
  { CTR_ALL, OP_IA_ALL, 0xe0, &um_empty, "BR_INST_DECODED", 
    "number of branch instructions decoded", 500 },
  { CTR_ALL, OP_IA_ALL, 0xe2, &um_empty, "BTB_MISSES", 
    "number of branches that miss the BTB", 500 },
  { CTR_ALL, OP_IA_ALL, 0xe4, &um_empty, "BR_BOGUS", 
    "number of bogus branches", 500 },
  { CTR_ALL, OP_IA_ALL, 0xe6, &um_empty, "BACLEARS", 
    "number of times BACLEAR is asserted", 500 },
  /* Stalls */
  { CTR_ALL, OP_IA_ALL, 0xa2, &um_empty, "RESOURCE_STALLS", 
    "cycles during resource related stalls", 500 },
  { CTR_ALL, OP_IA_ALL, 0xd2, &um_empty, "PARTIAL_RAT_STALLS", 
    "cycles or events for partial stalls", 500 },
  /* Segment Register Loads */
  { CTR_ALL, OP_IA_ALL, 0x06, &um_empty, "SEGMENT_REG_LOADS", 
    "number of segment register loads", 500 },
  /* MMX (Pentium II only) */
  { CTR_ALL, OP_PII, 0xb0, &um_empty, "MMX_INSTR_EXEC", 
    "number of MMX instructions executed", 3000 },
  { CTR_ALL, OP_PII_PIII, 0xb1, &um_empty, "MMX_SAT_INSTR_EXEC", 
    "number of MMX saturating instructions executed", 3000 },
  { CTR_ALL, OP_PII_PIII, 0xb2, &um_mmx_uops, "MMX_UOPS_EXEC", 
    "number of MMX UOPS executed", 3000 },
  { CTR_ALL, OP_PII_PIII, 0xb3, &um_mmx_instr_type_exec, "MMX_INSTR_TYPE_EXEC", 
    "number of MMX packing instructions", 3000 },
  { CTR_ALL, OP_PII_PIII, 0xcc, &um_mmx_trans, "FP_MMX_TRANS", 
    "MMX-floating point transitions", 3000 },
  { CTR_ALL, OP_PII_PIII, 0xcd, &um_empty, "MMX_ASSIST", 
    "number of EMMS instructions executed", 500 },
  { CTR_ALL, OP_PII_PIII, 0xce, &um_empty, "MMX_INSTR_RET", 
    "number of MMX instructions retired", 3000 },
  /* segment renaming (Pentium II only) */
  { CTR_ALL, OP_PII, 0xd4, &um_seg_rename, "SEG_RENAME_STALLS", 
    "number of segment register renaming stalls", 500 },
  { CTR_ALL, OP_PII, 0xd5, &um_seg_rename, "SEG_REG_RENAMES", 
    "number of segment register renames", 500 },
  { CTR_ALL, OP_PII, 0xd6, &um_empty, "RET_SEG_RENAMES", 
    "number of segment register rename events retired", 500 },

  /* athlon events */
  { CTR_ALL, OP_ATHLON, 0xc0, &um_empty, "RETIRED_INSNS", 
    "Retired instructions (includes exceptions, interrupts, resyncs)", 3000,},
  { CTR_ALL, OP_ATHLON, 0xc1, &um_empty, "RETIRED_OPS", 
    "Retired Ops", 500,},
  { CTR_ALL, OP_ATHLON, 0x80, &um_empty, "ICACHE_FETCHES", 
    "Instruction cache fetches", 500,},
  { CTR_ALL, OP_ATHLON, 0x81, &um_empty, "ICACHE_MISSES", 
    "Instruction cache misses", 500,},
  { CTR_ALL, OP_ATHLON, 0x40, &um_empty, "DATA_CACHE_ACCESSES", 
    "Data cache accesses", 500,},
  { CTR_ALL, OP_ATHLON, 0x41, &um_empty, "DATA_CACHE_MISSES", 
    "Data cache misses", 500,},
  { CTR_ALL, OP_ATHLON, 0x42, &um_moesi, "DATA_CACHE_REFILLS_FROM_L2", 
    "Data cache refills from L2", 500,},
  { CTR_ALL, OP_ATHLON, 0x43, &um_moesi, "DATA_CACHE_REFILLS_FROM_SYSTEM", 
    "Data cache refills from system", 500,},
  { CTR_ALL, OP_ATHLON, 0x44, &um_moesi, "DATA_CACHE_WRITEBACKS", 
    "Data cache write backs", 500,},
  { CTR_ALL, OP_ATHLON, 0xc2, &um_empty, "RETIRED_BRANCHES", 
    "Retired branches (conditional, unconditional, exceptions, interrupts)", 500,},
  { CTR_ALL, OP_ATHLON, 0xc3, &um_empty, "RETIRED_BRANCHES_MISPREDICTED", 
    "Retired branches mispredicted", 500,},
  { CTR_ALL, OP_ATHLON, 0xc4, &um_empty, "RETIRED_TAKEN_BRANCHES", 
    "Retired taken branches", 500,},
  { CTR_ALL, OP_ATHLON, 0xc5, &um_empty, "RETIRED_TAKEN_BRANCHES_MISPREDICTED", 
    "Retired taken branches mispredicted", 500,},
  { CTR_ALL, OP_ATHLON, 0x45, &um_empty, "L1_DTLB_MISSES_L2_DTLD_HITS", 
    "L1 DTLB misses and L2 DTLB hits", 500,},
  { CTR_ALL, OP_ATHLON, 0x46, &um_empty, "L1_AND_L2_DTLB_MISSES", 
    "L1 and L2 DTLB misses", 500,},
  { CTR_ALL, OP_ATHLON, 0x47, &um_empty, "MISALIGNED_DATA_REFS", 
    "Misaligned data references", 500,},
  { CTR_ALL, OP_ATHLON, 0x84, &um_empty, "L1_ITLB_MISSES_L2_ITLB_HITS", 
    "L1 ITLB misses (and L2 ITLB hits)", 500,},
  { CTR_ALL, OP_ATHLON, 0x85, &um_empty, "L1_AND_L2_ITLB_MISSES", 
    "L1 and L2 ITLB misses", 500,},
  { CTR_ALL, OP_ATHLON, 0xc6, &um_empty, "RETIRED_FAR_CONTROL_TRANSFERS", 
    "Retired far control transfers", 500,},
  { CTR_ALL, OP_ATHLON, 0xc7, &um_empty, "RETIRED_RESYNC_BRANCHES", 
    "Retired resync branches (only non-control transfer branches counted)", 500,},
  { CTR_ALL, OP_ATHLON, 0xcd, &um_empty, "INTERRUPTS_MASKED", 
    "Interrupts masked cycles (IF=0)", 500,},
  { CTR_ALL, OP_ATHLON, 0xce, &um_empty, "INTERRUPTS_MASKED_PENDING", 
    "Interrupts masked while pending cycles (INTR while IF=0)", 500,},
  { CTR_ALL, OP_ATHLON, 0xcf, &um_empty, "HARDWARE_INTERRUPTS", 
    "Number of taken hardware interrupts", 10,},

  /* other CPUs */
  { CTR_0, OP_RTC, 0xff, &um_empty, "RTC_Interrupts", 
    "RTC interrupts/sec (rounded up to power of two)", 2,},
};


/* the total number of events for all processor type */
u32 const op_nr_events = (sizeof(op_events)/sizeof(op_events[0]));

/**
 * op_check_unit_mask - sanity check unit mask value
 * @param allow  allowed unit mask array
 * @param um  unit mask value to check
 *
 * Verify that a unit mask value is within the allowed array.
 *
 * The function returns:
 * -1  if the value is not allowed,
 * 0   if the value is allowed and represents multiple units,
 * > 0 otherwise, in this case allow->um[return value - 1] == um so the
 * caller can access to the description of the unit_mask.
 */
int op_check_unit_mask(struct op_unit_mask const * allow, u8 um)
{
	u32 i, mask;

	switch (allow->unit_type_mask) {
		case utm_exclusive:
		case utm_mandatory:
			for (i=0; i < allow->num; i++) {
				if (allow->um[i].value == um)
					return i + 1;
			}
			break;

		case utm_bitmask:
			/* Must reject 0 bit mask because it can count nothing */
			if (um == 0)
				break;

			mask = 0;
			for (i=0; i < allow->num; i++) {
				if (allow->um[i].value == um)
					/* it is an exact match so return the index + 1 */
					return i + 1;

				mask |= allow->um[i].value;
			}

			if ((mask & um) == um)
				return 0;
			break;
	}

	return -1;
}

/**
 * op_min_count - get the minimum count value.
 * @param ctr_type  event value
 * @param cpu_type  cpu type
 *
 * The function returns > 0 if the event is found
 * 0 otherwise
 */
int op_min_count(u8 ctr_type, op_cpu cpu_type)
{
	int ret = 0;
	u32 i;
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
 * @param ctr  counter number
 * @param ctr_type  event value for counter 0
 * @param ctr_um  unit mask for counter 0
 * @param cpu_type  processor type
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
 * The function returns bitmask of failure cause
 * 0 otherwise
 */
int op_check_events(int ctr, u8 ctr_type, u8 ctr_um, op_cpu cpu_type)
{
	int ret = OP_OK_EVENT;
	u32 i;
	u32 cpu_mask = 1 << cpu_type;
	u32 ctr_mask = 1 << ctr;

	for (i = 0 ; i < op_nr_events; i++) {
		if (op_events[i].val == ctr_type && (op_events[i].cpu_mask & cpu_mask)) {
			if ((op_events[i].counter_mask & ctr_mask) == 0)
				ret |= OP_INVALID_COUNTER;

			if (op_events[i].unit->num &&
			    op_check_unit_mask(op_events[i].unit, ctr_um) < 0)
				ret |= OP_INVALID_UM;
			break;
		}
	}

	if (i == op_nr_events)
		ret |= OP_INVALID_EVENT;

	return ret;
}
