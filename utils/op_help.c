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

static char const ** chosen_events;

struct parsed_event {
	char * name;
	int count;
	int unit_mask;
	int counter;
	struct op_event * event;
} parsed_events[OP_MAX_COUNTERS];


static op_cpu cpu_type = CPU_NO_GOOD;

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

	/* FIXME, reintroduce
	if (event->counter_mask == CTR_ALL) {
		printf("all");
	} else {
	*/
		mask = event->counter_mask;
		for (i = 0; i < CHAR_BIT * sizeof(event->counter_mask); ++i) {
			if (mask & (1 << i)) {
				printf("%d", i);
				mask &= ~(1 << i);
				if (mask)
					printf(", ");
			}
		}
	/*}*/

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

	ret = strndup(*str, c - *str);
	*str += c - *str;
	return ret;
}


static int parse_events(void)
{
	int i = 0;

	while (chosen_events[i]) {
		int nr_counters = op_get_nr_counters(cpu_type);
		char const * cp = chosen_events[i];
		char * part = next_part(&cp);

		if (i >= nr_counters) {
			fprintf(stderr, "Too many events specified: CPU "
			        "only has %d counters.\n", nr_counters);
			exit(EXIT_FAILURE);
		}

		if (!part) {
			fprintf(stderr, "Invalid event %s\n", cp);
			exit(EXIT_FAILURE);
		}

		/* initial guess */
		parsed_events[i].counter = i;

		parsed_events[i].name = part;

		part = next_part(&cp);

		if (!part) {
			fprintf(stderr, "Invalid event %s\n",
			        chosen_events[i]);
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
	
		++i;
	}

	return i;
}


static void check_event(struct parsed_event * pev)
{
	struct op_event * event = pev->event;
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


static void allocate_counter(struct parsed_event * pev, int * alloced)
{
	int c = 0;

	for (; c < op_get_nr_counters(cpu_type); ++c) {
		int mask = 1 << c;
		if (!(*alloced & mask) && (pev->event->counter_mask & mask)) {
			pev->counter = c;
			*alloced |= mask;
			return;
		}
	}

	fprintf(stderr, "Couldn't allocate a hardware counter for "
	        "event %s: check op_help output\n", pev->name);
	exit(EXIT_FAILURE);
}


static void resolve_events(struct list_head * events)
{
	int count = parse_events();
	int i;
	int alloced = 0;

	for (i = 0; i < count; ++i) {
		struct list_head * pos;
		struct parsed_event * pev = &parsed_events[i];

		list_for_each(pos, events) {
			struct op_event * ev =
				list_entry(pos, struct op_event, event_next);

			if (strcmp(ev->name, pev->name) == 0) {
				pev->event = ev;
			}

		}

		check_event(pev);

		allocate_counter(pev, &alloced);
	}

	for (i = 0; i < count; ++i) {
		printf("%d ", parsed_events[i].counter);
	}
	printf("\n");
}


static void show_default_event(void)
{
	double freq = op_cpu_frequency();
	/* around 2000 ints/sec on a 100% busy CPU */
	unsigned long count = (unsigned long)(freq * 500.0);

	switch (cpu_type) {
		case CPU_PPRO:
		case CPU_PII:
		case CPU_PIII:
		case CPU_ATHLON:
		case CPU_HAMMER:
			printf("CPU_CLK_UNHALTED:%lu:0:1:1\n", count);
			break;

		case CPU_RTC:
			printf("RTC:1024:0:1:1\n");
			break;

		case CPU_P4:
		case CPU_P4_HT2:
			printf("GLOBAL_POWER_EVENTS:%lu:0x1:1:1\n", count);
			break;

		case CPU_IA64:
		case CPU_IA64_1:
		case CPU_IA64_2:
			printf("CPU_CYCLES:%lu:0:1:1\n", count);
			break;

		case CPU_AXP_EV4:
		case CPU_AXP_EV5:
		case CPU_AXP_PCA56:
		case CPU_AXP_EV6:
		case CPU_AXP_EV67:
			printf("CYCLES:%lu:0:1:1\n", count);
			break;

		default:
			break;
	}
}


static int showvers;
static int get_cpu_type;
static int check_events;
static int get_default_event;

static struct poptOption options[] = {
	{ "cpu-type", 'c', POPT_ARG_INT, &cpu_type, 0,
	  "use the given numerical CPU type", "cpu type", },
	{ "check-events", 'e', POPT_ARG_NONE, &check_events, 0,
	  "check the given event descriptions for validity", NULL, },
	{ "get-cpu-type", 'r', POPT_ARG_NONE, &get_cpu_type, 0,
	  "show the auto-detected CPU type", NULL, },
	{ "get-default-event", 'd', POPT_ARG_NONE, &get_default_event, 0,
	  "get the default event", NULL, },
	{ "version", 'v', POPT_ARG_NONE, &showvers, 0, "show version", NULL, },
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
	poptContext optcon;

	optcon = op_poptGetContext(NULL, argc, argv, options, 0);

	if (showvers) {
		show_version(argv[0]);
	}

	/* non-option, must be a valid event name or event specs*/
	chosen_events = poptGetArgs(optcon);

	// don't free the context, we need chosen_events
}


int main(int argc, char const *argv[])
{
	struct list_head * events;
	struct list_head * pos;
	char const * pretty;

	get_options(argc, argv);

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

	if (check_events) {
		if (!chosen_events) {
			fprintf(stderr, "No events given.\n");
			exit(EXIT_FAILURE);
		}
		resolve_events(events);
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
