/* COPYRIGHT (C) 2001 Philippe Elie
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59 Temple
 * Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;

#define OPD_MAGIC 0xdeb6

/* at the end of the sample files */
struct opd_footer_v2 {
	u16 magic;
	u16 version;
	u8 is_kernel;
	u8 ctr0_type_val;
	u8 ctr1_type_val;
	u8 ctr0_um;
	u8 ctr1_um;
	char md5sum[16];
};

struct opd_footer_v3 {
	struct opd_footer_v2 v2;
	u32 ctr0_count;
	u32 ctr1_count;
	/* Set to 0.0 if not available */
	double cpu_speed;
	/* binary compatibility reserve */
	u32  reserved[32];
};

/*
 * Precondition : fp is seeked on the begin of an opd_footer_vn, it is always ok to
 * read an opd_footer_v2 at the begin of function but the version number field is not
 * necessarilly 2
 */
typedef void (*fct_convert)(FILE* fp);

struct converter {
	fct_convert convert;
	int sizeof_struct;
	int version;
};

static void v2_to_v3(FILE* fp) {
	struct opd_footer_v2 footer_v2;
	struct opd_footer_v3 footer_v3;

	fread(&footer_v2, sizeof(footer_v2), 1, fp);

	footer_v3.v2 = footer_v2;
	footer_v3.v2.version = 3;

	footer_v3.ctr0_count = 0;
	footer_v3.ctr1_count = 0;
	footer_v3.cpu_speed = 0.0;
	/* binary compatibility reserve */
	memset(&footer_v3.reserved, '\0', sizeof(footer_v3.reserved));

	fseek(fp, -sizeof(footer_v2), SEEK_END);
	fwrite(&footer_v3, sizeof(footer_v3), 1, fp);
}

/* provide a default conversion function which just increase the version number, It must
 * be used when a reserved field is used without changing the total sizeof of opd_footer
 * *and* the zero value in the reserve work correctly with your new code */
/* Not used for now so no static to avoid warning. This code has not been tested */
/*static*/ void bump_version(FILE* fp) 
{
	struct opd_footer_v2 footer_v2;

	fread(&footer_v2, sizeof(footer_v2), 1, fp);

	footer_v2.version++;

	fseek(fp, -sizeof(footer_v2), SEEK_CUR);

	fwrite(&footer_v2, sizeof(footer_v2), 1, fp);
}

static struct converter converter_array[] = {
	{ v2_to_v3, sizeof(struct opd_footer_v2), 2 },
};

/* This acts as a samples of how to add new conversion from version N to M :

  define a  :

struct opd_footer_vM 
  { 
  struct opd_footer_vN; 
  // additionnal field

  };

 define a function :
void vN_to_vM(FILE* fp) {
  // get inspiration from v2_to_v3 code ...
}

  add the following entry at the end of converter_array:

	{ vN_to_vM, sizeof(struct opd_footer_vN), N },

  Note than if you use the reserved field to add additionnal field *and* the 0 binary
  value of the reserve acts as a default value for the new code which  use this new
  field then you do not have to write a converter, simply add the following line to the 
  conversion array:
     { bump_version, sizeof(struct opd_footer_vN), N },

  PHIL 20010623 : these features has not been tested.
*/

#define nr_converter (sizeof(converter_array) / sizeof(converter_array[0]))

/* return -1 if no converter are available, else return the index of the first
 * converter  */
static int get_converter_index(FILE* fp) 
{
	struct opd_footer_v2 footer_v2;
	int i;

	for (i = 0 ; i < nr_converter ; ++i) {

		fseek(fp, -converter_array[i].sizeof_struct, SEEK_END);
		fread(&footer_v2, sizeof(footer_v2), 1, fp);

		if (footer_v2.magic == OPD_MAGIC && 
		    footer_v2.version == converter_array[i].version) {

			fseek(fp, -converter_array[i].sizeof_struct, SEEK_END);

			return i;
		}
	}

	return -1;
}

int main(int argc, char* argv[]) 
{
	FILE* fp;
	int converter_index;
	int err = 0;
	int i;

	if (argc <= 1) {
		fprintf(stderr, "Syntax: %s filename [filenames]\n", argv[0]);

		exit(1);
	}

	/* Should use popt in future if new options are added */
	if (strcmp(argv[1], "--version") == 0 || strcmp(argv[1], "-v") == 0) {
		// TODO version string.
		printf("%s: %s compiled on %s %s\n", argv[0], "0.0.0(alpha)", __DATE__, __TIME__);

		exit(0);
	}

	if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
		printf("Syntax : %s filename [filenames]\n", argv[0]);

		exit(0);
	}

	for (i = 1 ; i < argc ; ++i) {
		fp = fopen(argv[i], "r+w");
		if (fp == NULL) {
			fprintf(stderr, "can not open %s for read/write\n", argv[i]);
			err++;
			continue;
		}

		converter_index = get_converter_index(fp);
		if (converter_index == -1) {
			fclose(fp);

			fprintf(stderr, "can not find a converter for %s (perhaps this file is already converted ?)\n", argv[i]);
			err++;
			continue;
		}

		for ( ; converter_index < nr_converter ; ++converter_index) {
			converter_array[converter_index].convert(fp);
		}

		fclose(fp);
	}

	return err;
}
