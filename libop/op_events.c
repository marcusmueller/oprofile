/**
 * @file op_events.c
 * Details of PMC profiling events
 *
 * You can have silliness here.
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author John Levon
 * @author Philippe Elie
 */

#include "op_events.h"
#include "op_libiberty.h"
#include "op_fileio.h"
#include "op_string.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

static LIST_HEAD(events_list);
static LIST_HEAD(um_list);

static char const * filename;
static unsigned int line_nr;

static void parse_error(char const * context)
{
	fprintf(stderr, "oprofile: parse error in %s, line %u\n",
		filename, line_nr);
	fprintf(stderr, "%s\n", context);
	exit(EXIT_FAILURE);
}


static int parse_int(char const * str)
{
	int value;
	if (sscanf(str, "%d", &value) != 1) {
		parse_error("expected decimal value");
	}

	return value;
}


static int parse_hex(char const * str)
{
	int value;
	if (sscanf(str, "%x", &value) != 1) {
		parse_error("expected hexadecimal value");
	}

	return value;
}


static struct op_unit_mask * new_unit_mask(void)
{
	struct op_unit_mask * um = xmalloc(sizeof(struct op_unit_mask));
	memset(um, '\0', sizeof(struct op_unit_mask));
	list_add_tail(&um->um_next, &um_list);

	return um;
}


static void delete_unit_mask(struct op_unit_mask * unit)
{
	u32 cur;
	for (cur = 0 ; cur < unit->num ; ++cur) {
		if (unit->um[cur].desc)
			free(unit->um[cur].desc);
	}

	if (unit->name)
		free(unit->name);

	list_del(&unit->um_next);
	free(unit);
}


static struct op_event * new_event(void)
{
	struct op_event * event = xmalloc(sizeof(struct op_event));
	memset(event, '\0', sizeof(struct op_event));
	list_add_tail(&event->event_next, &events_list);

	return event;
}


static void delete_event(struct op_event * event)
{
	if (event->name)
		free(event->name);
	if (event->desc)
		free(event->desc);

	list_del(&event->event_next);
	free(event);
}


/* name:MESI type:bitmask default:0x0f */
static void parse_um(struct op_unit_mask * um, char const * line)
{
	char const * valueend = line + 1;
       	char const * tagend = line + 1;
	char const * start = line;

	while (*valueend) {
		valueend = skip_nonws(valueend);

		while (*tagend != ':' && *tagend)
			++tagend;

		if (valueend == tagend)
			break;

		if (!*tagend)
			parse_error("parse_um() expected :value");

		++tagend;

		if (strisprefix(start, "name")) {
			um->name = op_xstrndup(tagend, valueend - tagend);
		} else if (strisprefix(start, "type")) {
			if (strisprefix(tagend, "mandatory")) {
				um->unit_type_mask = utm_mandatory;
			} else if (strisprefix(tagend, "bitmask")) {
				um->unit_type_mask = utm_bitmask;
			} else if (strisprefix(tagend, "exclusive")) {
				um->unit_type_mask = utm_exclusive;
			} else {
				parse_error("invalid unit mask type");
			}
		} else if (strisprefix(start, "default")) {
			um->default_mask = parse_hex(tagend);
		} else {
			parse_error("invalid unit mask tag");
		}

		valueend = skip_ws(valueend);
		tagend = valueend;
		start = valueend;
	}
}


/* \t0x08 (M)odified cache state */
static void parse_um_entry(struct op_described_um * entry, char const * line)
{
	char const * c = line;

	c = skip_ws(c);
	entry->value = parse_hex(c);
	c = skip_nonws(c);

	if (!*c)
		parse_error("invalid unit mask entry");

	c = skip_ws(c);

	if (!*c)
		parse_error("invalid unit mask entry");

	entry->desc = xstrdup(c);
}


/*
 * name:zero type:mandatory default:0x0
 * \t0x0 No unit mask
 */
static void read_unit_masks(char const * file)
{
	struct op_unit_mask * um = NULL;
	char * line;
	FILE * fp = fopen(file, "r");

	if (!fp) {
		fprintf(stderr,
			"oprofile: could not open unit mask description file %s\n", file);
		exit(EXIT_FAILURE);
	}

	filename = file;
	line_nr = 1;

	line = op_get_line(fp);

	while (line) {
		if (empty_line(line) || comment_line(line))
			goto next;

		if (line[0] != '\t') {
			um = new_unit_mask();
			parse_um(um, line);
		} else {
			if (!um) {
				parse_error("no unit mask name line");
			}
			if (um->num >= MAX_UNIT_MASK) {
				parse_error("oprofile: maximum unit mask entries exceeded");
			}
			parse_um_entry(&um->um[um->num], line);
			++(um->num);
		}

next:
		free(line);
		line = op_get_line(fp);
		++line_nr;
	}

	fclose(fp);
}


static u32 parse_counter_mask(char const * str)
{
	u32 mask = 0;
	char const * numstart = str;

	while (*numstart) {
		mask |= 1 << parse_int(numstart);

		while (*numstart && *numstart != ',')
			++numstart;
		/* skip , unless we reach eos */
		if (*numstart)
			++numstart;

		numstart = skip_ws(numstart);
	}

	return mask;
}


static struct op_unit_mask * find_um(char const * value)
{
	struct list_head * pos;

	list_for_each(pos, &um_list) {
		struct op_unit_mask * um = list_entry(pos, struct op_unit_mask, um_next);
		if (strcmp(value, um->name) == 0)
			return um;
	}

