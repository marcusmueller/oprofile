AM_CPPFLAGS=-I ${top_srcdir}/libop \
	-I ${top_srcdir}/libutil \
	@OP_CPPFLAGS@

AM_CFLAGS = @OP_CFLAGS@
AM_LDFLAGS = @OP_LDFLAGS@

LIBS=@POPT_LIBS@ @LIBERTY_LIBS@

bin_PROGRAMS = ophelp op-check-perfevents

op_check_perfevents_SOURCES = op_perf_events_checker.c
op_check_perfevents_CPPFLAGS = ${AM_CFLAGS} @PERF_EVENT_FLAGS@

ophelp_SOURCES = ophelp.c
ophelp_LDADD = ../libop/libop.a ../libutil/libutil.a
