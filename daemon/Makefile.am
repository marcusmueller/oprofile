dist_sources = \
	oprofiled.c \
	opd_stats.c \
	opd_sfile.c \
	opd_sfile.h \
	opd_kernel.c \
	opd_kernel.h \
	opd_trans.c \
	opd_trans.h \
	opd_printf.h \
	opd_stats.h \
	opd_cookie.c \
	opd_cookie.h \
	opd_interface.h \
	opd_mangling.c \
	opd_mangling.h \
	opd_perfmon.h

EXTRA_DIST = $(dist_sources) opd_perfmon.c

LIBS=@POPT_LIBS@ @LIBERTY_LIBS@

AM_CPPFLAGS = \
	-I ${top_srcdir}/libabi \
	-I ${top_srcdir}/libutil \
	-I ${top_srcdir}/libop \
	-I ${top_srcdir}/libdb

AM_CFLAGS = @OP_CFLAGS@

# code shared with 2.4 daemon
noinst_LIBRARIES = libdaemon.a
libdaemon_a_SOURCES = \
	opd_util.h \
	opd_util.c

if kernel_support

bin_PROGRAMS = oprofiled

oprofiled_SOURCES = $(dist_sources)

if enable_abi
oprofiled_LDADD = \
	libdaemon.a \
	../libabi/libabi.a \
	../libdb/libodb.a \
	../libop/libop.a \
	../libutil/libutil.a
LINKER=$(CXX)
else
oprofiled_LDADD = \
	libdaemon.a \
	../libdb/libodb.a \
	../libop/libop.a \
	../libutil/libutil.a
LINKER=$(CC)
endif

oprofiled_LINK = $(LINKER) $(AM_CFLAGS) $(CFLAGS) $(AM_LDFLAGS) $(LDFLAGS) -o $@

endif
