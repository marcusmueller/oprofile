/**
 * @file op_alloc_counter.c
 * hardware counter allocation
 *
 * You can have silliness here.
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author John Levon
 * @author Philippe Elie
 */

#include <stdlib.h>

#include "op_events.h"
#include "op_libiberty.h"


static void iter_swap(size_t * a, size_t * b)
{
	size_t tmp = *a;
	*a = *b;
	*b = tmp;
}


static void reverse(size_t * first, size_t * last)
{
	while (1) {
		if (first == last || first == --last)
			return;
		else
			iter_swap(first++, last);
	}
}


static int next_permutation(size_t * first, size_t * last)
{
	size_t * i;
	if (first == last)
		return 0;
	i = first;
	++i;
	if (i == last)
		return 0;
	i = last;
	--i;

	for(;;) {
		size_t * ii = i;
		--i;
		if (*i < *ii) {
			size_t * j = last;
			while (!(*i < *--j)) {
			}
			iter_swap(i, j);
			reverse(ii, last);
			return 1;
		}
		if (i == first) {
			reverse(first, last);
			return 0;
		}
	}
}

static int allocate_counter(size_t const * counter_map,
                            struct op_event const * pev[], int nr_events)
{
	int c = 0;

	for (; c < nr_events; ++c) {
		/* assert(c < nr_counters) */
		int mask = 1 << counter_map[c];
		if (!(pev[c]->counter_mask & mask))
			return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}


size_t * map_event_to_counter(struct op_event const * pev[], int nr_events,
                              op_cpu cpu_type)
{
	int nr_counters;
	size_t * counter_map;
	int success;
	int i;

	nr_counters = op_get_nr_counters(cpu_type);
	if (nr_counters < nr_events)
		return 0;

	counter_map = xmalloc(nr_counters * sizeof(size_t));

	for (i = 0; i < nr_counters; ++i)
		counter_map[i] = i;

	success = EXIT_FAILURE;
	do {
		success = allocate_counter(counter_map, pev, nr_events);

		if (success == EXIT_SUCCESS)
			break;
	} while (next_permutation(counter_map, counter_map + nr_counters));

	if (success == EXIT_FAILURE) {
		free(counter_map);
		counter_map = 0;
	}

	return counter_map;
}
