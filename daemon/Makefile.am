dist_sources = \
	oprofiled.c \
	opd_stats.c \
	opd_kernel.c \
	opd_image.c \
	opd_image.h \
	opd_printf.h \
	opd_stats.h \
	opd_kernel.h \
	opd_cookie.h

EXTRA_DIST = $(dist_sources)

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
	opd_util.c \
	opd_sample_files.c \
	opd_sample_files.h

if kernel_support

bin_PROGRAMS = oprofiled

oprofiled_SOURCES = $(dist_sources)

if enable_abi
oprofiled_LDADD = \
	../libabi/libabi.a \
	libdaemon.a \
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
