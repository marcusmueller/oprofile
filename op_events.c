/* $Id: op_events.c,v 1.6 2000/11/13 01:13:35 moz Exp $ */ 

/* Adapted from libpperf 0.5 by M. Patrick Goda and Michael S. Warren */

/* See IA32 Vol. 3 Appendix A */ 
 
/* for allowed */
#define OP_0_ONLY 	0
#define OP_1_ONLY 	1
#define OP_ANY	 	2
#define OP_PII_PIII	3 
#define OP_PII_ONLY	4
#define OP_PIII_ONLY	5
 
#ifdef __KERNEL__
#include <linux/string.h> 
#define strcmp(a,b) strnicmp((a),(b),strlen((b))) 
#else
#include <stdio.h> 
#include <string.h>
#endif 
 
#define u8 unsigned char 
#define uint unsigned int
 
struct op_event {
	uint allowed;
	u8 val; /* event number */ 
	u8 unit; /* which unit mask if any allowed */
	const char *name;
}; 

struct op_unit_mask {
	uint num; /* number of possible unit masks */ 
	/* up to six allowed unit masks */
	u8 um[6]; 
};

int op_check_events_str(char *ctr0_type, char *ctr1_type, u8 ctr0_um, u8 ctr1_um, int p2, u8 *ctr0_t, u8 *ctr1_t);
int op_check_events(u8 ctr0_type, u8 ctr1_type, u8 ctr0_um, u8 ctr1_um, int proc);
#ifdef OP_EVENTS_DESC 
void op_get_event_desc(u8 type, u8 um, char **typenamep, char **typedescp, char **umdescp); 
#endif 
 
static struct op_unit_mask op_unit_masks[] = {
	/* not used */
	{ 1, { 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 }, },
	/* MESI counters */ 
	{ 5, { 0x1, 0x2, 0x4, 0x8, 0xf, 0x0 }, },
	/* EBL self/any */
	{ 2, { 0x0, 0x20, 0x0, 0x0, 0x0, 0x0 }, },
	/* MMX PII events */
	{ 1, { 0xf, 0x0, 0x0, 0x0, 0x0, 0x0 }, },
	{ 6, { 0x1, 0x2, 0x4, 0x8, 0x10, 0x20 }, },
	{ 2, { 0x0, 0x1, 0x0, 0x0, 0x0, 0x0 }, },
	{ 5, { 0x1, 0x2, 0x4, 0x8, 0xf, 0x0 }, },
	/* KNI PIII events */
	{ 4, { 0x0, 0x1, 0x2, 0x3, 0x0, 0x0 }, },
	{ 2, { 0x0, 0x1, 0x0, 0x0, 0x0, 0x0 }, },
}; 
 
