/**
 * @file regex_test.cpp
 * A simple test for libregex. Run it through:
 * $ regex_test < mangled-name.txt
 * or by specifying your own data file test. See mangled-name.txt for
 * for the input file format
 *
 * @remark Copyright 2003 OProfile authors
 * @remark Read the file COPYING
 *
 * @author Philippe Elie
 */

#include "string_manip.h"

#include "op_regex.h"

#include <iostream>

using namespace std;

static void do_test()
{
	regular_expression_replace rep;

	setup_regex(rep, "stl.pat");

	string test, expect, last;
	bool first = true;
	while (getline(cin, last)) {
		last = trim(last);
		if (last.length() == 0 || last[0] == '#')
			continue;

		if (first) {
			test = last;
			first = false;
		} else {
			expect = last;
			first = true;
			string str(test);
			rep.execute(str);
			if (str != expect) {
				cerr << "mistmatch: test, expect, returned\n"
				     << '"' << test << '"' << endl
				     << '"' << expect << '"' << endl
				     << '"' << str << '"' << endl;
			}
		}
	}

	if (!first) {
		cerr << "input file ill formed\n";
	}
}

int main()
{
	try {
		do_test();
	}
	catch (bad_regex const & e) {
		cerr << "bad_regex " << e.what() << endl;
	}
	catch (exception const & e) {
		cerr << "exception: " << e.what() << endl;
	}
}

