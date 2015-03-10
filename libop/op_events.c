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
#include "op_cpufreq.h"
#include "op_hw_specific.h"
#include "op_parse_event.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>

static LIST_HEAD(events_list);
static LIST_HEAD(um_list);

static char const * filename;
static unsigned int line_nr;

static void delete_event(struct op_event * event);
static void read_events(char const * file);
static void read_unit_masks(char const * file);
static void free_unit_mask(struct op_unit_mask * um);

static char *build_fn(const char *cpu_name, const char *fn)
{
	char *s;
	static const char *dir;
	if (dir == NULL)
		dir = getenv("OPROFILE_EVENTS_DIR");
	if (dir == NULL)
		dir = OP_DATADIR;
	s = xmalloc(strlen(dir) + strlen(cpu_name) + strlen(fn) + 5);
	sprintf(s, "%s/%s/%s", dir, cpu_name, fn);
	return s;
}

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
	if (sscanf(str, "%d", &value) != 1)
		parse_error("expected decimal value");

	return value;
}


static int parse_hex(char const * str)
{
	int value;
	/* 0x/0X to force the use of hexa notation for field intended to
	   be in hexadecimal */
	if (sscanf(str, "0x%x", &value) != 1 &&
	    sscanf(str, "0X%x", &value) != 1)
		parse_error("expected hexadecimal value");

	return value;
}


static u64 parse_long_hex(char const * str)
{
	u64 value;
	if (sscanf(str, "%Lx", &value) != 1)
		parse_error("expected long hexadecimal value");

	fflush(stderr);
	return value;
}

static void include_um(const char *start, const char *end)
{
	char *s;
	char cpu[end - start + 1];
	int old_line_nr;
	const char *old_filename;

	strncpy(cpu, start, end - start);
	cpu[end - start] = 0;
	s = build_fn(cpu, "unit_masks");
	old_line_nr = line_nr;
	old_filename = filename;
	read_unit_masks(s);
	line_nr = old_line_nr;
	filename = old_filename;
	free(s);
}

/* extra:cmask=12,inv,edge */
unsigned parse_extra(const char *s)
{
	unsigned v, w;
	int o;

	/* This signifies that the first word of the description is unique */
	v = EXTRA_NONE;
	while (*s) {
		if (isspace(*s))
			break;
		if (strisprefix(s, "edge")) {
			v |= EXTRA_EDGE;
			s += 4;
		} else if (strisprefix(s, "inv")) {
			v |= EXTRA_INV;
			s += 3;
		} else if (sscanf(s, "cmask=%x%n", &w, &o) >= 1) {
			v |= (w & EXTRA_CMASK_MASK) << EXTRA_CMASK_SHIFT;
			s += o;
		} else if (strisprefix(s, "any")) {
			v |= EXTRA_ANY;
			s += 3;
		} else if (strisprefix(s, "pebs")) {
			v |= EXTRA_PEBS;
			s += 4;
		} else {
			parse_error("Illegal extra field modifier");
		}
		if (*s == ',')
			++s;
	}
	return v;
}

/* name:MESI type:bitmask default:0x0f */
static void parse_um(struct op_unit_mask * um, char const * line)
{
	int seen_name = 0;
	int seen_type = 0;
       	int seen_default = 0;
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

		if (strisprefix(start, "include")) {
			if (seen_name + seen_type + seen_default > 0)
				parse_error("include must be on its own");
			free_unit_mask(um);
			include_um(tagend, valueend);
			return;
		}

		if (strisprefix(start, "name")) {
			if (seen_name)
				parse_error("duplicate name: tag");
			seen_name = 1;
			um->name = op_xstrndup(tagend, valueend - tagend);
		} else if (strisprefix(start, "type")) {
			if (seen_type)
				parse_error("duplicate type: tag");
			seen_type = 1;
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
			if (seen_default)
				parse_error("duplicate default: tag");
			seen_default = 1;
			if (0 != strncmp(tagend, "0x", 2)) {
				um->default_mask_name = op_xstrndup(
					tagend, valueend - tagend);
			} else {
				um->default_mask = parse_hex(tagend);
			}
		} else {
			parse_error("invalid unit mask tag");
		}

		valueend = skip_ws(valueend);
		tagend = valueend;
		start = valueend;
	}

	if (!um->name)
		parse_error("Missing name for unit mask");
	if (!seen_type)
		parse_error("Missing type for unit mask");
}