static struct op_event op_events[] = { 
  /* Data Cache Unit (DCU) */
  {OP_ANY,0x43,0,"DATA_MEM_REFS",},
  {OP_ANY,0x45,0,"DCU_LINES_IN",},
  {OP_ANY,0x46,0,"DCU_M_LINES_IN",},
  {OP_ANY,0x47,0,"DCU_M_LINES_OUT",},
  {OP_ANY,0x48,0,"DCU_MISS_OUTSTANDING",},
  /* Intruction Fetch Unit (IFU) */
  {OP_ANY,0x80,0,"IFU_IFETCH",},
  {OP_ANY,0x81,0,"IFU_IFETCH_MISS",},
  {OP_ANY,0x85,0,"ITLB_MISS",},
  {OP_ANY,0x86,0,"IFU_MEM_STALL",},
  {OP_ANY,0x87,0,"ILD_STALL",},
  /* L2 Cache */
  {OP_ANY,0x28,1,"L2_IFETCH",},
  {OP_ANY,0x29,1,"L2_LD",},
  {OP_ANY,0x2a,1,"L2_ST",},
  {OP_ANY,0x24,0,"L2_LINES_IN",},
  {OP_ANY,0x26,0,"L2_LINES_OUT",},
  {OP_ANY,0x25,0,"L2_M_LINES_INM",},
  {OP_ANY,0x27,0,"L2_M_LINES_OUTM",},
  {OP_ANY,0x2e,1,"L2_RQSTS",},
  {OP_ANY,0x21,0,"L2_ADS",},
  {OP_ANY,0x22,0,"L2_DBUS_BUSY",},
  {OP_ANY,0x23,0,"L2_DMUS_BUSY_RD",},
  /* External Bus Logic (EBL) */
  {OP_ANY,0x62,2,"BUS_DRDY_CLOCKS",},
  {OP_ANY,0x63,2,"BUS_LOCK_CLOCKS",},
  {OP_ANY,0x60,0,"BUS_REQ_OUTSTANDING",},
  {OP_ANY,0x65,2,"BUS_TRAN_BRD",},
  {OP_ANY,0x66,2,"BUS_TRAN_RFO",},
  {OP_ANY,0x67,2,"BUS_TRANS_WB",},
  {OP_ANY,0x68,2,"BUS_TRAN_IFETCH",},
  {OP_ANY,0x69,2,"BUS_TRAN_INVAL",},
  {OP_ANY,0x6a,2,"BUS_TRAN_PWR",},
  {OP_ANY,0x6b,2,"BUS_TRANS_P",},
  {OP_ANY,0x6c,2,"BUS_TRANS_IO",},
  {OP_ANY,0x6d,2,"BUS_TRANS_DEF",},
  {OP_ANY,0x6e,2,"BUS_TRAN_BURST",},
  {OP_ANY,0x70,2,"BUS_TRAN_ANY",},
  {OP_ANY,0x6f,2,"BUS_TRAN_MEM",},
  {OP_ANY,0x64,0,"BUS_DATA_RCV",},
  {OP_ANY,0x61,0,"BUS_BNR_DRV",},
  {OP_ANY,0x7a,0,"BUS_HIT_DRV",},
  {OP_ANY,0x7b,0,"BUS_HITM_DRV",},
  {OP_ANY,0x7e,0,"BUS_SNOOP_STALL",},
  /* Floating Point Unit (FPU) */
  {OP_0_ONLY,0xc1,0,"COMP_FLOP_RET",},
  {OP_0_ONLY,0x10,0,"FLOPS",},
  {OP_1_ONLY,0x11,0,"FP_ASSIST",},
  {OP_1_ONLY,0x12,0,"MUL",},
  {OP_1_ONLY,0x13,0,"DIV",},
  {OP_0_ONLY,0x14,0,"CYCLES_DIV_BUSY",},
  /* Memory Ordering */
  {OP_ANY,0x03,0,"LD_BLOCKS",},
  {OP_ANY,0x04,0,"SB_DRAINS",},
  {OP_ANY,0x05,0,"MISALIGN_MEM_REF",},
  /* PIII KNI */
  {OP_PIII_ONLY,0x07,7,"EMON_KNI_PREF_DISPATCHED",},
  {OP_PIII_ONLY,0x4b,7,"EMON_KNI_PREF_MISS",},
  /* Instruction Decoding and Retirement */
  {OP_ANY,0xc0,0,"INST_RETIRED",},
  {OP_ANY,0xc2,0,"UOPS_RETIRED",},
  {OP_ANY,0xd0,0,"INST_DECODED",},
  /* PIII KNI */
  {OP_PIII_ONLY,0xd8,8,"EMON_KNI_INST_RETIRED",},
  {OP_PIII_ONLY,0xd9,8,"EMON_KNI_COMP_INST_RET",},
  /* Interrupts */
  {OP_ANY,0xc8,0,"HW_INT_RX",},
  {OP_ANY,0xc6,0,"CYCLES_INT_MASKED",},
  {OP_ANY,0xc7,0,"CYCLES_INT_PENDING_AND_MASKED",},
  /* Branches */
  {OP_ANY,0xc4,0,"BR_INST_RETIRED",},
  {OP_ANY,0xc5,0,"BR_MISS_PRED_RETIRED",},
  {OP_ANY,0xc9,0,"BR_TAKEN_RETIRED",},
  {OP_ANY,0xca,0,"BR_MISS_PRED_TAKEN_RET",},
  {OP_ANY,0xe0,0,"BR_INST_DECODED",},
  {OP_ANY,0xe2,0,"BTB_MISSES",},
  {OP_ANY,0xe4,0,"BR_BOGUS",},
  {OP_ANY,0xe6,0,"BACLEARS",},
  /* Stalls */
  {OP_ANY,0xa2,0,"RESOURCE_STALLS",},
  {OP_ANY,0xd2,0,"PARTIAL_RAT_STALLS",},
  /* Segment Register Loads */
  {OP_ANY,0x06,0,"SEGMENT_REG_LOADS",},
  /* Clocks */
  {OP_ANY,0x79,0,"CPU_CLK_UNHALTED",},
  /* MMX (Pentium II only) */
  {OP_PII_ONLY,0xb0,0,"MMX_INSTR_EXEC",},
  {OP_PII_PIII,0xb1,0,"MMX_SAT_INSTR_EXEC",},
  {OP_PII_PIII,0xb2,3,"MMX_UOPS_EXEC",},
  {OP_PII_PIII,0xb3,4,"MMX_INSTR_TYPE_EXEC",}, 
  {OP_PII_PIII,0xcc,5,"FP_MMX_TRANS",}, 
  {OP_PII_PIII,0xcd,0,"MMX_ASSIST",},
  {OP_PII_ONLY,0xce,0,"MMX_INSTR_RET",},
  /* segment renaming (Pentium II only) */ 
  {OP_PII_PIII,0xd4,6,"SEG_RENAME_STALLS",},
  {OP_PII_PIII,0xd5,6,"SEG_REG_RENAMES",},
  {OP_PII_PIII,0xd6,0,"RET_SEG_RENAMES",},
}; 

