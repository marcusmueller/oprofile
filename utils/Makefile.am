AM_CPPFLAGS=-I ${top_srcdir}/libop -I ${top_srcdir}/libutil
AM_CFLAGS = @OP_CFLAGS@

LIBS=@POPT_LIBS@ @LIBERTY_LIBS@

bin_PROGRAMS = op_help
dist_bin_SCRIPTS = opcontrol

op_help_SOURCES = op_help.c
op_help_LDADD = ../libop/libop.a ../libutil/libutil.a
