/**
 * \file op_deviceio.h
 * Copyright 2002 OProfile authors
 * Read the file COPYING
 *
 * \author John Levon <moz@compsoc.man.ac.uk>
 * \author Philippe Elie <phil_el@wanadoo.fr>
 */

#ifndef OP_DEVICEIO_H
#define OP_DEVICEIO_H

#ifdef __cplusplus
extern "C" {
#endif
 
#include "op_types.h"
 
#include <unistd.h>
 
#define op_try_open_device(n) op_open_device((n), 0)
fd_t op_open_device(char const * name, int fatal);
void op_close_device(fd_t devfd);
ssize_t op_read_device(fd_t devfd, void * buf, size_t size, int seek);

#ifdef __cplusplus
}
#endif

#endif /* OP_DEVICEIO_H */
