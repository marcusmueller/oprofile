AM_CPPFLAGS=-I ${top_srcdir}/libop -I ${top_srcdir}/libutil
AM_CFLAGS = @OP_CFLAGS@

LIBS=@POPT_LIBS@ @LIBERTY_LIBS@

bin_PROGRAMS = ophelp op-check-perfevents
dist_bin_SCRIPTS = opcontrol

op_check_perfevents_SOURCES = op_perf_events_checker.c

ophelp_SOURCES = ophelp.c
ophelp_LDADD = ../libop/libop.a ../libutil/libutil.a
