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

/* pentium 4 events */

/* BRANCH_RETIRED */
static struct op_unit_mask um_branch_retired =
	{4, utm_bitmask, 0x0c, 
	 { {0x01, "branch not-taken predicted"},
	   {0x02, "branch not-taken mispredicted"},
	   {0x04, "branch taken predicted"},
	   {0x08, "branch taken mispredicted"} } };

/* MISPRED_BRANCH_RETIRED */
static struct op_unit_mask um_mispred_branch_retired =
	{1, utm_bitmask, 0x01, 
	 { {0x01, "retired instruction is non-bogus"} } };

/* TC_DELIVER_MODE */
static struct op_unit_mask um_tc_deliver_mode =
	{8, utm_bitmask, 0x01, 
	 { {0x01, "both logical processors in deliver mode"}, 
	   {0x02, "logical processor 0 in deliver mode, 1 in build mode"}, 
	   {0x04, "logical processor 0 in deliver mode, 1 in halt/clear/trans mode"},
	   {0x08, "logical processor 0 in build mode, 1 in deliver mode"}, 
	   {0x10, "both logical processors in build mode"}, 
	   {0x20, "logical processor 0 in build mode, 1 in halt/clear/trans mode"}, 
	   {0x40, "logical processor 0 in halt/clear/trans mode, 1 in deliver mode"}, 
	   {0x80, "logical processor 0 in halt/clear/trans mode, 1 in build mode"} } };

/* BPU_FETCH_REQUEST */
static struct op_unit_mask um_bpu_fetch_request =
	{1, utm_bitmask, 0x00, 
	 {{0x01, "trace cache lookup miss"} } };

/* ITLB_REFERENCE */
static struct op_unit_mask um_itlb_reference =
	{3, utm_bitmask, 0x07, 
	 { {0x01, "ITLB hit"}, 
	   {0x02, "ITLB miss"}, 
	   {0x04, "uncacheable ITLB hit"} } };

/* MEMORY_CANCEL */
static struct op_unit_mask um_memory_cancel =
	{2, utm_bitmask, 0x06, 
	 { {0x04, "replayed because no store request buffer available"},
	   {0x08, "conflicts due to 64k aliasing"} } };

/* MEMORY_COMPLETE */
static struct op_unit_mask um_memory_complete =
	{2, utm_bitmask, 0x03, 
	 { {0x01, "load split completed, excluding UC/WC loads"},
	   {0x02, "any split stores completed"} } };

/* LOAD_PORT_REPLAY */
static struct op_unit_mask um_load_port_replay =
	{1, utm_bitmask, 0x02, 
	 { {0x02, "split load"} } };

/* STORE_PORT_REPLAY */
static struct op_unit_mask um_store_port_replay =
	{1, utm_bitmask, 0x02, 
	 { {0x02, "split store"} } };

/* MOB_LOAD_REPLAY */
static struct op_unit_mask um_mob_load_replay =
	{4, utm_bitmask, 0x3a, 
	 { {0x02, "replay cause: unknown store address"}, 
	   {0x08, "replay cause: unknown store data"}, 
	   {0x10, "replay cause: partial overlap between load and store"}, 
	   {0x20, "replay cause: mismatched low 4 bits between load and store addr"} } };

/* PAGE_WALK_TYPE */
static struct op_unit_mask um_page_walk_type =
	{2, utm_bitmask, 0x03, 
	 { {0x01, "page walk for data TLB miss"}, 
	   {0x02, "page walk for instruction TLB miss"} } };

/* BSQ_CACHE_REFERENCE */
static struct op_unit_mask um_bsq_cache_reference =	
	{9, utm_bitmask, 0x7ff, 
	 { {0x01, "read 2nd level cache hit shared"}, 
	   {0x02, "read 2nd level cache hit exclusive"},
	   {0x04, "read 2nd level cache hit modified"}, 
	   {0x08, "read 3rd level cache hit shared"}, 
	   {0x10, "read 3rd level cache hit exclusive"},
	   {0x20, "read 3rd level cache hit modified"}, 
	   {0x100, "read 2nd level cache miss"}, 
	   {0x200, "read 3rd level cache miss"}, 
	   {0x400, "writeback lookup from DAC misses 2nd level cache"} } };

