/**
 * @file op_popt.h
 * Wrapper for libpopt - always use this rather
 * than popt.h
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author John Levon
 * @author Philippe Elie
 */

#ifndef OP_POPT_H
#define OP_POPT_H

#include <popt.h>

// not in some versions of popt.h
#ifndef POPT_TABLEEND
#define POPT_TABLEEND { NULL, '\0', 0, 0, 0, NULL, NULL }
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * op_poptGetContext - wrapper for popt
 *
 * Use this instead of poptGetContext to cope with
 * different popt versions. This also handle unrecognized
 * options. All error are fatal.
 */
poptContext op_poptGetContext(char const * name,
                int argc, char const ** argv,
                struct poptOption const * options, int flags);

/**
 * op_poptGetOptions_getApp
 *
 * Use this function when the argv array may be of the form:
 *    <pgm> [options] <app-to-profile> [app-args]
 * The <app-to-profile and app-args are passed back in app_options.
 * The caller MUST allocate a char * array of size 2 and pass that
 * array in app_options argument.  The first member of this array will
 * be set to the app-to-profile pathname; the second member will be
 * set to the app's args.
 */
poptContext op_poptGetOptions_getApp(char const * name, int argc,
                                     char const ** argv,
                                     struct poptOption const * options,
                                     char **app_options, int flags);

#ifdef __cplusplus
}
#endif

#endif /* OP_POPT_H */
