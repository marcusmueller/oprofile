# Configure paths and libs when using Qt2 GUI
##    -*- shell-script -*-

##    Based on KDE file written by :
##    Copyright (C) 1997 Janos Farkas (chexum@shadow.banki.hu)
##              (C) 1997 Stephan Kulow (coolo@kde.org)

##    This file is free software; you can redistribute it and/or
##    modify it under the terms of the GNU Library General Public
##    License as published by the Free Software Foundation; either
##    version 2 of the License, or (at your option) any later version.

##    This library is distributed in the hope that it will be useful,
##    but WITHOUT ANY WARRANTY; without even the implied warranty of
##    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
##    Library General Public License for more details.

##    You should have received a copy of the GNU Library General Public License
##    along with this library; see the file COPYING.LIB.  If not, write to
##    the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
##    Boston, MA 02111-1307, USA.       

dnl Sanity check, we trust than we found qt but now we check than
dnl the used compiler is able to compile/link a basic qt program
dnl $1: include dir, $2: qt lib dir, $3: qt lib name, $4: output receiving
dnl YES/NO value
AC_DEFUN(CHECK_QT_LINK,
[
ac_cxxflags_safe="$CXXFLAGS"
ac_ldflags_safe="$LDFLAGS"
ac_libs_safe="$LIBS"

CXXFLAGS="$CXXFLAGS -I$1"
LDFLAGS="-L$2 $X_LDFLAGS"
LIBS="$LIBS -l$3 -lXext -lX11 $LIBSOCKET"

QT2_PRINT_PROGRAM

if AC_TRY_EVAL(ac_link) && test -s conftest; then
  $4=YES
else
  echo "configure: failed program was:" >&AC_FD_CC
  $4=NO
fi
rm -f conftest*
CXXFLAGS="$ac_cxxflags_safe"
LDFLAGS="$ac_ldflags_safe"
LIBS="$ac_libs_safe"
])

## ------------------------------------------------------------------------
## Find a file (or one of more files in a list of dirs)
## ------------------------------------------------------------------------
##
AC_DEFUN(AC_FIND_FILE,
[
$3=NO
for i in $2;
do
  for j in $1;
  do
    if test -r "$i/$j"; then
      $3=$i
      break 2
    fi
  done
done
])
 
AC_DEFUN(QT2_FIND_PATH,
[
   AC_MSG_CHECKING([for $1])
   AC_CACHE_VAL(qt2_cv_path_$1,
   [
     qt2_cv_path_$1="NONE"
     if test -n "$$2"; then
        qt2_cv_path_$1="$$2";
     else
        dirs="$3"
        qt2_save_IFS=$IFS
        IFS=':'
        for dir in $PATH; do
          dirs="$dirs $dir"
        done
        IFS=$qt2_save_IFS
 
        for dir in $dirs; do
          if test -x "$dir/$1"; then
            if test -n "$5"
            then
              evalstr="$dir/$1 $5 2>&1 "
              if eval $evalstr; then
                qt2_cv_path_$1="$dir/$1"
                break
              fi
            else
                qt2_cv_path_$1="$dir/$1"
                break
            fi
          fi
        done
 
     fi
 
   ])
 
   if test -z "$qt2_cv_path_$1" || test "$qt2_cv_path_$1" = "NONE"; then
      AC_MSG_RESULT(not found)
      $4
   else
     AC_MSG_RESULT($qt2_cv_path_$1)
     $2=$qt2_cv_path_$1
   fi
])
 
dnl ------------------------------------------------------------------------
dnl Find the UIC compiler if available
dnl ------------------------------------------------------------------------
AC_DEFUN(QT2_AC_PATH_UIC,
[
   QT2_FIND_PATH(uic, UIC, [$ac_qt2_bindir $QTDIR/bin \
            /usr/bin /usr/X11R6/bin /usr/lib/qt2/bin \
            /usr/local/qt2/bin /usr/local/qt/bin /usr/lib/qt/bin], )
   AC_SUBST(UIC)
])
 
