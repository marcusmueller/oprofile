/**
 * @file op_help.c
 * Print out PMC event information
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author John Levon
 * @author Philippe Elie
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#include "op_version.h"
#include "op_events.h"
#include "op_popt.h"
#include "op_cpufreq.h"
#include "op_hw_config.h"
#include "op_string.h"
#include "op_alloc_counter.h"
#include "op_parse_event.h"

static char const ** chosen_events;

struct parsed_event parsed_events[OP_MAX_COUNTERS];


static op_cpu cpu_type = CPU_NO_GOOD;

static poptContext optcon;

/**
 * help_for_event - output event name and description
 * @param i  event number
 *
 * output an help string for the event @i
 */
static void help_for_event(struct op_event * event)
{
	uint i, j;
	uint mask;

	printf("%s", event->name);

	printf(": (counter: ");

	mask = event->counter_mask;
	for (i = 0; i < CHAR_BIT * sizeof(event->counter_mask); ++i) {
		if (mask & (1 << i)) {
			printf("%d", i);
			mask &= ~(1 << i);
			if (mask)
				printf(", ");
		}
	}

	printf(")\n\t%s (min count: %d)\n", event->desc, event->min_count);

	if (strcmp(event->unit->name, "zero")) {

		printf("\tUnit masks\n");
		printf("\t----------\n");

		for (j=0; j < event->unit->num; j++) {
			printf("\t0x%.2x: %s\n",
			       event->unit->um[j].value,
			       event->unit->um[j].desc);
		}
	}
}


static void check_event(struct parsed_event * pev,
			struct op_event const * event)
{
	int ret;

	if (!event) {
		fprintf(stderr, "No event named %s is available.\n",
		        pev->name);
		exit(EXIT_FAILURE);
	}

	ret = op_check_events(0, event->val, pev->unit_mask, cpu_type);

	if (ret & OP_INVALID_UM) {
		fprintf(stderr, "Invalid unit mask 0x%x for event %s\n",
		        pev->unit_mask, pev->name);
		exit(EXIT_FAILURE);
	}

	if (pev->count < event->min_count) {
		fprintf(stderr, "Count %d for event %s is below the "
		        "minimum %d\n", pev->count, pev->name,
		        event->min_count);
		exit(EXIT_FAILURE);
	}
}


static void resolve_events(void)
{
	size_t count;
	size_t i, j;
	size_t * counter_map;
	size_t nr_counters = op_get_nr_counters(cpu_type);
	struct op_event const * selected_events[OP_MAX_COUNTERS];

	count = parse_events(parsed_events, OP_MAX_COUNTERS, chosen_events);
	if (count > nr_counters) {
		fprintf(stderr, "Not enough hardware counters.\n");
		exit(EXIT_FAILURE);
	}

	for (i = 0; i < count; ++i) {
		for (j = i + 1; j < count; ++j) {
			struct parsed_event * pev1 = &parsed_events[i];
			struct parsed_event * pev2 = &parsed_events[j];

			if (!strcmp(pev1->name, pev2->name) &&
			    pev1->count == pev2->count &&
			    pev1->unit_mask == pev2->unit_mask &&
			    pev1->kernel == pev2->kernel &&
			    pev1->user == pev2->user) {
				fprintf(stderr, "All events must be distinct.\n");
				exit(EXIT_FAILURE);
			}
		}
	}

	for (i = 0; i < count; ++i) {
		struct parsed_event * pev = &parsed_events[i];

		selected_events[i] = find_event_by_name(pev->name);

		check_event(pev, selected_events[i]);
	}

	counter_map = map_event_to_counter(selected_events, count, cpu_type);

	if (!counter_map) {
		fprintf(stderr, "Couldn't allocate hardware counters for the selected events.\n");
		exit(EXIT_FAILURE);
	}

	for (i = 0; i < count; ++i) {
		printf("%d ", (unsigned int) counter_map[i]);
	}
	printf("\n");

	free(counter_map);
}


static void show_unit_mask(void)
{
	struct op_event * event;
	size_t count;

	count = parse_events(parsed_events, OP_MAX_COUNTERS, chosen_events);
	if (count > 1) {
		fprintf(stderr, "More than one event specified.\n");
		exit(EXIT_FAILURE);
	}

	event = find_event_by_name(parsed_events[0].name);

	if (!event) {
		fprintf(stderr, "No such event found.\n");
		exit(EXIT_FAILURE);
	}

	printf("0x%x\n", event->unit->default_mask);
}


static void show_default_event(void)
{
	struct op_default_event_descr descr;

	op_default_event(cpu_type, &descr);

	if (descr.name[0] == '\0')
		return;

	printf("%s:%lu:%lu:1:1\n", descr.name, descr.count, descr.um);
}


static int show_vers;
static int get_cpu_type;
static int check_events;
static int unit_mask;
static int get_default_event;

