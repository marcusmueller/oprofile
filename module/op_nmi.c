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

#include "oprofile.h"

/* the MSRs we need */
static uint perfctr_msr[OP_MAX_COUNTERS];
static uint eventsel_msr[OP_MAX_COUNTERS];

/* number of counters physically present */
static uint op_nr_counters = 2;

/* whether we enable for each counter (athlon) or globally (intel) */
static int separate_running_bit;

/* ---------------- NMI handler ------------------ */

static void op_check_ctr(uint cpu, struct pt_regs *regs, int ctr)
{
	ulong l,h;
	get_perfctr(l, h, ctr);
	if (ctr_overflowed(l)) {
		op_do_profile(cpu, regs, ctr);
		set_perfctr(oprof_data[cpu].ctr_count[ctr], ctr);
	}
}

asmlinkage void op_do_nmi(struct pt_regs *regs)
{
	uint cpu = op_cpu_id();
	int i;

	for (i = 0 ; i < op_nr_counters ; ++i)
		op_check_ctr(cpu, regs, i);
}

/* ---------------- PMC setup ------------------ */

static void pmc_fill_in(uint *val, u8 kernel, u8 user, u8 event, u8 um)
{
	/* enable interrupt generation */
	*val |= (1<<20);
	/* enable/disable chosen OS and USR counting */
	(user)   ? (*val |= (1<<16))
		 : (*val &= ~(1<<16));

	(kernel) ? (*val |= (1<<17))
		 : (*val &= ~(1<<17));

	/* what are we counting ? */
	*val |= event;
	*val |= (um<<8);
}

static void pmc_setup(void *dummy)
{
	uint low, high;
	int i;

	/* IA Vol. 3 Figure 15-3 */

	/* Stop and clear all counter: IA32 use bit 22 of eventsel_msr0 to
	 * enable/disable all counter, AMD use separate bit 22 in each msr,
	 * all bits are cleared except the reserved bits 21 */
	for (i = 0 ; i < op_nr_counters ; ++i) {
		rdmsr(eventsel_msr[i], low, high);
		wrmsr(eventsel_msr[i], low & (1 << 21), high);

		/* avoid a false detection of ctr overflow in NMI handler */
		wrmsr(perfctr_msr[i], -1, -1);
	}

	/* setup each counter */
	for (i = 0 ; i < op_nr_counters ; ++i) {
		if (sysctl.ctr[i].event) {
			rdmsr(eventsel_msr[i], low, high);

			low &= 1 << 21;  /* do not touch the reserved bit */
			set_perfctr(sysctl.ctr[i].count, i);

			pmc_fill_in(&low, sysctl.ctr[i].kernel, sysctl.ctr[i].user,
				sysctl.ctr[i].event, sysctl.ctr[i].unit_mask);

			wrmsr(eventsel_msr[i], low, high);
		}
	}
	
	/* Here all setup is made except the start/stop bit 22, counter
	 * disabled contains zeros in the eventsel msr except the reserved bit
	 * 21 */
}

static int pmc_setup_all(void)
{
	if ((smp_call_function(pmc_setup, NULL, 0, 1)))
		return -EFAULT;

	pmc_setup(NULL);
	return 0;
}

inline static void pmc_start_P6(void)
{
	uint low,high;

	rdmsr(eventsel_msr[0], low, high);
	wrmsr(eventsel_msr[0], low | (1 << 22), high);
}

inline static void pmc_start_Athlon(void)
{
	uint low,high;
	int i;

	for (i = 0 ; i < op_nr_counters ; ++i) {
		if (sysctl.ctr[i].count) {
			rdmsr(eventsel_msr[i], low, high);
			wrmsr(eventsel_msr[i], low | (1 << 22), high);
		}
	}
}

static void pmc_start(void *info)
{
	if (info && (*((uint *)info) != op_cpu_id()))
		return;

	/* assert: all enable counter are setup except the bit start/stop,
	 * all counter disable contains zeroes (except perhaps the reserved
	 * bit 21), counter disable contains -1 sign extended in msr count */

	/* enable all needed counter */
	if (separate_running_bit == 0)
		pmc_start_P6();
	else
		pmc_start_Athlon();
}

inline static void pmc_stop_P6(void)
{
	uint low,high;

	rdmsr(eventsel_msr[0], low, high);
	wrmsr(eventsel_msr[0], low & ~(1 << 22), high);
}