dnl ------------------------------------------------------------------------
dnl Find the meta object compiler in the PATH, in $QTDIR/bin, and some
dnl more usual places
dnl ------------------------------------------------------------------------
dnl
AC_DEFUN(QT2_AC_PATH_MOC,
[
   QT2_FIND_PATH(moc, MOC, [$ac_qt2_bindir $QTDIR/bin \
            /usr/bin /usr/X11R6/bin /usr/lib/qt2/bin \
            /usr/local/qt2/bin /usr/local/qt/bin /usr/lib/qt/bin],)

   QT2_FIND_PATH(moc2, MOC2, [$ac_qt2_bindir $QTDIR/bin \
            /usr/bin /usr/X11R6/bin /usr/lib/qt2/bin \
            /usr/local/qt2/bin /usr/local/qt/bin /usr/lib/qt/bin],)
 
   if ! test -z "$MOC2"; then
     MOC="$MOC2";
   fi 
 
   if test -z "$MOC"; then
     if test -n "$ac_cv_path_qt2moc"; then
       output=`eval "$ac_cv_path_qt2moc --help 2>&1 | sed -e '1q' | grep Qt"`
     fi
     echo "configure:__oline__: tried to call $ac_cv_path_qt2moc --help 2>&1 | sed -e '1q' | grep Qt" >&AC_FD_CC
     echo "configure:__oline__: moc output: $output" >&AC_FD_CC

   fi

   AC_SUBST(MOC)
])

AC_DEFUN(QT2_PRINT_PROGRAM,
[
AC_LANG_CPLUSPLUS
cat > conftest.$ac_ext <<EOF
#include "confdefs.h"
#include <qmovie.h>
#include <qapplication.h>
int main() {
  QMovie m;
  m.setSpeed(20);
  return 0;
}
EOF
])

dnl check for -lqt2 not -lqt
AC_DEFUN(QT2_CHECK_LIB_NAME,
[
ac_cxxflags_safe="$CXXFLAGS"
ac_ldflags_safe="$LDFLAGS"
ac_libs_safe="$LIBS"

CXXFLAGS="$CXXFLAGS -I$qt2_incdir"
LDFLAGS="-L$qt2_libdir $X_LDFLAGS"
LIBS="$LIBS -lqt2 -lXext -lX11 $LIBSOCKET"

QT2_PRINT_PROGRAM

if AC_TRY_EVAL(ac_link) && test -s conftest; then
  rm -f conftest*
  ac_qt2_name="-lqt2"
else
  echo "configure: failed program was:" >&AC_FD_CC
  cat conftest.$ac_ext >&AC_FD_CC
fi
rm -f conftest*
CXXFLAGS="$ac_cxxflags_safe"
LDFLAGS="$ac_ldflags_safe"
LIBS="$ac_libs_safe"
])

