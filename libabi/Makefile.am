AM_CPPFLAGS=-I ../libop -I ../libutil -I ../libdb -I ../libopt++

noinst_LIBRARIES = libabi.a
libabi_a_SOURCES = abi.cpp op_abi.cpp

bin_PROGRAMS = op_import

op_import_SOURCES = op_import.cpp
op_import_LDADD = libabi.a ../libdb/libdb.a ../libopt++/libopt++.a ../libutil++/libutil++.a ../libutil/libutil.a

noinst_PROGRAMS = abi_test
 
abi_test_SOURCES = abi_test.cpp
abi_test_LDADD = libabi.a ../libdb/libdb.a ../libopt++/libopt++.a ../libutil++/libutil++.a ../libutil/libutil.a

EXTRA_DIST = abi.h op_abi.h

