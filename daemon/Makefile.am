distributed_sources = oprofiled.c opd_stats.c opd_kernel.c opd_image.c opd_sample_files.c

if kernel_support
 
AM_CPPFLAGS=-I ${top_srcdir}/libabi -I ${top_srcdir}/libutil -I ${top_srcdir}/libop -I ${top_srcdir}/libdb

bin_PROGRAMS = oprofiled

oprofiled_SOURCES = $(distributed_sources)

if enable_abi
oprofiled_LDADD = ../libabi/libabi.a ../libdb/libdb.a ../libop/libop.a ../libutil/libutil.a
else
oprofiled_LDADD = ../libdb/libdb.a ../libop/libop.a ../libutil/libutil.a
endif

endif
 
EXTRA_DIST = opd_image.h opd_printf.h opd_stats.h opd_kernel.h opd_sample_files.h p_module.h $(distributed_sources)

