SUBDIRS = ui

dist_sources = oprof_start.cpp oprof_start_config.cpp oprof_start_util.cpp oprof_start_main.cpp \
	oprof_start.h oprof_start_config.h oprof_start_util.h persistent_config.h

EXTRA_DIST = $(dist_sources)

if have_qt

AM_CPPFLAGS = @QT_INCLUDES@ -I ${top_srcdir}/libop -I ${top_srcdir}/libutil++ -I ${top_srcdir}/libutil -DKVERSION=\"@KVERS@\"

bin_PROGRAMS = oprof_start

oprof_start_SOURCES = $(dist_sources)
nodist_oprof_start_SOURCES = oprof_start.moc.cpp
oprof_start_LDADD = ../libutil++/libutil++.a ../libutil/libutil.a ../libop/libop.a ui/liboprof_start.a @QT_LDFLAGS@ @QT_LIB@ @X_LIBS@

oprof_start.moc.cpp: ${top_srcdir}/gui/oprof_start.h
	$(MOC) -o $@ ${top_srcdir}/gui/oprof_start.h

clean-local:
	rm -f oprof_start.moc.cpp

endif
 
