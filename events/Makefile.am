event_files = \
	alpha/ev4/events alpha/ev4/unit_masks \
	alpha/ev5/events alpha/ev5/unit_masks \
	alpha/ev67/events alpha/ev67/unit_masks \
	alpha/ev6/events alpha/ev6/unit_masks \
	alpha/pca56/events alpha/pca56/unit_masks \
	i386/athlon/events i386/athlon/unit_masks \
	i386/p4/events i386/p4-ht/events \
	i386/p4-ht/unit_masks i386/p4/unit_masks \
	i386/pii/events i386/pii/unit_masks \
	i386/piii/events i386/piii/unit_masks \
	i386/ppro/events i386/ppro/unit_masks \
	i386/p6_mobile/events i386/p6_mobile/unit_masks \
	ia64/ia64/events ia64/ia64/unit_masks \
	ia64/itanium2/events ia64/itanium2/unit_masks \
	ia64/itanium/events ia64/itanium/unit_masks \
	rtc/events rtc/unit_masks \
	x86-64/hammer/events x86-64/hammer/unit_masks

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