uint op_nr_events = sizeof(op_events)/sizeof(struct op_event); 
 
#define OP_EVENTS_OK 		0x0
#define OP_CTR0_NOT_FOUND 	0x1
#define OP_CTR1_NOT_FOUND 	0x2
#define OP_CTR0_NO_UM 		0x4
#define OP_CTR1_NO_UM 		0x8
#define OP_CTR0_NOT_ALLOWED 	0x10
#define OP_CTR1_NOT_ALLOWED 	0x20
#define OP_CTR0_PII_EVENT	0x40 
#define OP_CTR1_PII_EVENT	0x80 
#define OP_CTR0_PIII_EVENT	0x100 
#define OP_CTR1_PIII_EVENT	0x200 
 
/**
 * op_check_unit_mask - sanity check unit mask value
 * @allow: allowed unit mask array
 * @um: unit mask value to check
 *
 * Verify that a unit mask value is within the allowed array.
 */ 
static int op_check_unit_mask(struct op_unit_mask *allow, u8 um)
{
	u8 i;

	for (i=0; i < allow->num; i++) {
		if (allow->um[i]==um)
			return 0;
	}

	return 1; 
}
 
/**
 * op_check_events - sanity check event values
 * @ctr0_type: event value for counter 0
 * @ctr1_type: event value for counter 1
 * @ctr0_um: unit mask for counter 0
 * @ctr1_um: unit mask for counter 1
 * @proc: processor type
 *
 * Check that the counter event and unit mask values
 * are allowed. @proc should be set as follows :
 * 
 * 0 Pentium Pro
 * 
 * 1 Pentium II
 * 
 * 2 Pentium III
 * 
 * Use 0 values for @ctr0_type and @ctr1_type if the
 * counter is not used.
 * 
 * The function returns 1 if the values are allowed,
 * 0 otherwise
 */
