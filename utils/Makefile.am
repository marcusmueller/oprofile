AM_CPPFLAGS=-I ${top_srcdir}/libop -I ${top_srcdir}/libutil

bin_PROGRAMS = op_session op_help
bin_SCRIPTS = opcontrol
noinst_SCRIPTS = $(OP_START) $(OP_STOP) $(OP_DUMP)

op_session_SOURCES = op_session.c
op_session_LDADD = ../libutil/libutil.a

op_help_SOURCES = op_help.c
op_help_LDADD = ../libop/libop.a ../libutil/libutil.a

if kernel_support
OP_START=op_start_25
OP_STOP=op_stop_25
OP_DUMP=op_dump_25
else
OP_START=op_start
OP_STOP=op_stop
OP_DUMP=op_dump
endif

# back-compatability scripts
install-exec-local:
	$(INSTALL_SCRIPT) $(srcdir)/$(OP_START) $(DESTDIR)$(bindir)/op_start
	$(INSTALL_SCRIPT) $(srcdir)/$(OP_STOP) $(DESTDIR)$(bindir)/op_stop
	$(INSTALL_SCRIPT) $(srcdir)/$(OP_DUMP) $(DESTDIR)$(bindir)/op_dump

uninstall-local:
	rm -f $(DESTDIR)$(bindir)/op_start
	rm -f $(DESTDIR)$(bindir)/op_stop
	rm -f $(DESTDIR)$(bindir)/op_dump

# make sure everything gets distributed...
EXTRA_DIST = op_dump op_dump_25 op_start_25 op_stop op_stop_25 op_start opcontrol
