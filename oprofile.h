/* $Id: oprofile.h,v 1.30 2001/01/21 01:11:55 moz Exp $ */
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

#include <linux/config.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/malloc.h>
#include <linux/delay.h>
#include <linux/vmalloc.h>
#include <linux/sched.h> 
#include <linux/sysctl.h> 

#include <asm/uaccess.h>
#include <asm/smplock.h>
#include <asm/apic.h>

/* userspace/module interface */
#include "op_user.h" 

#define OP_NR_ENTRY (SMP_CACHE_BYTES/sizeof(struct op_sample))

struct op_entry {
	struct op_sample samples[OP_NR_ENTRY];
} __attribute__((__aligned__(SMP_CACHE_BYTES)));

/* per-cpu dynamic data */
struct _oprof_data {
	struct op_entry *entries; /* hash table */
	struct op_sample *buffer; /* eviction buffer */
	uint hash_size; /* nr. in hash table */
	uint buf_size; /* nr. in buffer */
	uint ctr_count[2]; /* reset counter values */
	uint nextbuf; /* next in buffer (atomic) */
	u16 next; /* next sample in entry */
	u16 ctrs; /* which counters are set */
} __attribute__((__aligned__(SMP_CACHE_BYTES)));

#define OP_CTR_0 0x1
#define OP_CTR_1 0x2

/* MSRs */
#ifndef MSR_IA32_PERFCTR0
#define MSR_IA32_PERFCTR0 0xc1
#endif
#ifndef MSR_IA32_PERFCTR1
#define MSR_IA32_PERFCTR1 0xc2
#endif
#ifndef MSR_IA32_EVNTSEL0
#define MSR_IA32_EVNTSEL0 0x186
#endif
#ifndef MSR_IA32_EVNTSEL1
#define MSR_IA32_EVNTSEL1 0x187
#endif
#ifndef MSR_IA32_APICBASE
#define MSR_IA32_APICBASE 0x1B
#endif

#ifndef APIC_SPIV_APIC_ENABLED
#define APIC_SPIV_APIC_ENABLED (1<<8)
#endif

#define streqn(a, b, len) (!strncmp((a), (b), (len)))

/* oprof_data->ready will be set this many samples
 * before the end of the eviction buffer
 */
#define OP_PRE_WATERMARK 256

/* maximal value before eviction */
#define OP_MAX_COUNT ((1U<<(16U-OP_BITS))-1U)

/* maximum number of events between
 * interrupts. Counters are 40 bits, but
 * for convenience we only use 32 bits.
 * The top bit is used for overflow detection,
 * so user can set up to (2^31)-1 */
#define OP_MAX_PERF_COUNT 2147483647UL

/* relying on MSR numbers being neighbours */
#define get_perfctr(l,h,c) do { rdmsr(MSR_IA32_PERFCTR0+c, (l),(h)); } while (0)
#define set_perfctr(l,c) do { wrmsr(MSR_IA32_PERFCTR0+c, -(u32)(l), 0); } while (0)
#define ctr_overflowed(n) (!((n) & (1U<<31)))

#define OP_EVENTS_OK            0x0
#define OP_CTR0_NOT_FOUND       0x1
#define OP_CTR1_NOT_FOUND       0x2
#define OP_CTR0_NO_UM           0x4
#define OP_CTR1_NO_UM           0x8
#define OP_CTR0_NOT_ALLOWED     0x10
#define OP_CTR1_NOT_ALLOWED     0x20
#define OP_CTR0_PII_EVENT       0x40
#define OP_CTR1_PII_EVENT       0x80
#define OP_CTR0_PIII_EVENT      0x100
#define OP_CTR1_PIII_EVENT      0x200

#define op_check_range(val,l,h,str) do { \
        if ((val) < (l) || (val) > (h)) { \
                printk(str,(val)); \
                return 0; \
        } } while (0);

asmlinkage void op_nmi(void);

/* for installing and restoring the NMI handler */ 
 
#define store_idt(addr) \
	do { \
		__asm__ __volatile__ ( "sidt %0" \
			: "=m" (addr) \
			: : "memory" ); \
	} while (0)

#define _set_gate(gate_addr,type,dpl,addr) \
	do { \
		int __d0, __d1; \
		__asm__ __volatile__ ( \
			"movw %%dx,%%ax\n\t" \
			"movw %4,%%dx\n\t" \
			"movl %%eax,%0\n\t" \
			"movl %%edx,%1" \
			:"=m" (*((long *) (gate_addr))), \
			"=m" (*(1+(long *) (gate_addr))), "=&a" (__d0), "=&d" (__d1) \
			:"i" ((short) (0x8000+(dpl<<13)+(type<<8))), \
			"3" ((char *) (addr)),"2" (__KERNEL_CS << 16)); \
	} while (0)

struct _descr { u16 limit; u32 base; } __attribute__((__packed__));
struct _idt_descr { u32 a; u32 b; } __attribute__((__packed__));

/* If the do_nmi() patch has been applied, we can use the NMI watchdog */
#ifdef OP_EXPORTED_DO_NMI
void do_nmi(struct pt_regs * regs, long error_code);
#define DO_NMI(r,e) do { do_nmi((r),(e)); } while (0)
#else
void my_do_nmi(struct pt_regs * regs, long error_code);
#define DO_NMI(r,e) do { my_do_nmi((r),(e)); } while (0)
#endif

void my_set_fixmap(void);
int op_check_events(u8 ctr0_type, u8 ctr1_type, u8 ctr0_um, u8 ctr1_um, int proc);
void op_intercept_syscalls(void);
void op_replace_syscalls(void);
int is_map_ready(void); 
int oprof_hash_map_open(void);
int oprof_hash_map_release(void);
int oprof_hash_map_mmap(struct file *file, struct vm_area_struct *vma);
int oprof_map_open(void);
int oprof_map_release(void);
int oprof_map_read(char *buf, size_t count, loff_t *ppos);
int oprof_init_hashmap(void);
void oprof_free_hashmap(void);
