AM_CPPFLAGS=-I ${top_srcdir}/libop -I ${top_srcdir}/libutil -I ${top_srcdir}/libdb -I ${top_srcdir}/libopt++ -I ${top_srcdir}/libutil++ -I ${top_srcdir}/libop++

noinst_LIBRARIES = libpp.a
libpp_a_SOURCES = opp_symbol.cpp samples_container.cpp \
 sample_container_imp.cpp counter_array.cpp symbol_container_imp.cpp \
 derive_files.cpp counter_util.cpp opp_symbol.cpp samples_file.cpp \
 opp_samples_files.cpp session.cpp

bin_PROGRAMS = oprofpp op_to_source op_time op_merge

oprofpp_SOURCES = oprofpp.cpp oprofpp_options.cpp
oprofpp_LDADD = libpp.a ../libopt++/libopt++.a ../libop++/libop++.a ../libutil++/libutil++.a ../libutil/libutil.a ../libop/libop.a ../libdb/libdb.a

op_to_source_SOURCES = op_to_source.cpp op_to_source_options.cpp
op_to_source_LDADD = libpp.a ../libopt++/libopt++.a ../libop++/libop++.a ../libutil++/libutil++.a ../libutil/libutil.a ../libop/libop.a ../libdb/libdb.a

op_time_SOURCES = op_time.cpp op_time_options.cpp
op_time_LDADD = libpp.a ../libopt++/libopt++.a ../libop++/libop++.a ../libutil++/libutil++.a ../libutil/libutil.a ../libop/libop.a  ../libdb/libdb.a

op_merge_SOURCES = op_merge.cpp
op_merge_LDADD = libpp.a ../libopt++/libopt++.a ../libop++/libop++.a ../libutil++/libutil++.a ../libutil/libutil.a ../libdb/libdb.a

EXTRA_DIST = counter_array.h opp_samples_files.h samples_container.h counter_util.h opp_symbol.h samples_file.h derive_files.h oprofpp_options.h session.h op_time_options.h outsymbflag.h symbol_container_imp.h op_to_source_options.h sample_container_imp.h symbol_functors.h

