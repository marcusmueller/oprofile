/**
 * @file op_parse_event.c
 * event parsing
 *
 * You can have silliness here.
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author John Levon
 * @author Philippe Elie
 */

#include <stdio.h>
#include <stdlib.h>

#include "op_parse_event.h"
#include "op_string.h"

static char * next_part(char const ** str)
{
	char const * c;
	char * ret;

	if ((*str)[0] == '\0')
		return NULL;

	if ((*str)[0] == ':')
		++(*str);

	c = *str;

	while (*c != '\0' && *c != ':')
		++c;

	if (c == *str)
		return NULL;

	ret = op_xstrndup(*str, c - *str);
	*str += c - *str;
	return ret;
}


size_t parse_events(struct parsed_event * parsed_events, size_t max_events,
                  char const * const * events)
{
	size_t i = 0;

	while (events[i]) {
		char const * cp = events[i];
		char * part = next_part(&cp);

		if (i >= max_events) {
			fprintf(stderr, "Too many events specified: CPU "
			        "only has %lu counters.\n",
				(unsigned long) max_events);
			exit(EXIT_FAILURE);
		}

		if (!part) {
			fprintf(stderr, "Invalid event %s\n", cp);
			exit(EXIT_FAILURE);
		}

		parsed_events[i].name = part;

		part = next_part(&cp);

		if (!part) {
			fprintf(stderr, "Invalid event %s\n", events[i]);
			exit(EXIT_FAILURE);
		}

		parsed_events[i].count = strtoul(part, NULL, 0);
		free(part);

		parsed_events[i].unit_mask = 0;
		part = next_part(&cp);

		if (part) {
			parsed_events[i].unit_mask = strtoul(part, NULL, 0);
			free(part);
		}

		parsed_events[i].kernel = 1;
		part = next_part(&cp);

		if (part) {
			parsed_events[i].kernel = strtoul(part, NULL, 0);
			free(part);
		}

		parsed_events[i].user = 1;
		part = next_part(&cp);

		if (part) {
			parsed_events[i].user = strtoul(part, NULL, 0);
			free(part);
		}
	
		++i;
	}

	return i;
}
