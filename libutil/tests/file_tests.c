/**
 * @file file_tests.c
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author John Levon
 * @author Philippe Elie
 */

#include "op_file.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

static char * tests[][2] = {
	{ "/usr/bin/../bin", "/usr/bin" },
	{ "/../foo/bar/", "/foo/bar" },
	{ "/../../foo/bar/", "/foo/bar" },
	{ "/../../foo/bar/.", "/foo/bar" },
	{ "/../../foo/bar/./", "/foo/bar" },
	{ "/foo/./bar", "/foo/bar" },
	{ "/foo/././bar", "/foo/bar" },
	{ "/foo///", "/foo" },
	{ "../", "/" },
	{ "./", "/usr" },
	{ ".", "/usr" },
	{ "./../", "/" },
	{ "bin/../bin/../" , "/usr" },
	{ "../../../../../", "/" },
	{ "/usr/bin/../../..", "/" },
	{ "/usr/bin/../../../", "/" },
	{ "././.", "/usr" },
	/* Posix says this is valid */
	{ "//", "//" },
	/* but our implementation stolen from gcc treat this as "/" */
	{ "///", "/" },
	{ NULL, NULL },
};

int main(void)
{
	size_t i = 0;

	while (tests[i][0]) {
		char * res = op_relative_to_absolute_path(tests[i][0], "/usr");
		if (!res) {
			fprintf(stderr, "NULL return for %s\n", tests[i][0]);
			exit(EXIT_FAILURE);
		}

		if (strcmp(res, tests[i][1])) {
			fprintf(stderr, "%s does not match %s given %s\n",
			        res, tests[i][1], tests[i][0]);
			exit(EXIT_FAILURE);
		}
		free(res);
		++i;
	}

	return EXIT_SUCCESS;
}
