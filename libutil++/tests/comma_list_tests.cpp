/**
 * @file comma_list_tests.cpp
 *
 * @remark Copyright 2003 OProfile authors
 * @remark Read the file COPYING
 *
 * @author John Levon
 * @author Philippe Elie
 */

#include <stdlib.h>

#include <iostream>

#include "comma_list.h"

using namespace std;

#define check(clist, val, result) \
	if (clist.match(val) != result) { \
		cerr << "\"" << #val << "\" matched with " #clist \
		     << " did not return " #result << endl; \
		exit(EXIT_FAILURE); \
	}

int main()
{
	comma_list<int> c1;

	check(c1, 1, true);

	c1.set("2", false);

	check(c1, 2, true);
	check(c1, 3, false);

	c1.set("3", false);

	check(c1, 2, false);
	check(c1, 3, true);

	c1.set("2", true);

	check(c1, 2, true);
	check(c1, 3, true);
	check(c1, 4, false);

	c1.set("all", false);

	check(c1, 2, true);
	check(c1, 4, true);
	check(c1, 5, true);

	comma_list<int> c2;

	c2.set("6", true);
	c2.set("all", true);

	check(c2, 4, true);
	check(c2, 0, true);

	c2.set("10", true);
	check(c2, 10, true);
	check(c2, 11, true);
	return EXIT_SUCCESS;
}