int op_check_events(u8 ctr0_type, u8 ctr1_type, u8 ctr0_um, u8 ctr1_um, int proc)
{
	int ret = 0x0; 
	uint i;
	int ctr0_e=0,ctr1_e=0;

	for (i=0; i<op_nr_events && !ctr0_e && !ctr1_e; i++) {
		if (op_events[i].val==ctr0_type) {
			switch (op_events[i].allowed) {
				case OP_1_ONLY:
					ret |= OP_CTR0_NOT_ALLOWED;
					break;
				
				case OP_PII_ONLY:
					if (proc!=1)
						ret |= OP_CTR0_PII_EVENT;
					break;
		
				case OP_PIII_ONLY:
					if (proc!=2)
						ret |= OP_CTR0_PIII_EVENT;
					break;

				case OP_PII_PIII:
					if (!proc)
						ret |= OP_CTR0_PII_EVENT;
					break;
				default:
					break;
			}
			if (op_events[i].unit)
				ret |= OP_CTR0_NO_UM*op_check_unit_mask(&op_unit_masks[op_events[i].unit],ctr0_um);
			ctr0_e=1; 
		} 
		if (op_events[i].val==ctr1_type) {
			switch (op_events[i].allowed) {
				case OP_0_ONLY:
					ret |= OP_CTR1_NOT_ALLOWED;
					break;
				
				case OP_PII_ONLY:
					if (proc!=1)
						ret |= OP_CTR1_PII_EVENT;
					break;
		
				case OP_PIII_ONLY:
					if (proc!=2)
						ret |= OP_CTR1_PIII_EVENT;
					break;

				case OP_PII_PIII:
					if (!proc)
						ret |= OP_CTR1_PII_EVENT;
					break;
				default:
					break;
			}
			if (op_events[i].unit)
				ret |= OP_CTR1_NO_UM*op_check_unit_mask(&op_unit_masks[op_events[i].unit],ctr1_um);
			ctr1_e=1; 
		} 
	}

	if (!ctr0_e && ctr0_type)
		ret |= OP_CTR0_NOT_FOUND;
	if (!ctr1_e && ctr1_type)
		ret |= OP_CTR1_NOT_FOUND;

	return ret; 
} 

/**
 * op_check_events_str - sanity check event strings and unit masks
 * @ctr0_type: event name for counter 0
 * @ctr1_type: event name for counter 1
 * @ctr0_um: unit mask for counter 0
 * @ctr1_um: unit mask for counter 1
 * @p2: processor type
 * @ctr0_t: event value for counter 0
 * @ctr1_t: event value for counter 1
 *
 * Check that the counter event and unit mask values
 * are allowed. @p2 should be set as follows :
 *
 * 0 Pentium Pro
 * 
 * 1 Pentium II
 * 
 * 2 Pentium III
 * 
 * Use "" strings for @ctr0_type and @ctr1_type if the
 * counter is not used.
 * 
 * On successful return, @ctr0_t and @ctr1_t will contain
 * the event values for counters 0 and 1 respectively
 * 
 * The function returns 1 if the values are allowed,
 * 0 otherwise
 */
int op_check_events_str(char *ctr0_type, char *ctr1_type, u8 ctr0_um, u8 ctr1_um, int p2, u8 *ctr0_t, u8 *ctr1_t)
{
	uint i;
	int ctr0_e=0;
	int ctr1_e=0;

	if (!ctr0_type)
		ctr0_type=""; 
	if (!ctr1_type)
		ctr1_type=""; 
 
	for (i=0; i<op_nr_events && !ctr0_e && !ctr1_e; i++) {
		if (!strcmp(ctr0_type,op_events[i].name)) {
			ctr0_e=1;
			*ctr0_t=op_events[i].val;
		}
		if (!strcmp(ctr1_type,op_events[i].name)) {
			ctr1_e=1;
			*ctr1_t=op_events[i].val;
		}
	}

	if (strcmp(ctr0_type,"") && !ctr0_e)
		return OP_CTR0_NOT_FOUND;

	if (strcmp(ctr1_type,"") && !ctr1_e)
		return OP_CTR1_NOT_FOUND;

	return op_check_events(*ctr0_t,*ctr1_t,ctr0_um,ctr1_um,p2);
}

#ifdef OP_EVENTS_DESC 
struct op_unit_desc {
	char *desc[6];
};
 
