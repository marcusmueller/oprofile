/* oprofile.h */
/* continuous profiling module for Linux 2.3 */
/* John Levon (moz@compsoc.man.ac.uk) */
/* May 2000 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h> 
#include <linux/malloc.h> 
#include <linux/poll.h> 
#include <linux/delay.h> 
 
#include <asm/smplock.h> 
#include <asm/apic.h>
 
struct op_sample {
	u16 count;
	u16 pid;
	u32 eip;
} __attribute__((__packed__,__aligned__(8)));

#define OP_NR_ENTRY (SMP_CACHE_BYTES/sizeof(struct op_sample)) 

struct op_entry {
	struct op_sample samples[OP_NR_ENTRY];
} __attribute__((__aligned__(SMP_CACHE_BYTES)));
 
/* per-cpu dynamic data */ 
struct _oprof_data {
	struct op_entry *entries;
	struct op_sample *buffer;
	unsigned int hash_size;
	unsigned int buf_size;
	unsigned int ctr_count[2];
	unsigned int nextbuf;
	u8 next;
	u8 ctrs;
} __attribute__((__aligned__(SMP_CACHE_BYTES)));
 
 
#define OP_CTR_0 0x1
#define OP_CTR_1 0x2
 
/* MSRs */ 
#define P6_MSR_PERFCTR0 0xc1
#define P6_MSR_PERFCTR1 0xc2
#define P6_MSR_EVNTSEL0 0x186 
#define P6_MSR_EVNTSEL1 0x187 
#define MSR_APIC_BASE   0x1B
 
#define OP_BITS 2
/* 1==mapping info, 0 otherwise */ 
#define OP_MAPPING (1U<<15) 
/* 1==PERFCTR1, 0==PERFCTR0 */ 
#define OP_COUNTER (1U<<14)
 
/* fork(),vfork(),clone() */
#define OP_FORK ((1U<<15)|(1U<<0))
/* execve() */
#define OP_DROP ((1U<<15)|(1U<<1))
/* mapping */
#define OP_MAP ((1U<<15)|(1U<<2))
/* init_module() */
#define OP_DROP_MODULES ((1U<<15)|(1U<<3))
/* exit() */ 
#define OP_EXIT ((1U<<15)|(1U<<4))

struct op_map {
	u32 addr;
	u32 len;
	u32 offset;
	char path[4];
} __attribute__((__packed__,__aligned__(16)));
 
/* size of map buffer: 8K */
#define OP_MAX_MAP_BUF 512

/* maximal value before eviction */ 
#define OP_MAX_COUNT ((1U<<(16U-OP_BITS))-1U)

/* maximum number of events between
 * interrupts. Counters are 40 bits, but 
 * for convenience we only use 32 bits.
 * The top bit is used for overflow detection,
 * so user can set up to (2^31)-1 */
#define OP_MAX_PERF_COUNT 2147483647UL
 
/* relying on MSR numbers being neighbours */ 
#define get_perfctr(l,h,c) do { rdmsr(P6_MSR_PERFCTR0+c, (l),(h)); } while (0)
#define set_perfctr(l,c) do { wrmsr(P6_MSR_PERFCTR0+c, -(u32)(l), 0); } while (0)
#define ctr_overflowed(n) (!((n) & (1U<<31)))                                                                                                
 
asmlinkage void op_nmi(void);
unsigned long idt_addr; 
unsigned long kernel_nmi; 
 
/* If the do_nmi() patch has been applied, we can use the NMI watchdog */ 
#ifdef OP_EXPORTED_DO_NMI
void do_nmi(struct pt_regs * regs, long error_code);
#define DO_NMI(r,e) do { do_nmi((r),(e)); } while (0)
#else 
void my_do_nmi(struct pt_regs * regs, long error_code);
#define DO_NMI(r,e) do { my_do_nmi((r),(e)); } while (0)
#endif
 
void my_set_fixmap(void);
int op_check_events_str(char *ctr0_type, char *ctr1_type, u8 ctr0_um, u8 ctr1_um, int proc, u8 *ctr0_t, u8 *ctr1_t);
void op_intercept_syscalls(void);
void op_replace_syscalls(void);
int oprof_map_open(void);
int oprof_map_release(void);
int oprof_map_read(char *buf, size_t count, loff_t *ppos);