## ------------------------------------------------------------------------
## Try to find the Qt2 headers and libraries.
## $(QT_LDFLAGS) will be -Lqt2liblocation (if needed)
## and $(QT_INCLUDES) will be -Iqt2hdrlocation (if needed)
## ------------------------------------------------------------------------
##
AC_DEFUN(QT2_AC_PATH,
[
AC_MSG_CHECKING([for Qt 2])
ac_qt2_includes=NO ac_qt2_libraries=NO ac_qt2_bindir=NO
qt2_libraries=""
qt2_includes=""
 
AC_ARG_WITH(qt2-dir,
    [  --with-qt2-dir           where the root of Qt2 is installed ],
    [  ac_qt2_includes=`eval echo "$withval"/include`
       ac_qt2_libraries=`eval echo "$withval"/lib`
       ac_qt2_bindir=`eval echo "$withval"/bin`
    ])

AC_ARG_WITH(qt2-includes,
    [  --with-qt2-includes      where the Qt2 includes are. ],
    [ 
       ac_qt2_includes=`eval echo "$withval"`
    ])
   
qt2_libs_given=no

AC_ARG_WITH(qt2-libraries,
    [  --with-qt2-libraries     where the Qt2 library is installed.],
    [  ac_qt2_libraries=`eval echo "$withval"`
       qt2_libs_given=yes
    ])

if test "$ac_qt2_includes" = NO || test "$ac_qt2_libraries" = NO; then

AC_CACHE_VAL(ac_cv_have_qt2,
[#try to guess Qt2 locations

qt2_incdirs="$ac_qt2_includes /usr/lib/qt2/include /usr/local/qt2/include /usr/include/qt2 /usr/X11R6/include/X11/qt2 /usr/lib/qt/include /usr/local/qt/include /usr/include/qt /usr/include /usr/X11R6/include/X11/qt $x_includes $QTINC"
test -n "$QTDIR" && qt2_incdirs="$QTDIR/include/qt2 $QTDIR/include/qt $QTDIR/include $QTDIR $qt2_incdirs"
AC_FIND_FILE(qmovie.h, $qt2_incdirs, qt2_incdir)
ac_qt2_includes="$qt2_incdir"

if test ! "$ac_qt2_libraries" = "NO"; then
  qt2_libdirs="$ac_qt2_libraries"
fi

qt2_libdirs="$qt2_libdirs /usr/lib/qt2/lib /usr/lib/qt/lib /usr/X11R6/lib /usr/lib /usr/local/qt2/lib /usr/lib/qt2 /usr/local/qt/lib /usr/lib/qt $x_libraries $QTLIB"
test -n "$QTDIR" && qt2_libdirs="$QTDIR/lib $QTDIR $qt2_libdirs"

test=NONE
qt2_libdir=NONE
for dir in $qt2_libdirs; do
  try="ls -1 $dir/libqt*"
  if test=`eval $try 2> /dev/null`; then qt2_libdir=$dir; break; else echo "tried $dir" >&AC_FD_CC ; fi
done

ac_qt2_name="-lqt"
 
AC_FIND_FILE(libqt.so.2.2.3 libqt.so.2.2 libqt.so.2 libqt2.so libqt.a libqt.sl, $qt2_libdirs, qt2_libdir)
 
ac_qt2_libraries="$qt2_libdir"

QT2_CHECK_LIB_NAME

if test "$ac_qt2_name" = "-lqt"; then
  ac_qt2_libraries="$qt2_libdir"

  ac_cxxflags_safe="$CXXFLAGS"
  ac_ldflags_safe="$LDFLAGS"
  ac_libs_safe="$LIBS"

  CXXFLAGS="$CXXFLAGS -I$qt2_incdir"
  LDFLAGS="-L$qt2_libdir $X_LDFLAGS"
  LIBS="$LIBS -lqt -lXext -lX11 $LIBSOCKET"

  QT2_PRINT_PROGRAM

  if AC_TRY_EVAL(ac_link) && test -s conftest; then
    rm -f conftest*
  else
      echo "configure: failed program was:" >&AC_FD_CC
      cat conftest.$ac_ext >&AC_FD_CC
      ac_qt2_libraries="NO"
  fi
  rm -f conftest*
  CXXFLAGS="$ac_cxxflags_safe"
  LDFLAGS="$ac_ldflags_safe"
  LIBS="$ac_libs_safe"
fi

if test "$ac_qt2_includes" = NO || test "$ac_qt2_libraries" = NO; then
  ac_cv_have_qt2="have_qt=no"
  ac_qt2_notfound=""
  if test "$ac_qt2_includes" = NO; then
    if test "$ac_qt2_libraries" = NO; then
      ac_qt2_notfound="(headers and libraries)";
    else
      ac_qt2_notfound="(headers)";
    fi
  else
    ac_qt2_notfound="(libraries)";
  fi
else
  have_qt="yes"
fi
])
else
  dnl libs and headers supplied. Need to check lib name
  qt2_incdir="$ac_qt2_includes"
  qt2_libdir="$ac_qt2_libraries" 
  ac_qt2_name="-lqt"
  QT2_CHECK_LIB_NAME
  have_qt="yes"
fi

eval "$ac_cv_have_qt2"

if test "$have_qt" != yes; then
  AC_MSG_RESULT([$have_qt]);
else
  AC_MSG_RESULT([libraries $ac_qt2_libraries, headers $ac_qt2_includes])
 
  qt2_libraries="$ac_qt2_libraries"
  qt2_includes="$ac_qt2_includes"

  dnl check it is Qt2

  SAVE_CXXFLAGS="$CXXFLAGS"
  CXXFLAGS="$CXXFLAGS -I$qt2_includes -L$qt2_libraries"
  dnl specify we are definitely C++ compiling first
  AC_LANG_CPLUSPLUS

  dnl first check we can compile 
  AC_TRY_COMPILE([
  #include <qglobal.h>
  ],
  [],
  ac_qt2_ok=yes,
  ac_qt2_ok=no
  )

  if test "$ac_qt2_ok" = "yes"; then 
    AC_TRY_COMPILE([
    #include <qglobal.h>
    ],
      [
      #if (QT_VERSION < 221)
      break_me_(\\\);
      #endif
    ],
    ac_qt2_ok=yes,
    ac_qt2_ok=no
    )
    test "$ac_qt2_ok" = no && AC_MSG_WARN([Found an earlier version of Qt - you must specify the path to Qt2])
  fi
 
  CXXFLAGS="$SAVE_CXXFLAGS"

  have_qt=$ac_qt2_ok
fi

ac_cv_have_qt2="have_qt=$have_qt ac_qt2_name=$ac_qt2_name \
  ac_qt2_includes=$ac_qt2_includes ac_qt2_libraries=$ac_qt2_libraries"
 
AC_SUBST(qt2_libraries)
AC_SUBST(qt2_includes)

if test "$qt2_includes" = "$x_includes" || test -z "$qt2_includes"; then
 QT_INCLUDES="";
else
 QT_INCLUDES="-I$qt2_includes"
 all_includes="$QT_INCLUDES $all_includes"
fi

if test "$qt2_libraries" = "$x_libraries" || test -z "$qt2_libraries"; then
 QT_LDFLAGS=""
else
 QT_LDFLAGS="-L$qt2_libraries"
 all_libraries="$QT_LDFLAGS $all_libraries"
fi

QT_LIBS="$ac_qt2_name"
 
AC_SUBST(have_qt)
AC_SUBST(QT_INCLUDES)
AC_SUBST(QT_LDFLAGS)
AC_SUBST(QT_LIBS)
QT2_AC_PATH_MOC
QT2_AC_PATH_UIC
])

