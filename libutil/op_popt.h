/**
 * \file op_popt.h
 * Copyright 2002 OProfile authors
 * Read the file COPYING
 *
 * \author John Levon <moz@compsoc.man.ac.uk>
 * \author Philippe Elie <phil_el@wanadoo.fr>
 */

#ifndef OP_POPT_H
#define OP_POPT_H

#include <popt.h>

#ifdef __cplusplus
extern "C" {
#endif

poptContext op_poptGetContext(char const * name,
                int argc, char const ** argv,
                struct poptOption const * options, int flags);

#ifdef __cplusplus
}
#endif

#endif /* OP_POPT_H */
