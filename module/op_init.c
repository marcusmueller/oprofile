/* $Id: op_init.c,v 1.10 2002/02/28 03:39:18 movement Exp $ */
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

static __init op_cpu get_cpu_type(void)
{
	__u8 vendor = current_cpu_data.x86_vendor;
	__u8 family = current_cpu_data.x86;
	__u8 model = current_cpu_data.x86_model;

	/* unknown vendor */
	if (vendor != X86_VENDOR_INTEL && vendor != X86_VENDOR_AMD) {
		return CPU_RTC;
	}

	/* not a P6-class processor */
	if (family != 6)
		return CPU_RTC;

	if (vendor == X86_VENDOR_AMD)
		return CPU_ATHLON;
 
	if (model > 5)
		return CPU_PIII;
	else if (model > 2)
		return CPU_PII; 

	return CPU_PPRO;
}

int __init stub_init(void)
{
	sysctl.cpu_type = get_cpu_type();
	return oprof_init();
}

void __exit stub_exit(void)
{
	oprof_exit();
}

module_init(stub_init);
module_exit(stub_exit);