inline static void pmc_stop_Athlon(void)
{
	uint low,high;
	int i;

	for (i = 0 ; i < op_nr_counters ; ++i) {
		if (sysctl.ctr[i].count) {
			rdmsr(eventsel_msr[i], low, high);
			wrmsr(eventsel_msr[i], low & ~(1 << 22), high);
		}
	}
}

static void pmc_stop(void *info)
{
	if (info && (*((uint *)info) != op_cpu_id()))
		return;

	/* disable counters */
	if (separate_running_bit == 0)
		pmc_stop_P6();
	else
		pmc_stop_Athlon();
}

static void pmc_select_start(uint cpu)
{
	/* we must make sure not to re-enable the counters
	 * after a dump_stop
	 */
	if (partial_stop)
		return;

	if (cpu == op_cpu_id())
		pmc_start(NULL);
	else
		smp_call_function(pmc_start, &cpu, 0, 1);
}

static void pmc_select_stop(uint cpu)
{
	if (partial_stop)
		return;

	if (cpu == op_cpu_id())
		pmc_stop(NULL);
	else
		smp_call_function(pmc_stop, &cpu, 0, 1);
}

static void pmc_start_all(void)
{
	int cpu, i;
 
	for (cpu=0; cpu < smp_num_cpus; cpu++) {
		struct _oprof_data * data = &oprof_data[cpu];

		for (i = 0 ; i < op_nr_counters ; ++i) {
			if (sysctl.ctr[i].enabled)
				data->ctr_count[i] = sysctl.ctr[i].count;
			else
				data->ctr_count[i] = 0;
		}
	}
 
	install_nmi();
	smp_call_function(pmc_start, NULL, 0, 1);
	pmc_start(NULL);
}

static void pmc_stop_all(void)
{
	smp_call_function(pmc_stop, NULL, 0, 1);
	pmc_stop(NULL);
	restore_nmi();
}
 
static int pmc_check_params(void)
{
	int i;
	int enabled = 0;
	int ok = 0;
 
	for (i = 0; i < op_nr_counters ; i++) {
 
		if (sysctl.ctr[i].enabled) {
			int min_count = op_min_count(sysctl.ctr[i].event, sysctl.cpu_type);

			if (!sysctl.ctr[i].user && !sysctl.ctr[i].kernel) {
				printk(KERN_ERR "oprofile: neither kernel nor user "
					"set for counter %d\n", i);
				return 0;
			}
			op_check_range(sysctl.ctr[i].count, min_count,
				OP_MAX_PERF_COUNT,
				"ctr count value %d not in range (%d %ld)\n");

			enabled = 1;
		}
	}

	if (!enabled) {
		printk(KERN_ERR "oprofile: no counters have been enabled.\n");
		return -EINVAL;
	}

	for (i = 0 ; i < op_nr_counters ; ++i) {
		int ret = op_check_events(i, sysctl.ctr[i].event, sysctl.ctr[i].unit_mask, sysctl.cpu_type);

		if (ret & OP_EVT_NOT_FOUND)
			printk(KERN_ERR "oprofile: ctr%d: %d: no such event for cpu %d\n", i, sysctl.ctr[i].event, sysctl.cpu_type);

		if (ret & OP_EVT_NO_UM)
			printk(KERN_ERR "oprofile: ctr%d: 0x%.2x: invalid unit mask for cpu %d\n", i, sysctl.ctr[i].unit_mask, sysctl.cpu_type);

		if (ret & OP_EVT_CTR_NOT_ALLOWED)
			printk(KERN_ERR "oprofile: ctr%d: %d: can't count event for this counter\n", i, sysctl.ctr[i].event);

		if (ret != OP_EVENTS_OK)
			ok = -EINVAL;
	}

	return ok;
}

static uint saved_perfctr_low[OP_MAX_COUNTERS];
static uint saved_perfctr_high[OP_MAX_COUNTERS];
static uint saved_eventsel_low[OP_MAX_COUNTERS];
static uint saved_eventsel_high[OP_MAX_COUNTERS];
 
