/* COPYRIGHT (C) 2000,2001 THE VICTORIA UNIVERSITY OF MANCHESTER and John Levon
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

#include <linux/vmalloc.h>
#include <linux/wrapper.h>
#include <linux/pagemap.h>

#include "compat.h"

/* Given PGD from the address space's page table, return the kernel
 * virtual mapping of the physical memory mapped at ADR.
 */
static inline unsigned long uvirt_to_kva(pgd_t *pgd, unsigned long adr)
{
	unsigned long ret = 0UL;
	pmd_t *pmd;
	pte_t *ptep, pte;

	if (!pgd_none(*pgd)) {
		pmd = pmd_offset(pgd, adr);
		if (!pmd_none(*pmd)) {
			ptep = pte_offset(pmd, adr);
			pte = *ptep;
			if (pte_present(pte)) {
				ret = (unsigned long) pte_page_address(pte);
				ret |= adr & (PAGE_SIZE-1);
			}
		}
	}
	return ret;
}

/* Here we want the physical address of the memory.
 * This is used when initializing the contents of the
 * area and marking the pages as reserved.
 */
unsigned long kvirt_to_pa(unsigned long adr)
{
	unsigned long va, kva, ret;

	va = VMALLOC_VMADDR(adr);
	kva = uvirt_to_kva(pgd_offset_k(va), va);
	ret = __pa(kva);
	return ret;
}

void * rvmalloc(signed long size)
{
	void * mem;
	unsigned long adr, page;

	mem = vmalloc_32(size);
	if (!mem)
		return NULL;

	memset(mem, 0, size);
	
	adr=(unsigned long) mem;
	while (size > 0) {
		page = kvirt_to_pa(adr);
		mem_map_reserve(virt_to_page(__va(page)));
		adr += PAGE_SIZE;
		size -= PAGE_SIZE;
	}
	return mem;
}

void rvfree(void * mem, signed long size)
{
	unsigned long adr, page;

	if (!mem)
		return;
		
	adr=(unsigned long) mem;
	while (size > 0) {
		page = kvirt_to_pa(adr);
		mem_map_unreserve(virt_to_page(__va(page)));
		adr += PAGE_SIZE;
		size -= PAGE_SIZE;
	}
	vfree(mem);
}
