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
#include "version.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

static LIST_HEAD(events_list);
static LIST_HEAD(um_list);

static char const * filename;
static unsigned int line_nr;

static void parse_error()
{
	fprintf(stderr, "oprofile: parse error in %s, line %u\n",
		filename, line_nr);
	exit(EXIT_FAILURE);
}


/* return true if s2 is a prefix of s1 */
/* FIXME: move to libutil */
static int strisprefix(char const * s1, char const * s2)
{
	if (strlen(s1) < strlen(s2))
		return 0;

	return strncmp(s1, s2, strlen(s2)) == 0;
}


/* FIXME: move to libutil */
char const * skip_ws(char const * c)
{
	while (*c && (*c == ' ' || *c == '\t' || *c == '\n'))
		++c;
	return c;
}


/* FIXME: move to libutil */
char const * skip_nonws(char const * c)
{
	while (*c && !(*c == ' ' || *c == '\t' || *c == '\n'))
		++c;
	return c;
}


/* FIXME: move to libutil */
int empty_line(char const * c)
{
	return !(*skip_ws(c));
}


/* FIXME: move to libutil */
int comment_line(char const * c)
{
	c = skip_ws(c);
	return *c == '#';
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

		++tagend;

		if (strisprefix(start, "name")) {
			um->name = xmalloc(1 + valueend - tagend);
			strncpy(um->name, tagend, valueend - tagend);
			um->name[valueend - tagend] = '\0';
		} else if (strisprefix(start, "type")) {
			if (strisprefix(tagend, "mandatory")) {
				um->unit_type_mask = utm_mandatory;
			} else if (strisprefix(tagend, "bitmask")) {
				um->unit_type_mask = utm_bitmask;
			} else if (strisprefix(tagend, "exclusive")) {
				um->unit_type_mask = utm_exclusive;
			} else {
				parse_error();
			}
		} else if (strisprefix(start, "default")) {
			int um_val;
			char * val = xmalloc(1 + valueend - tagend);
			strncpy(val, tagend, valueend - tagend);
			val[valueend - tagend] = '\0';
			sscanf(val, "%x", &um_val);
			um->default_mask = um_val;
			free(val);
		} else {
			parse_error();
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
	char const * c2;
	size_t len;
	int um_val;
	char * val;
       
	c = skip_ws(c);
	c2 = c;
	c = skip_nonws(c);

	if (!*c)
		parse_error();

	len = (c - c2);
	val = xmalloc(len + 1);
	strncpy(val, c2, len);
	val[len] = '\0';
	sscanf(val, "%x", &um_val);
	entry->value = um_val;
	free(val);

	c = skip_ws(c);

	if (!*c)
		parse_error();

	entry->desc = xstrdup(c);
}


static void read_unit_masks(char const * file)
{
	struct op_unit_mask * um = NULL;
	int nr_entries = 0;
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
			if (um) {
				um->num = nr_entries;
				list_add_tail(&um->um_next, &um_list);
			}
			nr_entries = 0;
			um = xmalloc(sizeof(struct op_unit_mask));
			parse_um(um, line);
		} else {
			if (nr_entries >= MAX_UNIT_MASK) {
				fprintf(stderr, "oprofile: maximum unit mask entries exceeded\n");
				exit(EXIT_FAILURE);
			}
			parse_um_entry(&um->um[nr_entries], line);
			++nr_entries;
		}

next:
		free(line);
		line = op_get_line(fp);
		++line_nr;
	}

	if (um) {
		um->num = nr_entries;
		list_add_tail(&um->um_next, &um_list);
	}

	fclose(fp);
}


u32 parse_counter_mask(char const * str)
{
	u32 mask = 0;
	char * valstr;
	int val;
	char const * numend = str;
	char const * numstart = str;

	while (*numend) {

		while (*numend && *numend != ',')
			++numend;

		valstr = xmalloc(1 + numend - numstart);
		strncpy(valstr, numstart, numend - numstart);
		valstr[numend - numstart] = '\0';
		sscanf(valstr, "%d", &val);
		mask |= (1 << val);
		free(valstr);

		/* skip , */
		++numend;

		numend = skip_ws(numend);
		numstart = numend;
	}

	return mask;
}


struct op_unit_mask * find_um(char const * value)
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


int next_token(char const ** cp, char ** name, char ** value)
{
	size_t tag_len;
	size_t val_len;
	char const * c = *cp;
	char const * end;
	char const * colon;

	c = skip_ws(c);
	end = colon = c;
	end = skip_nonws(end);

	while (*colon && *colon != ':')
		++colon;

	if (!*colon)
		return 0;

	if (colon >= end)
		parse_error();

	tag_len = colon - c;
	val_len = end - (colon + 1);

	/* trailing description */
	if (!tag_len) {
		end = skip_ws(end);
		val_len = strlen(end);
		*name = xstrdup("desc");
		*value = xmalloc(val_len + 1);
		strcpy(*value, end);
		end += val_len;
		*cp = end;
		return 1;
	} else {
		*name = xmalloc(tag_len + 1);
		strncpy(*name, c, tag_len);
		*(*name + tag_len) = '\0';
	}

	*value = xmalloc(val_len + 1);
	strncpy(*value, colon + 1, val_len);
	*(*value + val_len) = '\0';
	end = skip_ws(end);
	*cp = end;
	return 1;
}


void read_events(char const * file)
{
	struct op_event * event = NULL;
	char * line;
	char * name;
	char * value;
	char * c;
	FILE * fp = fopen(file, "r");

	if (!fp) {
		fprintf(stderr, "oprofile: could not open event description file %s\n", filename);
		exit(EXIT_FAILURE);
	}

	filename = file;
	line_nr = 1;

	line = op_get_line(fp);

	while (line) {
		if (empty_line(line) || comment_line(line))
			goto next;

		event = xmalloc(sizeof(struct op_event));

		c = line;
		while (next_token((char const **)&c, &name, &value)) {
			if (strcmp(name, "name") == 0) {
				event->name = value;
			} else if (strcmp(name, "event") == 0) {
				int val;
				sscanf(value, "%x", &val);
				event->val = val;
				free(value);
			} else if (strcmp(name, "counters") == 0) {
				event->counter_mask = parse_counter_mask(value);
				free(value);
			} else if (strcmp(name, "um") == 0) {
				event->unit = find_um(value);
				free(value);
			} else if (strcmp(name, "minimum") == 0) {
				sscanf(value, "%d", &event->min_count);
				free(value);
			} else if (strcmp(name, "desc") == 0) {
				event->desc = value;
			} else {
				printf("Parse error: unknown tag %s in file %s\n", name, filename);
				exit(1);
			}

			free(name);
		}

		list_add_tail(&event->event_next, &events_list);

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
	exit(1);
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