/* IOQ_ALLOCATION */
/* IOQ_ACTIVE_ENTRIES */
static struct op_unit_mask um_ioq =
	{15, utm_bitmask, 0xefe1, 
	 { {0x01, "bus request type bit 0"},  
	   {0x02, "bus request type bit 1"},  
	   {0x04, "bus request type bit 2"}, 
	   {0x08, "bus request type bit 3"}, 
	   {0x10, "bus request type bit 4"}, 
	   {0x20, "count read entries"},  
	   {0x40, "count write entries"}, 
	   {0x80, "count UC memory access entries"},
	   {0x100, "count WC memory access entries"}, 
	   {0x200, "count write-through memory access entries"}, 
	   {0x400, "count write-protected memory access entries"}, 
	   {0x800, "count WB memory access entries"}, 
	   {0x2000, "count own store requests"}, 
	   {0x4000, "count other / DMA store requests"}, 
	   {0x8000, "count HW/SW prefetch requests"} } };

/* FSB_DATA_ACTIVITY */
static struct op_unit_mask um_fsb_data_activity =	 
	{6, utm_bitmask, 0x3f, 
	 { {0x01, "count when this processor drives data onto bus"}, 
	   {0x02, "count when this processor reads data from bus"}, 
	   {0x04, "count when data is on bus but not sampled by this processor"}, 
	   {0x08, "count when this processor reserves bus for driving"}, 
	   {0x10, "count when other reserves bus and this processor will sample"}, 
	   {0x20, "count when other reserves bus and this processor will not sample"} } };

/* BSQ_ALLOCATION */
/* BSQ_ACTIVE_ENTRIES */
static struct op_unit_mask um_bsq =	
	{13, utm_bitmask, 0x21, 
	 { {0x01, "(r)eq (t)ype (e)ncoding, bit 0: see next event"}, 
	   {0x02, "rte bit 1: 00=read, 01=read invalidate, 10=write, 11=writeback"}, 
	   {0x04, "req len bit 0"}, 
	   {0x08, "req len bit 1"}, 
	   {0x20, "request type is input (0=output)"}, 
	   {0x40, "request type is bus lock"}, 
	   {0x80, "request type is cacheable"},
	   {0x100, "request type is 8-byte chunk split across 8-byte boundary"}, 
	   {0x200, "request type is demand (0=prefetch)"}, 
	   {0x400, "request type is ordered"}, 
	   {0x800, "(m)emory (t)ype (e)ncoding, bit 0: see next events"}, 
	   {0x1000, "mte bit 1: see next event"}, 
	   {0x2000, "mte bit 2: 000=UC, 001=USWC, 100=WT, 101=WP, 110=WB"} } };

/* X87_ASSIST */
static struct op_unit_mask um_x87_assist =
	{5, utm_bitmask, 0x1f, 
	 { {0x01, "handle FP stack underflow"}, 
	   {0x02, "handle FP stack overflow"}, 
	   {0x04, "handle x87 output overflow"}, 
	   {0x08, "handle x87 output underflow"}, 
	   {0x10, "handle x87 input assist"} } };

/* SSE_INPUT_ASSIST */
/* {PACKED,SCALAR}_{SP,DP}_UOP */
/* {64,128}BIT_MMX_UOP */
/* X87_FP_UOP */
static struct op_unit_mask um_flame_uop =
	{1, utm_bitmask, 0x8000, 
	 { {0x8000, "count all uops of this type" } } };

/* X87_SIMD_MOVES_UOP */
static struct op_unit_mask um_x87_simd_moves_uop =
	{2, utm_bitmask, 0x18, 
	 { { 0x08, "count all x87 SIMD store/move uops"}, 
	   { 0x10, "count all x87 SIMD load uops"} } };

/* MACHINE_CLEAR */
static struct op_unit_mask um_machine_clear =
	{3, utm_bitmask, 0x1, 
	 { {0x01, "count a portion of cycles the machine is cleared for any cause"}, 
	   {0x40, "count cycles machine is cleared due to memory ordering issues"}, 
	   {0x80, "count cycles machine is cleared due to self modifying code"} } };

/* GLOBAL_POWER_EVENTS */
static struct op_unit_mask um_global_power_events =
	{1, utm_bitmask, 0x1, 
	 { {0x01, "count cycles when processor is active"} } };

