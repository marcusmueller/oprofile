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

#include <linux/ioport.h>
#include <linux/mc146818rtc.h>
 
#include "oprofile.h"

#define RTC_IO_PORTS 2
#define OP_RTC_MIN     2
#define OP_RTC_MAX  4096 

/* ---------------- RTC handler ------------------ */
 
// FIXME: share 
inline static int op_check_pid(void)
{
	if (unlikely(sysctl.pid_filter) && 
	    likely(current->pid != sysctl.pid_filter))
		return 1;

	if (unlikely(sysctl.pgrp_filter) && 
	    likely(current->pgrp != sysctl.pgrp_filter))
		return 1;
		
	return 0;
}

static void do_rtc_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
	uint cpu = op_cpu_id();
	unsigned char intr_flags;

	spin_lock(&rtc_lock);
	/* read and ack the interrupt */
	intr_flags = CMOS_READ(RTC_INTR_FLAGS);
	/* Is this my type of interrupt? */
	if (intr_flags & RTC_PF) {
		if (likely(!op_check_pid()))
			op_do_profile(cpu, regs, 0);
	}
	spin_unlock(&rtc_lock);
	return;
}

static int rtc_setup(void)
{
	unsigned char tmp_control;
	unsigned long flags;

 	spin_lock_irqsave(&rtc_lock, flags);

	/* disable periodic interrupts */
	tmp_control = CMOS_READ(RTC_CONTROL);
	tmp_control &= ~RTC_PIE;
	CMOS_WRITE(tmp_control, RTC_CONTROL);
	CMOS_READ(RTC_INTR_FLAGS);

	/* Set the frequency for periodic interrupts by finding the
	 * closest power of two within the allowed range.
	 */
 
// FIXME
#if 0 
	unsigned char tmp_freq_select;
	unsigned long target, rem;
	unsigned int exp, freq;
	if (sysctl.ctr[0].count != 0) {
		/* need to reverse the daemon's calculation without
		 * being able to get cpu_khz parameter.
		 */
		target = (boot_cpu_data.loops_per_jiffy * 100)/sysctl.ctr[0].count;
		rem = (boot_cpu_data.loops_per_jiffy * 100)%sysctl.ctr[0].count;
		/* round up */
		if (rem >= (sysctl.ctr[0].count >> 1))
			target++;
#ifdef BOBM_DEBUG
		printk(KERN_ERR "oprofile: loops_per_jiffy = %lu\n", 
				boot_cpu_data.loops_per_jiffy);
		printk(KERN_ERR "oprofile: requested value = %lu\n", target);
#endif
		if (target < OP_RTC_MIN) 
			target = OP_RTC_MIN;
		else if (target > OP_RTC_MAX)
			target = OP_RTC_MAX;
		
	} else 
		target = 128;

	exp = 0;
	while (target > (1 << exp) + ((1 << exp) >> 1))
		exp++;
	freq = 16 - exp;
#ifdef BOBM_DEBUG
	printk(KERN_ERR "oprofile: rtc freq: %d, code: %d\n", (1 << exp), freq);
	printk(KERN_ERR "oprofile: old ctr[0].count = %d\n", sysctl.ctr[0].count);
#endif
	/* store our actual "effective" count back for the daemon */
	sysctl.ctr[0].count = (boot_cpu_data.loops_per_jiffy * 100) / (1<<exp);
	sysctl_parms.ctr[0].count = sysctl.ctr[0].count;
#ifdef BOBM_DEBUG
	printk(KERN_ERR "oprofile: new ctr[0].count = %d\n", sysctl.ctr[0].count);
#endif

	tmp_freq_select = CMOS_READ(RTC_FREQ_SELECT);
	tmp_freq_select = (tmp_freq_select & 0xf0) | freq;
	CMOS_WRITE(tmp_freq_select, RTC_FREQ_SELECT);

	spin_unlock_irqrestore(&rtc_lock, flags);
#endif 
	return 0; 
}

static void rtc_start(void)
{
	unsigned char tmp_control;
	unsigned long flags;

	spin_lock_irqsave(&rtc_lock, flags);
	/* Enable periodic interrupts */
	tmp_control = CMOS_READ(RTC_CONTROL);
	tmp_control |= RTC_PIE;
	CMOS_WRITE(tmp_control, RTC_CONTROL);

	/* read the flags register to start interrupts */
	CMOS_READ(RTC_INTR_FLAGS);

	spin_unlock_irqrestore(&rtc_lock, flags);
}

