event_files = \
	alpha/ev67/events alpha/ev67/unit_masks \
	i386/athlon/events i386/athlon/unit_masks \
	i386/core_2/events i386/core_2/unit_masks \
	i386/p4/events i386/p4-ht/events \
	i386/p4-ht/unit_masks i386/p4/unit_masks \
	i386/pii/events i386/pii/unit_masks \
	i386/piii/events i386/piii/unit_masks \
	i386/ppro/events i386/ppro/unit_masks \
	i386/p6_mobile/events i386/p6_mobile/unit_masks \
	i386/core/events i386/core/unit_masks \
	i386/arch_perfmon/events i386/arch_perfmon/unit_masks \
	i386/atom/events i386/atom/unit_masks \
	i386/core_i7/events i386/core_i7/unit_masks \
	i386/nehalem/events i386/nehalem/unit_masks \
	i386/westmere/events i386/westmere/unit_masks \
	i386/sandybridge/events i386/sandybridge/unit_masks \
	i386/ivybridge/events i386/ivybridge/unit_masks \
	i386/haswell/events i386/haswell/unit_masks \
	i386/broadwell/events i386/broadwell/unit_masks \
	i386/silvermont/events i386/silvermont/unit_masks \
	ppc64/architected_events_v1/events ppc64/architected_events_v1/unit_masks \
	ppc64/power4/events ppc64/power4/event_mappings ppc64/power4/unit_masks \
	ppc64/power5/events ppc64/power5/event_mappings ppc64/power5/unit_masks \
	ppc64/power5+/events ppc64/power5+/event_mappings ppc64/power5+/unit_masks \
	ppc64/power5++/events ppc64/power5++/event_mappings ppc64/power5++/unit_masks \
	ppc64/power6/events ppc64/power6/event_mappings ppc64/power6/unit_masks \
	ppc64/power7/events ppc64/power7/event_mappings ppc64/power7/unit_masks \
	ppc64/power8/events ppc64/power8/unit_masks \
	ppc64/970/events ppc64/970/event_mappings ppc64/970/unit_masks \
	ppc64/970MP/events ppc64/970MP/event_mappings ppc64/970MP/unit_masks \
	x86-64/hammer/events x86-64/hammer/unit_masks \
	x86-64/family10/events x86-64/family10/unit_masks \
	x86-64/family11h/events x86-64/family11h/unit_masks \
	x86-64/family12h/events x86-64/family12h/unit_masks \
	x86-64/family14h/events x86-64/family14h/unit_masks \
	x86-64/family15h/events x86-64/family15h/unit_masks \
	x86-64/generic/events x86-64/generic/unit_masks \
	arm/xscale1/events arm/xscale1/unit_masks \
	arm/xscale2/events arm/xscale2/unit_masks \
	arm/armv6/events arm/armv6/unit_masks \
	arm/armv7-common/events arm/armv7-common/unit_masks \
	arm/armv7/events arm/armv7/unit_masks \
	arm/armv7-scorpion/events arm/armv7-scorpion/unit_masks \
	arm/armv7-scorpionmp/events arm/armv7-scorpionmp/unit_masks \
	arm/armv7-krait/events arm/armv7-krait/unit_masks \
	arm/armv7-ca9/events arm/armv7-ca9/unit_masks \
	arm/armv7-ca5/events arm/armv7-ca5/unit_masks \
	arm/armv7-ca7/events arm/armv7-ca7/unit_masks \
	arm/armv7-ca15/events arm/armv7-ca15/unit_masks \
	arm/mpcore/events arm/mpcore/unit_masks \
	arm/armv8-pmuv3-common/events arm/armv8-pmuv3-common/unit_masks \
	arm/armv8-xgene/events arm/armv8-xgene/unit_masks \
	arm/armv8-ca57/events arm/armv8-ca57/unit_masks \
	arm/armv8-ca53/events arm/armv8-ca53/unit_masks \
	mips/20K/events mips/20K/unit_masks \
	mips/24K/events mips/24K/unit_masks \
	mips/25K/events mips/25K/unit_masks \
	mips/34K/events mips/34K/unit_masks \
	mips/5K/events mips/5K/unit_masks \
	mips/rm7000/events mips/rm7000/unit_masks \
	mips/rm9000/events mips/rm9000/unit_masks \
	mips/sb1/events mips/sb1/unit_masks \
	mips/r10000/events mips/r10000/unit_masks \
	mips/r12000/events mips/r12000/unit_masks \
	mips/vr5432/events mips/vr5432/unit_masks \
	mips/vr5500/events mips/vr5500/unit_masks \
	mips/loongson2/events mips/loongson2/unit_masks \
	mips/1004K/events mips/1004K/unit_masks \
	mips/74K/events mips/74K/unit_masks \
	ppc/7450/events ppc/7450/unit_masks \
	ppc/e500/events ppc/e500/unit_masks \
	ppc/e500v2/events ppc/e500v2/unit_masks \
	ppc/e500mc/events ppc/e500mc/unit_masks \
	ppc/e6500/events ppc/e6500/unit_masks \
	ppc/e300/events ppc/e300/unit_masks \
	tile/tile64/events tile/tile64/unit_masks \
	tile/tilepro/events tile/tilepro/unit_masks \
	tile/tilegx/events tile/tilegx/unit_masks \
	s390/z10/events s390/z10/unit_masks \
	s390/z196/events s390/z196/unit_masks \
	s390/zEC12/events s390/zEC12/unit_masks

install-data-local:
	for i in ${event_files} ; do \
		dir=`dirname $$i` ; \
		mkdir -p $(DESTDIR)$(pkgdatadir)/$$dir ; \
		$(INSTALL_DATA) $(top_srcdir)/events/$$i $(DESTDIR)$(pkgdatadir)/$$i ; \
	done

uninstall-local:
	for i in ${event_files} ; do \
		dir=`dirname $$i` ; \
		archdir=`dirname $$dir` ; \
		if test -f $(DESTDIR)$(pkgdatadir)/$$i ; then \
			rm $(DESTDIR)$(pkgdatadir)/$$i ; \
		fi;  \
		if test -d $(DESTDIR)$(pkgdatadir)/$$dir ; then \
			rmdir --ignore-fail-on-non-empty $(DESTDIR)$(pkgdatadir)/$$dir ; \
		fi; \
		if test $$archdir != "." -a -d $(DESTDIR)$(pkgdatadir)/$$archdir ; then \
			rmdir --ignore-fail-on-non-empty $(DESTDIR)$(pkgdatadir)/$$archdir ; \
		fi; \
	done

EXTRA_DIST = $(event_files)
