# format :
# $name = "regular_definition"
# "pattern" = "substitued_pattern"
# pattern can contain reference to regular definition with ${name}
# this occurence are substitued in pattern by their definition

# regular_definition containing other regular_definition refer always to a
# previously defined regular definition so they can look like recursive but are
# not. op_regex.cpp do sucessive apply of pattern whilst change occur (with a
# hard limit on number of subsitutions) so you can apply successive change to
# translate first to an intermediate simplified form then continue substitution
# in another pattern (see iosfwd section). The number of grouping regexp is
# limited to 10 currently, see static const size_t max_match = 10; in
# op_regex.h. Note than mangled name produce can be illegal as I choose
# to output like vector<type<T>> rather than vector<type<T> >

# man regex is a friend, is it your ?

# FIXME bug: this work very well on x86 but their is a dependancy on
# ptrdiff_t and size_t, I assume here there are int and unsigned

$integer = "\<[0-9]+"
$identifier = "\<[_a-zA-Z][_a-zA-Z0-9]*"
$typename = "${identifier}(::${identifier})*"
$typename = "${typename}(<${typename}(,[ ]*${typename})*[ ]*>[ ]*)*[ ]*\**"
# adding more substitution allow more nested templated type but we run out of
# \digit which is a wall. Indeed if you add more () grouping you need to
# rename all relevant \digit in pattern which use this regular definition
# $typename = "${typename}(<${typename}(,[ ]*${typename})*[ ]*>[ ]*)*"

# FIXME: really discussable but simplify output and the next pattern.
"\<std::" = ""

# specific to gcc 2.95
"\<basic_string<char, string_char_traits<char>, __default_alloc_template<true, 0> >" = "string"
"\<(multi)?map<(${typename}), (${typename}), less<\2[ ]*>, allocator<\8>[ ]*>" = "\1map<\2, \8>"

# common to all supported gcc version.
"\<deque<(${typename}), allocator<\1[ ]*>, 0>" = "deque<\1>"
"\<(stack|queue)<(${typename}), deque<\2>[ ]*>" = "\1<\2>"
"\<(vector|list|deque)<(${typename}), allocator<\2[ ]*> >" = "\1<\2>"
# strictly speaking 3rd parameters is less<ContainerType::value_type>
"\<priority_queue<(${typename}), vector<\1>, less<\1>[ ]*>" = "priority_queue<\1>"
"\<(multi)?set<(${typename}), less<\2[ ]*>, allocator<\2>[ ]*>" = "\1set<\2>"

"\<bitset<(${integer}), unsigned long>" = "bitset<\1>"
"\<([io]stream_iterator)<char, int>" = "\1<char>"

# gcc 3.2, not tested on 3.0, 3.1 but probably work.
# FIXME: there is a potential problem here with map<int const, long>
# the pair become pair<\2, \8> not pair<\2 const, \8>, who use the above,
# is it legal ?
"\<(multi)?map<(${typename}), (${typename}), less<\2[ ]*>, allocator<pair<\2 const, \8>[ ]*>[ ]*>" = "\1map<\2, \8>"

"\<bitset<\(unsigned\)(${integer})>" = "bitset<\1>"

# iterator
# FIXME: 3rd params is ptrdiff_t
"\<iterator<(input|output|forward|bidirectional|random)_iterator_tag, (${typename}), int, \2\*, \2&>" = "iterator<\1_iterator_tag, \2>"
# FIXME: 4th parms is ptrdiff_t
"\<([io]stream_iterator)<(${typename}), char, char_traits<char>, int>" = "\1<\2>"

# iosfwd, std::string and std::wstring
# first translate from "basic_xxx<T, char_traits<T> >" to "basic_xxx<T>"
"\<([io]streambuf_iterator|basic_(ios|streambuf|([io]|io)stream|filebuf|[io]?fstream))<(${typename}), char_traits<\4> >" = "\1<\4>"
# as above translate from "basic_xxx<T, char_traits<T>, ...>" to "basic_xxx<T>"
"\<basic_(string(buf)?|[io]?stringstream)?<(${typename}), char_traits<\3>, allocator<\3> >" = "basic_\1<\3>"
# now we can translate the two above for char, wchar_t to standardese typedef
$iosfwd_name = "\<basic_(string|ios|(stream|file|string)buf|(i|o|io)stream|[io]?(fstream|stringstream))"
"${iosfwd_name}<char>" = "\1"
"${iosfwd_name}<wchar_t>" = "w\1"

# streampos and wstreampos decay to the same type, they are undistingushable
# in mangled name so substitute for the most probably, not a big deal
"fpos<__mbstate_t>" = "streampos"

# locale
"(money|time|num)_put<(${typename}), ostreambuf_iterator<\2> >" = "\1_put<\2>"
"(money|time|num)_get<(${typename}), istreambuf_iterator<\2> >" = "\1_get<\2>"
"moneypunct(_byname)?<(${typename}), \(bool\)0>" = "moneypunct\1<\2>"
