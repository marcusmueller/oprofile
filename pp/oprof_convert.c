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

/* Do not touch this file, the v5 samples files format make it easier to
 * convert but the method used here is not adapated. This utilities can convert
 * from version 2,3,4 to 5 newer version should use a new stuff */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>

#include "../version.h"
#include "../dae/opd_util.h"
#include "../util/misc.h"

static char * filename;
 
/* magic number <= v4 */
#define OPD_MAGIC_V4 0xdeb6

#define OPD_MAGIC_V5 "DAE\n"

/* at the end of the sample files */
struct opd_header_v2 {
	u16 magic;
	u16 version;
	u8 is_kernel;
	u8 ctr0_type_val;
	u8 ctr1_type_val;
	u8 ctr0_um;
	u8 ctr1_um;
	char md5sum[16];
};

struct opd_header_v3 {
	struct opd_header_v2 v2;
	u32 ctr0_count;
	u32 ctr1_count;
	/* Set to 0.0 if not available */
	double cpu_speed;
	/* binary compatibility reserve */
	u32  reserved[32];
};

struct opd_header_v4 {
	struct opd_header_v2 v2;
	u32 ctr0_count;
	u32 ctr1_count;
	/* Set to 0.0 if not available */
	double cpu_speed;
	/* binary compatibility reserve */
	time_t mtime;
	u32  reserved[31];
};

/* At the begin of samples files */
struct opd_header_v5 {
	u8  magic[4];
	u32 version;
	u8 is_kernel;
	u32 ctr_event;
	u32 ctr_um;
	/* ctr number, used for sanity checking */
	u32 ctr;
	u32 cpu_type;
	u32 ctr_count;
	double cpu_speed;
	time_t mtime;
	/* binary compatibility reserve */
	u32  reserved2[21];
};

/*
 * Precondition : fp is seeked on the begin of an opd_header_vn, it is always ok to
 * read an opd_header_v2 at the begin of function but the version number field is not
 * necessarilly 2
 */
typedef void (*fct_convert)(FILE* fp);

struct converter {
	fct_convert convert;
	int sizeof_struct;
	int version;
};

static void v2_to_v3(FILE* fp) {
	struct opd_header_v2 header_v2;
	struct opd_header_v3 header_v3;

	fread(&header_v2, sizeof(header_v2), 1, fp);

	header_v3.v2 = header_v2;
	header_v3.v2.version = 3;

	header_v3.ctr0_count = 0;
	header_v3.ctr1_count = 0;
	header_v3.cpu_speed = 0.0;
	/* binary compatibility reserve */
	memset(&header_v3.reserved, '\0', sizeof(header_v3.reserved));

	fseek(fp, -sizeof(header_v2), SEEK_END);
	fwrite(&header_v3, sizeof(header_v3), 1, fp);
}

/* this function is used only between conversion from v3 to v4, (ChangeLog
 * 2001-07-25), but backup of samples file has been added at 2001-07-15 (no
 * release between this two date). This means than cvs between this can have
 * backup and this files are mishandled by this function du the "-%d" suffix
 * It is a don't take care case.
 *
 * Symptom is mtime checking loss (mtime is set to 0), people who have
 * this problem have tried to use a cvs alpha version as a production tool
 * let's fix themself the problem */
static char * get_binary_name(void)
{
	char *file; 
	char *mang;
	char *c;

	mang = opd_relative_to_absolute_path(filename, NULL);
		 
	c = &mang[strlen(mang)];
	/* strip leading dirs */
	while (c != mang && *c != '/')
		c--;

	c++;

	file = xstrdup(c);

	c=file;

	do {
		if (*c == OPD_MANGLE_CHAR)
			*c='/';
	} while (*c++);

	free(mang);
	return file; 
}

/* just reset the md5sum to 0, don't change the size of opd_header. */
static void v3_to_v4(FILE* fp) {
	struct opd_header_v3 header_v3;
	struct opd_header_v4 header_v4;
	char * name;

	printf("v4\n"); 
	fread(&header_v3, sizeof(header_v3), 1, fp);

	header_v4.v2 = header_v3.v2;
	header_v4.v2.version = 4;

	header_v4.ctr0_count = header_v3.ctr0_count;
	header_v4.ctr1_count = header_v3.ctr1_count;
	header_v4.cpu_speed  = header_v3.cpu_speed;

	name = get_binary_name();
	header_v4.mtime = opd_get_mtime(name);
	free(name); 

	memset(&header_v4.v2.md5sum, '\0', sizeof(header_v4.v2.md5sum));

	/* binary compatibility reserve */
	memset(&header_v4.reserved, '\0', sizeof(header_v4.reserved));

	fseek(fp, -sizeof(header_v3), SEEK_END);
	fwrite(&header_v4, sizeof(header_v4), 1, fp);
}
 
