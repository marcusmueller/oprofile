/**
 * @file db-hash-travel.c
 * Inspection of a DB tree
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author Philippe Elie
 */

#include "db_hash.h"

static void do_travel(samples_db_t const * hash, db_key_t first, db_key_t last,
		samples_db_travel_callback callback, void * data)
{
	size_t pos;
	for (pos = 1; pos < hash->descr->current_size ; ++pos) {
		db_node_t const * node = &hash->node_base[pos];
		/* look db_insert about locking rationale and the need
		 * to never pass to callback() a 0 key */
		if (node->key >= first && node->key < last && node->key) {
			callback(node->key, node->value, data);
		}
	}
}
 

void samples_db_travel(samples_db_t const * hash, db_key_t first, db_key_t last,
		samples_db_travel_callback callback, void * data)
{
	do_travel(hash, first, last, callback, data);
}


db_node_t * db_get_iterator(samples_db_t const * hash, db_node_nr_t * nr)
{
	/* node zero is unused */
	*nr = hash->descr->current_size - 1;
	return hash->node_base + 1;
}
