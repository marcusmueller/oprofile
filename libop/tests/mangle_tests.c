/**
 * @file mangle_tests.c
 *
 * @remark Copyright 2003 OProfile authors
 * @remark Read the file COPYING
 *
 * @author John Levon
 * @author Philippe Elie
 */

#include <stdio.h>
#include <stdlib.h>

#include "op_mangle.h"
#include "op_config.h"

struct test_input {
	struct mangle_values values;
	char const * result;
};

static struct test_input const tests[] = {
	{ { MANGLE_NONE, "foo", "bar", "EVENT", 0, 0, 0, 0, 0 },
	  OP_SAMPLES_CURRENT_DIR "{root}/foo/EVENT.0.0.all.all.all" },
	{ { MANGLE_DEP_NAME, "foo", "bar", "EVENT", 0, 0, 0, 0, 0 },
	  OP_SAMPLES_CURRENT_DIR "{root}/bar/{dep}/{root}/foo/EVENT.0.0.all.all.all" },
	{ { MANGLE_CPU, "foo", "bar", "EVENT", 0, 0, 0, 0, 2 },
	  OP_SAMPLES_CURRENT_DIR "{root}/foo/EVENT.0.0.all.all.2" },

	{ { MANGLE_TID, "foo", "bar", "EVENT", 0, 0, 0, 33, 0 },
	  OP_SAMPLES_CURRENT_DIR "{root}/foo/EVENT.0.0.all.33.all" },
	{ { MANGLE_TGID, "foo", "bar", "EVENT", 0, 0, 34, 0, 0 },
	  OP_SAMPLES_CURRENT_DIR "{root}/foo/EVENT.0.0.34.all.all" },
	{ { MANGLE_KERNEL, "foo", "bar", "EVENT", 0, 0, 0, 0, 0 },
	  OP_SAMPLES_CURRENT_DIR "{kern}/foo/EVENT.0.0.all.all.all" },
	{ { MANGLE_DEP_NAME|MANGLE_CPU|MANGLE_TID|MANGLE_TID|MANGLE_TGID|MANGLE_KERNEL, "foo", "bar", "EVENT", 1234, 8192, 34, 35, 2 },
	  OP_SAMPLES_CURRENT_DIR "{kern}/bar/{dep}/{kern}/foo/EVENT.1234.8192.34.35.2" },
	{ { MANGLE_DEP_NAME|MANGLE_CPU|MANGLE_TID|MANGLE_TID|MANGLE_TGID|MANGLE_KERNEL, "foo1/foo2", "bar1/bar2", "EVENT", 1234, 8192, 34, 35, 2 },
	  OP_SAMPLES_CURRENT_DIR "{root}/bar1/bar2/{dep}/{root}/foo1/foo2/EVENT.1234.8192.34.35.2" },

	{ { 0, NULL, NULL, NULL, 0, 0, 0, 0, 0 }, NULL }
};


int main(void)
{
	struct test_input const * test;
	for (test = tests; test->result; ++test) {
		char * result = op_mangle_filename(&test->values);
		if (strcmp(result, test->result)) {
			fprintf(stderr, "test %d:\nfound: %s\nexpect:%s\n",
				test - tests, result, test->result);
			exit(EXIT_FAILURE);
		}
		free(result);
	}

	return EXIT_SUCCESS;
}