/* the "old" samples file format */
struct old_opd_fentry {
	u32 count0;
	u32 count1;
};

static op_cpu cpu_type = CPU_NO_GOOD;

/* PHE FIXME: really a fucking function, if you have problem with it flame
 * me at <phe@club-internet.fr> */
/* create one row file during translate from colum to row format v4 --> v5 */
static void do_mapping_transfer(uint nr_samples, int counter, 
				const struct opd_header_v4* header_v4, 
				const struct old_opd_fentry* old_samples,
				int backup_number)
{
	char* out_filename;
	int out_fd;
	struct opd_header_v5* header;
	u32* samples;
	uint k;
	u32 size;
	int dirty;

	if (cpu_type == -1 || cpu_type < 0 || cpu_type > 3) {
		/* do not default it to allow tricky user to convert on a
		 * hardware that is not the harware used to generate the sample
		 * file */
		fprintf(stderr, "converting %s to new file format require option --cpu-type=[0|1|2|3]\n", filename);
		fprintf(stderr, "use \"op_help --get-cpu-type\" to get your cpu type or specify the cpu type used to generate this samples file\n");
		return;
	}

	out_filename = (char *)xmalloc(strlen(filename) + 32);
	strcpy(out_filename, filename);
	sprintf(out_filename + strlen(out_filename), "#%d", counter);

	size = (nr_samples * sizeof(u32)) + sizeof(struct opd_header_v5);

	/* New samples files are lazilly created, respect this behavior
	 * by not creating file with no valid ctr_type_val */
	dirty = 0;

	if ((counter == 0 && header_v4->v2.ctr0_type_val) ||
	    (counter == 1 && header_v4->v2.ctr1_type_val)) {
		if (backup_number != -1)
			sprintf(out_filename + strlen(filename), "-%d",
				backup_number);

		unlink(out_filename);

		out_fd = open(out_filename, O_CREAT|O_RDWR, 0644);
		if (out_fd == -1) {
			fprintf(stderr,"oprofiled: open of image sample file \"%s\" failed: %s\n", out_filename, strerror(errno));
			goto err1;
		}

		/* truncate to grow the file is ok on linux */
		if (ftruncate(out_fd, size) == -1) {
			fprintf(stderr, "oprof_convert: ftruncate failed for \"%s\". %s\n", out_filename, strerror(errno));
			goto err2;
		}

		header = (struct opd_header_v5*)mmap(0, size, PROT_READ|PROT_WRITE, MAP_SHARED, out_fd, 0);

		if (header == (void *)-1) {
			fprintf(stderr,"oprof_convert: mmap of output sample file \"%s\" failed: %s\n", out_filename, strerror(errno));
			goto err2;
		}

		samples = (u32 *)(header + 1);

		memset(header, '\0', sizeof(struct opd_header_v5));
		memcpy(header->magic, OPD_MAGIC_V5, sizeof(header->magic));
		header->version = 5;
		header->is_kernel = header_v4->v2.is_kernel;
		header->ctr_event = (counter == 0)
			? header_v4->v2.ctr0_type_val 
			: header_v4->v2.ctr1_type_val;
		
		header->ctr_um =  (counter == 0)
			? header_v4->v2.ctr0_um 
			: header_v4->v2.ctr1_um;

		header->ctr_count = (counter == 0)
			? header_v4->ctr0_count 
			: header_v4->ctr1_count;

		header->ctr = counter;

		header->cpu_type = cpu_type;
		header->cpu_speed = header_v4->cpu_speed;
		header->mtime = header_v4->mtime;

		for (k = 0 ; k < nr_samples ; ++k) {
			u32 sample = counter
				? old_samples[k].count1
				: old_samples[k].count0;

			/* keep the sparse of the file */
			if (sample) {
				samples[k] = sample;
				dirty = 1;
			}
		}

		munmap(header, size);

err2:
		close(out_fd);
err1:;
	}

	/* respect the new lazilly file creation behavior of the daemon */
	if (dirty == 0) {
		unlink(out_filename);
	}

	free(out_filename);
}

/* PHE FIXME: really a fucking function, if you have problem with it flame
 * me at <phe@club-internet.fr> */
