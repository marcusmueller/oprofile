if build_oprof_report
if have_qt

SUBDIRS = ui

distributed_sources = oprof_report.cpp hotspot_view.cpp oprof_report_main.cpp oprofpp_view.cpp

MOC=@MOC@

AM_CPPFLAGS = @QT_INCLUDES@ -I ${top_srcdir}/libop -I ${top_srcdir}/libutil++ -I ${top_srcdir}/libutil -DBINDIR=\"$(BINDIR)\" -DKVERSION=\"@KVERS@\"

bin_PROGRAMS = oprof_report

oprof_report_SOURCES = $(distributed_sources)
nodist_oprof_report_SOURCES = oprof_report.moc.cpp
oprof_report_LDADD = ../libutil++/libutil++.a ../libutil/libutil.a ../libop/libop.a ui/liboprof_report.a @QT_LDFLAGS@ @QT_LIB@

oprof_report.moc.cpp: oprof_report.h
	$(MOC) -o $@ oprof_report.h

clean-local:
	rm -f oprof_report.moc.cpp

EXTRA_DIST = oprof_report.h hotspot_view.h op_view.h oprofpp_view.h $(distributed_sources)
endif
endif
