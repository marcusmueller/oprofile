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


FILE * op_try_open_file(char const * name, char const * mode)
{
	return op_do_open_file(name, mode, 0);
}


FILE * op_open_file(char const * name, char const * mode)
{
	return op_do_open_file(name, mode, 1);
}


void op_close_file(FILE * fp)
{
	if (fclose(fp))
		perror("oprofiled:op_close_file: ");
}


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


void op_write_u8(FILE * fp, u8 val)
{
	op_write_file(fp, &val, sizeof(val));
}


void op_write_u32(FILE * fp, u32 val)
{
	op_write_file(fp, &val, sizeof(val));
}


void op_write_u64(FILE * fp, u64 val)
{
	op_write_file(fp, &val, sizeof(val));
}


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