static void v4_to_v5(FILE* fp)
{
	struct opd_header_v4 header_v4;
	char *backup_str;
	int backup_number;
	int ok;
	int counter;
	int old_size, old_fd, nr_samples;
	size_t len_filename;
	struct old_opd_fentry *old_samples;

	fread(&header_v4, sizeof(header_v4), 1, fp);

	ok = 1;
	backup_number = -1;

	backup_str = strrchr(filename, '-');
	if (backup_str) {
		int bytes_read = 0;
		if (sscanf(backup_str + 1, "%d%n", &backup_number, &bytes_read) == 1) {

			/* Dangerous if the bin filename end with "-%d" I
			 * prefer reject this corner case */
			ok = backup_str[bytes_read] && !backup_str[bytes_read + 1];
		}
	}

	if (ok == 0) {
		backup_number = -1;
	}

	old_fd = open(filename, O_RDONLY);
	if (old_fd == -1) {
		fprintf(stderr, "oprof_convert: Opening %s failed. %s\n", filename, strerror(errno));
		return;
	}

	old_size = opd_get_fsize(filename, 1);
	if (old_size < (off_t)sizeof(struct opd_header_v4)) {
		fprintf(stderr, "oprof_convert: sample file %s not big enough big %d, expected %d\n", 
			filename, old_size, sizeof(struct opd_header_v4));
		goto err1;
	}

	old_samples = (struct old_opd_fentry *)mmap(0, old_size, PROT_READ, MAP_PRIVATE, old_fd, 0);
	if (old_samples == (void *)-1) {
		fprintf(stderr, "oprof_convert: mmap of %s failed. %s\n", filename, strerror(errno));
		goto err1;
	}

	nr_samples = (old_size - sizeof(struct opd_header_v4)) / sizeof(struct old_opd_fentry);

	len_filename = strlen(filename);

	for (counter = 0; counter < 2 ; ++counter) {
		do_mapping_transfer(nr_samples, counter,
				    &header_v4, old_samples,
				    backup_number);
	}

	munmap(old_samples, old_size);

err1:
	close(old_fd);
}

static struct converter converter_array[] = {
	{ v2_to_v3, sizeof(struct opd_header_v2), 2 },
	{ v3_to_v4, sizeof(struct opd_header_v3), 3 }, 
	{ v4_to_v5, sizeof(struct opd_header_v4), 4 }, 
};

#define nr_converter (signed int)((sizeof(converter_array) / sizeof(converter_array[0])))

/* return -1 if no converter are available, else return the index of the first
 * converter  */
static int get_converter_index(FILE* fp, int last_index) 
{
	struct opd_header_v2 header_v2;
	int i;

	for (i = last_index ; i < nr_converter ; ++i) {

		fseek(fp, -converter_array[i].sizeof_struct, SEEK_END);
		fread(&header_v2, sizeof(header_v2), 1, fp);

		if (header_v2.magic == OPD_MAGIC_V4 && 
		    header_v2.version == converter_array[i].version) {

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
	int converted;

	if (argc <= 1) {
		fprintf(stderr, "Syntax: %s filename [filenames]\n", argv[0]);

		exit(EXIT_FAILURE);
	}

	/* Should use popt in future if new options are added */
	if (strcmp(argv[1], "--version") == 0 || strcmp(argv[1], "-v") == 0) {
		printf("%s : " VERSION_STRING " compiled on " __DATE__ " " __TIME__ "\n", argv[0]);

		exit(EXIT_SUCCESS);
	}

	if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
		printf("Syntax : %s filename [filenames]\n", argv[0]);

		exit(EXIT_SUCCESS);
	}

	i = 1;

	/* for v4 --> v5 */
	if (strncmp(argv[i], "--cpu-type", strlen("--cpu-type")) == 0) {
		sscanf(argv[i] + strlen("--cpu-type="), "%d", (int *)&cpu_type);
		++i;
	}

	for ( ; i < argc ; ++i) {
		/* used in v3 conversion */
		filename = argv[i];
 
		fp = fopen(argv[i], "r+w");
		if (fp == NULL) {
			fprintf(stderr, "can not open %s for read/write\n", argv[i]);
			err++;
			continue;
		}

		converted = 0;
		converter_index = 0;
		while ((converter_index = get_converter_index(fp, converter_index)) != -1) {
			converter_array[converter_index].convert(fp);
			converter_index++;
			converted = 1;
		}

		if (converted == 0) {
			fprintf(stderr, "no converter found for %s (file already converted ?)\n", argv[i]);

			err++;
		}

		fclose(fp);
	}

	return err;
}
