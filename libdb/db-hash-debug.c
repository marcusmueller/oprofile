/**
 * @file db-hash-debug.c
 * Debug routines for libdb-hash
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author Philippe Elie
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "db-hash.h"

static void display_callback(db_key_t key, db_value_t value, void * data)
{
	printf("%x %d\n", key, value);
	data = data;
}

void db_display_hash(samples_db_t const * hash)
{
	if (!db_check_hash(hash)) {
		samples_db_travel(hash, 0, ~0, display_callback, 0);
	}
}

void db_raw_display_hash(samples_db_t const * hash)
{
	size_t pos;
	for (pos = 1 ; pos < hash->descr->current_size ; ++pos) {
		db_node_t const * node = &hash->node_base[pos];
		printf("%x %d %d\n", node->key, node->value, node->next);
	}
}

static int check_circular_list(samples_db_t const * hash)
{
	size_t pos;
	int do_abort = 0;
	unsigned char * bitmap = malloc(hash->descr->current_size);
	memset(bitmap, '\0', hash->descr->current_size);

	for (pos = 0 ; pos < hash->descr->size*BUCKET_FACTOR ; ++pos) {

		size_t index = hash->hash_base[pos];
		if (index && !do_abort) {
			while (index) {
				if (bitmap[index]) {
					do_abort = 1;
				}
				bitmap[index] = 1;
				index = hash->node_base[index].next;
			}
		}

		if (do_abort) {
			printf("circular list detected size: %d\n",
			       hash->descr->current_size);

			memset(bitmap, '\0', hash->descr->current_size);

			index = hash->hash_base[pos];
			while (index) {
				printf("%d ", index);
				if (bitmap[index]) {
					exit(1);
				}
				bitmap[index] = 1;
				index = hash->node_base[index].next;
			}
		}

		/* purely an optimization: intead of memset the map reset only
		 * the needed part: not my use to optimize test but here the
		 * test was so slow it was useless */
		index = hash->hash_base[pos];
		while (index) {
			bitmap[index] = 1;
			index = hash->node_base[index].next;
		}
	}

	free(bitmap);

	return do_abort;
}

static int check_redundant_key(samples_db_t const * hash, db_key_t max)
{
	size_t pos;

	unsigned char * bitmap = malloc(max + 1);
	memset(bitmap, '\0', max + 1);

	for (pos = 1 ; pos < hash->descr->current_size ; ++pos) {
		if (bitmap[hash->node_base[pos].key]) {
			printf("redudant key found %d\n",
			       hash->node_base[pos].key);
			return 1;
		}
		bitmap[hash->node_base[pos].key] = 1;
	}
	free(bitmap);

	return 0;
}

int db_check_hash(samples_db_t const * hash)
{
	size_t pos;
	size_t nr_node = 0;
	size_t nr_node_out_of_bound = 0;
	int ret = 0;
	db_key_t max = 0;

	for (pos = 0 ; pos < hash->descr->size * BUCKET_FACTOR ; ++pos) {
		size_t index = hash->hash_base[pos];
		while (index) {
			if (index >= hash->descr->current_size) {
				nr_node_out_of_bound++;
				break;
			}
			++nr_node;

			if (hash->node_base[index].key > max)
				max = hash->node_base[index].key;

			index = hash->node_base[index].next;
		}
	}

	if (nr_node != hash->descr->current_size - 1) {
		printf("hash table walk found %d node expect %d node\n",
		       nr_node, hash->descr->current_size - 1);
		ret = 1;
	}

	if (nr_node_out_of_bound) {
		printf("out of bound node index: %d\n", nr_node_out_of_bound);
		ret = 1;
	}

	if (ret == 0) {
		ret = check_circular_list(hash);
	}

	if (ret == 0) {
		ret = check_redundant_key(hash, max);
	}

	if (ret == 0) {
		db_hash_stat_t * stats;
		stats = db_hash_stat(hash);
		db_hash_display_stat(stats);
		db_hash_free_stat(stats);
	}

	return ret;
}
