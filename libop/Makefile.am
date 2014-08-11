SUBDIRS = . tests

AM_CPPFLAGS=-I${top_srcdir}/libutil @OP_CPPFLAGS@
AM_CFLAGS = @OP_CFLAGS@

noinst_LIBRARIES = libop.a
libop_a_SOURCES = \
	op_events.c \
	op_events.h \
	op_parse_event.c \
	op_parse_event.h \
	op_cpu_type.c \
	op_cpu_type.h \
	op_mangle.c \
	op_mangle.h \
	op_alloc_counter.c \
	op_alloc_counter.h \
	op_hw_config.h \
	op_config.c \
	op_config.h \
	op_sample_file.h \
	op_xml_events.c \
	op_xml_events.h \
	op_xml_out.c \
	op_xml_out.h \
	op_hw_specific.h \
	op_netburst.c \
	op_netburst.h
