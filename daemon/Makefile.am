distributed_sources = oprofiled.c opd_stats.c opd_kernel.c opd_image.c opd_sample_files.c

if WITH_KERNEL_SUPPORT
AM_CPPFLAGS=-I ../libutil -I ../libop -I ../libdb

bin_PROGRAMS = oprofiled

oprofiled_SOURCES = $(distributed_sources)

# TODO: add abi libraries optionnaly
oprofiled_LDADD = ../libabi/libabi.a ../libdb/libdb.a ../libop/libop.a ../libutil/libutil.a
endif

EXTRA_DIST = opd_image.h opd_printf.h opd_stats.h opd_kernel.h opd_sample_files.h p_module.h $(distributed_sources)

