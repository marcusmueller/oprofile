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

/* return true if s2 is a prefix of s1 */
/* FIXME: move to libutil */
static int strisprefix(char const * s1, char const * s2)
{
	if (strlen(s1) < strlen(s2))
		return 0;

	return strncmp(s1, s2, strlen(s2)) == 0;
}


/* name:MESI type:bitmask default:0x0f */
static void parse_um(struct op_unit_mask * um, char const * line)
{
	size_t valueend = 1, tagend = 1, start = 0;

	while (line[valueend]) {
		while (line[valueend] != ' ' && line[valueend] != '\t' && line[valueend])
			++valueend;

		while (line[tagend] != ':' && line[tagend])
			++tagend;

		if (valueend == tagend)
			break;

		++tagend;

		if (strisprefix(line + start, "name")) {
			um->name = xmalloc(1 + valueend - tagend);
			strncpy(um->name, line + tagend, valueend - tagend);
			um->name[valueend - tagend] = '\0';
		} else if (strisprefix(line + start, "type")) {
			if (strisprefix(line + tagend, "mandatory")) {
				um->unit_type_mask = utm_mandatory;
			} else if (strisprefix(line + tagend, "bitmask")) {
				um->unit_type_mask = utm_bitmask;
			} else if (strisprefix(line + tagend, "exclusive")) {
				um->unit_type_mask = utm_exclusive;
			} else {
				fprintf(stderr, "oprofile: parse error in \"%s\"\n", line);
				exit(EXIT_FAILURE);
			}
		} else if (strisprefix(line + start, "default")) {
			int um_val;
			char * val = xmalloc(1 + valueend - tagend);
			strncpy(val, line + tagend, valueend - tagend);
			val[valueend - tagend] = '\0';
			sscanf(val, "%x", &um_val);
			um->default_mask = um_val;
			free(val);
		} else {
			fprintf(stderr, "oprofile: parse error in \"%s\"\n", line);
			exit(EXIT_FAILURE);
		}

		if (!line[valueend])
			break;
		++valueend;
		tagend = valueend;
		start = valueend;
	}
}


/* \t0x08 (M)odified cache state */
static void parse_um_entry(struct op_described_um * entry, char const * line)
{
	int i = 0;
	int um_val;
	char * val;
       
	/* skip the tab */
	++line;

	while (line[i] != ' ' && line[i] != '\t' && line[i])
		++i;

	if (!line[i]) {
		fprintf(stderr, "oprofile: parse error in \"%s\"\n", line);
		exit(EXIT_FAILURE);
	}

	val = xmalloc(i + 1);
	strncpy(val, line, i);
	val[i] = '\0';
	sscanf(val, "%x", &um_val);
	entry->value = um_val;
	free(val);

	while (line[i] != ' ' && line[i])
		++i;

	if (!line[i]) {
		fprintf(stderr, "oprofile: missing description for unit mask in \"%s\"\n", line);
		exit(EXIT_FAILURE);
	}
	
	// skip space
	++i;

	entry->desc = xstrdup(line + i);
}


static void read_unit_masks(char const * filename)
{
	struct op_unit_mask * um = NULL;
	int nr_entries = 0;
	char * line;
	FILE * fp = fopen(filename, "r");

	if (!fp) {
		fprintf(stderr, "oprofile: could not open unit mask description file %s\n", filename);
		exit(EXIT_FAILURE);
	}

	line = op_get_line(fp);

	while (line) {
		if (!strlen(line))
			goto next;
		if (line[0] == '#')
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
			parse_um_entry(&um->um[nr_entries], line);
			++nr_entries;
			if (nr_entries == 16) {
				fprintf(stderr, "oprofile: maximum unit mask entries exceeded\n");
				exit(EXIT_FAILURE);
			}
		}

next:
		free(line);
		line = op_get_line(fp);
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

		if (!*numend)
			break;

		++numend;
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
	exit(1);
}


int next_token(char ** c, char ** name, char ** value)
{
	int i = 0, j;

	while (*(*c + i) && *(*c + i) != ':')
		++i;

	if (!i && **c != ':')
		return 0;

	/* trailing description */
	if (**c == ':') {
		size_t len = strlen(*c + i);
		*name = xstrdup("desc");
		i += 2;
		*value = xmalloc(len + 1);
		strcpy(*value, *c + i);
		*c += len;
		return 1;
	} else {
		*name = xmalloc(i + 1);
		strncpy(*name, *c, i);
		*(*name + i) = '\0';
	}

	++i;

	j = i;

	while (*(*c + j) && *(*c + j) != ' ' && *(*c + j) != '\t')
		++j;

	*value = xmalloc(1 + j - i);
	strncpy(*value, *c + i, j - i);
	*(*value + j - i) = '\0';
	*c += 1 + j;
	return 1;
}


void read_events(char const * filename)
{
	struct op_event * event = NULL;
	char * line;
	char * name;
	char * value;
	char * c;
	FILE * fp = fopen(filename, "r");

	if (!fp) {
		fprintf(stderr, "oprofile: could not open event description file %s\n", filename);
		exit(EXIT_FAILURE);
	}

	line = op_get_line(fp);

	while (line) {
		if (!strlen(line))
			goto next;
		if (line[0] == '#')
			goto next;

		event = xmalloc(sizeof(struct op_event));
		c = line;
		while (next_token(&c, &name, &value)) {
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
