/**
 * \file op_mangle.h
 * Copyright 2002 OProfile authors
 * Read the file COPYING
 *
 * \author John Levon <moz@compsoc.man.ac.uk>
 * \author Philippe Elie <phil_el@wanadoo.fr>
 */

#ifndef OP_MANGLE_H
#define OP_MANGLE_H

#ifdef __cplusplus
extern "C" {
#endif

char * op_mangle_filename(char const * filename, char const * app_name);

#ifdef __cplusplus
}
#endif

#endif /* OP_MANGLE_H */
