AM_CPPFLAGS=-I ${top_srcdir}/libop -I ${top_srcdir}/libutil -I ${top_srcdir}/libdb -I ${top_srcdir}/libopt++

noinst_LIBRARIES = libabi.a
libabi_a_SOURCES = abi.cpp op_abi.cpp

bin_PROGRAMS = op_import

op_import_SOURCES = op_import.cpp
op_import_LDADD = libabi.a ../libdb/libdb.a ../libopt++/libopt++.a ../libutil++/libutil++.a ../libutil/libutil.a

noinst_PROGRAMS = abi_test
 
abi_test_SOURCES = abi_test.cpp
abi_test_LDADD = libabi.a ../libdb/libdb.a ../libopt++/libopt++.a ../libutil++/libutil++.a ../libutil/libutil.a

EXTRA_DIST = abi.h op_abi.h

