/**
 * \file op_fileio.c
 * Copyright 2002 OProfile authors
 * Read the file COPYING
 *
 * \author John Levon <moz@compsoc.man.ac.uk>
 * \author Philippe Elie <phil_el@wanadoo.fr>
 */

#include "op_fileio.h"

#include "op_libiberty.h"
 
#include <errno.h>
#include <string.h>
#include <stdlib.h>
 
/**
 * op_do_open_file - open a file
 * @param name  file name
 * @param mode  mode string
 * @param fatal  is failure fatal or not
 *
 * Open a file name.
 * Returns file handle or %NULL on failure.
 */
FILE * op_do_open_file(char const * name, char const * mode, int fatal)
{
	FILE * fp;

	fp = fopen(name, mode);

	if (!fp) {
		if (fatal) {
			fprintf(stderr,"oprofiled:op_do_open_file: %s: %s",
				name, strerror(errno));
			exit(EXIT_FAILURE);
		}
	}

	return fp;	
}

/**
 * op_close_file - close a file
 * @param fp  file pointer
 *
 * Closes a file pointer. A non-fatal
 * error message is produced if the
 * close fails.
 */
void op_close_file(FILE * fp)
{
	if (fclose(fp))
		perror("oprofiled:op_close_file: ");
}

/**
 * op_do_read_file - read a file
 * @param fp  file pointer
 * @param buf  buffer
 * @param size  size in bytes to read
 * @param fatal  is failure fatal or not
 *
 * Read from a file. It is considered an error
 * if anything less than @size bytes is read.
 */
void op_do_read_file(FILE * fp, void * buf, size_t size, int fatal)
{
	size_t count;

	count = fread(buf, size, 1, fp);

	if (fatal && count != 1) {
		if (feof(fp)) {
			fprintf(stderr,
				"oprofiled:op_read_file: read less than expected %d bytes\n",
				size);
		} else {
			fprintf(stderr,
				"oprofiled:op_read_file: error reading\n");
		}
		exit(EXIT_FAILURE);
	}
}

/**
 * op_do_read_u8 - read a byte from a file
 * @param fp  file pointer
 *
 * Read an unsigned byte from a file.
 * 0 is returned if the read fails in any way.
 */
u8 op_read_u8(FILE * fp)
{
	u8 val = 0;
	op_do_read_file(fp, &val, sizeof(u8), 0);

	return val;
}

/**
 * op_do_read_u16_he - read two bytes from a file
 * @param fp  file pointer
 *
 * Read an unsigned two-byte value from a file.
 * 0 is returned if the read fails in any way.
 *
 * No byte-swapping is done.
 */
u16 op_read_u16_he(FILE * fp)
{
	u16 val = 0;
	op_do_read_file(fp, &val, sizeof(u16), 0);
	return val;
}

/**
 * op_do_read_u32_he - read four bytes from a file
 * @param fp  file pointer
 *
 * Read an unsigned four-byte value from a file.
 * 0 is returned if the read fails in any way.
 *
 * No byte-swapping is done.
 */
u32 op_read_u32_he(FILE * fp)
{
	u32 val = 0;
	op_do_read_file(fp, &val, sizeof(u32), 0);

	return val;
}

/**
 * op_write_file - write to a file
 * @param fp  file pointer
 * @param buf  buffer
 * @param size  nr. of bytes to write
 *
 * Write @size bytes of buffer @buf to a file.
 * Failure is fatal.
 */
void op_write_file(FILE * fp, const void * buf, size_t size)
{
	size_t written;

	written = fwrite(buf, size, 1, fp);

	if (written != 1) {
		fprintf(stderr,
			"oprofiled:op_write_file: wrote less than expected: %d bytes.\n",
			size);
		exit(EXIT_FAILURE);
	}
}

/**
 * op_write_u8 - write a byte to a file
 * @param fp  file pointer
 * @param val  value to write
 *
 * Write the unsigned byte value @val to a file.
 * Failure is fatal.
 */
void op_write_u8(FILE * fp, u8 val)
{
	op_write_file(fp, &val, sizeof(val));
}

/**
 * op_write_u16_he - write two bytes to a file
 * @param fp  file pointer
 * @param val  value to write
 *
 * Write an unsigned two-byte value @val to a file.
 * Failure is fatal.
 *
 * No byte-swapping is done.
 */
void op_write_u16_he(FILE * fp, u16 val)
{
	op_write_file(fp, &val, sizeof(val));
}

/**
 * op_write_u23_he - write four bytes to a file
 * @param fp  file pointer
 * @param val  value to write
 *
 * Write an unsigned four-byte value @val to a file.
 * Failure is fatal.
 *
 * No byte-swapping is done.
 */
void op_write_u32_he(FILE * fp, u32 val)
{
	op_write_file(fp, &val, sizeof(val));
}

/**
 * op_read_int_from_file - parse an ASCII value from a file into an integer
 * @param filename  name of file to parse integer value from
 *
 * Reads an ASCII integer from the given file. All errors are fatal.
 * The value read in is returned.
 */
u32 op_read_int_from_file(char const * filename)
{
	FILE * fp;
	u32 value;

	fp = fopen(filename, "r");
	if (fp == NULL) {
		fprintf(stderr,
			"op_read_int_from_file: Failed to open %s, reason %s\n",
			filename, strerror(errno));
		exit(EXIT_FAILURE);
	}

	if (fscanf(fp, "%u", &value) != 1) {
		fclose(fp);
		fprintf(stderr,
			"op_read_int_from_file: Failed to convert contents of file %s to integer\n",
			filename);
		exit(EXIT_FAILURE);
	}

	fclose(fp);

	return value;
}

/**
 * op_get_line - read an ASCII line from a file
 * @param fp  file pointer
 *
 * Get a line of ASCII text from a file. The file is read
 * up to the first \0 or \n. A trailing \n is deleted.
 *
 * Empty lines are not handled.
 *
 * Returns the dynamically-allocated string containing
 * that line. At the end of a file a string "" will
 * be returned.
 *
 * In *both* cases, the string must be free()d at some
 * point.
 *
 * FIXME: we should use GNU getline and kill this
 */
char *op_get_line(FILE * fp)
{
	char * buf;
	char * cp;
	int c;
	/* average allocation is about 31, so 64 should be good */
	size_t max = 64;

	buf = xmalloc(max);
	cp = buf;

	while (1) {
		switch (c = fgetc(fp)) {
			case EOF:
			case '\n':
			case '\0':
				*cp = '\0';
				return buf;
				break;

			default:
				*cp = (char)c;
				cp++;
				if (((size_t)(cp - buf)) == max) {
					buf = xrealloc(buf, max + 64);
					cp = buf + max;
					max += 64;
				}
				break;
		}
	}
}