/* \t0x08 (M)odified cache state */
/* \t0x08 extra:inv,cmask=... mod_cach_state (M)odified cache state */
static void parse_um_entry(struct op_described_um * entry, char const * line)
{
	char const * c = line;

	/* value */
	c = skip_ws(c);
	entry->value = parse_hex(c);

	/* extra: */
	c = skip_nonws(c);
	c = skip_ws(c);
	if (!*c)
		goto invalid_out;

	if (strisprefix(c, "extra:")) {
		c += 6;
		entry->extra = parse_extra(c);
		/* include the regular umask if there are real extra bits */
		if (entry->extra != EXTRA_NONE)
			entry->extra |= (entry->value & UMASK_MASK) << UMASK_SHIFT;
		/* named mask */
		c = skip_nonws(c);
		c = skip_ws(c);
		if (!*c)
			goto invalid_out;

		/* "extra:" !!ALWAYS!! followed by named mask */
		entry->name = op_xstrndup(c, strcspn(c, " \t"));
		c = skip_nonws(c);
		c = skip_ws(c);
	} else {
		entry->extra = 0;
	}

	/* desc */
	if (!*c) {
		/* This is a corner case where the named unit mask entry
		 * only has one word.  This should really be fixed in the
		 * unit_mask file */
		entry->desc = xstrdup(entry->name);
	} else
		entry->desc = xstrdup(c);
	return;

invalid_out:
	parse_error("invalid unit mask entry");
}


static struct op_unit_mask * new_unit_mask(void)
{
	struct op_unit_mask * um = xmalloc(sizeof(struct op_unit_mask));
	memset(um, '\0', sizeof(struct op_unit_mask));
	list_add_tail(&um->um_next, &um_list);

	return um;
}