static void rtc_stop(void)
{
	unsigned char tmp_control;
	unsigned long flags;

 	spin_lock_irqsave(&rtc_lock, flags);

	/* disable periodic interrupts */
	tmp_control = CMOS_READ(RTC_CONTROL);
	tmp_control &= ~RTC_PIE;
	CMOS_WRITE(tmp_control, RTC_CONTROL);
	CMOS_READ(RTC_INTR_FLAGS);

	spin_unlock_irqrestore(&rtc_lock, flags);
}

static void rtc_start_cpu(uint cpu)
{
	rtc_start();
}

static void rtc_stop_cpu(uint cpu)
{
	rtc_stop();
}
 
static int rtc_check_params(void)
{
	// FIXME 
	return 0;
}

static int rtc_init(void)
{
	 /* request_region returns 0 on **failure** */
	if (!request_region(RTC_PORT(0), RTC_IO_PORTS, "oprofile")) {
		printk(KERN_ERR "oprofile: can't get RTC I/O Ports\n");
		return -EBUSY;
	}

	/* request_irq returns 0 on **success** */
	if (request_irq(RTC_IRQ, do_rtc_interrupt,
			SA_INTERRUPT, "oprofile", NULL)) {
		printk(KERN_ERR "oprofile: IRQ%d busy \n", RTC_IRQ);
		release_region(RTC_PORT(0), RTC_IO_PORTS);
		return -EBUSY;
	}
	return 0;
}

static void rtc_deinit(void)
{
	free_irq(RTC_IRQ, NULL);
	release_region(RTC_PORT(0), RTC_IO_PORTS);
}
 
static int rtc_add_sysctls(ctl_table * next)
{
	return 0; 
	/* OK, I see a simple "rtc_value" here with which we set it up,
	 * maybe a better way ?
	 */
#if 0 
	ctl_table * start = next; 
	ctl_table * tab; 
	int i, j;
 
	for (i=0; i < op_nr_counters; i++) {
		next->ctl_name = 1;
		next->procname = names[i];
		next->mode = 0700;

		if (!(tab = kmalloc(sizeof(ctl_table)*7, GFP_KERNEL)))
			goto cleanup;
 
		next->child = tab;

		memset(tab, 0, sizeof(ctl_table)*7);
		tab[0] = ((ctl_table){ 1, "enabled", &sysctl_parms.ctr[i].enabled, sizeof(int), 0600, NULL, lproc_dointvec, NULL, });
		tab[1] = ((ctl_table){ 1, "event", &sysctl_parms.ctr[i].event, sizeof(int), 0600, NULL, lproc_dointvec, NULL,  });
		tab[2] = ((ctl_table){ 1, "count", &sysctl_parms.ctr[i].count, sizeof(int), 0600, NULL, lproc_dointvec, NULL, });
		tab[3] = ((ctl_table){ 1, "unit_mask", &sysctl_parms.ctr[i].unit_mask, sizeof(int), 0600, NULL, lproc_dointvec, NULL, });
		tab[4] = ((ctl_table){ 1, "kernel", &sysctl_parms.ctr[i].kernel, sizeof(int), 0600, NULL, lproc_dointvec, NULL, });
		tab[5] = ((ctl_table){ 1, "user", &sysctl_parms.ctr[i].user, sizeof(int), 0600, NULL, lproc_dointvec, NULL, });
		next++;
	}

	return 0;

cleanup:
	next = &oprof_table[nr_oprof_static];
	for (j = 0; j < i; j++) {
		kfree(next->child);
		next++;
	}
	return -EFAULT;
#endif 
}

static void rtc_remove_sysctls(ctl_table * next)
{
#if 0 
	int i = smp_num_cpus;
	while (i-- > 0) {
		kfree(next->child);
		next++;
	}
#endif 
}
 
struct op_int_operations op_rtc_ops = {
	init: rtc_init,
	deinit: rtc_deinit,
	add_sysctls: rtc_add_sysctls,
	remove_sysctls: rtc_remove_sysctls,
	check_params: rtc_check_params,
	setup: rtc_setup,
	start: rtc_start,
	stop: rtc_stop,
	start_cpu: rtc_start_cpu,
	stop_cpu: rtc_stop_cpu,
};
