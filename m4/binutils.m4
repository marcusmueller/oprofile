dnl AX_BINUTILS - check for needed binutils stuff
AC_DEFUN([AX_BINUTILS],
[
dnl some distro have a libiberty.a but does not have a libiberty.h
AC_CHECK_HEADERS(libiberty.h)
AC_CHECK_LIB(iberty, cplus_demangle,, AC_MSG_ERROR([liberty library not found]))
AC_CHECK_FUNCS(xcalloc)
AC_CHECK_FUNCS(xmemdup)
AC_CHECK_LIB(dl, dlopen, LIBS="$LIBS -ldl"; DL_LIB="-ldl", DL_LIB="")
AC_CHECK_LIB(intl, main, LIBS="$LIBS -lintl"; INTL_LIB="-lintl", INTL_LIB="")
AC_CHECK_LIB(bfd, bfd_openr,, AC_MSG_ERROR([bfd library not found]))

AC_LANG_PUSH(C)
SAVE_LIBS=$LIBS
LIBS=" -lbfd -liberty "

# Determine if bfd_get_synthetic_symtab macro is available
OS="`uname`"
if test "$OS" = "Linux"; then
	AC_MSG_CHECKING([whether bfd_get_synthetic_symtab() exists in BFD library])
	rm -f test-for-synth
	AC_COMPILE_IFELSE(
		[AC_LANG_PROGRAM([[#include <bfd.h>]],
			[[asymbol * synthsyms;	bfd * ibfd = 0; 
			long synth_count = bfd_get_synthetic_symtab(ibfd, 0, 0, 0, 0, &synthsyms);
			extern const bfd_target bfd_elf64_powerpc_vec;
			extern const bfd_target bfd_elf64_powerpcle_vec;
			char * ppc_name = bfd_elf64_powerpc_vec.name;
			char * ppcle_name = bfd_elf64_powerpcle_vec.name;]])
		],
		[AC_DEFINE([SYNTHESIZE_SYMBOLS],
			[1],
			[Synthesize special symbols when needed])
		AC_MSG_RESULT([yes])],
		[AC_MSG_RESULT([no])]
	)
	rm -f test-for-synth*

fi
AC_LANG_POP(C)
LIBS=$SAVE_LIBS
]
)