/* TC_MS_XFER */
static struct op_unit_mask um_tc_ms_xfer =
	{1, utm_bitmask, 0x1, 
	 { {0x01, "count TC to MS transfers"} } };

/* UOP_QUEUE_WRITES */
static struct op_unit_mask um_uop_queue_writes =
	{3, utm_bitmask, 0x7, 
	 { {0x01, "count uops written to queue from TC build mode"}, 
	   {0x02, "count uops written to queue from TC deliver mode"}, 
	   {0x04, "count uops written to queue from microcode ROM" } } };

/* FRONT_END_EVENT */
static struct op_unit_mask um_front_end_event =
	{2, utm_bitmask, 0x1, 
	 { {0x01, "count marked uops which are non-bogus"}, 
	   {0x02, "count marked uops which are bogus"} } };

/* EXECUTION_EVENT */
static struct op_unit_mask um_execution_event =
	{8, utm_bitmask, 0x1, 
	 { {0x01, "count 1st marked uops which are non-bogus"}, 
	   {0x02, "count 2ns marked uops which are non-bogus"}, 
	   {0x04, "count 3rd marked uops which are non-bogus"}, 
	   {0x08, "count 4th marked uops which are non-bogus"}, 
	   {0x10, "count 1st marked uops which are bogus"}, 
	   {0x20, "count 2nd marked uops which are bogus"}, 
	   {0x40, "count 3rd marked uops which are bogus"}, 
	   {0x80, "count 4th marked uops which are bogus"} } };

/* REPLAY_EVENT */
static struct op_unit_mask um_replay_event =
	{2, utm_bitmask, 0x1, 
	 { {0x01, "count marked uops which are non-bogus"}, 
	   {0x02, "count marked uops which are bogus"} } };

/* INSTR_RETIRED */
static struct op_unit_mask um_instr_retired =
	{4, utm_bitmask, 0x1, 
	 { {0x01, "count non-bogus instructions which are not tagged"}, 
	   {0x02, "count non-bogus instructions which are tagged"}, 
	   {0x04, "count bogus instructions which are not tagged"}, 
	   {0x08, "count bogus instructions which are tagged"} } };

/* UOPS_RETIRED */
static struct op_unit_mask um_uops_retired =
	{2, utm_bitmask, 0x1, 
	 { {0x01, "count marked uops which are non-bogus"}, 
	   {0x02, "count marked uops which are bogus"} } };

/* UOP_TYPE */
static struct op_unit_mask um_uop_type =
	{2, utm_bitmask, 0x2, 
	 { {0x02, "count uops which are load operations"}, 
	   {0x04, "count uops which are store operations"} } };

/* RETIRED_MISPRED_BRANCH_TYPE */
/* RETIRED_BRANCH_TYPE */
static struct op_unit_mask um_branch_type =
	{4, utm_bitmask, 0x1e, 
	 { {0x02, "count conditional jumps"}, 
	   {0x04, "count indirect call branches"}, 
	   {0x08, "count return branches"}, 
	   {0x10, "count returns, indirect calls or indirect jumps"} } };



/* the following are just short cut for filling the table of event */
#define OP_RTC		(1 << CPU_RTC)
#define OP_ATHLON	(1 << CPU_ATHLON)
#define OP_PPRO		(1 << CPU_PPRO)
#define OP_PII		(1 << CPU_PII)
#define OP_PIII		(1 << CPU_PIII)
#define OP_PII_PIII	(OP_PII | OP_PIII)
#define OP_IA_ALL	(OP_PII_PIII | OP_PPRO)

#define OP_P4           (1 << CPU_P4)

#define CTR_0		(1 << 0)
#define CTR_1		(1 << 1)

/* the pentium 4 has a complex set of restrictions between its 18
   counters, so we simplify it a little and say there are 8 counters. these
   8 at least can be treated as entirely independent, although they can
   each only count certain classes of events. these defines are also
   present in module/x86/op_nmi.c. */

#define CTR_BPU_0      (1 << 0)
#define CTR_BPU_2      (1 << 1)
#define CTR_BPU_ALL    (CTR_BPU_0 | CTR_BPU_2)