static struct poptOption options[] = {
	{ "cpu-type", 'c', POPT_ARG_INT, &cpu_type, 0,
	  "use the given numerical CPU type", "cpu type", },
	{ "check-events", 'e', POPT_ARG_NONE, &check_events, 0,
	  "check the given event descriptions for validity", NULL, },
	{ "unit-mask", 'u', POPT_ARG_NONE, &unit_mask, 0,
	  "default unit mask for the given event", NULL, },
	{ "get-cpu-type", 'r', POPT_ARG_NONE, &get_cpu_type, 0,
	  "show the auto-detected CPU type", NULL, },
	{ "get-default-event", 'd', POPT_ARG_NONE, &get_default_event, 0,
	  "get the default event", NULL, },
	{ "version", 'v', POPT_ARG_NONE, &show_vers, 0,
	   "show version", NULL, },
	POPT_AUTOHELP
	{ NULL, 0, 0, NULL, 0, NULL, NULL, },
};

/**
 * get_options - process command line
 * @param argc  program arg count
 * @param argv  program arg array
 *
 * Process the arguments, fatally complaining on error.
 */
static void get_options(int argc, char const * argv[])
{
	optcon = op_poptGetContext(NULL, argc, argv, options, 0);

	if (show_vers) {
		show_version(argv[0]);
	}

	/* non-option, must be a valid event name or event specs */
	chosen_events = poptGetArgs(optcon);

	/* don't free the context now, we need chosen_events */
}


/** make valgrind happy */
static void cleanup(void)
{
	int i;
	for (i = 0; i < op_get_nr_counters(cpu_type); ++i) {
		if (parsed_events[i].name)
			free(parsed_events[i].name);
	}
	op_free_events();
	if (optcon)
		poptFreeContext(optcon);
}


int main(int argc, char const *argv[])
{
	struct list_head * events;
	struct list_head * pos;
	char const * pretty;

	atexit(cleanup);

	get_options(argc, argv);

	/* usefull for testing purpose to allow to force the cpu type
	 * with --cpu-type */
	if (cpu_type == CPU_NO_GOOD)
		cpu_type = op_get_cpu_type();

	if (cpu_type < 0 || cpu_type >= MAX_CPU_TYPE) {
		fprintf(stderr, "cpu_type '%d' is not valid\n", cpu_type);
		exit(EXIT_FAILURE);
	}

	pretty = op_get_cpu_type_str(cpu_type);

	if (get_cpu_type) {
		printf("%s\n", pretty);
		exit(EXIT_SUCCESS);
	}

	if (get_default_event) {
		show_default_event();
		exit(EXIT_SUCCESS);
	}

	if (cpu_type == CPU_TIMER_INT) {
		if (!check_events)
			printf("Using timer interrupt.\n");
		exit(EXIT_SUCCESS);
	}

	events = op_events(cpu_type);

	if (!chosen_events && (unit_mask || check_events)) {
		fprintf(stderr, "No events given.\n");
		exit(EXIT_FAILURE);
	}

	if (unit_mask) {
		show_unit_mask();
		exit(EXIT_SUCCESS);
	}

	if (check_events) {
		resolve_events();
		exit(EXIT_SUCCESS);
	}

	/* without --check-events, the only argument must be an event name */
	if (chosen_events && chosen_events[0]) {
		if (chosen_events[1]) {
			fprintf(stderr, "Too many arguments.\n");
			exit(EXIT_FAILURE);
		}

		list_for_each(pos, events) {
			struct op_event * event = list_entry(pos, struct op_event, event_next);

			if (strcmp(event->name, chosen_events[0]) == 0) {
				printf("%d\n", event->val);
				exit(EXIT_SUCCESS);
			}
		}
		fprintf(stderr, "No such event \"%s\"\n", chosen_events[0]);
		exit(EXIT_FAILURE);
	}

	/* default: list all events */

	printf("oprofile: available events for CPU type \"%s\"\n\n", pretty);
	switch (cpu_type) {
	case CPU_HAMMER:
		break;
	case CPU_ATHLON:
		printf ("See AMD document x86 optimisation guide (22007.pdf), Appendix D\n\n");
		break;
	case CPU_PPRO:
	case CPU_PII:
	case CPU_PIII:
	case CPU_P4:
	case CPU_P4_HT2:
		printf("See Intel Architecture Developer's Manual Volume 3, Appendix A and\n"
		"Intel Architecture Optimization Reference Manual (730795-001)\n\n");
		break;
	case CPU_IA64:
	case CPU_IA64_1:
	case CPU_IA64_2:
		printf("See Intel Itanium Processor Reference Manual\n"
		       "for Software Development (Document 245320-003),\n"
		       "Intel Itanium Processor Reference Manual\n"
		       "for Software Optimization (Document 245473-003),\n"
		       "Intel Itanium 2 Processor Reference Manual\n"
		       "for Software Development and Optimization (Document 251110-001),\n\n");
		break;
	case CPU_AXP_EV4:
	case CPU_AXP_EV5:
	case CPU_AXP_PCA56:
	case CPU_AXP_EV6:
	case CPU_AXP_EV67:
		printf("See Alpha Architecture Reference Manual\n"
		       "ftp://ftp.compaq.com/pub/products/alphaCPUdocs/alpha_arch_ref.pdf\n");
		break;
	case CPU_RTC:
		break;
	default:
		printf("%d is not a valid processor type,\n", cpu_type);
	}

	list_for_each(pos, events) {
		struct op_event * event = list_entry(pos, struct op_event, event_next);
		help_for_event(event);
	}

	return EXIT_SUCCESS;
}
