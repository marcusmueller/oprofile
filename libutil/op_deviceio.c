/**
 * @file op_deviceio.c
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 * 
 * @author John Levon <moz@compsoc.man.ac.uk>
 * @author Philippe Elie <phil_el@wanadoo.fr>
 */

#include "op_deviceio.h"

#include <sys/types.h>
#include <fcntl.h>
 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
 
/**
 * op_open_device - open a special char device for reading
 * @param name  file name of device file
 * @param fatal  fatal or not
 *
 * Open the special file @name. Returns the file descriptor
 * for the file or -1 on error.
 */ 
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

/**
 * op_close_device - close a special char device
 * @param devfd  file descriptor of device
 *
 * Close a special file. Failure is fatal.
 */ 
void op_close_device(fd_t devfd)
{
	if (close(devfd)) {
		perror("oprofiled:op_close_device: ");
		exit(EXIT_FAILURE);
	}	
} 
 
/**
 * op_read_device - read from a special char device
 * @param devfd  file descriptor of device
 * @param buf  buffer
 * @param size  size of buffer
 * @param seek  seek to the start or not 
 *
 * Read @size bytes from a device into buffer @buf.
 * A seek to the start of the device file is done first
 * if @seek is non-zero, then a read is requested in one 
 * go of @size bytes.
 *
 * It is the caller's responsibility to do further op_read_device()
 * calls if the number of bytes read is not what is requested
 * (where this is applicable).
 *
 * The number of bytes read is returned, or a negative number
 * on failure (in which case errno will be set). If the call is
 * interrupted, then errno will be EINTR, and the client should
 * arrange for re-starting the read if necessary.
 */ 
ssize_t op_read_device(fd_t devfd, void * buf, size_t size, int seek)
{
	ssize_t count;
	
	if (seek)
		lseek(devfd,0,SEEK_SET);
 
	count = read(devfd, buf, size);

	if (count < 0 && errno != EINTR && errno != EAGAIN) {
		perror("oprofiled:op_read_device: ");
		exit(EXIT_FAILURE);
	}
 
	return count;
}