	fprintf(stderr, "oprofile: could not find unit mask %s\n", value);
	exit(EXIT_FAILURE);
}


/* parse either a "tag:value" or a ": trailing description string" */
static int next_token(char const ** cp, char ** name, char ** value)
{
	size_t tag_len;
	size_t val_len;
	char const * c = *cp;
	char const * end;
	char const * colon;

	c = skip_ws(c);
	end = colon = c;
	end = skip_nonws(end);

	colon = strchr(colon, ':');

	if (!colon) {
		if (*c) {
			parse_error("next_token(): garbage at end of line");
		}
		return 0;
	}

	if (colon >= end)
		parse_error("next_token() expected ':'");

	tag_len = colon - c;
	val_len = end - (colon + 1);

	if (!tag_len) {
		/* : trailing description */
		end = skip_ws(end);
		*name = xstrdup("desc");
		*value = xstrdup(end);
		end += strlen(end);
	} else {
		/* tag:value */
		*name = op_xstrndup(c, tag_len);
		*value = op_xstrndup(colon + 1, val_len);
		end = skip_ws(end);
	}

	*cp = end;
	return 1;
}


/* event:0x00 counters:0 um:zero minimum:4096 name:ISSUES : Total issues */
static void read_events(char const * file)
{
	struct op_event * event = NULL;
	char * line;
	char * name;
	char * value;
	char const * c;
	FILE * fp = fopen(file, "r");

	if (!fp) {
		fprintf(stderr, "oprofile: could not open event description file %s\n", file);
		exit(EXIT_FAILURE);
	}

	filename = file;
	line_nr = 1;

	line = op_get_line(fp);

	while (line) {
		if (empty_line(line) || comment_line(line))
			goto next;

		event = new_event();

		c = line;
		while (next_token(&c, &name, &value)) {
			if (strcmp(name, "name") == 0) {
				event->name = value;
			} else if (strcmp(name, "event") == 0) {
				event->val = parse_hex(value);
				free(value);
			} else if (strcmp(name, "counters") == 0) {
				event->counter_mask = parse_counter_mask(value);
				free(value);
			} else if (strcmp(name, "um") == 0) {
				event->unit = find_um(value);
				free(value);
			} else if (strcmp(name, "minimum") == 0) {
				event->min_count = parse_int(value);
				free(value);
			} else if (strcmp(name, "desc") == 0) {
				event->desc = value;
			} else {
				parse_error("unknown tag");
			}

			free(name);
		}
next:
		free(line);
		line = op_get_line(fp);
		++line_nr;
	}

	fclose(fp);
}


static void load_events(op_cpu cpu_type)
{
	char const * cpu_name = op_get_cpu_name(cpu_type);
	char * event_dir;
	char * event_file;
	char * um_file;

	if (!list_empty(&events_list))
		return;

	event_dir = xmalloc(strlen(OP_DATADIR) + strlen(cpu_name) + strlen("/") + 1);
	strcpy(event_dir, OP_DATADIR);
	strcat(event_dir, cpu_name);
	strcat(event_dir, "/");

	event_file = xmalloc(strlen(event_dir) + strlen("events") + 1);
	strcpy(event_file, event_dir);
	strcat(event_file, "events");

	um_file = xmalloc(strlen(event_dir) + strlen("unit_masks") + 1);
	strcpy(um_file, event_dir);
	strcat(um_file, "unit_masks");

	read_unit_masks(um_file);
	read_events(event_file);

	free(um_file);
	free(event_file);
	free(event_dir);
}


struct list_head * op_events(op_cpu cpu_type)
{
	load_events(cpu_type);
	return &events_list;
}

void op_free_events(void)
{
	struct list_head * pos, * pos2;
	list_for_each_safe(pos, pos2, &events_list) {
		struct op_event * event = list_entry(pos, struct op_event, event_next);
		delete_event(event);
	}

	list_for_each_safe(pos, pos2, &um_list) {
		struct op_unit_mask * unit = list_entry(pos, struct op_unit_mask, um_next);
		delete_unit_mask(unit);
	}
}

struct op_event * find_event(u8 nr)
{
	struct list_head * pos;

	list_for_each(pos, &events_list) {
		struct op_event * event = list_entry(pos, struct op_event, event_next);
		if (event->val == nr)
			return event;
	}

	return NULL;
}


struct op_event * op_find_event(op_cpu cpu_type, u8 nr)
{
	struct op_event * event;

	load_events(cpu_type);

	event = find_event(nr);

	if (event)
		return event;

	fprintf(stderr, "oprofile: could not find event %d\n", nr);
	exit(EXIT_FAILURE);
}


int op_check_events(int ctr, u8 nr, u16 um, op_cpu cpu_type)
{
	int ret = OP_OK_EVENT;
	struct op_event * event;
	size_t i;
	u32 ctr_mask = 1 << ctr;

	load_events(cpu_type);

	event = find_event(nr);

	if (!event) {
		ret |= OP_INVALID_EVENT;
		return ret;
	}

	if ((event->counter_mask & ctr_mask) == 0)
		ret |= OP_INVALID_COUNTER;

	for (i = 0; i < event->unit->num; ++i) {
		if (event->unit->um[i].value == um)
			break;
	}

	if (i == event->unit->num)
		ret |= OP_INVALID_UM;

	return ret;
}

unsigned int op_min_count(u8 ctr_type, op_cpu cpu_type)
{
	struct op_event * event;

	load_events(cpu_type);

	event = find_event(ctr_type);
	
	return event ? event->min_count : 0;
}
