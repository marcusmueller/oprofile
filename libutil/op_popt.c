/**
 * @file op_popt.c
 * Wrapper for libpopt - always use this rather
 * than popt.h
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author John Levon
 * @author Philippe Elie
 */

#include <stdlib.h>
#include <string.h>
#include "op_libiberty.h"
#include "op_popt.h"

poptContext op_poptGetContext(char const * name,
		int argc, char const ** argv,
		struct poptOption const * options, int flags)
{
	poptContext optcon;
	int c;

	xmalloc_set_program_name(argv[0]);

#ifdef CONST_POPT
	optcon = poptGetContext(name, argc, argv, options, flags);
#else
	optcon = poptGetContext((char *)name, argc, (char **)argv, options, flags);
#endif

	c = poptGetNextOpt(optcon);

	if (c < -1) {
		fprintf(stderr, "%s: %s: %s\n", argv[0],
			poptBadOption(optcon, POPT_BADOPTION_NOALIAS),
			poptStrerror(c));
		poptPrintHelp(optcon, stderr, 0);
		exit(EXIT_FAILURE);
	}

	return optcon;
}

poptContext op_poptGetOptions_getApp(char const * name, int argc,
                                     char const ** argv,
                                     struct poptOption const * options,
                                     char **app_options, int flags)
{
	poptContext optcon;
	const char * leftover = NULL;
	int c;

	xmalloc_set_program_name(argv[0]);

#ifdef CONST_POPT
	optcon = poptGetContext(name, argc, argv, options, flags);
#else
	optcon = poptGetContext((char *)name, argc, (char **)argv, options, flags);
#endif

	c = poptGetNextOpt(optcon);
	if (c < 0) {
		leftover = poptGetArg(optcon);
		if (leftover) {
			int arg_idx, app_name_found, length, num_app_args;
			char * app_name = (char *)xcalloc(strlen(leftover) + 1, 1);
			strncpy(app_name, leftover, strlen(leftover) + 1);
			app_options[0] = app_name;
			for (arg_idx = 1, app_name_found = 0, length = 0, num_app_args = 0;
					arg_idx < argc;
					arg_idx++) {
				if (app_name_found) {
					num_app_args++;
					length += strlen(argv[arg_idx]) + 1;
				}
				if (!strcmp(argv[arg_idx], app_name)) {
					app_name_found = 1;
				}
			}
			if (num_app_args)
				app_options[1] = (char *)xcalloc(length, 1);
			else
				app_options[1] = "";
			for (arg_idx = argc - num_app_args; arg_idx < argc; arg_idx++) {
				if (arg_idx > (argc - num_app_args))
					app_options[1] = strcat(app_options[1], " ");
				app_options[1] = strcat(app_options[1], argv[arg_idx]);
			}
		} else if (c < -1) {
			fprintf(stderr, "%s: %s: %s\n", argv[0],
			        poptBadOption(optcon, POPT_BADOPTION_NOALIAS),
			        poptStrerror(c));
			poptPrintHelp(optcon, stderr, 0);
			exit(EXIT_FAILURE);
		}
	}

	return optcon;

}
