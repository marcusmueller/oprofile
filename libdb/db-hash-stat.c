/**
 * @file db-hash-stat.c
 * Statistics routines for libdb-hash
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author Philippe Elie
 */

#include <stdlib.h>
#include <stdio.h>

#include "db-hash.h"

struct db_hash_stat_t {
	db_node_nr_t node_nr;
	db_node_nr_t used_node_nr;
	db_index_t   hash_table_size;
	size_t       max_list_length;		/**< worst case   */
	double       average_list_length;	/**< average case */
	/* do we need variance ? */
};

db_hash_stat_t * db_hash_stat(samples_db_t const * hash)
{
	size_t max_length = 0;
	double total_length = 0.0;
	size_t nr_non_empty_list = 0;
	size_t pos;

	db_hash_stat_t * result = calloc(1, sizeof(db_hash_stat_t));
	if (!result) {
		fprintf(stderr, "not enough memory\n");
		exit(EXIT_FAILURE);
	}

	result->node_nr = hash->descr->size;
	result->used_node_nr = hash->descr->current_size;
	result->hash_table_size = hash->descr->size * BUCKET_FACTOR;

	/* FIXME: I'm dubious if this do right statistics for hash table
	 * efficiency check */

	for (pos = 0 ; pos < result->hash_table_size ; ++pos) {
		size_t cur_length = 0;
		size_t index = hash->hash_base[pos];
		while (index) {
			index = hash->node_base[index].next;
			++cur_length;
		}

		if (cur_length > max_length)
			max_length = cur_length;

		if (cur_length) {
			total_length += cur_length;
			++nr_non_empty_list;
		}
	}

	result->max_list_length = max_length;
	result->average_list_length = total_length / nr_non_empty_list;

	return result;
}


void db_hash_display_stat(db_hash_stat_t const * stat)
{
	printf("total node number:   %d\n", stat->node_nr);
	printf("total used node:     %d\n", stat->used_node_nr);
	printf("hash table size:     %d\n", stat->hash_table_size);
	printf("greater list length: %d\n", stat->max_list_length);
	printf("average non empty list length: %2.4f\n", stat->average_list_length);
}


void db_hash_free_stat(db_hash_stat_t * stat)
{
	free(stat);
}
