#include <sys/resource.h>
#include <stdlib.h>
#include <stdio.h>

#include "db.h"

#define TEST_FILENAME "test-db.dat"

static int nr_error;

static double user_time(void)
{
  struct rusage  usage;

  getrusage(RUSAGE_SELF, &usage);

  return usage.ru_utime.tv_sec + (usage.ru_utime.tv_usec / 1000000.0);
}

/* create nr item randomly created with nr_unique_item distinct items */
static void speed_test(size_t nr_item, size_t nr_unique_item)
{
	size_t i;
	double begin, end;
	db_tree_t tree;

	db_open(&tree, TEST_FILENAME, 128);
	begin = user_time();
	for (i = 0 ; i < nr_item ; ++i) {
		db_insert(&tree, (random() % nr_unique_item) + 1, 1);
	}
	end = user_time();
	db_close(&tree);

	remove(TEST_FILENAME);

	fprintf(stderr, "nr item: %d, unique item: %d, elapsed: %f\n",
		nr_item, nr_unique_item, end - begin);
}

static void do_speed_test(void)
{
	int i, j;

	for (i = 1000 ; i <= 1000000 ; i *= 10) {
		for (j = 100 ; j <= i / 10 ; j *= 10) {
			speed_test(i, j);
		}
	}
}

static int test(size_t nr_item, size_t nr_unique_item)
{
	size_t i;
	db_tree_t tree;
	int ret;

	db_open(&tree, TEST_FILENAME, 128);


	for (i = 0 ; i < nr_item ; ++i) {
		db_insert(&tree, (random() % nr_unique_item) + 1, 1);
	}

	ret = db_check_tree(&tree);

	db_close(&tree);

	remove(TEST_FILENAME);

	return ret;
}

static void do_test(void)
{
	int i, j;

	for (i = 1000 ; i <= 1000000 ; i *= 10) {
		for (j = 100 ; j <= i / 10 ; j *= 10) {
			if (test(i, j)) {
				printf("%s:%d failure for %d %d\n",
				       __FILE__, __LINE__, i, j);
				nr_error++;
			} else {
				printf("test() ok %d %d\n", i, j);
			}
		}
	}
}


static db_key_t range_first, range_last;
static db_key_t last_key_found;
static void call_back(db_key_t key, db_value_t info, void * data)
{
	if (&info) {} if (&data) {}	/* suppress unused parameters */

	if (key <= last_key_found) {
		printf("%x %x\n", key, last_key_found);
		nr_error++;
	}

	if (key < range_first || key >= range_last) {
		printf("%x %x %x\n", key, range_first, range_last);
		nr_error++;
	}

	last_key_found = key;
}

static int callback_test(size_t nr_item, size_t nr_unique_item)
{
	size_t i;
	db_tree_t tree;
	db_key_t first_key, last_key;
	int old_nr_error = nr_error;

	db_open(&tree, TEST_FILENAME, 128);

	for (i = 0 ; i < nr_item ; ++i) {
		db_insert(&tree, (random() % nr_unique_item) + 1, 1);
	}


	last_key = (db_key_t)-1;
	first_key = 0;
	
	for ( ; first_key < last_key ; last_key /= 2) {

		last_key_found = first_key == 0 ? first_key : first_key - 1;
		range_first = first_key;
		range_last = last_key;

		db_travel(&tree, first_key, last_key, call_back, 0);

		first_key = first_key == 0 ? 1 : first_key * 2;
	}

	db_close(&tree);

	remove(TEST_FILENAME);

	return old_nr_error == nr_error ? 0 : 1;
}

static void do_callback_test(void)
{
	int i, j;
	for (i = 1000 ; i <= 1000000 ; i *= 10) {
		for (j = 100 ; j <= i / 10 ; j *= 10)
			if (callback_test(i, j)) {
				printf("%s:%d failure for %d %d\n",
				       __FILE__, __LINE__, i, j);
				nr_error++;
			} else {
				printf("callback_test() ok %d %d\n", i, j);
			}
	}
}

int main(void)
{
	do_test();

	do_callback_test();

	do_speed_test();

	if (nr_error) {
		printf("%d error occured\n", nr_error);
	} else {
		printf("no error occur\n");
	}

	return 0;
}
