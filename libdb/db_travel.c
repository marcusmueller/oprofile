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

odb_node_t * odb_get_iterator(samples_odb_t const * hash, odb_node_nr_t * nr)
{
	/* node zero is unused */
	*nr = hash->data->descr->current_size - 1;
	return hash->data->node_base + 1;
}