static struct op_unit_desc op_unit_descs[] = {
	{ { NULL, NULL, NULL, NULL, NULL, NULL, }, },
	{ { "(I)nvalid cache state",
	  "(S)hared cache state",
	  "(E)xclusive cache state",
	  "(M)odified cache state",
	  "MESI cache state", NULL, }, },
	{ { "self-generated transactions", 
	  "any transactions", NULL, NULL, NULL, NULL, }, },
	{ { "mandatory", NULL, NULL, NULL, NULL, NULL, }, },
	{ { "MMX packed multiplies",
	  "MMX packed shifts",
	  "MMX pack operations",
	  "MMX unpack operations",
	  "MMX packed logical",
	  "MMX packed arithmetic", }, },
	{ { "transitions from MMX to floating point",
	  "transitions from floating point to MMX",
	  NULL, NULL, NULL, NULL, }, },
	{ { "ES register",
	  "DS register",
	  "FS register",
	/* IA manual says this is actually FS again - no mention in errata */
	  "FS register",
	  "ES,DS,FS,GS registers", NULL, }, },
	{ { "prefetch NTA",
	  "prefetch T1",
	  "prefetch T2",
	  "weakly ordered stores", NULL, NULL, }, },
	{ { "packed and scalar", "packed", NULL, NULL, NULL, NULL, }, },

};
 
static char *op_event_descs[] = {
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
  "clocks processor is not halted",
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
};

/**
 * op_get_event_desc - get event name and description
 * @type: event value
 * @um: unit mask
 * @typenamep: returned event name string 
 * @typedescp: returned event description string
 * @umdescp: returned unit mask description string
 *
 * Get the associated event name and descriptions given
 * the event value and unit mask value. It is a fatal error
 * to supply a non-valid @type value, but an invalid @um
 * will not exit.
 * 
 * @typenamep, @typedescp, @umdescp are filled in with pointers
 * to the relevant name and descriptions. @umdescp can be set to 
 * NULL when @um is invalid for the given @type value.
 * These strings are in text section so should not be freed.
 */ 
void op_get_event_desc(u8 type, u8 um, char **typenamep, char **typedescp, char **umdescp) 
{
	uint i;
	uint j;
	
	*typenamep = *typedescp = *umdescp = NULL;

	for (i=0; i < op_nr_events; i++) {
		if (op_events[i].val == type) {
			*typenamep = (char *)op_events[i].name;
			*typedescp = op_event_descs[i];
			if (op_events[i].unit) { 
				for (j=0; j < op_unit_masks[op_events[i].unit].num; j++) {
					if (op_unit_masks[op_events[i].unit].um[j]==um)
						*umdescp = op_unit_descs[op_events[i].unit].desc[j];
				} 
			}
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
 
int main (int argc, char *argv[])
{
	uint i;
	uint j;

	if (argc>1 && (!strcmp(argv[1],"--version") || !strcmp(argv[1],"-v"))) {
		printf(VERSION_STRING " compiled on " __DATE__ " " __TIME__ "\n");
		exit(0);
	}
 
	printf("oprofile: available events\n");
	printf("--------------------------\n\n");
	printf("See Intel Architecture Developer's Manual\nVol. 3, Appendix A\n\n"); 

	for (i=0; i<op_nr_events; i++) {
		printf("%s:",op_events[i].name);
		printf("\n\t%s ",op_event_descs[i]);
		switch (op_events[i].allowed) {
			case 0: printf("- counter 0 only\n"); break;
			case 1: printf("- counter 1 only\n"); break;
			case 3: printf("- Pentium II/III only\n"); break;
			case 4: printf("- Pentium II only\n"); break;
			case 5: printf("- Pentium III only\n"); break;
			default: printf("\n"); break; 
		} 
		if (op_events[i].unit) {
			printf("\tUnit masks\n");
			printf("\t----------\n");
			for (j=0; j<op_unit_masks[op_events[i].unit].num; j++) {
				printf("\t%.2x: %s\n",
					op_unit_masks[op_events[i].unit].um[j],
					op_unit_descs[op_events[i].unit].desc[j]);
			}
		}
	}

	return 0; 
} 
#endif /* OP_EVENTS_MAIN */
