
dnl http://ac-archive.sourceforge.net/C_Support/ax_cflags_gcc_option.html
dnl slightly adapted

AC_DEFUN([AX_CFLAGS_OPTION], [dnl
AS_VAR_PUSHDEF([FLAGS],[CFLAGS])dnl
AS_VAR_PUSHDEF([VAR],[ac_cv_cflags_option_$2])dnl
AC_CACHE_CHECK([${CC} for m4_ifval($2,$2,-option) option],
VAR,[VAR="no, unknown"
 AC_LANG_SAVE
 AC_LANG_C
 ac_save_[]FLAGS="$[]FLAGS"
for ac_arg dnl
in "-pedantic  % m4_ifval($2,$2,-option)"  dnl   GCC
   # 
do FLAGS="$ac_save_[]FLAGS "`echo $ac_arg | sed -e 's,%%.*,,' -e 's,%,,'`
   AC_TRY_COMPILE([],[return 0;],
   [VAR=`echo $ac_arg | sed -e 's,.*% *,,'` ; break])
done
 FLAGS="$ac_save_[]FLAGS"
 AC_LANG_RESTORE
])
case ".$VAR" in
     .ok|.ok,*) m4_ifvaln($3,$3) ;;
   .|.no|.no,*) m4_ifvaln($4,$4) ;;
   *) m4_ifvaln($3,$3,[
   if echo " $[]m4_ifval($1,$1,FLAGS) " | grep " $VAR " 2>&1 >/dev/null
   then AC_RUN_LOG([: m4_ifval($1,$1,FLAGS) does contain $VAR])
   else AC_RUN_LOG([: m4_ifval($1,$1,FLAGS)="$m4_ifval($1,$1,FLAGS) $VAR"])
                      m4_ifval($1,$1,FLAGS)="$m4_ifval($1,$1,FLAGS) $VAR"
   fi ]) ;;
esac
AS_VAR_POPDEF([VAR])dnl
AS_VAR_POPDEF([FLAGS])dnl
])


dnl the only difference - the LANG selection... and the default FLAGS

AC_DEFUN([AX_CXXFLAGS_OPTION], [dnl
AS_VAR_PUSHDEF([FLAGS],[CXXFLAGS])dnl
AS_VAR_PUSHDEF([VAR],[ac_cv_cxxflags_option_$2])dnl
AC_CACHE_CHECK([${CXX} for m4_ifval($2,$2,-option) option],
VAR,[VAR="no, unknown"
 AC_LANG_SAVE
 AC_LANG_CPLUSPLUS
 ac_save_[]FLAGS="$[]FLAGS"
for ac_arg dnl
in "-pedantic  % m4_ifval($2,$2,-option)"  dnl   GCC
   # 
do FLAGS="$ac_save_[]FLAGS "`echo $ac_arg | sed -e 's,%%.*,,' -e 's,%,,'`
   AC_TRY_COMPILE([],[return 0;],
   [VAR=`echo $ac_arg | sed -e 's,.*% *,,'` ; break])
done
 FLAGS="$ac_save_[]FLAGS"
 AC_LANG_RESTORE
])
case ".$VAR" in
     .ok|.ok,*) m4_ifvaln($3,$3) ;;
   .|.no|.no,*) m4_ifvaln($4,$4) ;;
   *) m4_ifvaln($3,$3,[
   if echo " $[]m4_ifval($1,$1,FLAGS) " | grep " $VAR " 2>&1 >/dev/null
   then AC_RUN_LOG([: m4_ifval($1,$1,FLAGS) does contain $VAR])
   else AC_RUN_LOG([: m4_ifval($1,$1,FLAGS)="$m4_ifval($1,$1,FLAGS) $VAR"])
                      m4_ifval($1,$1,FLAGS)="$m4_ifval($1,$1,FLAGS) $VAR"
   fi ]) ;;
esac
AS_VAR_POPDEF([VAR])dnl
AS_VAR_POPDEF([FLAGS])dnl
])
