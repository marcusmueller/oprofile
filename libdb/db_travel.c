/**
 * @file db_travel.c
 * Inspection of a DB tree
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author Philippe Elie
 */

#include "odb_hash.h"

static void do_travel(samples_odb_t const * hash, odb_key_t first, odb_key_t last,
		samples_odb_travel_callback callback, void * data)
{
	size_t pos;
	for (pos = 1; pos < hash->descr->current_size ; ++pos) {
		odb_node_t const * node = &hash->node_base[pos];
		/* look odb_insert about locking rationale and the need
		 * to never pass to callback() a 0 key */
		if (node->key >= first && node->key < last && node->key) {
			callback(node->key, node->value, data);
		}
	}
}
 

void samples_odb_travel(samples_odb_t const * hash, odb_key_t first, odb_key_t last,
		samples_odb_travel_callback callback, void * data)
{
	do_travel(hash, first, last, callback, data);
}


odb_node_t * odb_get_iterator(samples_odb_t const * hash, odb_node_nr_t * nr)
{
	/* node zero is unused */
	*nr = hash->descr->current_size - 1;
	return hash->node_base + 1;
}
