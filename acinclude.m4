
dnl Please leave this alone. I use this file in
dnl oprofile.
FATAL=0

dnl find a binary in the path
AC_DEFUN(QT_FIND_PATH,
[
   AC_MSG_CHECKING([for $1])
   AC_CACHE_VAL(qt_cv_path_$1,
   [
     qt_cv_path_$1="NONE"
     if test -n "$$2"; then
        qt_cv_path_$1="$$2";
     else
        dirs="$3"
        qt_save_IFS=$IFS
        IFS=':'
        for dir in $PATH; do
          dirs="$dirs $dir"
        done
        IFS=$qt_save_IFS
 
        for dir in $dirs; do
          if test -x "$dir/$1"; then
            if test -n "$5"
            then
              evalstr="$dir/$1 $5 2>&1 "
              if eval $evalstr; then
                qt_cv_path_$1="$dir/$1"
                break
              fi
            else
                qt_cv_path_$1="$dir/$1"
                break
            fi
          fi
        done
 
     fi
 
   ])
 
   if test -z "$qt_cv_path_$1" || test "$qt_cv_path_$1" = "NONE"; then
      AC_MSG_RESULT(not found)
      $4
   else
     AC_MSG_RESULT($qt_cv_path_$1)
     $2=$qt_cv_path_$1
   fi
])

dnl Find the uic compiler on the path or in ac_qt_dir
AC_DEFUN(QT_FIND_UIC,
[
	QT_FIND_PATH(uic, ac_uic, $ac_qt_dir/bin)
	if test -z "$ac_uic" -a "$FATAL" = 1; then
		AC_MSG_ERROR([uic binary not found in \$PATH or $ac_qt_dir/bin !])
	fi
])
 
dnl Find the right moc in path/ac_qt_dir
AC_DEFUN(QT_FIND_MOC,
[
	QT_FIND_PATH(moc2, ac_moc2, $ac_qt_dir/bin)
	QT_FIND_PATH(moc, ac_moc1, $ac_qt_dir/bin)

	if test -n "$ac_moc1" -a -n "$ac_moc2"; then
		dnl found both. Prefer Qt3's if it exists else moc2
		AC_MSG_ERROR([FIXME])
	else
		if test -n "$ac_moc1"; then
			ac_moc=$ac_moc1;
		else
			ac_moc=$ac_moc2;
		fi
	fi

	if test -z "$ac_moc"  -a "$FATAL" = 1; then
		AC_MSG_ERROR([moc binary not found in \$PATH or $ac_qt_dir/bin !])
	fi
])

dnl check a particular libname
AC_DEFUN(QT_TRY_LINK,
[
	CXXFLAGS="$BASE_CXXFLAGS $1"
	AC_TRY_LINK([
	#include <qglobal.h>
	#include <qstring.h>
		],
	[
	QString s("mangle_failure");
	#if (QT_VERSION < 221)
	break_me_(\\\);
	#endif
	],
	ac_qt_libname=$1,
	)
	CXXFLAGS="$BASE_CXXFLAGS"
])
 
dnl check we can do a compile
AC_DEFUN(QT_CHECK_COMPILE,
[
	AC_LANG_CPLUSPLUS
	SAVE_CXXFLAGS=$CXXFLAGS
	BASE_CXXFLAGS="$CXXFLAGS $QT_INCLUDES $QT_LDFLAGS" 

	AC_MSG_CHECKING([for Qt library name])
 
	for libname in -lqt3 -lqt2 -lqt -lqt_mt;
	do
		QT_TRY_LINK($libname)
		if test -n "$ac_qt_libname"; then
			break;
		fi
	done

	if test -z "$ac_qt_libname"; then
		AC_MSG_RESULT([failed]) 
		if test "$FATAL" = 1 ; then
			AC_MSG_ERROR([Cannot compile a simple Qt executable. Check you have the right \$QTDIR !])
		fi
	else
		AC_MSG_RESULT([$ac_qt_libname])
	fi
 
	CXXFLAGS=$SAVE_CXXFLAGS
])

dnl start here 
AC_DEFUN(QT_DO_IT_ALL,
[
	AC_ARG_WITH(qt-dir, [  --with-qt-dir           where the root of Qt is installed ],
		[ ac_qt_dir=`eval echo "$withval"/` ])
	 
	AC_ARG_WITH(qt-includes, [  --with-qt-includes      where the Qt includes are. ],
		[ ac_qt_includes=`eval echo "$withval"` ])
	 
	AC_ARG_WITH(qt-libraries, [  --with-qt-libraries     where the Qt library is installed.],
		[  ac_qt_libraries=`eval echo "$withval"` ])

	dnl pay attention to $QTDIR unless overridden
	if test -z "$ac_qt_dir"; then
		ac_qt_dir=$QTDIR
	fi

	dnl derive inc/lib if needed
	if test -n "$ac_qt_dir"; then
		if test -z "$ac_qt_includes"; then
			ac_qt_includes=$ac_qt_dir/include
		fi
		if test -z "$ac_qt_libraries"; then
			ac_qt_libraries=$ac_qt_dir/lib
		fi
	fi

	dnl flags for compilation
	if test -n "$ac_qt_includes"; then
		QT_INCLUDES="-I$ac_qt_includes"
		AC_SUBST(QT_INCLUDES)
	fi
	if test -n "$ac_qt_libraries"; then
		QT_LDFLAGS="-L$ac_qt_libraries"
		AC_SUBST(QT_LDFLAGS)
	fi
 
	QT_FIND_MOC
	MOC=$ac_moc
	AC_SUBST(MOC)
	QT_FIND_UIC
	UIC=$ac_uic
	AC_SUBST(UIC)

	QT_CHECK_COMPILE
 
	QT_LIB=$ac_qt_libname;
	AC_SUBST(QT_LIB)
])