#define CTR_MS_0       (1 << 2)
#define CTR_MS_2       (1 << 3)
#define CTR_MS_ALL     (CTR_MS_0 | CTR_MS_2)

#define CTR_FLAME_0    (1 << 4)
#define CTR_FLAME_2    (1 << 5)
#define CTR_FLAME_ALL  (CTR_FLAME_0 | CTR_FLAME_2)

#define CTR_IQ_4       (1 << 6)    /* #4 for compatibility with PEBS */
#define CTR_IQ_5       (1 << 7)
#define CTR_IQ_ALL     (CTR_IQ_4 | CTR_IQ_5)

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
  { CTR_ALL, OP_IA_ALL, 0x23, &um_empty, "L2_DBUS_BUSY_RD", 
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

  /* pentium 4 events */
  { CTR_IQ_ALL,    OP_P4, 0x01, &um_branch_retired, "BRANCH_RETIRED", 
    "retired branches", 3000},
  { CTR_IQ_ALL,    OP_P4, 0x02, &um_mispred_branch_retired, "MISPRED_BRANCH_RETIRED", 
    "retired mispredicted branches", 3000},
  { CTR_MS_ALL,    OP_P4, 0x03, &um_tc_deliver_mode, "TC_DELIVER_MODE", 
    "duration (in clock cycles) in the trace cache and decode engine", 3000},  
  { CTR_BPU_ALL,   OP_P4, 0x04, &um_bpu_fetch_request, "BPU_FETCH_REQUEST", 
    "instruction fetch requests from the branch predict unit", 3000},
  { CTR_BPU_ALL,   OP_P4, 0x05, &um_itlb_reference, "ITLB_REFERENCE", 
    "translations using the instruction translation lookaside buffer", 3000},
  { CTR_FLAME_ALL, OP_P4, 0x06, &um_memory_cancel, "MEMORY_CANCEL", 
    "cancelled requesets in data cache address control unit", 3000},
  { CTR_FLAME_ALL, OP_P4, 0x07, &um_memory_complete, "MEMORY_COMPLETE", 
    "completed load split, store split, uncacheable split, uncacheable load", 3000},
  { CTR_FLAME_ALL, OP_P4, 0x08, &um_load_port_replay, "LOAD_PORT_REPLAY", 
    "replayed events at the load port", 3000},
  { CTR_FLAME_ALL, OP_P4, 0x09, &um_store_port_replay, "STORE_PORT_REPLAY", 
    "replayed events at the store port", 3000},
  { CTR_BPU_ALL,   OP_P4, 0x0a, &um_mob_load_replay, "MOB_LOAD_REPLAY", 
    "replayed loads from the memory order buffer", 3000},
  { CTR_BPU_ALL,   OP_P4, 0x0b, &um_page_walk_type, "PAGE_WALK_TYPE", 
    "page walks by the page miss handler", 3000},
  { CTR_BPU_ALL,   OP_P4, 0x0c, &um_bsq_cache_reference, "BSQ_CACHE_REFERENCE", 
    "cache references seen by the bus unit", 3000},
  { CTR_BPU_0,     OP_P4, 0x0d, &um_ioq, "IOQ_ALLOCATION", 
    "bus transactions", 3000},
  { CTR_BPU_2,     OP_P4, 0x0e, &um_ioq, "IOQ_ACTIVE_ENTRIES", 
    "number of entries in the IOQ which are active", 3000},
  { CTR_BPU_ALL,   OP_P4, 0x0f, &um_fsb_data_activity, "FSB_DATA_ACTIVITY", 
    "DRDY or DBSY events on the front side bus", 3000},
  { CTR_BPU_0,     OP_P4, 0x10, &um_bsq, "BSQ_ALLOCATION", 
    "allocations in the bus sequence unit", 3000},
  { CTR_BPU_2,     OP_P4, 0x11, &um_bsq, "BSQ_ACTIVE_ENTRIES", 
    "number of entries in the bus sequence unit which are active", 3000},
  { CTR_IQ_ALL,    OP_P4, 0x12, &um_x87_assist, "X87_ASSIST", 
    "retired x87 instructions which required special handling", 3000},
  { CTR_FLAME_ALL, OP_P4, 0x13, &um_flame_uop, "SSE_INPUT_ASSIST", 
    "input assists requested for SSE or SSE2 operands", 3000},
  { CTR_FLAME_ALL, OP_P4, 0x14, &um_flame_uop, "PACKED_SP_UOP", 
    "packed single precision uops", 3000},
  { CTR_FLAME_ALL, OP_P4, 0x15, &um_flame_uop, "PACKED_DP_UOP", 
    "packed double precision uops", 3000},
  { CTR_FLAME_ALL, OP_P4, 0x16, &um_flame_uop, "SCALAR_SP_UOP", 
    "scalar single precision uops", 3000},
  { CTR_FLAME_ALL, OP_P4, 0x17, &um_flame_uop, "SCALAR_DP_UOP", 
    "scalar double presision uops", 3000},
  { CTR_FLAME_ALL, OP_P4, 0x18, &um_flame_uop, "64BIT_MMX_UOP", 
    "64 bit SIMD MMX instructions", 3000},
  { CTR_FLAME_ALL, OP_P4, 0x19, &um_flame_uop, "128BIT_MMX_UOP", 
    "128 bit SIMD SSE2 instructions", 3000},
  { CTR_FLAME_ALL, OP_P4, 0x1a, &um_flame_uop, "X87_FP_UOP", 
    "x87 floating point uops", 3000},
  { CTR_FLAME_ALL, OP_P4, 0x1b, &um_x87_simd_moves_uop, "X87_SIMD_MOVES_UOP", 
    "x87 FPU, MMX, SSE, or SSE2 loads, stores and reg-to-reg moves", 3000},
  { CTR_IQ_ALL,    OP_P4, 0x1c, &um_machine_clear, "MACHINE_CLEAR", 
    "cycles with entire machine pipeline cleared", 3000},
  { CTR_BPU_ALL,   OP_P4, 0x1d, &um_global_power_events, "GLOBAL_POWER_EVENTS", 
    "time during which processor is not stopped", 3000},
  { CTR_MS_ALL,    OP_P4, 0x1e, &um_tc_ms_xfer, "TC_MS_XFER", 
    "number of times uops deliver changed from TC to MS ROM", 3000},
  { CTR_MS_ALL,    OP_P4, 0x1f, &um_uop_queue_writes, "UOP_QUEUE_WRITES", 
    "number of valid uops written to the uop queue", 3000},
  { CTR_IQ_ALL,    OP_P4, 0x20, &um_front_end_event, "FRONT_END_EVENT", 
    "retired uops, tagged with front-end tagging", 3000},
  { CTR_IQ_ALL,    OP_P4, 0x21, &um_execution_event, "EXECUTION_EVENT", 
    "retired uops, tagged with execution tagging", 3000},
  { CTR_IQ_ALL,    OP_P4, 0x22, &um_replay_event, "REPLAY_EVENT", 
    "retired uops, tagged with replay tagging", 3000},
  { CTR_IQ_ALL,    OP_P4, 0x23, &um_instr_retired, "INSTR_RETIRED", 
    "retired instructions", 3000},
  { CTR_IQ_ALL,    OP_P4, 0x24, &um_uops_retired, "UOPS_RETIRED", 
    "retired uops", 3000},
  { CTR_IQ_ALL,    OP_P4, 0x25, &um_uop_type, "UOP_TYPE", 
    "type of uop tagged by front-end tagging", 3000},
  { CTR_MS_ALL,    OP_P4, 0x26, &um_branch_type, "RETIRED_MISPRED_BRANCH_TYPE", 
    "retired mispredicted branched, selected by type", 3000},
  { CTR_MS_ALL,    OP_P4, 0x27, &um_branch_type, "RETIRED_BRANCH_TYPE", 
    "retired branches, selected by type", 3000},

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
int op_check_unit_mask(struct op_unit_mask const * allow, u16 um)
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
 * are allowed. cpu_type should be set as follows :
 *
 * 0 Pentium Pro
 *
 * 1 Pentium II
 *
 * 2 Pentium III
 *
 * 3 AMD Athlon
 *
 * 6 Pentium 4 / Xeon
 *
 * The function returns bitmask of failure cause
 * 0 otherwise
 */
int op_check_events(int ctr, u8 ctr_type, u16 ctr_um, op_cpu cpu_type)
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
