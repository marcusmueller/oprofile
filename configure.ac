#   Autoconf script for oprofile
#
#   Copyright 1999 Olaf Titz <olaf@bigred.inka.de>
#   Adapted for oprofile
#
#   This program is free software; you can redistribute it and/or
#   modify it under the terms of the GNU General Public License
#   as published by the Free Software Foundation; either version
#   2 of the License, or (at your option) any later version.
#

AC_PREREQ(2.13)

AC_INIT([OProfile], [0.9.9])
AC_CONFIG_SRCDIR([libop/op_config.h])
AM_INIT_AUTOMAKE
AC_CONFIG_HEADERS(config.h)

AC_CHECK_DECLS([basename], [], [], [[#include <libgen.h>]])
AC_PROG_RANLIB
AC_PROG_LIBTOOL
PKG_PROG_PKG_CONFIG

dnl for the man page
DATE="`date '+%a %d %B %Y'`"
AC_SUBST(DATE)

# Since we should not permanently alter user environment variables, we'll
# save the contents of the original flags in case the user has set them
# prior to running this configue script.
CPPFLAGS_SAVE="$CPPFLAGS"
LDFLAGS_SAVE="$LDFLAGS"
AC_ARG_WITH(binutils,
[  --with-binutils=dir		Path to binutils installation to use], BINUTILSDIR=$withval)

if test "$BINUTILSDIR" != ""; then
	LD="$BINUTILSDIR/ld"
	CPPFLAGS="$CPPFLAGS -I$BINUTILSDIR/include"
	AC_ARG_WITH(binutils-libname,
		[  --with-binutils-libname       Lib dir name under binutils installation; @<:@lib@:>@]],
		BINUTILSLIB=$withval)
	if test "$BINUTILSLIB" = ""; then
		BINUTILSLIB="lib"
	fi
	LDFLAGS="$LDFLAGS -L$BINUTILSDIR/$BINUTILSLIB -Xlinker -R -Xlinker $BINUTILSDIR/$BINUTILSLIB"
	OP_LDFLAGS="$LDFLAGS"
	OP_CPPFLAGS="$CPPFLAGS"
else
	OP_LDFLAGS=
	OP_CPPFLAGS=
fi

# We can't restore original flag values for CPPFLAGS and LDFLAGS until we're done
# checking for bfd.h and libiberty.h (in AX_BINUTILS).

AC_PROG_CC
AC_PROG_CPP
AC_PROG_CXX
AC_CHECK_PROG(LD,ld,ld,)
test "$LD" || AC_ERROR(ld not found)

# --with-kernel for cross compilation
AC_ARG_WITH(kernel,
[AS_HELP_STRING([--with-kernel=dir], [Path to kernel include directory (e.g. /tmp/linux-xyz) to use.
If this option is not specified, configure will look for kernel header files in the usual installation location
for a kernel-headers package -- /usr. Use this option in cross-compile enviroments
or in situations where the host system does not support perf_events but you wish to build binaries
for a target system that does support perf_events. Because of OProfile's use of syscalls,
kernel headers used during build must match the architecture of the intended target system.
NOTE:  Run the command 'make headers_install INSTALL_HDR_PATH=<kernel-hdrs-install-dir>'
from the root directory of your kernel source tree, and use <kernel-hdrs-install-dir> for
oprofile's '--with-kernel' configure option.])],
KERNELDIR=$withval)


dnl Check kernel version for perf_events supported
if test "$KERNELDIR" != ""; then
	if test -d $KERNELDIR; then
		KINC="$KERNELDIR/include"
		PERF_EVENT_FLAGS=" -I$KERNELDIR/include"
		AC_SUBST(PERF_EVENT_FLAGS)
		PERF_EVENT_H="$KERNELDIR/include/linux/perf_event.h"
	else
		echo "$KERNELDIR does not exist."
	fi
else
	PERF_EVENT_H="/usr/include/linux/perf_event.h"
fi

PERF_EVENT_H_EXISTS="no"
kernel_may_have_perf_events_support="no"
AX_KERNEL_VERSION(2, 6, 31, <=, kernel_may_have_perf_events_support="yes",
kernel_has_perf_events_support="no")

dnl The AX_KERNEL_VERSION macro may return kernel_may_have_perf_events_support="yes",
dnl indicating a partial answer.  Some architectures do not implement the Performance
dnl Events Kernel Subsystem even with kernel versions > 2.6.31 -- i.e., not even
dnl implementing the perf_event_open syscall to return ENOSYS.  So the check below
dnl will identify and handle such situations.

if test "$kernel_may_have_perf_events_support" = "yes"; then
	AC_CHECK_HEADER($PERF_EVENT_H,PERF_EVENT_H_EXISTS="yes")
	AC_MSG_CHECKING([kernel supports perf_events])
	if test "$PERF_EVENT_H_EXISTS" = "yes"; then
		rm -f test-for-PERF_EVENT_OPEN
		AC_LANG_CONFTEST(
			[AC_LANG_PROGRAM([[#include <linux/perf_event.h>
				#include <asm/unistd.h>
				#include <sys/types.h>
				#include <string.h>
			]],
			[[struct perf_event_attr attr;
				pid_t pid;
				memset(&attr, 0, sizeof(attr));
				attr.size = sizeof(attr);
				attr.sample_type = PERF_SAMPLE_IP;
				pid = getpid();
				syscall(__NR_perf_event_open, &attr, pid, 0, -1, 0);
				]])
		])
		$CC conftest.$ac_ext $CFLAGS $LDFLAGS $LIBS $PERF_EVENT_FLAGS -o test-for-PERF_EVENT_OPEN  > /dev/null 2>&1
		if test -f test-for-PERF_EVENT_OPEN; then
			kernel_has_perf_events_support="yes"
			AC_MSG_RESULT(yes)
		else
			AC_MSG_RESULT(no)
			kernel_has_perf_events_support="no"
		fi
		rm -f test-for-PERF_EVENT_OPEN
	else
		AC_MSG_RESULT(unknown -- perf_event.h not found)
	fi
else
	AC_MSG_RESULT(kernel supports perf_events... no)
	kernel_has_perf_events_support="no"
fi

AM_CONDITIONAL(BUILD_FOR_PERF_EVENT, test "$kernel_has_perf_events_support" = "yes")

if test "$kernel_has_perf_events_support" = "yes"; then
	HAVE_PERF_EVENTS='1'
	AC_MSG_CHECKING([whether PERF_RECORD_MISC_GUEST_KERNEL is defined in perf_event.h])
	rm -f test-for-PERF_GUEST
	AC_LANG_CONFTEST(
		[AC_LANG_PROGRAM([[#include <linux/perf_event.h>]],
			[[unsigned int pr_guest_kern = PERF_RECORD_MISC_GUEST_KERNEL;
			unsigned int pr_guest_user = PERF_RECORD_MISC_GUEST_USER;]])
		])
	$CC conftest.$ac_ext $CFLAGS $LDFLAGS $LIBS $PERF_EVENT_FLAGS -o test-for-PERF_GUEST  > /dev/null 2>&1
	if test -f test-for-PERF_GUEST; then
		echo "yes"
		HAVE_PERF_GUEST_MACROS='1'
	else
		echo "no"
		HAVE_PERF_GUEST_MACROS='0'
	fi
	AC_DEFINE_UNQUOTED(HAVE_PERF_GUEST_MACROS, $HAVE_PERF_GUEST_MACROS, [PERF_RECORD_MISC_GUEST_KERNEL is defined in perf_event.h])
	rm -f test-for-PERF_GUEST*

	AC_MSG_CHECKING([whether precise_ip is defined in perf_event.h])
	rm -f test-for-precise-ip
	AC_LANG_CONFTEST(
		[AC_LANG_PROGRAM([[#include <linux/perf_event.h>]],
			[[struct perf_event_attr attr;
			attr.precise_ip = 2;]])
		])
	$CC conftest.$ac_ext $CFLAGS $LDFLAGS $LIBS $PERF_EVENT_FLAGS -o test-for-precise-ip  > /dev/null 2>&1
	if test -f test-for-precise-ip; then
		echo "yes"
		HAVE_PERF_PRECISE_IP='1'
	else
		echo "no"
		HAVE_PERF_PRECISE_IP='0'
	fi
	AC_DEFINE_UNQUOTED(HAVE_PERF_PRECISE_IP, $HAVE_PERF_PRECISE_IP, [precise_ip is defined in perf_event.h])
	rm -f test-for-precise-ip*

else
	HAVE_PERF_EVENTS='0'
	AC_MSG_RESULT([No perf_events support available; falling back to legacy oprofile])
fi

AC_DEFINE_UNQUOTED(HAVE_PERF_EVENTS, $HAVE_PERF_EVENTS, [Kernel support for perf_events exists])
AC_CANONICAL_HOST
if test "$HAVE_PERF_EVENTS" = "1"; then
	PFM_LIB=
	if test "$host_cpu" = "powerpc64"; then
		AC_CHECK_HEADER(perfmon/pfmlib.h,,[AC_MSG_ERROR([pfmlib.h not found; usually provided in papi devel package])])
		AC_CHECK_LIB(pfm,pfm_get_os_event_encoding, HAVE_LIBPFM3='0'; HAVE_LIBPFM='1', [
			AC_CHECK_LIB(pfm, pfm_get_event_name, HAVE_LIBPFM3='1'; HAVE_LIBPFM='1',
			[AC_MSG_ERROR([libpfm not found; usually provided in papi devel package])])])
		PFM_LIB="-lpfm"
		AC_DEFINE_UNQUOTED(HAVE_LIBPFM3, $HAVE_LIBPFM3, [Define to 1 if using libpfm3; 0 if using newer libpfm])
		AC_DEFINE_UNQUOTED(HAVE_LIBPFM, $HAVE_LIBPFM, [Define to 1 if libpfm is available])
	fi
	AC_SUBST(PFM_LIB)
fi

AC_ARG_WITH(java,
[  --with-java=java-home        Path to Java home directory (default is "no"; "yes" will use /usr as Java home)],
JAVA_HOMEDIR=$with_java, [with_java=no])

if test "x$with_java" = "xyes"; then
	JAVA_HOMEDIR="/usr"
else if test "x$with_java" = "xno"; then
	JAVA_HOMEDIR=
	fi
fi
AC_SUBST(JAVA_HOMEDIR)

if test -n "$JAVA_HOMEDIR"; then
	AC_CHECK_FILE("$JAVA_HOMEDIR/include/jvmti.h",JVMTI_H_EXISTS="yes",)
	AC_CHECK_FILE("$JAVA_HOMEDIR/include/jvmpi.h",JVMPI_H_EXISTS="yes",)
fi

AM_CONDITIONAL(BUILD_JVMTI_AGENT, test -n "$JVMTI_H_EXISTS")
AM_CONDITIONAL(BUILD_JVMPI_AGENT, test -n "$JVMPI_H_EXISTS")

AX_MALLOC_ATTRIBUTE
AX_BUILTIN_EXPECT

AC_SUBST(EXTRA_CFLAGS_MODULE)

topdir=`pwd`
AC_SUBST(topdir)

AX_EXTRA_DIRS

ORIG_SAVE_LIBS="$LIBS"

dnl advanced glibc features which we need but may not be present
AC_CHECK_FUNCS(sched_setaffinity perfmonctl)

AC_CHECK_LIB(popt, poptGetContext,, AC_MSG_ERROR([popt library not found]))
AX_BINUTILS
# Now we can restore original flag values, and may as well do the
# AC_SUBST, too.
CPPFLAGS="$CPPFLAGS_SAVE"
LDFLAGS="$LDFLAGS_SAVE"
AC_SUBST(OP_CPPFLAGS)
AC_SUBST(OP_LDFLAGS)

AX_CELL_SPU

# C++ tests

AC_LANG_CPLUSPLUS

AX_POPT_CONST
AX_CHECK_SSTREAM

dnl bfd.h pre 1998 check only for gnu 2.xx series, so gcc 3.0 can't compile it
AC_MSG_CHECKING([whether bfd defines bool])
AC_TRY_COMPILE([#include <bfd.h>], [],
AC_MSG_RESULT([no]);,
AC_MSG_RESULT([yes]); AC_DEFINE(TRUE_FALSE_ALREADY_DEFINED, 1, [whether bfd.h defines bool values]))

dnl smart demangler need to know what are the underlined type for some typedef
AX_TYPEDEFED_NAME(size_t, "unsigned" "unsigned long", SIZE_T_TYPE)
AC_SUBST(SIZE_T_TYPE)
AX_TYPEDEFED_NAME(ptrdiff_t, "int" "long", PTRDIFF_T_TYPE)
AC_SUBST(PTRDIFF_T_TYPE)

AC_PATH_XTRA
ORIG_X_SAVE_LIBS="$LIBS"
LIBS="$X_PRE_LIBS $LIBS $X_LIBS -lX11 $X_EXTRA_LIBS"
X_LIBS="$LIBS"
AC_SUBST(X_LIBS)

AC_ARG_ENABLE(gui,[  --enable-gui  compile with gui component (qt3|qt4|yes|no),
		     if not given or set to yes, gui defaults to qt3],, enable_gui=qt3)

if test "x$enable_gui"  = "xqt3"  || test "x$enable_gui"  = "xyes"; then
	QT_VERSION=3
	QT_DO_IT_ALL
	if test -n "$QT_LIB"; then
		QT_LIBS="$QT_LIB $QT_LDFLAGS"
	fi
	QT_CFLAGS="$QT_INCLUDES"
fi

if test "x$enable_gui"  = "xqt4"; then
	QT_VERSION=4
	PKG_CHECK_MODULES(QT, Qt3Support QtGui QtCore ,,[
	echo "You requested QT4 but its build files are not available. Exiting now."
	exit
	])
	MOC=$(pkg-config --variable=moc_location QtCore)
	UIC=$(dirname $(pkg-config --variable=uic_location QtCore))/uic3
	QT_VERSION=$(pkg-config --modversion QtCore)

dnl following are some sanity tests and workarounds for buggy QtCore.pc files
	if test "x$MOC" = "x"; then
		echo "WARNING: Your QtCore.pc file is buggy, it doesn't provide the variable 'moc_location'"
		echo "WARNING: I will try to find it in your PATH ..."
		AC_CHECK_PROG(MOC, moc, moc)
		if test "x$MOC" = "x"; then
			echo "WARNING: You can fix this by adding the location of moc to your path."
			echo "WARNING: Exiting now."
			exit
		fi
	fi

	if test "x$UIC" = "x3"; then
		echo "WARNING: Your QtCore.pc file is buggy, it doesn't provide the variable 'uic_location'"
		echo "WARNING: I will try to find it in your PATH ..."
		AC_CHECK_PROG(UIChelp, uic3, uic3)
		if test "x$UIChelp" = "x"; then
			echo "WARNING: You can fix this by adding the location of uic3 to your path."
			echo "WARNING: Exiting now."
			exit
		else
			UIC="$UIChelp"
		fi
	fi

	flags_has_qt3support=$(echo $QT_CFLAGS | grep QT3_SUPPORT)
	if test "x$flags_has_qt3support" = "x" ; then
		echo "WARNING: Your Qt3Support package is buggy; it dosn't include the 'QT3_SUPPORT' flag"
		echo "WARNING: adding it manually"
		QT_CFLAGS="$QT_CFLAGS -DQT3_SUPPORT"
	fi
fi
AM_CONDITIONAL(have_qt, test -n "$QT_LIBS")

LIBS="$ORIG_X_SAVE_LIBS"

dnl enable pch for c++
AC_ARG_ENABLE(pch,
	[  --enable-pch                 enable precompiled header (default is disabled)],
	enable_pch=$enableval, enable_pch=no)
if test "$enable_pch" = yes; then
	AX_CHECK_PRECOMPILED_HEADER(OP_CXXFLAGS)
fi

AX_CHECK_DOCBOOK

dnl finally restore the original libs setting
LIBS="$ORIG_SAVE_LIBS"
LIBERTY_LIBS="-liberty $DL_LIB $INTL_LIB"
BFD_LIBS="-lbfd -liberty $DL_LIB $INTL_LIB $Z_LIB"
POPT_LIBS="-lpopt"
AC_SUBST(LIBERTY_LIBS)
AC_SUBST(BFD_LIBS)
AC_SUBST(POPT_LIBS)

# do NOT put tests here, they will fail in the case X is not installed !


AX_CFLAGS_OPTION(OP_CFLAGS,[-W])
AX_CXXFLAGS_OPTION(OP_CXXFLAGS,[-W])
AX_CFLAGS_OPTION(OP_CFLAGS,[-Wall])
AX_CXXFLAGS_OPTION(OP_CXXFLAGS,[-Wall])
AX_CFLAGS_OPTION(OP_CFLAGS,[-fno-common])
AX_CXXFLAGS_OPTION(OP_CXXFLAGS,[-fno-common])
AX_CXXFLAGS_OPTION(OP_CXXFLAGS,[-ftemplate-depth-50])
AX_CFLAGS_OPTION(OP_CFLAGS,[-Wdeclaration-after-statement])

dnl enable option to use GCC test coverage
AC_ARG_ENABLE(gcov,
	[  --enable-gcov                enable option for gcov coverage testing (default is disabled)],
	enable_gcov=$enableval, enable_gcov=no)
if test "$enable_gcov" = yes; then
	AX_CFLAGS_OPTION(OP_CFLAGS,[-fprofile-arcs])
	AX_CXXFLAGS_OPTION(OP_CXXFLAGS,[-fprofile-arcs])
	AX_CFLAGS_OPTION(OP_CFLAGS,[-ftest-coverage])
	AX_CXXFLAGS_OPTION(OP_CXXFLAGS,[-ftest-coverage])
fi

AC_ARG_ENABLE(werror,
	[  --disable-werror            disable -Werror flag (default is enabled for non-release)],
        enable_werror=$enableval, enable_werror=yes)
if test "$enable_werror" = yes; then
	# enable -Werror for non-release versions.
	if echo "$VERSION" | grep git >/dev/null; then
		AX_CFLAGS_OPTION(OP_CFLAGS,[-Werror])
		AX_CXXFLAGS_OPTION(OP_CXXFLAGS,[-Werror])
	fi
fi

AC_ARG_ENABLE(optimization,
	[  --disable-optimization      disable optimization flags (default is enabled)],
	enable_optimization=$enableval, enable_optimisation=yes)
if test "$enable_optimization" = "no"; then
	CFLAGS=`echo $CFLAGS | sed 's/-O2//g'`
	CXXFLAGS=`echo $CXXFLAGS | sed 's/-O2//g'`
fi

AC_ARG_ENABLE(account-check,
	[  --disable-account-check     disable account check (default is enabled)],
	enable_account_check=$enableval, enable_account_check=yes)

AM_CONDITIONAL(CHECK_ACCOUNT, test "x$enable_account_check" = "xyes")

AC_SUBST(OP_CFLAGS)
AC_SUBST(OP_CXXFLAGS)

# fixups for config.h
if test "$prefix" = "NONE"; then
	my_op_prefix="$ac_default_prefix"
else
	my_op_prefix="$prefix"
fi
my_op_datadir="$my_op_prefix/share"

if test "$exec_prefix" = "NONE"; then
	my_op_exec_prefix="$my_op_prefix"
else
	my_op_exec_prefix="$exec_prefix"
fi
my_op_bindir="$my_op_exec_prefix/bin"

OP_DATADIR=`eval echo "$my_op_datadir/$PACKAGE/"`
AC_DEFINE_UNQUOTED(OP_DATADIR, "$OP_DATADIR", [package data directory])
OP_BINDIR=`eval echo "$my_op_bindir/"`
AC_DEFINE_UNQUOTED(OP_BINDIR, "$OP_BINDIR", [package binary directory])
OP_DOCDIR=`eval echo "${my_op_prefix}/share/doc/$PACKAGE/"`
AC_SUBST(OP_DOCDIR)

AC_OUTPUT(Makefile \
	pe_counting/Makefile \
	libpe_utils/Makefile \
	pe_profiling/Makefile \
	libperf_events/Makefile \
	m4/Makefile \
	libutil/Makefile \
	libutil/tests/Makefile \
	libutil++/Makefile \
	libutil++/tests/Makefile \
	libop/Makefile \
	libop/tests/Makefile \
	libopagent/Makefile \
	libopt++/Makefile \
	libdb/Makefile \
	libdb/tests/Makefile \
	libabi/Makefile \
	libabi/tests/Makefile \
	libregex/Makefile \
	libregex/tests/Makefile \
	libregex/stl.pat \
	libregex/tests/mangled-name \
	daemon/Makefile \
	events/Makefile \
	utils/Makefile \
	doc/Makefile \
	doc/xsl/catalog-1.xml \
	doc/oprofile.1 \
	doc/opcontrol.1 \
	doc/ophelp.1 \
	doc/op-check-perfevents.1 \
	doc/oprof_start.1 \
	doc/opreport.1 \
	doc/opannotate.1 \
	doc/opgprof.1 \
	doc/oparchive.1 \
	doc/opimport.1 \
	doc/operf.1 \
	doc/ocount.1 \
	doc/srcdoc/Doxyfile \
	libpp/Makefile \
	opjitconv/Makefile \
	pp/Makefile \
	gui/Makefile \
	gui/ui/Makefile \
	agents/Makefile \
	agents/jvmti/Makefile \
	agents/jvmpi/Makefile)

AX_COPY_IF_CHANGE(doc/xsl/catalog-1.xml, doc/xsl/catalog.xml)

if test "x$enable_gui" = "xno" ; then
	echo "No GUI will be built as it was explicitly disabled."
else
	if test -z "$QT_LIBS"; then
		echo "Warning: QT version $QT_VERSION was requested but not found. No GUI will be built."
	else
		echo "Building GUI with QT $QT_VERSION"
	 fi
fi

if ! test "x$enable_account_check" = "xyes"; then
	:
elif test "`getent passwd oprofile 2>/dev/null`" == "" || \
     test "`getent group oprofile 2>/dev/null`" == ""; then
	if test `id -u` != "0"; then
		echo "Warning: The user account 'oprofile:oprofile' does not exist on the system."
		echo "         To profile JITed code, this special user account must exist."
		echo "         Please ask your system administrator to add the following user and group:"
		echo "             user name : 'oprofile'"
		echo "             group name: 'oprofile'"
		echo "         The 'oprofile' group must be the default group for the 'oprofile' user."
	else
		echo "Warning: The user account 'oprofile:oprofile' does not exist on the system."
		echo "         To profile JITed code, this special user account must exist."
		echo "         Please add the following user and group:"
		echo "             user name : 'oprofile'"
		echo "             group name: 'oprofile'"
		echo "         The 'oprofile' group must be the default group for the 'oprofile' user."
	fi
fi

if  test "$PERF_EVENT_H_EXISTS" != "yes" && test "$kernel_may_have_perf_events_support" = "yes"; then
	echo "Warning: perf_event.h not found.  Either install the kernel headers package or"
	echo "use the --with-kernel option if you want the non-root, single application"
	echo "profiling support provided by operf."
	echo ""
	echo "If you run 'make' now, only the legacy ocontrol-based profiler will be built."
fi

if test "$KERNELDIR" != "" && test "$kernel_has_perf_events_support" != "yes"; then
	if ! test -d $KERNELDIR; then
		echo "WARNING: You passed '--with-kernel=$KERNELDIR', but $KERNELDIR"
		echo "does not exist."
	else
		echo "Warning: You requested to build with the '--with-kernel' option, but your kernel"
		echo "headers were not accessible at the given location. Be sure you have run the following"
		echo "command from within your kernel source tree:"
		echo "     make headers_install INSTALL_HDR_PATH=<kernel-hdrs-install-dir>"
		echo "Then pass <kernel-hdrs-install-dir> to oprofile's '--with-kernel' configure option."
	fi
	echo ""
	echo "If you run 'make' now, only the legacy ocontrol-based profiler will be built."
fi
