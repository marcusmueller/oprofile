/**
 * @file op_deviceio.c
 * Reading from a special device
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author John Levon
 * @author Philippe Elie
 */

#include "op_deviceio.h"

#include <sys/types.h>
#include <fcntl.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

fd_t op_open_device(char const * name, int fatal)
{
	fd_t fd;

	fd = open(name, O_RDONLY);
	if (fatal && fd == -1) {
		fprintf(stderr, "oprofiled:op_open_device: %s: %s\n",
			name, strerror(errno));
		exit(EXIT_FAILURE);
	}

	return fd;
}


void op_close_device(fd_t devfd)
{
	if (close(devfd)) {
		perror("oprofiled:op_close_device: ");
		exit(EXIT_FAILURE);
	}
}


ssize_t op_read_device(fd_t devfd, void * buf, size_t size)
{
	ssize_t count;

	lseek(devfd, 0, SEEK_SET);

	count = read(devfd, buf, size);

	if (count < 0 && errno != EINTR && errno != EAGAIN) {
		perror("oprofiled:op_read_device: ");
		exit(EXIT_FAILURE);
	}

	return count;
}
