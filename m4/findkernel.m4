dnl Find the kernel and handle 2.5 settings
AC_DEFUN(AX_FIND_KERNEL,
[
AC_MSG_CHECKING([for kernel OProfile support])
AC_ARG_WITH(kernel-support, [  --with-kernel-support        Use 2.5 kernel (no kernel source tree required)],
if test "$withval" = "yes"; then
	OPROFILE_25=yes
fi
) 

if test "$OPROFILE_25" != "yes"; then
	dnl  --- Find the Linux kernel, at least the headers ---
 
	AC_SUBST(KSRC)
	KSRC=/lib/modules/`uname -r`/build
	AC_ARG_WITH(linux, [  --with-linux=dir             Path to Linux source tree], KSRC=$withval) 
	KINC=$KSRC/include
	AC_SUBST(KINC)

	AX_KERNEL_OPTION(CONFIG_OPROFILE, OPROFILE_25=yes, OPROFILE_25=no)
	AX_KERNEL_OPTION(CONFIG_OPROFILE_MODULE, OPROFILE_25=yes, OPROFILE_25=$OPROFILE_25)
fi
AC_MSG_RESULT($OPROFILE_25)

AM_CONDITIONAL(kernel_support, test $OPROFILE_25 = yes)
]
)
