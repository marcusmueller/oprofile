/*
 * op_x86.c
 *
 * A variety of Intel hardware grubbery.
 *
 * Based in part on arch/i386/kernel/mpparse.c
 */

#include <linux/mm.h>
#include <linux/init.h>
#include <linux/config.h>

#include <asm/smp.h>
#include <asm/mpspec.h>
#include <asm/io.h>

static int __init mpf_checksum(unsigned char *mp, int len)
{
	int sum = 0;

	while (len--)
		sum += *mp++;

	return sum & 0xFF;
}

static int __init mpf_table_ok(struct intel_mp_floating * mpf, unsigned long *bp)
{
	if (*bp != SMP_MAGIC_IDENT)
		return 0;
	if (mpf->mpf_length != 1)
		return 0;
	if (mpf_checksum((unsigned char *)bp, 16))
		return 0;

	return (mpf->mpf_specification == 1 || mpf->mpf_specification == 4);
}

static int __init smp_scan_config (unsigned long base, unsigned long length)
{
	unsigned long *bp = phys_to_virt(base);
	struct intel_mp_floating *mpf;

	while (length > 0) {
		mpf = (struct intel_mp_floating *)bp;
		if (mpf_table_ok(mpf, bp))
			return 1;
		bp += 4;
		length -= 16;
	}
	return 0;
}

int __init find_intel_smp (void)
{
	unsigned int address;

	/*
	 * FIXME: Linux assumes you have 640K of base ram..
	 * this continues the error...
	 *
	 * 1) Scan the bottom 1K for a signature
	 * 2) Scan the top 1K of base RAM
	 * 3) Scan the 64K of bios
	 */
	if (smp_scan_config(0x0,0x400) ||
		smp_scan_config(639*0x400,0x400) ||
			smp_scan_config(0xF0000,0x10000))
		return 1;
	/*
	 * If it is an SMP machine we should know now, unless the
	 * configuration is in an EISA/MCA bus machine with an
	 * extended bios data area.
	 *
	 * there is a real-mode segmented pointer pointing to the
	 * 4K EBDA area at 0x40E, calculate and scan it here.
	 *
	 * NOTE! There are Linux loaders that will corrupt the EBDA
	 * area, and as such this kind of SMP config may be less
	 * trustworthy, simply because the SMP table may have been
	 * stomped on during early boot. These loaders are buggy and
	 * should be fixed.
	 */

	address = *(unsigned short *)phys_to_virt(0x40E);
	address <<= 4;
	return smp_scan_config(address, 0x1000);
}
