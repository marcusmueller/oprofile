distributed_sources = opd_stats.c opd_kernel.c opd_proc.c opd_image.c opd_mapping.c opd_parse_proc.c opd_sample_files.c oprofiled.c

if !WITH_KERNEL_SUPPORT
AM_CPPFLAGS=-I ${top_srcdir}/libutil -I ${top_srcdir}/libop -I ${top_srcdir}/libdb

bin_PROGRAMS = oprofiled

oprofiled_SOURCES = $(distributed_sources)

# TODO: add abi libraries optionnaly
oprofiled_LDADD = ../libabi/libabi.a ../libdb/libdb.a ../libop/libop.a ../libutil/libutil.a
endif

EXTRA_DIST = opd_image.h opd_mapping.h opd_printf.h opd_sample_files.h p_module.h opd_kernel.h opd_parse_proc.h opd_proc.h opd_stats.h $(distributed_sources)
