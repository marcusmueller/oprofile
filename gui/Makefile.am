SUBDIRS = ui

distributed_sources = oprof_start.cpp oprof_start_config.cpp oprof_start_util.cpp oprof_start_main.cpp

if have_qt

MOC=@MOC@

AM_CPPFLAGS = @QT_INCLUDES@ -I ../libop -I ../libutil++ -I ../libutil -DBINDIR=\"$(BINDIR)\" -DKVERSION=\"@KVERS@\"

bin_PROGRAMS = oprof_start

oprof_start_SOURCES = $(distributed_sources)
nodist_oprof_start_SOURCES = oprof_start.moc.cpp
oprof_start_LDADD = ../libutil++/libutil++.a ../libutil/libutil.a ../libop/libop.a ui/liboprof_start.a @QT_LDFLAGS@ @QT_LIB@

oprof_start.moc.cpp: oprof_start.h
	$(MOC) -o $@ oprof_start.h

clean-local:
	rm -f oprof_start.moc.cpp

EXTRA_DIST = oprof_start.h oprof_start_config.h oprof_start_util.h persistent_config.h $(distributed_sources)

endif
