dist_sources = \
	opd_stats.c \
	opd_kernel.c \
	opd_proc.c \
	opd_image.c \
	opd_mapping.c \
	opd_parse_proc.c \
	oprofiled.c \
	opd_image.h \
	opd_mapping.h \
	p_module.h \
	opd_kernel.h \
	opd_parse_proc.h \
	opd_proc.h \
	opd_stats.h

EXTRA_DIST = $(dist_sources)

LIBS=@POPT_LIBS@ @LIBERTY_LIBS@

if !kernel_support

AM_CPPFLAGS = \
	-I ${top_srcdir}/libabi \
	-I ${top_srcdir}/libutil \
	-I ${top_srcdir}/libop \
	-I ${top_srcdir}/libdb \
	-I ${top_srcdir}/daemon

bin_PROGRAMS = oprofiled

oprofiled_SOURCES = $(dist_sources)

if enable_abi
oprofiled_LDADD = \
	../libabi/libabi.a \
	../daemon/libdaemon.a \
	../libdb/libodb.a \
	../libop/libop.a \
	../libutil/libutil.a
LINKER=$(CXX)
else
oprofiled_LDADD = \
	../daemon/libdaemon.a \
	../libdb/libodb.a \
	../libop/libop.a \
	../libutil/libutil.a
LINKER=$(CC)
endif

oprofiled_LINK = $(LINKER) $(AM_CFLAGS) $(CFLAGS) $(AM_LDFLAGS) $(LDFLAGS) -o $@

endif