static void free_unit_mask(struct op_unit_mask * um)
{
	list_del(&um->um_next);
	free(um);
	um = NULL;
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
			if (!um)
				parse_error("no unit mask name line");

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

static struct op_unit_mask * try_find_um(char const * value)
{
	struct list_head * pos;

	list_for_each(pos, &um_list) {
		struct op_unit_mask * um = list_entry(pos, struct op_unit_mask, um_next);
		if (strcmp(value, um->name) == 0) {
			um->used = 1;
			return um;
		}
	}
	return NULL;
}

static struct op_unit_mask * find_um(char const * value)
{
	struct op_unit_mask * um = try_find_um(value);
	if (um)
		return um;
	fprintf(stderr, "oprofile: could not find unit mask %s\n", value);
	exit(EXIT_FAILURE);
}

/* um:a,b,c,d merge multiple unit masks */
static struct op_unit_mask * merge_um(char * value)
{
	int num;
	char *s;
	struct op_unit_mask *new, *um;
	enum unit_mask_type type = -1U;

	um = try_find_um(value);
	if (um)
		return um;

	new = new_unit_mask();
	new->name = xstrdup(value);
	new->used = 1;
	num = 0;
	while ((s = strsep(&value, ",")) != NULL) {
		unsigned c;
		um = find_um(s);
		if (type == -1U)
			type = um->unit_type_mask;
		if (um->unit_type_mask != type)
			parse_error("combined unit mask must be all the same types");
		if (type != utm_bitmask && type != utm_exclusive)
			parse_error("combined unit mask must be all bitmasks or exclusive");
		new->default_mask |= um->default_mask;
		new->num += um->num;
		if (new->num > MAX_UNIT_MASK)
			parse_error("too many members in combined unit mask");
		for (c = 0; c < um->num; c++, num++) {
			new->um[num] = um->um[c];
			new->um[num].desc = xstrdup(new->um[num].desc);
		}
	}
	if (type == -1U)
		parse_error("Empty unit mask");
	new->unit_type_mask = type;
	return new;
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
		if (*c)
			parse_error("next_token(): garbage at end of line");
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

static void include_events (char *value)
{
	char * event_file;
	const char *old_filename;
	int old_line_nr;

	event_file = build_fn(value, "events");
	old_line_nr = line_nr;
	old_filename = filename;
	read_events(event_file);
	line_nr = old_line_nr;
	filename = old_filename;
	free(event_file);
}

static struct op_event * new_event(void)
{
	struct op_event * event = xmalloc(sizeof(struct op_event));
	memset(event, '\0', sizeof(struct op_event));
	list_add_tail(&event->event_next, &events_list);

	return event;
}

static void free_event(struct op_event * event)
{
	list_del(&event->event_next);
	free(event);
}

/* event:0x00 counters:0 um:zero minimum:4096 name:ISSUES : Total issues */
/* event:0x00 ext:xxxxxx um:zero minimum:4096 name:ISSUES : Total issues */
static void read_events(char const * file)
{
	struct op_event * event = NULL;
	char * line;
	char * name;
	char * value;
	char const * c;
	int seen_event, seen_counters, seen_um, seen_minimum, seen_name, seen_ext;
	FILE * fp = fopen(file, "r");
	int tags;
	int fail = 0;

	if (!fp) {
		fprintf(stderr, "oprofile: could not open event description file %s\n", file);
		exit(EXIT_FAILURE);
	}

	filename = file;
	line_nr = 1;

	line = op_get_line(fp);

	while (line) {
		int bad_val = 0;
		u64 tmp_val = 0ULL;
		if (empty_line(line) || comment_line(line))
			goto next;

		tags = 0;
		seen_name = 0;
		seen_event = 0;
		seen_counters = 0;
		seen_ext = 0;
		seen_um = 0;
		seen_minimum = 0;
		event = new_event();
		event->filter = -1;
		event->ext = NULL;

		c = line;
		while (next_token(&c, &name, &value)) {
			if (strcmp(name, "name") == 0) {
				if (seen_name)
					parse_error("duplicate name: tag");
				seen_name = 1;
				if (strchr(value, '/') != NULL)
					parse_error("invalid event name");
				if (strchr(value, '.') != NULL)
					parse_error("invalid event name");
				event->name = value;
				if (bad_val) {
					fprintf(stderr, "Event %s event code (0x%llx) is too big to fit in an int\n",
					        event->name, tmp_val);
					fail = 1;
					bad_val = 0;
				}
			} else if (strcmp(name, "event") == 0) {
				if (seen_event)
					parse_error("duplicate event: tag");
				seen_event = 1;
				tmp_val = parse_long_hex(value);
				if (tmp_val > 0xffffffff)
					bad_val = 1;
				else
					event->val = (u32)tmp_val;
				free(value);
			} else if (strcmp(name, "counters") == 0) {
				if (seen_counters)
					parse_error("duplicate counters: tag");
				seen_counters = 1;
				if (!strcmp(value, "cpuid"))
					event->counter_mask = arch_get_counter_mask();
				else
					event->counter_mask = parse_counter_mask(value);
				free(value);
			} else if (strcmp(name, "ext") == 0) {
				if (seen_ext)
					parse_error("duplicate ext: tag");
				seen_ext = 1;
				event->ext = value;
			} else if (strcmp(name, "um") == 0) {
				if (seen_um)
					parse_error("duplicate um: tag");
				seen_um = 1;
				if (strchr(value, ','))
					event->unit = merge_um(value);
				else
					event->unit = find_um(value);
				free(value);
			} else if (strcmp(name, "minimum") == 0) {
				if (seen_minimum)
					parse_error("duplicate minimum: tag");
				seen_minimum = 1;
				event->min_count = parse_int(value);
				free(value);
			} else if (strcmp(name, "desc") == 0) {
				event->desc = value;
			} else if (strcmp(name, "filter") == 0) {
				event->filter = parse_int(value);
				free(value);
			} else if (strcmp(name, "include") == 0) {
				if (tags > 0)
					parse_error("tags before include:");
				free_event(event);
				include_events(value);
				free(value);
				c = skip_ws(c);
				if (*c != '\0' && *c != '#')
					parse_error("non whitespace after include:");
				break;
			} else {
				parse_error("unknown tag");
			}
			tags++;

			free(name);
		}
next:
		free(line);
		line = op_get_line(fp);
		++line_nr;
	}

	fclose(fp);
	if (fail)
		exit(EXIT_FAILURE);
}


/* usefull for make check */
static int check_unit_mask(struct op_unit_mask const * um,
	char const * cpu_name)
{
	u32 i;
	int err = 0;

	if (!um->used) {
		fprintf(stderr, "um %s is not used\n", um->name);
		err = EXIT_FAILURE;
	}

	if (um->unit_type_mask == utm_mandatory && um->num != 1) {
		fprintf(stderr, "mandatory um %s doesn't contain exactly one "
			"entry (%s)\n", um->name, cpu_name);
		err = EXIT_FAILURE;
	} else if (um->unit_type_mask == utm_bitmask) {
		u32 default_mask = um->default_mask;
		for (i = 0; i < um->num; ++i)
			default_mask &= ~um->um[i].value;

		if (default_mask) {
			fprintf(stderr, "um %s default mask is not valid "
				"(%s)\n", um->name, cpu_name);
			err = EXIT_FAILURE;
		}
	} else if (um->unit_type_mask == utm_exclusive) {
		if (um->default_mask_name) {
			for (i = 0; i < um->num; ++i) {
				if (0 == strcmp(um->default_mask_name,
						um->um[i].name))
					break;
			}
		} else {
			for (i = 0; i < um->num; ++i) {
				if (um->default_mask == um->um[i].value)
					break;
			}
		}

		if (i == um->num) {
			fprintf(stderr, "exclusive um %s default value is not "
				"valid (%s)\n", um->name, cpu_name);
			err = EXIT_FAILURE;
		}
	}
	return err;
}

static void arch_filter_events(op_cpu cpu_type)
{
	struct list_head * pos, * pos2;
	unsigned filter = arch_get_filter(cpu_type);
	if (!filter)
		return;
	list_for_each_safe (pos, pos2, &events_list) {
		struct op_event * event = list_entry(pos, struct op_event, event_next);
		if (event->filter >= 0 && ((1U << event->filter) & filter))
			delete_event(event);
	}
}

static void load_events_name(const char *cpu_name)
{
	char * event_file;
	char * um_file;

	event_file = build_fn(cpu_name, "events");
	um_file = build_fn(cpu_name, "unit_masks");

	read_unit_masks(um_file);
	read_events(event_file);

	free(um_file);
	free(event_file);
}

static void load_events(op_cpu cpu_type)
{
	const char * cpu_name = op_get_cpu_name(cpu_type);
	struct list_head * pos;
	int err = 0;

	if (!list_empty(&events_list))
		return;

	load_events_name(cpu_name);

	arch_filter_events(cpu_type);

	/* sanity check: all unit mask must be used */
	list_for_each(pos, &um_list) {
		struct op_unit_mask * um = list_entry(pos, struct op_unit_mask, um_next);
		err |= check_unit_mask(um, cpu_name);
	}
	if (err)
		exit(err);

}

struct list_head * op_events(op_cpu cpu_type)
{
	load_events(cpu_type);
	arch_filter_events(cpu_type);
	return &events_list;
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


static void delete_event(struct op_event * event)
{
	if (event->name)
		free(event->name);
	if (event->desc)
		free(event->desc);

	list_del(&event->event_next);
	free(event);
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

/* There can be actually multiple events here, so this is not quite correct */
static struct op_event * find_event_any(u32 nr)
{
	struct list_head * pos;

	list_for_each(pos, &events_list) {
		struct op_event * event = list_entry(pos, struct op_event, event_next);
		if (event->val == nr)
			return event;
	}

	return NULL;
}

static struct op_event * find_event_um(u32 nr, u32 um)
{
	struct list_head * pos;
	unsigned int i;

	list_for_each(pos, &events_list) {
		struct op_event * event = list_entry(pos, struct op_event, event_next);
		if (event->val == nr) {
			for (i = 0; i < event->unit->num; i++) {
				if (event->unit->um[i].value == um)
					return event;
			}
		}
	}

	return NULL;
}

static FILE * open_event_mapping_file(char const * cpu_name)
{
	char * ev_map_file;
	char * dir;
	dir = getenv("OPROFILE_EVENTS_DIR");
	if (dir == NULL)
		dir = OP_DATADIR;

	ev_map_file = xmalloc(strlen(dir) + strlen("/") + strlen(cpu_name) +
	                    strlen("/") + + strlen("event_mappings") + 1);
	strcpy(ev_map_file, dir);
	strcat(ev_map_file, "/");

	strcat(ev_map_file, cpu_name);
	strcat(ev_map_file, "/");
	strcat(ev_map_file, "event_mappings");
	filename = ev_map_file;
	return (fopen(ev_map_file, "r"));
}


/**
 *  This function is PPC64-specific.
 */
static char const * get_mapping(u32 nr, FILE * fp)
{
	char * line;
	char * name;
	char * value;
	char const * c;
	char * map = NULL;
	int seen_event = 0, seen_mmcr0 = 0, seen_mmcr1 = 0, seen_mmcra = 0;
	u32 mmcr0 = 0;
	u64 mmcr1 = 0;
	u32 mmcra = 0;
	int event_found = 0;

	line_nr = 1;
	line = op_get_line(fp);
	while (line && !event_found) {
		if (empty_line(line) || comment_line(line))
			goto next;

		seen_event = 0;
		seen_mmcr0 = 0;
		seen_mmcr1 = 0;
		seen_mmcra = 0;
		mmcr0 = 0;
		mmcr1 = 0;
		mmcra = 0;

		c = line;
		while (next_token(&c, &name, &value)) {
			if (strcmp(name, "event") == 0) {
				u32 evt;
				if (seen_event)
					parse_error("duplicate event tag");
				seen_event = 1;
				evt = parse_hex(value);
				if (evt == nr)
					event_found = 1;
				free(value);
			} else if (strcmp(name, "mmcr0") == 0) {
				if (seen_mmcr0)
					parse_error("duplicate mmcr0 tag");
				seen_mmcr0 = 1;
				mmcr0 = parse_hex(value);
				free(value);
			} else if (strcmp(name, "mmcr1") == 0) {
				if (seen_mmcr1)
					parse_error("duplicate mmcr1: tag");
				seen_mmcr1 = 1;
				mmcr1 = parse_long_hex(value);
				free(value);
			} else if (strcmp(name, "mmcra") == 0) {
				if (seen_mmcra)
					parse_error("duplicate mmcra: tag");
				seen_mmcra = 1;
				mmcra = parse_hex(value);
				free(value);
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
	if (event_found) {
		if (!seen_mmcr0 || !seen_mmcr1 || !seen_mmcra) {
			fprintf(stderr, "Error: Missing information in line %d of event mapping file %s\n", line_nr, filename);
			exit(EXIT_FAILURE);
		}
		map = xmalloc(70);
		snprintf(map, 70, "mmcr0:%u mmcr1:%Lu mmcra:%u",
		         mmcr0, mmcr1, mmcra);
	}

	return map;
}


char const * find_mapping_for_event(u32 nr, op_cpu cpu_type)
{
	char const * cpu_name = op_get_cpu_name(cpu_type);
	FILE * fp = open_event_mapping_file(cpu_name);
	char const * map = NULL;
	switch (cpu_type) {
		case CPU_PPC64_970:
		case CPU_PPC64_970MP:
		case CPU_PPC64_POWER4:
		case CPU_PPC64_POWER5:
		case CPU_PPC64_POWER5p:
		case CPU_PPC64_POWER5pp:
		case CPU_PPC64_POWER6:
		case CPU_PPC64_POWER7:
		// For ppc64 types of CPU_PPC64_ARCH_V1 and higher, we don't need an event_mappings file
			if (!fp) {
				fprintf(stderr, "oprofile: could not open event mapping file %s\n", filename);
				exit(EXIT_FAILURE);
			} else {
				map = get_mapping(nr, fp);
			}
			break;
		default:
			break;
	}

	if (fp)
		fclose(fp);

	return map;
}

static int match_event(int i, struct op_event *event, unsigned um)
{
	unsigned v = event->unit->um[i].value;

	switch (event->unit->unit_type_mask) {
	case utm_exclusive:
	case utm_mandatory:
		return v == um;

	case utm_bitmask:
		return (v & um) || (!v && v == 0);
	}

	abort();
}

struct op_event * find_event_by_name(char const * name, unsigned um, int um_valid)
{
	struct list_head * pos;

	list_for_each(pos, &events_list) {
		struct op_event * event = list_entry(pos, struct op_event, event_next);
		if (strcmp(event->name, name) == 0) {
			if (um_valid) {
				unsigned i;

				for (i = 0; i < event->unit->num; i++)
					if (match_event(i, event, um))
						return event;
				continue;
			}
			return event;
		}
	}

	return NULL;
}


static struct op_event * find_next_event(struct op_event * e)
{
	struct list_head * n;

	for (n = e->event_next.next; n != &events_list; n = n->next) {
		struct op_event * ne = list_entry(n, struct op_event, event_next);
		if (!strcmp(e->name, ne->name))
			return ne;
	}
	return NULL;
}

struct op_event * op_find_event(op_cpu cpu_type, u32 nr, u32 um)
{
	struct op_event * event;

	load_events(cpu_type);

	event = find_event_um(nr, um);

	return event;
}

struct op_event * op_find_event_any(op_cpu cpu_type, u32 nr)
{
	load_events(cpu_type);

	return find_event_any(nr);
}

static int _is_um_valid_bitmask(struct op_event * event, u32 passed_um)
{
	int duped_um[MAX_UNIT_MASK];
	int retval = 0;
	u32 masked_val = 0;
	u32 i, k;
	int dup_value_used = 0;

	struct op_event evt;
	struct op_unit_mask * tmp_um = xmalloc(sizeof(struct op_unit_mask));
	struct op_unit_mask * tmp_um_no_dups = xmalloc(sizeof(struct op_unit_mask));
	memset(tmp_um, '\0', sizeof(struct op_unit_mask));;
	memset(tmp_um_no_dups, '\0', sizeof(struct op_unit_mask));
	memset(duped_um, '\0', sizeof(int) * MAX_UNIT_MASK);

	// First, we make a copy of the event, with just its unit mask values.
	evt.unit = tmp_um;
	evt.unit->num = event->unit->num;
	for (i = 0; i < event->unit->num; i++)
		evt.unit->um[i].value = event->unit->um[i].value;

	// Next, we sort the unit mask values in ascending order.
	for (i = 1; i < evt.unit->num; i++) {
		int j = i - 1;
		u32 tmp = evt.unit->um[i].value;
		while (j >= 0 && tmp < evt.unit->um[j].value) {
			evt.unit->um[j + 1].value = evt.unit->um[j].value;
			j -= 1;
		}
		evt.unit->um[j + 1].value = tmp;
	}

	/* Now we remove duplicates. Duplicate unit mask values were not
	 * allowed until the "named unit mask" support was added in
	 * release 0.9.7.  The down side to this is that if the user passed
	 * a unit mask value that includes one of the duplicated values,
	 * we have no way of differentiating between the duplicates, so
	 * the meaning of the bitmask would be ambiguous if we were to
	 * allow it.  Thus, we must prevent the user from specifying such
	 * bitmasks.
	 */
	for (i = 0, k = 0; k < evt.unit->num; i++) {
		tmp_um_no_dups->um[i].value = evt.unit->um[k].value;
		tmp_um_no_dups->num++;
		k++;
		while ((evt.unit->um[i].value == evt.unit->um[k].value) && i < evt.unit->num) {
			k++;
			duped_um[i] = 1;
		}
	}
	evt.unit = tmp_um_no_dups;

	// Now check if passed um==0 and if the defined event has a UM with value '0'.
	if (!passed_um) {
		for (i = 0; i < evt.unit->num; i++) {
			if (!evt.unit->um[i].value)
				return 1;
		}
	}

	/* Finally, we'll see if the passed unit mask value can be matched with a
	 * mask of available unit mask values. We check for this by determining
	 * whether the exact bits set in the current um are also set in the
	 * passed um; if so, we OR those bits into a cumulative masked_val variable.
	 * Simultaneously, we check if the passed um contains a non-unique unit
	 * mask value, in which case, it's invalid..
	 */
	for (i = 0; i < evt.unit->num; i++) {
		if ((evt.unit->um[i].value & passed_um) == evt.unit->um[i].value) {
			masked_val |= evt.unit->um[i].value;
			if (duped_um[i]) {
				dup_value_used = 1;
				break;
			}
		}
	}

	if (dup_value_used) {
		fprintf(stderr, "Ambiguous bitmask: Unit mask values"
		        " cannot include non-unique numerical values (i.e., 0x%x).\n",
		        evt.unit->um[i].value);
		fprintf(stderr, "Use ophelp to see the unit mask values for event %s.\n",
		        event->name);
	} else if (masked_val == passed_um && passed_um != 0) {
		retval = 1;
	}
	free(tmp_um);
	free(tmp_um_no_dups);
	return retval;
}

static int _is_ppc64_cpu_type(op_cpu cpu_type) {
	char const * cpu_name = op_get_cpu_name(cpu_type);
	if (strncmp(cpu_name, "ppc64/power", strlen("ppc64/power")) == 0)
		return 1;
	else
		return 0;
}

int op_check_events(char * evt_name, int ctr, u32 nr, u32 um, op_cpu cpu_type)
{
	int ret = OP_INVALID_EVENT;
	size_t i;
	u32 ctr_mask = 1 << ctr;
	struct list_head * pos;
	int ibm_power_proc = _is_ppc64_cpu_type(cpu_type);

	load_events(cpu_type);

	list_for_each(pos, &events_list) {
		struct op_event * event = list_entry(pos, struct op_event, event_next);
		if (event->val != nr)
			continue;

		// Why do we have to do this, since event codes are supposed to be unique?
		// See the big comment below.
		if (ibm_power_proc && strcmp(evt_name, event->name))
			continue;

		ret = OP_OK_EVENT;

		if ((event->counter_mask & ctr_mask) == 0)
			ret |= OP_INVALID_COUNTER;

		if (event->unit->unit_type_mask == utm_bitmask) {
			if (!_is_um_valid_bitmask(event, um))
				ret |= OP_INVALID_UM;
		} else {
			for (i = 0; i < event->unit->num; ++i) {
				if (event->unit->um[i].value == um)
					break;
			}
			/* A small number of events on the IBM Power8 processor have real event
			 * codes that are larger than sizeof(int). Rather than change the width of
			 * the event code everywhere to be a long int (which would include having to
			 * change the sample file format), we have defined some internal-use-only
			 * unit masks for those events. In oprofile's power8 events file, we have
			 * truncated those event codes to integer size, and the truncated bits are
			 * used as a unit mask value which is ORed into the event code by
			 * libpe_utils/op_pe_utils.cpp:_get_event_code(). This technique allowed
			 * us to handle this situation with minimal code perturbation.  The one
			 * downside is that the truncated event codes are not unique.  So in this
			 * function, where we're searching for events by 'nr' (i.e., the event code),
			 * we have to also make sure the name matches.
			 *
			 * If the user gives us an event specification such as:
			 *      PM_L1MISS_LAT_EXC_256:0x0:1:1
			 * the above code will actually find a non-zero unit mask for this event and
			 * we'd normally fail at this point since the user passed '0x0' for a unit mask.
			 * But we don't expose these internal-use-only UMs to the user, so there's
			 * no way for them to know about it or to try to use it in their event spec;
			 * thus, we handle it below.
			 */
			if ((i == event->unit->num) && !((um == 0) && ibm_power_proc))
				ret |= OP_INVALID_UM;
		}

		if (ret == OP_OK_EVENT)
			return ret;
	}

	return ret;
}


void op_default_event(op_cpu cpu_type, struct op_default_event_descr * descr)
{
	descr->name = "";
	descr->um = 0x0;
	/* A fixed value of CPU cycles; this should ensure good
	 * granulity even on faster CPUs, though it will generate more
	 * interrupts.
	 */
	descr->count = 100000;

	switch (cpu_type) {
		case CPU_PPRO:
		case CPU_PII:
		case CPU_PIII:
		case CPU_P6_MOBILE:
		case CPU_CORE:
		case CPU_CORE_2:
		case CPU_ATHLON:
		case CPU_HAMMER:
		case CPU_FAMILY10:
		case CPU_ARCH_PERFMON:
		case CPU_FAMILY11H:
 		case CPU_ATOM:
 		case CPU_CORE_I7:
		case CPU_NEHALEM:
		case CPU_HASWELL:
		case CPU_BROADWELL:
		case CPU_SILVERMONT:
		case CPU_WESTMERE:
		case CPU_SANDYBRIDGE:
		case CPU_IVYBRIDGE:
		case CPU_MIPS_LOONGSON2:
		case CPU_FAMILY12H:
		case CPU_FAMILY14H:
		case CPU_FAMILY15H:
		case CPU_AMD64_GENERIC:
			descr->name = "CPU_CLK_UNHALTED";
			break;

		case CPU_P4:
		case CPU_P4_HT2:
			descr->name = "GLOBAL_POWER_EVENTS";
			descr->um = 0x1;
			break;

		case CPU_AXP_EV67:
			descr->name = "CYCLES";
			descr->um = 0x1;
			break;

		// we could possibly use the CCNT
		case CPU_ARM_XSCALE1:
		case CPU_ARM_XSCALE2:
		case CPU_ARM_MPCORE:
		case CPU_ARM_V6:
		case CPU_ARM_V7:
		case CPU_ARM_V7_CA5:
		case CPU_ARM_V7_CA7:
		case CPU_ARM_V7_CA9:
		case CPU_ARM_V7_CA15:
		case CPU_ARM_SCORPION:
		case CPU_ARM_SCORPIONMP:
		case CPU_ARM_KRAIT:
		case CPU_ARM_V8_APM_XGENE:
		case CPU_ARM_V8_CA57:
		case CPU_ARM_V8_CA53:
			descr->name = "CPU_CYCLES";
			break;

		case CPU_PPC64_970:
		case CPU_PPC64_970MP:
		case CPU_PPC_7450:
		case CPU_PPC64_POWER4:
		case CPU_PPC64_POWER5:
		case CPU_PPC64_POWER6:
		case CPU_PPC64_POWER5p:
		case CPU_PPC64_POWER5pp:
		case CPU_PPC64_POWER7:
		case CPU_PPC64_ARCH_V1:
		case CPU_PPC64_POWER8:
			descr->name = "CYCLES";
			break;

		case CPU_MIPS_20K:
			descr->name = "CYCLES";
			break;

		case CPU_MIPS_24K:
		case CPU_MIPS_34K:
		case CPU_MIPS_74K:
		case CPU_MIPS_1004K:
			descr->name = "INSTRUCTIONS";
			break;

		case CPU_MIPS_5K:
		case CPU_MIPS_25K:
			descr->name = "CYCLES";
			break;

		case CPU_MIPS_R10000:
		case CPU_MIPS_R12000:
			descr->name = "INSTRUCTIONS_GRADUATED";
			break;

		case CPU_MIPS_RM7000:
		case CPU_MIPS_RM9000:
			descr->name = "INSTRUCTIONS_ISSUED";
			break;

		case CPU_MIPS_SB1:
			descr->name = "INSN_SURVIVED_STAGE7";
			break;

		case CPU_MIPS_VR5432:
		case CPU_MIPS_VR5500:
			descr->name = "INSTRUCTIONS_EXECUTED";
			break;

		case CPU_PPC_E500:
		case CPU_PPC_E500_2:
		case CPU_PPC_E500MC:
		case CPU_PPC_E6500:
		case CPU_PPC_E300:
			descr->name = "CPU_CLK";
			break;

		case CPU_S390_Z10:
		case CPU_S390_Z196:
		case CPU_S390_ZEC12:
			descr->name = "CPU_CYCLES";
			descr->count = 4127518;
			break;

		case CPU_TILE_TILE64:
		case CPU_TILE_TILEPRO:
		case CPU_TILE_TILEGX:
			descr->name = "ONE";
			break;

		// don't use default, if someone add a cpu he wants a compiler
		// warning if he forgets to handle it here.
		case CPU_TIMER_INT:
		case CPU_NO_GOOD:
		case MAX_CPU_TYPE:
			break;
	}
}

static void extra_check(struct op_event *e, char *name, unsigned w)
{
	int found;
	unsigned i;

	if (!e->unit->um[w].extra) {
		fprintf(stderr,
			"Named unit mask (%s) not allowed for event without 'extra:' values.\n"
			"Please specify the numerical value for the unit mask. See the 'operf'\n"
			"man page for more info.\n", name);
		exit(EXIT_FAILURE);
	}

	found = 0;
	for (i = 0; i < e->unit->num; i++) {
		int len = strcspn(e->unit->um[i].desc, " \t");
		if (!strncmp(name, e->unit->um[i].desc, len) &&
		    name[len] == '\0')
			found++;
	}

	if (found > 1) {
		fprintf(stderr,
			"Unit mask name `%s' not unique. Please use a numerical unit mask\n", name);
		exit(EXIT_FAILURE);
	}
}

static void do_resolve_unit_mask(struct op_event *e,
	struct parsed_event *pe, u32 *extra)
{
	unsigned i;

	/* If not specified um and the default um is name type
	 * we populate pe unitmask name with default name */
	if ((e->unit->default_mask_name != NULL) &&
			(pe->unit_mask_name == NULL) && (!pe->unit_mask_valid)) {
		pe->unit_mask_name = xstrdup(e->unit->default_mask_name);
	}

	for (;;) {
		if (pe->unit_mask_name == NULL) {
			/* For numerical unit mask */
			int found = 0;
			int old_um_valid = pe->unit_mask_valid;

			/* Use default unitmask if not specified */
			if (!pe->unit_mask_valid) {
				pe->unit_mask_valid = 1;
				pe->unit_mask = e->unit->default_mask;
			}

			/* Checking to see there are any duplicate numerical unit mask
			 * in which case it should be using named unit mask instead.
			 */
			for (i = 0; i < e->unit->num; i++) {
				if (e->unit->um[i].value == (unsigned int)pe->unit_mask)
					found++;
			}
			if (found > 1) {
				if (!old_um_valid)
					fprintf(stderr,
						"Default unit mask not supported for this event.\n"
						"Please speicfy a unit mask by name, using the first "
						"word of the unit mask description\n");
				else
					fprintf(stderr,
						"Unit mask (0x%x) is non unique.\n"
						"Please specify the unit mask using the first "
						"word of the description\n",
					pe->unit_mask);
				exit(EXIT_FAILURE);
			}

			if (i == e->unit->num) {
				e = find_next_event(e);
				if (e != NULL)
					continue;
			}
			return;
		} else {
			/* For named unit mask */
			for (i = 0; i < e->unit->num; i++) {
				int len = 0;

				if (e->unit->um[i].name)
					len = strlen(e->unit->um[i].name);

				if (len
				&&  (!strncmp(pe->unit_mask_name,
					      e->unit->um[i].name, len))
				&&  (pe->unit_mask_name[len] == '\0'))
					break;
			}
			if (i == e->unit->num) {
				e = find_next_event(e);
				if (e != NULL)
					continue;
				fprintf(stderr, "Cannot find unit mask %s for %s\n",
					pe->unit_mask_name, pe->name);
				exit(EXIT_FAILURE);
			}
			extra_check(e, pe->unit_mask_name, i);
			pe->unit_mask_valid = 1;
			pe->unit_mask = e->unit->um[i].value;
			if (extra) {
				if (e->unit->um[i].extra == EXTRA_NONE)
					*extra = e->unit->um[i].value;
				else
					*extra = e->unit->um[i].extra;
			}
			return;
		}
	}
}

void op_resolve_unit_mask(struct parsed_event *pe, u32 *extra)
{
	struct op_event *e;

	e = find_event_by_name(pe->name, 0, 0);
	if (!e) {
		fprintf(stderr, "Cannot find event %s\n", pe->name);
		exit(EXIT_FAILURE);
	}
	return do_resolve_unit_mask(e, pe, extra);
}
