if build_oprof_report
if have_qt

AM_CPPFLAGS = @QT_INCLUDES@

noinst_LIBRARIES = liboprof_report.a
nodist_liboprof_report_a_SOURCES = oprof_report.base.cpp oprof_report.base.moc.cpp

oprof_report.base.h: oprof_report.base.ui
	$(UIC) -o $@ $<

oprof_report.base.cpp: oprof_report.base.h oprof_report.base.ui
	$(UIC) -o $@ -impl $^

oprof_report.base.moc.cpp: oprof_report.base.h
	$(MOC) -o $@ $<

clean-local:
	rm -f oprof_report.base.h oprof_report.base.cpp oprof_report.base.moc.cpp

endif
endif

EXTRA_DIST = oprof_report.base.ui
