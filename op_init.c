/* $Id: op_init.c,v 1.8 2001/09/08 21:46:03 phil_e Exp $ */
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

EXPORT_NO_SYMBOLS;

MODULE_PARM(expected_cpu_type, "i");
MODULE_PARM_DESC(expected_cpu_type, "Allow checking of detected hardware from the user space");
static int expected_cpu_type = -1;

extern int cpu_type;
extern uint op_nr_counters;
extern int separate_running_bit;

static int __init hw_ok(void)
{
	/* we want to include all P6 processors (i.e. > Pentium Classic,
	 * < Pentium IV
	 */
	if ((current_cpu_data.x86_vendor != X86_VENDOR_INTEL &&
	    current_cpu_data.x86 != 6) ||
		(current_cpu_data.x86_vendor != X86_VENDOR_AMD &&
		 current_cpu_data.x86 != 6)) {
		printk(KERN_ERR "oprofile: not an Intel P6 or AMD Athlon processor. Sorry.\n");
		return CPU_NO_GOOD;
	}

	/* 0 if PPro, 1 if PII, 2 if PIII, 3 if Athlon */
	if (current_cpu_data.x86_vendor == X86_VENDOR_AMD)
		cpu_type = CPU_ATHLON;
	else
		cpu_type = (current_cpu_data.x86_model > 5) ? CPU_PIII :
			(current_cpu_data.x86_model > 2);
 
	if (cpu_type == CPU_ATHLON) {
		op_nr_counters = 4;
		separate_running_bit = 1;
	}

	if (expected_cpu_type != -1 && expected_cpu_type != cpu_type) {

		printk("oprofile: user space/module cpu detection mismatch\n");

		/* FIXME: oprofile list */
		printk("please send the next line and your /proc/cpuinfo to moz@compsoc.man.ac.uk\n");

		printk("vendor %d step %d model %d, expected_cpu_type %d, cpu_type %d\n",
		       current_cpu_data.x86_vendor, current_cpu_data.x86,
		       current_cpu_data.x86_model, expected_cpu_type,
		       cpu_type);

		return CPU_NO_GOOD;
	}
	return cpu_type;
}

int __init stub_init(void)
{
	if (hw_ok() == CPU_NO_GOOD)
		return -EINVAL;

	return oprof_init();
}

void __exit stub_exit(void)
{
	oprof_exit();
	return;
}

module_init(stub_init);
module_exit(stub_exit);
