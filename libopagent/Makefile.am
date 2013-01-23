pkglib_LTLIBRARIES = libopagent.la

# install opagent.h to include directory
include_HEADERS = opagent.h

libopagent_la_SOURCES = opagent.c \
			jitdump.h \
			opagent.h

EXTRA_DIST = opagent_symbols.ver


libopagent_la_CFLAGS = -fPIC
libopagent_la_CPPFLAGS = -I ${top_srcdir}/libop \
	-I ${top_srcdir}/libutil \
	@OP_CPPFLAGS@

libopagent_la_LIBADD = $(BFD_LIBS)

# Do not increment the major version for this library except to
# intentionally break backward ABI compatability.  Use the
# symbol versioning technique (via the version script) to add new or
# change existing functions; then just increment the minor version.
#
# See http://www.gnu.org/software/libtool/manual/html_node/Updating-version-info.html
# for information about library versioning.
#
# See http://www.gnu.org/software/gnulib/manual/html_node/LD-Version-Scripts.html
# for details about the --version-script option.
libopagent_la_LDFLAGS = -version-info  1:1:0 \
			-Wl,--version-script=${top_srcdir}/libopagent/opagent_symbols.ver \
			@OP_LDFLAGS@