AC_DEFUN(QT2_DO_IT_ALL,
[
if test "$QT2DIR" != ""; then
	QTDIR=$QT2DIR
fi
QT2_AC_PATH
])

# the test are simple for now, QTQIR must point to qt3 this is sufficient to
# work with kde3 on all sane recent distro.
AC_DEFUN(QT3_DO_IT_ALL,
[
AC_MSG_CHECKING(for qt3)
AC_FIND_FILE(qmovie.h, $QTDIR/include, qt3_incdir)
AC_FIND_FILE(uic, $QTDIR/bin, qt3_bindir)
dnl if this not fail we know we have a valid qt3, other tools are found under
dnl the same sub-dir and so on must be sane.
AC_FIND_FILE(libqt-mt.so.3, $QTDIR/lib, qt3_libdir)

if test "$qt3_incdir" != "NO" -a "$qt3_bindir" != "NO" -a "$qt3_libdir" != "NO"; then
	CHECK_QT_LINK($qt3_incdir, $qt3_libdir, qt-mt, qt_ok)
	if test $qt_ok == "YES"; then
		AC_MSG_RESULT(yes)
		have_qt="yes"
		QT_LDFLAGS=-L"$QTDIR"/lib
		QT_LIBS=-lqt-mt
		QT_INCLUDES=-I"$qt3_incdir"
		MOC="$QTDIR"/bin/moc
		UIC="$QTDIR"/bin/uic
		AC_SUBST(have_qt)
		AC_SUBST(QT_INCLUDES)
		AC_SUBST(QT_LDFLAGS)
		AC_SUBST(QT_LIBS)
		AC_SUBST(UIC)
		AC_SUBST(MOC)
	else
		AC_MSG_RESULT(no)
	fi
else
	AC_MSG_RESULT(no)
fi
])
