
dnl find pfmlib and check it's the min version
AC_DEFUN(AX_CHECK_PERFMON,
[
	AC_MSG_CHECKING([for libpfm >= 3.0])
	SAVE_LIBS="$LIBS"
	LIBS="$LIBS -lpfm"
	AC_TRY_LINK([
	#include <perfmon/perfmon.h>
	#include <perfmon/pfmlib.h>
	],
	[
	#if PFMLIB_MAJ_VERSION(PFMLIB_VERSION) < 3
	break_me_(\\\);
	#endif
	],
	PFM_LIBS="-lpfm"
	AC_MSG_RESULT(yes),
	AC_MSG_RESULT(no)
	)
	LIBS="$SAVE_LIBS"
	AC_SUBST(PFM_LIBS)
])
