/**
 * @file op_fileio.c
 * Reading from / writing to files
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author John Levon
 * @author Philippe Elie
 */

#include <unistd.h>

#include "op_fileio.h"

#include "op_libiberty.h"

#include <errno.h>
#include <string.h>
#include <stdlib.h>

static FILE * op_do_open_file(char const * name, char const * mode, int fatal)
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
 * op_try_open_file - open a file
 * @param name  file name
 * @param mode  mode string
 *
 * Open a file name.
 * Returns file handle or %NULL on failure.
 */
FILE * op_try_open_file(char const * name, char const * mode)
{
	return op_do_open_file(name, mode, 0);
}

/**
 * op_open_file - open a file
 * @param name  file name
 * @param mode  mode string
 *
 * Open a file name.
 * Failure to open is fatal.
 */
FILE * op_open_file(char const * name, char const * mode)
{
	return op_do_open_file(name, mode, 1);
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
 * op_read_file - read a file
 * @param fp  file pointer
 * @param buf  buffer
 * @param size  size in bytes to read
 *
 * Read from a file. It is considered an error
 * if anything less than size bytes is read.
 * Failure is fatal.
 */
void op_read_file(FILE * fp, void * buf, size_t size)
{
	size_t count;

	count = fread(buf, size, 1, fp);

	if (count != 1) {
		if (feof(fp)) {
			fprintf(stderr,
				"oprofiled:op_read_file: read less than expected %lu bytes\n",
				(unsigned long)size);
		} else {
			fprintf(stderr,
				"oprofiled:op_read_file: error reading\n");
		}
		exit(EXIT_FAILURE);
	}
}

/**
 * op_write_file - write to a file
 * @param fp  file pointer
 * @param buf  buffer
 * @param size  nr. of bytes to write
 *
 * Write size bytes of buffer buf to a file.
 * Failure is fatal.
 */
void op_write_file(FILE * fp, void const * buf, size_t size)
{
	size_t written;

	written = fwrite(buf, size, 1, fp);

	if (written != 1) {
		fprintf(stderr,
			"oprofiled:op_write_file: wrote less than expected: %lu bytes.\n",
			(unsigned long)size);
		exit(EXIT_FAILURE);
	}
}

/**
 * op_write_u8 - write a byte to a file
 * @param fp  file pointer
 * @param val  value to write
 *
 * Write an unsigned byte value val to a file.
 * Failure is fatal.
 */
void op_write_u8(FILE * fp, u8 val)
{
	op_write_file(fp, &val, sizeof(val));
}

/**
 * op_write_u32 - write four bytes to a file
 * @param fp  file pointer
 * @param val  value to write
 *
 * Write an unsigned four-byte value val to a file.
 * Failure is fatal.
 *
 * No byte-swapping is done.
 */
void op_write_u32(FILE * fp, u32 val)
{
	op_write_file(fp, &val, sizeof(val));
}

/**
 * op_write_u64 - write eight bytes to a file
 * @param fp  file pointer
 * @param val  value to write
 *
 * Write an unsigned eight-byte value val to a file.
 * Failure is fatal.
 *
 * No byte-swapping is done.
 */
void op_write_u64(FILE * fp, u64 val)
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
 * up to the first '\0' or '\n'. A trailing '\n' is deleted.
 *
 * Returns the dynamically-allocated string containing
 * that line. At the end of a file NULL will be returned.
 * be returned.
 *
 * The string returned must be free()d by the caller.
 *
 * getline() is not a proper solution to replace this function
 */
char *op_get_line(FILE * fp)
{
	char * buf;
	char * cp;
	int c;
	size_t max = 512;

	buf = xmalloc(max);
	cp = buf;

	while (1) {
		switch (c = getc(fp)) {
			case EOF:
				free(buf);
				return NULL;
				break;

			case '\n':
			case '\0':
				*cp = '\0';
				return buf;
				break;

			default:
				*cp = (char)c;
				cp++;
				if (((size_t)(cp - buf)) == max) {
					buf = xrealloc(buf, max + 128);
					cp = buf + max;
					max += 128;
				}
				break;
		}
	}
}


/**
 * from a symlink filename get the pointed filename
 * @param filename the symlink filename
 *
 *  On error the returned string is NULL else the returned pointer
 * is dynamically allocated.
 *
 */
char * op_get_link(char const * filename)
{
	char  * linkbuf;
	int c;

	linkbuf = xmalloc(FILENAME_MAX);

	c = readlink(filename, linkbuf, FILENAME_MAX);

	if (c == -1) {
		free(linkbuf);
		return NULL;
	}

	if (c == FILENAME_MAX)
		linkbuf[FILENAME_MAX-1] = '\0';
	else
		linkbuf[c] = '\0';
	return linkbuf;
}