static int pmc_init(void)
{
	int i;
	int err = 0; 
 
	if (sysctl.cpu_type == CPU_ATHLON) {
		op_nr_counters = 4;
		separate_running_bit = 1;
	}
 
	/* let's use the right MSRs */
	switch (sysctl.cpu_type) {
		case CPU_ATHLON:
			eventsel_msr[0] = MSR_K7_PERFCTL0;
			eventsel_msr[1] = MSR_K7_PERFCTL1;
			eventsel_msr[2] = MSR_K7_PERFCTL2;
			eventsel_msr[3] = MSR_K7_PERFCTL3;
			perfctr_msr[0] = MSR_K7_PERFCTR0;
			perfctr_msr[1] = MSR_K7_PERFCTR1;
			perfctr_msr[2] = MSR_K7_PERFCTR2;
			perfctr_msr[3] = MSR_K7_PERFCTR3;
			break;
		default:
			eventsel_msr[0] = MSR_P6_EVNTSEL0;
			eventsel_msr[1] = MSR_P6_EVNTSEL1;
			perfctr_msr[0] = MSR_P6_PERFCTR0;
			perfctr_msr[1] = MSR_P6_PERFCTR1;
			break;
	}

	for (i = 0 ; i < op_nr_counters ; ++i) {
		rdmsr(eventsel_msr[i], saved_eventsel_low[i], saved_eventsel_high[i]);
		rdmsr(perfctr_msr[i], saved_perfctr_low[i], saved_perfctr_high[i]);
	}

	/* setup each counter */
	if ((err = apic_setup()))
		return err;

	if ((err = smp_call_function(lvtpc_apic_setup, NULL, 0, 1))) {
		lvtpc_apic_restore(NULL);
	}

	return err;
}
 
static void pmc_deinit(void)
{
	int i;
 
	smp_call_function(lvtpc_apic_restore, NULL, 0, 1);
	lvtpc_apic_restore(NULL);
 
	for (i = 0 ; i < op_nr_counters ; ++i) {
		wrmsr(eventsel_msr[i], saved_eventsel_low[i], saved_eventsel_high[i]);
		wrmsr(perfctr_msr[i], saved_perfctr_low[i], saved_perfctr_high[i]);
	}

	apic_restore();
}
 
static char *names[] = { "0", "1", "2", "3", "4", };

static int pmc_add_sysctls(ctl_table * next)
{
	ctl_table * start = next; 
	ctl_table * tab; 
	int i, j;
 
	for (i=0; i < op_nr_counters; i++) {
		next->ctl_name = 1;
		next->procname = names[i];
		next->mode = 0755;

		if (!(tab = kmalloc(sizeof(ctl_table)*7, GFP_KERNEL)))
			goto cleanup;
 
		next->child = tab;

		memset(tab, 0, sizeof(ctl_table)*7);
		tab[0] = ((ctl_table){ 1, "enabled", &sysctl_parms.ctr[i].enabled, sizeof(int), 0644, NULL, lproc_dointvec, NULL, });
		tab[1] = ((ctl_table){ 1, "event", &sysctl_parms.ctr[i].event, sizeof(int), 0644, NULL, lproc_dointvec, NULL,  });
		tab[2] = ((ctl_table){ 1, "count", &sysctl_parms.ctr[i].count, sizeof(int), 0644, NULL, lproc_dointvec, NULL, });
		tab[3] = ((ctl_table){ 1, "unit_mask", &sysctl_parms.ctr[i].unit_mask, sizeof(int), 0644, NULL, lproc_dointvec, NULL, });
		tab[4] = ((ctl_table){ 1, "kernel", &sysctl_parms.ctr[i].kernel, sizeof(int), 0644, NULL, lproc_dointvec, NULL, });
		tab[5] = ((ctl_table){ 1, "user", &sysctl_parms.ctr[i].user, sizeof(int), 0644, NULL, lproc_dointvec, NULL, });
		next++;
	}

	return 0;

cleanup:
	next = start;
	for (j = 0; j < i; j++) {
		kfree(next->child);
		next++;
	}
	return -EFAULT;
}

static void pmc_remove_sysctls(ctl_table * next)
{
	int i;

	for (i=0; i < op_nr_counters; i++) {
		kfree(next->child);
		next++;
	}
}
 
struct op_int_operations op_nmi_ops = {
	init: pmc_init,
	deinit: pmc_deinit,
	add_sysctls: pmc_add_sysctls,
	remove_sysctls: pmc_remove_sysctls,
	check_params: pmc_check_params,
	setup: pmc_setup_all,
	start: pmc_start_all,
	stop: pmc_stop_all,
	start_cpu: pmc_select_start,
	stop_cpu: pmc_select_stop, 
};
