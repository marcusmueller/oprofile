/* $Id: op_init.c,v 1.8 2002/02/09 21:17:56 movement Exp $ */
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

/* these routines are in a separate file so the rest can be compiled for i686 */
 
EXPORT_NO_SYMBOLS;

static void __init set_cpu_type(void)
{
	/* we want to include all P6 processors (i.e. > Pentium Classic,
	 * < Pentium IV
	 */
	if ((current_cpu_data.x86_vendor != X86_VENDOR_INTEL &&
	    current_cpu_data.x86 != 6) ||
		(current_cpu_data.x86_vendor != X86_VENDOR_AMD &&
		 current_cpu_data.x86 != 6)) {
		sysctl.cpu_type = CPU_RTC;
		return;
	}

	/* 0 if PPro, 1 if PII, 2 if PIII, 3 if Athlon */
	if (current_cpu_data.x86_vendor == X86_VENDOR_AMD) {
		sysctl.cpu_type = CPU_ATHLON;
	} else {
		sysctl.cpu_type = (current_cpu_data.x86_model > 5) ? CPU_PIII :
			(current_cpu_data.x86_model > 2);
	}
}

int __init stub_init(void)
{
	set_cpu_type();
	return oprof_init();
}

void __exit stub_exit(void)
{
	oprof_exit();
}

module_init(stub_init);
module_exit(stub_exit);
