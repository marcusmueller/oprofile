dnl AX_MSG_RESULT_YN(a)
dnl results "yes" iff a==1, "no" else
AC_DEFUN(AX_MSG_RESULT_YN, [x=no
test "x$1" = "x1" && x=yes
AC_MSG_RESULT($x)])

dnl AX_EGREP(expr, file, action-if-found, action-if-not-found)
dnl egrep for expr in file
AC_DEFUN(AX_EGREP, [dnl
changequote(, )dnl
  if egrep "$1" $2 >/dev/null 2>&1; then
changequote([, ])dnl
  ifelse([$3], , :, [$3])
ifelse([$4], , , [else
  $4
])dnl
fi
])

AC_DEFUN(CPROFILE_EXTRA_PATHS,
[

AC_ARG_WITH(extra-includes,
[  --with-extra-includes=DIR
                          add extra include paths],
  use_extra_includes="$withval",
  use_extra_includes=NO
)
if test -n "$use_extra_includes" && \
        test "$use_extra_includes" != "NO"; then
  ac_save_ifs=$IFS
  IFS=':'
  for dir in $use_extra_includes; do
    extra_includes="$extra_includes -I$dir"
  done
  IFS=$ac_save_ifs
  CPPFLAGS="$CPPFLAGS $extra_includes"
fi

AC_ARG_WITH(extra-libs,
[  --with-extra-libs=DIR   add extra library paths],
  use_extra_libs=$withval,
  use_extra_libs=NO
)
if test -n "$use_extra_libs" && \
        test "$use_extra_libs" != "NO"; then
   ac_save_ifs=$IFS
   IFS=':'
   for dir in $use_extra_libs; do
     extra_libraries="$extra_libraries -L$dir"
   done
   IFS=$ac_save_ifs
fi

AC_SUBST(extra_includes)
AC_SUBST(extra_libraries)

]) 
