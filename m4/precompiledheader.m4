dnl AX_CXXFLAGS_OPTIONS(var-name, option)
dnl add option to var-name if $CXX support it.
AC_DEFUN(AX_CHECK_PRECOMPILED_HEADER, [
AC_MSG_CHECKING([whether ${CXX} support precompiled header])
AC_LANG_SAVE
AC_LANG_CPLUSPLUS
SAVE_CXXFLAGS=$CXXFLAGS
CXXFLAGS=-Winvalid-pch
AC_TRY_COMPILE(,[;],AC_MSG_RESULT([yes]); $1="${$1} -Winvalid-pch -include pch-c++.h"; $2=yes,AC_MSG_RESULT([no]); $2=no)
CXXFLAGS=$SAVE_CXXFLAGS
AC_LANG_RESTORE
])
