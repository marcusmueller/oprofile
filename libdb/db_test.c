/**
 * @file db_test.c
 * Tests for DB hash
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author Philippe Elie
 */

#include <sys/resource.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>

#include "op_sample_file.h"
#include "db_hash.h"

#define TEST_FILENAME "test-hash-db.dat"

static int nr_error;

static double used_time(void)
{
  struct rusage  usage;

  getrusage(RUSAGE_SELF, &usage);

  return usage.ru_utime.tv_sec + usage.ru_stime.tv_sec + 
	((usage.ru_utime.tv_usec + usage.ru_stime.tv_usec) / 1000000.0);
}

/* create nr item randomly created with nr_unique_item distinct items */
static void speed_test(int nr_item, int nr_unique_item)
{
	int i;
	double begin, end;
	samples_db_t hash;
	int rc;
	char * err_msg;

	rc = db_open(&hash, TEST_FILENAME, DB_RDWR, sizeof(struct opd_header),
		&err_msg);
	if (rc != EXIT_SUCCESS) {
		fprintf(stderr, "%s", err_msg);
		free(err_msg);
		exit(EXIT_FAILURE);
	}
	begin = used_time();
	for (i = 0 ; i < nr_item ; ++i) {
		rc = db_insert(&hash, (random() % nr_unique_item) + 1, 1);
		if (rc != EXIT_SUCCESS) {
			fprintf(stderr, "db_insert() failed !\n");
			exit(EXIT_FAILURE);
		}
	}
	end = used_time();
	db_close(&hash);

	remove(TEST_FILENAME);

	fprintf(stderr, "nr item: %d, unique item: %d, elapsed: %f\n",
		nr_item, nr_unique_item, end - begin);
}

static void do_speed_test(void)
{
	int i, j;

	for (i = 1000 ; i <= 10000000 ; i *= 10) {
		for (j = 100 ; j <= i / 10 ; j *= 10) {
			speed_test(i, j);
		}
	}
}

static int test(int nr_item, int nr_unique_item)
{
	int i;
	samples_db_t hash;
	int ret;
	int rc;
	char * err_msg;

	rc = db_open(&hash, TEST_FILENAME, DB_RDWR, sizeof(struct opd_header),
		&err_msg);
	if (rc != EXIT_SUCCESS) {
		fprintf(stderr, "%s", err_msg);
		free(err_msg);
		exit(EXIT_FAILURE);
	}


	for (i = 0 ; i < nr_item ; ++i) {
		db_key_t key = (random() % nr_unique_item) + 1;
		rc = db_insert(&hash, key, 1);
		if (rc != EXIT_SUCCESS) {
			fprintf(stderr, "db_insert() failed !\n");
			exit(EXIT_FAILURE);
		}
	}

	ret = db_check_hash(&hash);

	db_close(&hash);

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
static void call_back(db_key_t key, db_value_t info, void * data)
{
	info = info; data = data;	/* suppress unused parameters */

	if (key < range_first || key >= range_last) {
		printf("%x %x %x\n", key, range_first, range_last);
		nr_error++;
	}
}

static int callback_test(int nr_item, int nr_unique_item)
{
	int i;
	samples_db_t tree;
	db_key_t first_key, last_key;
	int old_nr_error = nr_error;
	int rc;
	char * err_msg;

	rc = db_open(&tree, TEST_FILENAME, DB_RDWR, sizeof(struct opd_header),
		     &err_msg);
	if (EXIT_SUCCESS != rc) {
		fprintf(stderr, "%s", err_msg);
	        free(err_msg);
	        exit(EXIT_FAILURE);
	}

	for (i = 0 ; i < nr_item ; ++i) {
		rc = db_insert(&tree, (random() % nr_unique_item) + 1, 1);
		if (rc != EXIT_SUCCESS) {
			fprintf(stderr, "db_insert() failed !\n");
			exit(EXIT_FAILURE);
		}
	}

	last_key = (db_key_t)-1;
	first_key = 0;

	for ( ; first_key < last_key ; last_key /= 2) {

		range_first = first_key;
		range_last = last_key;

		samples_db_travel(&tree, first_key, last_key, call_back, 0);

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

static void sanity_check(char const * filename)
{
	samples_db_t hash;
	int rc;
	char * err_msg;

	rc = db_open(&hash, filename, DB_RDONLY, sizeof(struct opd_header),
		     &err_msg);
	if (EXIT_SUCCESS != rc) {
		fprintf(stderr, "%s", err_msg);
	        free(err_msg);
	        exit(EXIT_FAILURE);
	}

	if (db_check_hash(&hash)) {
		printf("checking file %s FAIL\n", filename);
		++nr_error;
	}

	db_close(&hash);
}

int main(int argc, char * argv[1])
{
	/* if a filename is given take it as: "check this db" */
	if (argc > 1) {
		int i;
		for (i = 1 ; i < argc ; ++i)
			sanity_check(argv[i]);
		return 0;
	}
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
