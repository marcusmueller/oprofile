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

AC_CHECK_LIB(bfd, bfd_openr, LIBS="-lbfd $LIBS"; Z_LIB="",
	[AC_CHECK_LIB(z, compress,
dnl Use a different bfd function here so as not to use cached result from above
		[AC_CHECK_LIB(bfd, bfd_fdopenr, LIBS="-lbfd -lz $LIBS"; Z_LIB="-lz",
			[AC_MSG_ERROR([bfd library not found])], -lz)
		],
		[AC_MSG_ERROR([libz library not found; required by libbfd])])
	]
)

AC_LANG_PUSH(C)
# Determine if bfd_get_synthetic_symtab macro is available
AC_MSG_CHECKING([whether bfd_get_synthetic_symtab() exists in BFD library])
AC_LINK_IFELSE([AC_LANG_PROGRAM([[#include <bfd.h>]
	[#include <stdio.h>]
	[static bfd _ibfd;]],
	[[asymbol * synthsyms;	bfd * ibfd = &_ibfd;
	long synth_count = bfd_get_synthetic_symtab(ibfd, 0, 0, 0, 0, &synthsyms);
	extern const bfd_target powerpc_elf64_vec;
	char *ppc_name = powerpc_elf64_vec.name;
	printf("%s\n", ppc_name);
	]])],
	[AC_MSG_RESULT([yes])
	SYNTHESIZE_SYMBOLS=2],
	[AC_LINK_IFELSE([AC_LANG_PROGRAM([[#include <bfd.h>]
		[#include <stdio.h>]
		[static bfd _ibfd;]],
		[[asymbol * synthsyms;	bfd * ibfd = &_ibfd;
		long synth_count = bfd_get_synthetic_symtab(ibfd, 0, 0, 0, 0, &synthsyms);
		extern const bfd_target bfd_elf64_powerpc_vec;
		char *ppc_name = bfd_elf64_powerpc_vec.name;
		printf("%s\n", ppc_name);
		]])],
		[AC_MSG_RESULT([yes])
		SYNTHESIZE_SYMBOLS=1],
		[AC_MSG_RESULT([no])
		SYNTHESIZE_SYMBOLS=0])
	])
AC_DEFINE_UNQUOTED(SYNTHESIZE_SYMBOLS, $SYNTHESIZE_SYMBOLS, [Synthesize special symbols when needed])

AC_LANG_POP(C)
]
)
