/* $Id: oprofpp.c,v 1.43 2001/09/15 20:55:45 movement Exp $ */
/* COPYRIGHT (C) 2000 THE VICTORIA UNIVERSITY OF MANCHESTER and John Levon
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

/*
 * You can start this program in various ways, e.g.
 *
 * oprofpp <options> /var/opd/samples/}bin}mv
 * oprofpp <options> /bin/mv
 * oprofpp <options> -f /var/opd/samples/}bin}mv -i /bin/mv
 * oprofpp <options> -i /path/to/the/same/bin/mv /var/opd/samples/}bin}mv
 *
 */

#include "oprofpp.h"
 
static int showvers;
static int verbose; 
static uint op_nr_counters = 2;
 
static char const *samplefile;
static char *basedir="/var/opd";
static const char *imagefile;
static char *gproffile;
static char *symbol;
static int list_symbols;
static int demangle;
static int list_all_symbols_details;
static int output_linenr_info;

/* PHE FIXME would be an array of struct ? */
/* indexed by samples[counter_nr][offset] */
static struct opd_fentry *samples[OP_MAX_COUNTERS];
static struct opd_footer *footer[OP_MAX_COUNTERS];
static char *ctr_name[OP_MAX_COUNTERS];
static char *ctr_desc[OP_MAX_COUNTERS];
static char *ctr_um_desc[OP_MAX_COUNTERS];
static uint nr_samples; 
/* if != -1 appended to samples files with "-%d" format */
static int session_number = -1;
/* PHE FIXME: check the logic and this comment */
/* counter k is selected if (ctr == -1 || ctr == k), if ctr == -1 counter 0
 * is used for sort purpose */
static int ctr = -1;
static u32 sect_offset; 

static int accumulate_samples(u32 counter[OP_MAX_COUNTERS], int index);

static struct poptOption options[] = {
	{ "samples-file", 'f', POPT_ARG_STRING, &samplefile, 0, "image sample file", "file", },
	{ "image-file", 'i', POPT_ARG_STRING, &imagefile, 0, "image file", "file", },
	{ "list-symbols", 'l', POPT_ARG_NONE, &list_symbols, 0, "list samples by symbol", NULL, },
	{ "dump-gprof-file", 'g', POPT_ARG_STRING, &gproffile, 0, "dump gprof format file", "file", },
	{ "list-symbol", 's', POPT_ARG_STRING, &symbol, 0, "give detailed samples for a symbol", "symbol", },
	{ "demangle", 'd', POPT_ARG_NONE, &demangle, 0, "demangle GNU C++ symbol names", NULL, },
	/* DJ FIXME: --counter should show max-counter.*/
	{ "counter", 'c', POPT_ARG_INT, &ctr, 0, "which counter to use", "0|1", }, 
	{ "version", 'v', POPT_ARG_NONE, &showvers, 0, "show version", NULL, },
	{ "verbose", 'V', POPT_ARG_NONE, &verbose, 0, "verbose output", NULL, },
	{ "base-dir", 'b', POPT_ARG_STRING, &basedir, 0, "base directory of profile daemon", NULL, }, 
	{ "list-all-symbols-details", 'L', POPT_ARG_NONE, &list_all_symbols_details, 0, "list samples for all symbols", NULL, },
	{ "output-linenr-info", 'o', POPT_ARG_NONE, &output_linenr_info, 0, "output filename:linenr info", NULL },
	/* PHE FIXME document later */
	{ "session-number", 'S', POPT_ARG_INT, &session_number, 0, "which session use", "session number" },
	POPT_AUTOHELP
	{ NULL, 0, 0, NULL, 0, NULL, NULL, },
};

char *remangle(const char *image)
{
	char *file;
	char *c; 

	if (!basedir || !*basedir)
		basedir = "/var/opd";
	else if (basedir[strlen(basedir)-1] == '/')
		basedir[strlen(basedir)-1] = '\0';

	file = opd_malloc(strlen(basedir) + strlen("/samples/") + strlen(image) + 1);
	
	strcpy(file, basedir);
	strcat(file, "/samples/");
	c = &file[strlen(file)];
	strcat(file, image);

	while (*c) {
		if (*c == '/')
			*c = OPD_MANGLE_CHAR;
		c++;
	}
	
	return file;
}

/**
 * get_options - process command line
 * @argc: program arg count
 * @argv: program arg array
 *
 * Process the arguments, fatally complaining on
 * error. samplefile is guaranteed to have some
 * non-NULL value after this function.
 */
static void get_options(int argc, char const *argv[])
{
	poptContext optcon;
	char c; 
	const char *file;
	const char *file_session_str;
	char *file_ctr_str;
	int counter;
	
	/* Some old version of popt needs the cast to char ** */
	optcon = poptGetContext(NULL, argc, (char **)argv, options, 0);

	c=poptGetNextOpt(optcon);

	if (c<-1) {
		fprintf(stderr, "oprofpp: %s: %s\n",
	                poptBadOption(optcon, POPT_BADOPTION_NOALIAS),
	                poptStrerror(c));
	        poptPrintHelp(optcon, stderr, 0);
	        exit(EXIT_FAILURE);
	}

	if (showvers) {
		printf(VERSION_STRING " compiled on " __DATE__ " " __TIME__ "\n");
		exit(EXIT_SUCCESS);
	}
 
	if (!list_all_symbols_details && !list_symbols && 
	    !gproffile && !symbol) {
		fprintf(stderr, "oprofpp: no mode specified. What do you want from me ?\n");
		poptPrintHelp(optcon, stderr, 0);
		exit(EXIT_FAILURE);
	}

	/* check only one major mode specified */
	if ((list_all_symbols_details + list_symbols + (gproffile != 0) + (symbol != 0)) > 1) {
		fprintf(stderr, "oprofpp: must specify only one output type.\n");
		poptPrintHelp(optcon, stderr, 0);
		exit(EXIT_FAILURE);
	}

	if (output_linenr_info && !list_all_symbols_details && !symbol) {
		fprintf(stderr, "oprofpp: cannot list debug info without -L or -s option.\n");
		poptPrintHelp(optcon, stderr, 0);
		exit(EXIT_FAILURE);
	}
 
	/* non-option file, either a sample or binary image file */
	file = poptGetArg(optcon);

	/* some minor memory leak from the next call */
	if (imagefile)
		imagefile = opd_relative_to_absolute_path(imagefile, NULL);

	if (samplefile)
		samplefile = opd_relative_to_absolute_path(samplefile, NULL);

	if (file)
		file = opd_relative_to_absolute_path(file, NULL);

	if (file) {
		if (strchr(file, OPD_MANGLE_CHAR))
			samplefile = file;
		else
			imagefile = file;
	}

	if (!samplefile) { 
		if (!imagefile) { 
			fprintf(stderr, "oprofpp: no samples file specified.\n");
			poptPrintHelp(optcon, stderr, 0);
			exit(EXIT_FAILURE);
		} else {
			/* we'll "leak" this memory */
			samplefile = remangle(imagefile);
		}
	}

	/* We must get account 2 var : ctr, mangled filename, if ctr is not
         * provided it can be provided inside the filename itself by the #%d
	 * terminateur, if ctr is given we check an eventual conflict */
	/* PHE FIXME: can be simplified probably ? */
	file_ctr_str = strrchr(samplefile, '#');
	counter = -1;
	if (file_ctr_str) {
		sscanf(file_ctr_str + 1, "%d", &counter);
	}

	if (ctr != counter) {
		/* a --counter=x have priority on the # suffixe of filename */
		if (ctr != -1 && counter != -1) {
			/* conflict between #%d and --ctr option */
			fprintf(stderr, "oprofpp: conflict between %s filename counter nr and --ctr %d option, using --ctr option %d\n", file_ctr_str, ctr, ctr);
			counter = ctr;
		}
	}

	if (ctr == -1)
		ctr = counter;

	if (ctr == -1) {
		/* list_all_symbols_details always output all counter and do
		 * not made any sort, it is the responsability of the backend
		 * (op_to_source) to treat this */
		if (!list_all_symbols_details)
			ctr = 0;
	}

	/* Now record the session number */
	if (file_ctr_str) {
		file_session_str = strchr(file_ctr_str, '-');
		if (file_session_str) {
			sscanf(file_session_str + 1, "%d", &session_number);
		}
	}

	if (file_ctr_str)
		file_ctr_str[0] = '\0';

	/* yes, OP_MAX_COUNTERS. We should initially allow for seeing if
	 * there are samples for any counter
	 */
	if (ctr != -1 && (ctr < 0 || ctr >= OP_MAX_COUNTERS)) {
		fprintf(stderr, "oprofpp: invalid counter number %u\n", ctr);
		exit(EXIT_FAILURE);
	}
}

/**
 * printf_symbol - output a symbol name
 * @name: verbatim symbol name
 *
 * Print the symbol name to stdout, demangling
 * the symbol if the global variable demangle is %TRUE.
 *
 * The demangled name lists the parameters and type
 * qualifiers such as "const".
 */
void printf_symbol(const char *name)
{
	if (demangle) {
		char *cp = (char *)name;
		char *unmangled = cp;

		while (*cp && *cp == '_')
			cp++;

		if (*cp) {
			unmangled = cplus_demangle(cp, DMGL_PARAMS | DMGL_ANSI);
			if (unmangled) {
				/* FIXME: print leading underscores ? */
				printf("%s", unmangled);
				free(unmangled);
				return;
			}
		}
	}
	printf("%s", name);
}

/**
 * open_image_file - open associated binary image
 * @mangled: name of samples file
 * @mtime: mtime from samples file
 *
 * This function will open and process the binary
 * image associated with the samples file @mangled,
 * unless imagefile is non-%NULL, in which case
 * that will be opened instead.
 *
 * @mangled may be a relative or absolute pathname.
 *
 * @mangled may be %NULL if imagefile is the filename
 * of the image to open.
 *
 * Failure to open the image is fatal. This function
 * returns a pointer to the bfd descriptor for the
 * image.
 *
 * The global struct footer must be valid. sect_offset
 * will be set as appropriate.
 */
bfd *open_image_file(char const * mangled, time_t mtime)
{
	char *file;
	char **matching;
	time_t newmtime;
	bfd *ibfd;
	uint i;
	 
	file = (char *)imagefile;

	if (!mangled) {
		if (!file)
			return NULL;
	} else if (!imagefile) {
		char *mang;
		char *c;

		mang = opd_strdup(mangled); 
		 
		c = &mang[strlen(mang)];
		/* strip leading dirs */
		while (c != mang && *c != '/')
			c--;

		c++;

		file = opd_strdup(c);

		c=file;

		do {
			if (*c == OPD_MANGLE_CHAR)
				*c='/';
		} while (*c++);

		free(mang);
	}

	newmtime = opd_get_mtime(file);
	if (newmtime != mtime) {
		fprintf(stderr, "oprofpp: the last modified time of the binary file %s does not match "
			"that of the sample file. Either this is the wrong binary or the binary "
			"has been modified since the sample file was created.\n", file);
		exit(EXIT_FAILURE);
	}
			 
	ibfd = bfd_openr(file, NULL);
 
	if (!ibfd) {
		fprintf(stderr,"oprofpp: bfd_openr of %s failed.\n", file);
		exit(EXIT_FAILURE);
	}
	 
	if (!bfd_check_format_matches(ibfd, bfd_object, &matching)) { 
		fprintf(stderr,"oprofpp: BFD format failure for %s.\n", file);
		exit(EXIT_FAILURE);
	}
 
 	if (!imagefile)
		free(file);

	for (i = 0; i < op_nr_counters; ++i) {
		if (footer[i])
			break;
	}

	/* should never happens */
	if (i == op_nr_counters) {
		fprintf(stderr,"oprofpp: open_image_file() no samples file open for %s.\n", file);
		exit(EXIT_FAILURE);
	}

	if (footer[i]->is_kernel) {
		asection *sect; 
 
		sect = bfd_get_section_by_name(ibfd, ".text");
		sect_offset = OPD_KERNEL_OFFSET - sect->filepos;
		verbprintf("Adjusting kernel samples by 0x%x, .text filepos 0x%lx\n", sect_offset, sect->filepos); 
	}
 
	return ibfd; 
}

/**
 * sym_offset - return offset from a symbol's start
 * @sym: symbol
 * @num: number of fentry
 *
 * Returns the offset of a sample at position @num
 * in the samples file from the start of symbol @sym.
 */
u32 sym_offset(asymbol *sym, u32 num)
{
	if (num - sect_offset > num) {
		fprintf(stderr,"oprofpp: less than zero offset ? \n");
		exit(EXIT_FAILURE); 
	}
	 
	/* adjust for kernel images */
	num -= sect_offset;
	/* take off section offset */
	num -= sym->section->filepos;
	/* and take off symbol offset from section */
	num -= sym->value;

	return num;
}
 
int symcomp(const void *a, const void *b)
{
	u32 va = (*((asymbol **)a))->value + (*((asymbol **)a))->section->filepos; 
	u32 vb = (*((asymbol **)b))->value + (*((asymbol **)b))->section->filepos; 

	if (va < vb)
		return -1;
	return (va > vb);
}

/* need a better filter, but only this gets rid of _start
 * and other crud easily. 
 */
static int interesting_symbol(asymbol *sym)
{
	if (!(sym->section->flags & SEC_CODE))
		return 0;

	if (streq("", sym->name))
		return 0;

	if (streq("_init", sym->name))
		return 0;

	if (!(sym->flags & BSF_FUNCTION))
		return 0;

	return 1;
}

/**
 * translate_address - lookup and output linenr info from a vma address
 * in a given section to standard output.
 * @ibfd: the bfd
 * @syms: pointer to array of symbol pointers
 * @section: the section containing the vma
 * @pc: the virtual memory address
 *
 * Do not change output format without changing the corresponding tools
 * that work with output from oprofpp.
 */
static void
translate_address (bfd* ibfd, asymbol **syms, asection *section, bfd_vma pc)
{
	int found;
	bfd_vma vma;

	const char *filename;
	const char *functionname;
	unsigned int line;

	if ((bfd_get_section_flags (ibfd, section) & SEC_ALLOC) == 0)
		return;

	vma = bfd_get_section_vma (ibfd, section);
	if (pc < vma)
		return;

	found = bfd_find_nearest_line (ibfd, section, syms, pc - vma,
				       &filename, &functionname, &line);

	if (!found) {
		printf ("??:0");
	}
	else {
		printf ("%s:%u", filename, line);
	}
}

/**
 * get_symbols - get symbols from bfd
 * @ibfd: bfd to read from
 * @symsp: pointer to array of symbol pointers
 *
 * Parse and sort in ascending order all symbols
 * in the file pointed to by @ibfd that reside in
 * a %SEC_CODE section. Returns the number
 * of symbols found. @symsp will be set to point
 * to the symbol pointer array.
 */
uint get_symbols(bfd *ibfd, asymbol ***symsp)
{
	uint nr_all_syms;
	uint nr_syms = 0; 
	uint i; 
	size_t size;
	asymbol **filt_syms;
	asymbol **syms; 

	if (!(bfd_get_file_flags(ibfd) & HAS_SYMS))
		return 0;

	size = bfd_get_symtab_upper_bound(ibfd);

	/* HAS_SYMS can be set with no symbols */
	if (size<1)
		return 0;

	syms = opd_malloc(size);
	nr_all_syms = bfd_canonicalize_symtab(ibfd, syms);
	if (nr_all_syms < 1) {
	        opd_free(syms);
	        return 0;
	}

	for (i=0; i < nr_all_syms; i++) {
		if (interesting_symbol(syms[i]))
			nr_syms++;
	}
 
	filt_syms = opd_malloc(sizeof(asymbol *) * nr_syms);

	for (nr_syms = 0,i = 0; i < nr_all_syms; i++) {
		if (interesting_symbol(syms[i])) {
			filt_syms[nr_syms] = syms[i];
			nr_syms++;
		}
	}
 
	qsort(filt_syms, nr_syms, sizeof(asymbol *), symcomp);
 
	*symsp = filt_syms;
	return nr_syms;
}
 
/**
 * get_symbol_range - get range of symbol
 * @sym: symbol
 * @next: next symbol
 * @start: pointer to start var
 * @end: pointer to end var
 *
 * Calculates the range of sample file entries covered
 * by @sym. @start and @end will be filled in appropriately.
 * If @next is %NULL, all entries up to the end of the sample
 * file will be used.
 */
void get_symbol_range(asymbol *sym, asymbol *next, u32 *start, u32 *end)
{
	verbprintf("Symbol %s, value 0x%lx\n", sym->name, sym->value); 
	*start = sym->value;
	/* offset of section */
	*start += sym->section->filepos;
	verbprintf("in section %s, filepos 0x%lx\n", sym->section->name, sym->section->filepos);
	/* adjust for kernel image */
	*start += sect_offset;
	if (next) {
		*end = next->value;
		/* offset of section */
		*end += next->section->filepos;
		/* adjust for kernel image */
		*end += sect_offset;
	} else
		*end = nr_samples;
	verbprintf("start 0x%x, end 0x%x\n", *start, *end); 
}
 
struct opp_count {
	asymbol *sym;
	u32 count[OP_MAX_COUNTERS];
};

int countcomp(const void *a, const void *b)
{
	struct opp_count *ca= (struct opp_count *)a;	 
	struct opp_count *cb= (struct opp_count *)b;	 

	/* note than ctr must be sanitized before calling qsort */
	if (ca->count[ctr] < cb->count[ctr])
		return -1;
	return (ca->count[ctr] > cb->count[ctr]);
}
 
/**
 * do_list_symbols - list symbol samples for an image
 * @ibfd: the bfd 
 *
 * Lists all the symbols from the image specified by samplefile,
 * in decreasing sample count order, to standard out.
 */
void do_list_symbols(asymbol **syms, uint num)
{
	struct opp_count *scounts;
	u32 start, end;
	uint tot[OP_MAX_COUNTERS],i,j;
	int found_samples;
	uint k;

	for (i = 0; i < op_nr_counters; ++i) {
		tot[i] = 0;
	}

	scounts = opd_calloc0(num,sizeof(struct opp_count));

	for (i=0; i < num; i++) {
		scounts[i].sym = syms[i];
		get_symbol_range(syms[i], (i == num-1) ? NULL : syms[i+1], &start, &end); 
		/* FIXME PHE: was in old code, seems bad at my eyes (compare
		 * an adress with a number of samples) */
		if (start >= nr_samples) {
			fprintf(stderr,"oprofpp: start 0x%x out of range (max 0x%x)\n", start, nr_samples);
			exit(EXIT_FAILURE);
		}
		if (end > nr_samples) {
			fprintf(stderr,"oprofpp: end 0x%x out of range (max 0x%x)\n", end, nr_samples);
			exit(EXIT_FAILURE);
		}

		for (j = start; j < end; j++) {
			accumulate_samples(scounts[i].count, j);
		}

		for (k = 0 ; k < op_nr_counters ; ++k) {
			tot[k] += scounts[i].count[k];
		}
	}

	qsort(scounts, num, sizeof(struct opp_count), countcomp);

	for (i=0; i < num; i++) {
		printf_symbol(scounts[i].sym->name);

		found_samples = 0;
		for (k = 0; k < op_nr_counters ; ++k) {
			if (scounts[i].count[k]) {
				printf("[0x%.8lx]: %2.4f%% (%u samples)\n", scounts[i].sym->value+scounts[i].sym->section->vma,
				       (((double)scounts[i].count[k]) / tot[k])*100.0, scounts[i].count[k]);
				found_samples = 1;
			}
			
		}
		if (found_samples == 0 && !streq("", scounts[i].sym->name)) {
			printf(" (0 samples)\n");
		}
	}
 
	opd_free(scounts);
}
 
/**
 * do_list_symbol - list detailed samples for a symbol
 * @ibfd: the bfd
 *
 * Lists all the samples for the symbol specified by symbol, from the image 
 * specified by @samplefile, in decreasing sample count order, to standard out.
 */
void do_list_symbol(bfd * ibfd, asymbol **syms, uint num)
{
	u32 start, end;
	u32 i, j;
	uint k;

	for (i=0; i < num; i++) {
		if (streq(syms[i]->name, symbol))
			goto found;
	}

	fprintf(stderr, "oprofpp: symbol \"%s\" not found in image file.\n", symbol);
	return;
found:
	printf("Samples for symbol \"%s\" in image %s\n", symbol, ibfd->filename);
	get_symbol_range(syms[i], (i == num-1) ? NULL : syms[i+1], &start, &end);
	for (j=start; j < end; j++) {
		for (k = 0 ; k < op_nr_counters ; ++k) {
			if (samples[k] && samples[k][j].count)
				break;
		}

		/* k != op_nr_counters <==> one of the counter is not empty */
		if (k != op_nr_counters) {
			bfd_vma vma, base_vma;
			base_vma = syms[i]->value + syms[i]->section->vma;
			vma = sym_offset(syms[i], j) + base_vma;

			if (output_linenr_info)	{
				translate_address(ibfd, syms, syms[i]->section, vma);
				printf(" ");
			}

			printf("%s+%x/%x:", symbol, sym_offset(syms[i], j), end-start);
 
			for (k = 0 ; k < op_nr_counters ; ++k) {
				if (samples[k])
					/* PHE: please keep this space a few
					 * time, I can't live without it ;) */
					printf("\t%u ", samples[k][j].count);
				else
					printf("\t%u", 0U);
			}
			printf("\n");
		}
	}
}

#define GMON_VERSION 1
#define GMON_TAG_TIME_HIST 0
#define MULTIPLIER 2

struct gmon_hdr {
	char cookie[4];
	u32 version;
	u32 spare[3];
};
 
/**
 * do_dump_gprof - produce gprof sample output
 * @ibfd: the bfd 
 *
 * Dump gprof-format samples for the image specified by samplefile to
 * the file specified by gproffile.
 */
/* PHE: this fun has not been tested */
void do_dump_gprof(asymbol **syms, uint num)
{
	static struct gmon_hdr hdr = { "gmon", GMON_VERSION, {0,0,0,},}; 
	FILE *fp; 
	u32 start, end;
	uint i, j;
	u32 low_pc = (u32)-1;
	u32 high_pc = 0;
	u16 * hist;
	u32 histsize;
 
	fp=opd_open_file(gproffile, "w");

	opd_write_file(fp,&hdr, sizeof(struct gmon_hdr));

	opd_write_u8(fp, GMON_TAG_TIME_HIST);

	for (i=0; i < num; i++) {
		start = syms[i]->value + syms[i]->section->vma;
		if (i == num - 1) {
			get_symbol_range(syms[i], NULL, &start, &end);
			end -= start;
			start = syms[i]->value + syms[i]->section->vma;
			end += start;
		} else
			end = syms[i+1]->value + syms[i+1]->section->vma;
 
		if (start < low_pc)
			low_pc = start;
		if (end > high_pc)
			high_pc = end;
	}

	histsize = ((high_pc - low_pc) / MULTIPLIER) + 1; 
 
	opd_write_u32_he(fp, low_pc);
	opd_write_u32_he(fp, high_pc);
	/* size of histogram */
	opd_write_u32_he(fp, histsize);
	/* profiling rate */
	opd_write_u32_he(fp, 1);
	opd_write_file(fp, "samples\0\0\0\0\0\0\0\0", 15); 
	/* abbreviation */
	opd_write_u8(fp, '1');
	
	hist = opd_malloc(sizeof(u16) * histsize); 
	memset(hist, 0, sizeof(u16) * histsize);
 
	for (i=0; i < num; i++) {
		get_symbol_range(syms[i], (i == num-1) ? NULL : syms[i+1], &start, &end); 
		for (j=start; j < end; j++) {
			u32 count = 0;
			u32 pos;
			pos = (sym_offset(syms[i],j) + syms[i]->value + syms[i]->section->vma - low_pc) / MULTIPLIER; 

			/* get_options have set ctr to one value != -1 */
			count = samples[ctr][j].count;

			if (count > (u16)-1) {
				printf("Warning: capping sample count !\n");
				count = (u16)-1;
			}
			if (pos >= histsize) {
				fprintf(stderr, "Bogus histogram bin %u, larger than %u !", pos, histsize);
				continue;
			}
			hist[pos] += (u16)count;
		}
	}
	 
	opd_write_file(fp, hist, histsize * sizeof(u16));
	opd_close_file(fp);
}

/**
 * accumulate_samples - lookup and output linenr info from a vma address
 * in a given section to standard output.
 * @counter: where to accumulate the samples
 * @index: index number of the samples.
 *
 * return 0 if no samples has been found else return 1
 */
static int accumulate_samples(u32 counter[OP_MAX_COUNTERS], int index)
{
	uint k;
	int found_samples = 0;

	for (k = 0; k < op_nr_counters; ++k) {
		if (samples[k] && samples[k][index].count) {
			found_samples = 1;
			counter[k] += samples[k][index].count;
		}
	}

	return found_samples;
}

/**
 * do_list_all_symbols_details - list all samples for all symbols.
 * @ibfd: the bfd
 *
 * Lists all the samples for all the symbols, from the image specified by
 * @samplefile, in increasing order of vma, to standard out.
 *
 * Do not change output format without changing the corresponding tools
 * that work with output from oprofpp
 */
void do_list_all_symbols_details(bfd *ibfd, asymbol **syms, uint num)
{
	uint i, j;
	u32 start, end;

	for (i = 0 ; i < num ; ++i) {
		u32 counter[OP_MAX_COUNTERS];
		uint k;
		int found_samples;

		get_symbol_range(syms[i], (i == num-1) ? NULL : syms[i+1], 
				 &start, &end);

		for (k = 0; k < op_nr_counters; ++k) {
			counter[k] = 0;
		}

		/* To avoid outputing 0 samples symbols */
		found_samples = 0;
		for (j = start; j < end; ++j) {
			found_samples |= accumulate_samples(counter, j);
		}

		if (found_samples) {
			bfd_vma vma, base_vma;

			base_vma = syms[i]->value + syms[i]->section->vma;

			vma = sym_offset(syms[i], start) + base_vma;

			if (output_linenr_info)	{
				translate_address(ibfd, syms, syms[i]->section,
						  vma);

				printf(" ");
			}

			printf("%.8lx ", base_vma);
			for (k = 0 ; k < op_nr_counters ; ++k) {
				printf("%u ", counter[k]);
			}
			printf_symbol(syms[i]->name);
			printf("\n");

			for (j = start; j < end; j++) {

				for (k = 0; k < op_nr_counters; ++k) {
					counter[k] = 0;
				}

				found_samples = accumulate_samples(counter, j);

				if (found_samples) {
					vma = sym_offset(syms[i], j) + 
						base_vma;

					if (output_linenr_info)	{
						printf(" ");

						translate_address(ibfd, syms,
								  syms[i]->section,
								  vma);
					}

					printf(" %.8lx", vma);

					for (k = 0; k < op_nr_counters; ++k) {
						printf(" %u", counter[k]);
					}

					printf("\n");
				}
			}
		}
	}
}

/**
 * open_samples_file - open a samples file
 * @counter: the counter number
 * @size: where to return the mapped size, this do NOT
 * include the footer size
 * @can_fail: allow to fail gracefully
 *
 * open and mmap the given samples files, return -1 
 * the global var samples[@counter] and footer[@counter]
 * are updated in case of success else a fatal error occur
 * The following field of the footer are checked
 *	u8  magic[4];
 *	u16 version;
 *	u8 ctr_event;
 *	u8 ctr_um;
 *	u8 ctr;
 *	u8 cpu_type;
 *
 * return the file descriptor (can be -1 if @can_fail) else
 * produces a fatal error
 */
static fd_t open_samples_file(u32 counter, size_t *size,
			      int can_fail)
{
	fd_t fd;
	char* temp;

	temp = opd_malloc(strlen(samplefile) +  32);

	strcpy(temp, samplefile);

	sprintf(temp + strlen(temp), "#%d", counter);
	if (session_number != -1)
		sprintf(temp + strlen(temp), "-%d", session_number);

	fd = open(temp, O_RDONLY);
	if (fd == -1) {
		if (!can_fail)	{
			fprintf(stderr, "oprofpp: Opening %s failed. %s\n", temp, strerror(errno));
			exit(EXIT_FAILURE);
		}

		goto err1;
	}

	*size = opd_get_fsize(temp, 1) - sizeof(struct opd_footer); 
	if (*size < sizeof(struct opd_footer)) {
		if (!can_fail) {
			fprintf(stderr, "oprofpp: sample file %s not enough big %d, expected %d\n", temp, *size, sizeof(struct opd_footer));
			exit(EXIT_FAILURE);
		}

		goto err2;
	}

	footer[counter] = mmap(0, *size + sizeof(struct opd_footer), PROT_READ,
			       MAP_PRIVATE, fd, 0);
	if (footer[counter] == (void *)-1) {
		if (!can_fail) {
			fprintf(stderr, "oprofpp: mmap of %s failed. %s\n", temp, strerror(errno));
			exit(EXIT_FAILURE);
		}
		goto err2;
	}

	/* need a cast here :( */
	samples[counter] = (struct opd_fentry *)(footer[counter] + 1);

	if (memcmp(footer[counter]->magic, OPD_MAGIC, sizeof(footer[0]->magic))) {
		if (!can_fail) {
			/* FIXME: is 4.4 ok : there is no zero terminator */
			fprintf(stderr, "oprofpp: wrong magic %4.4s, expected %s.\n", footer[counter]->magic, OPD_MAGIC);
			exit(EXIT_FAILURE);
		}
		goto  err3;
	}

	if (footer[counter]->version != OPD_VERSION) {
		if (!can_fail) {
			fprintf(stderr, "oprofpp: wrong version 0x%x, expected 0x%x.\n", footer[counter]->version, OPD_VERSION);
			exit(EXIT_FAILURE);
		}
		goto err3;
	}

	/* This should be warrented by the daemon */
	if (footer[counter]->ctr != counter) {
		if (!can_fail) {
			fprintf(stderr, "oprofpp: sanity check counter number fail %d, expect %d.\n", footer[counter]->ctr, counter);
			exit(EXIT_FAILURE);
		}
		goto err3;
	}

	opd_free(temp);

	return fd;

err3:
	munmap(footer[counter], *size + sizeof(struct opd_footer));
err2:
	close(fd);
err1:
	fd = -1;
	footer[counter] = NULL;
	samples[counter] = NULL;
	*size = 0;
	return fd;
}

/**
 * check_and_output_event - translate event number, um mask
 * to string and output them.
 * @counter: counter number
 *
 * if event is valid output to stdout the description
 * of event.
 */
static void check_and_output_event(int i)
{
	op_get_event_desc(footer[i]->cpu_type, footer[i]->ctr_event,
			  footer[i]->ctr_um, &ctr_name[i], &ctr_desc[i],
			  &ctr_um_desc[i]);

	printf("Counter %d counted %s events (%s) with a unit mask of "
	       "0x%.2x (%s) count %u\n", 
	       i, ctr_name[i], ctr_desc[i], footer[i]->ctr_um,
	       ctr_um_desc[i] ? ctr_um_desc[i] : "Not set", 
	       footer[i]->ctr_count);
}

/**
 * check_footers - check coherence between two footers.
 * @f1: first footer
 * @f2: second footer
 *
 * verify than footer @f1 and @f2 are coherent.
 * fatal error ocur if the footers are not coherent.
 */
static void check_footers(struct opd_footer* f1, struct opd_footer* f2)
{
	if (f1->mtime != f2->mtime) {
		fprintf(stderr, "oprofpp: footer time stamp are different (%ld, %ld)\n", f1->mtime, f2->mtime);
		exit(EXIT_FAILURE);
	}

	if (f1->is_kernel != f2->is_kernel) {
		fprintf(stderr, "oprofpp: footer is_kernel are different\n");
		exit(EXIT_FAILURE);
	}

	if (f1->cpu_speed != f2->cpu_speed) {
		fprintf(stderr, "oprofpp: footer cpu speed are different (%f, %f)",
			f2->cpu_speed, f2->cpu_speed);
		exit(EXIT_FAILURE);
	}
}

int main(int argc, char const *argv[])
{
	bfd *ibfd; 
	asymbol **syms;
	uint i, j;
	fd_t fd[OP_MAX_COUNTERS];
	size_t size[OP_MAX_COUNTERS];
	uint num;
	time_t mtime = 0;
 
	get_options(argc, argv);

	/* op_nr_counters is discovered from the samples itself, until we
	 * have opened the samples file(s) we must loop over all the possible
	 * available counters. This is a little what ugly but it allows
	 * to interpret samples files on a different hardware where the
	 * the profiler has run */
	for (i = 0; i < OP_MAX_COUNTERS; ++i) {
		fd[i] = -1;
		size[i] = 0;
		samples[i] = NULL;
		footer[i] = NULL;
	}

	for (i = 0; i < OP_MAX_COUNTERS ; ++i) {
		if (ctr == -1 || ctr == (int)i)
			/* if ctr == i, this means than we open only one
			 * samples file so don't allow opening failure to get
			 * a more precise error message */
			fd[i] = open_samples_file(i, &size[i], ctr != (int)i);
	}

	for (i = 0; i < OP_MAX_COUNTERS ; ++i) {
		if (fd[i] != -1)
			break;
	}

	if (i == OP_MAX_COUNTERS) {
		fprintf(stderr, "Can not open any samples files for %s last error %s\n", samplefile, strerror(errno));
		exit(EXIT_FAILURE);
	}

	/* sanity check between the different samples files */
	for (i = 0 ; i < OP_MAX_COUNTERS; ++i) {
		if (fd[i] != -1)
			break;
	}

	for (j = i + 1; j < OP_MAX_COUNTERS; ++j) {
		if (fd[j] != -1) {
			if (size[i] != size[j]) {
				fprintf(stderr, "oprofpp: mapping file size "
					"for ctr (%d, %d) are different "
					"(%d, %d)\n", i, j, size[i], size[j]);

				exit(EXIT_FAILURE);
			}
			
			check_footers(footer[i], footer[j]);
		}
	}

	/* output and sanity check on ctr_um, ctr_event and cpu_type */
	for (i = 0 ; i < OP_MAX_COUNTERS; ++i) {
		if (fd[i] != -1) {
			check_and_output_event(i);

			/* redundant set but correct and simplify the code */
			nr_samples = size[i] / sizeof(struct opd_fentry);
			mtime = footer[i]->mtime;
		}
	}

	verbprintf("nr_samples %d\n", nr_samples); 

	for (i = 0; i < OP_MAX_COUNTERS; ++i) {
		if (fd[i] != -1)
			break;
	}

	if (footer[i]->cpu_type == CPU_ATHLON)
		op_nr_counters = 4;

	ibfd = open_image_file(samplefile, mtime);

	num = get_symbols(ibfd, &syms);

	verbprintf("nr symbols %u\n", num); 

	if (!num) {
		fprintf(stderr, "oprofpp: couldn't get any symbols from image file.\n");
		exit(EXIT_FAILURE);
	}

	if (list_all_symbols_details)
		/* TODO: temporary hack to fix and easy life of opf_filter.cpp
		 * Will be cleanup when linking opf_filter with oprofpp. */
		printf("Cpu type: %d\n", footer[i]->cpu_type);
	else
		printf("Cpu type: %s\n", op_get_cpu_type_str(footer[i]->cpu_type));

	printf("Cpu speed was (MHz estimation) : %f\n", footer[i]->cpu_speed);

	if (list_symbols) {
		do_list_symbols(syms, num);
	} else if (symbol) {
		do_list_symbol(ibfd, syms, num);
	} else if (gproffile) {
		do_dump_gprof(syms, num);
	} else if (list_all_symbols_details) {
		do_list_all_symbols_details(ibfd, syms, num);
	}

	opd_free(syms);
	bfd_close(ibfd);

	for (i = 0 ; i < op_nr_counters ; ++i) {
		if (footer[i]) {
			munmap(footer[i], size[i] + sizeof(struct opd_footer));
			close(fd[i]);
		}
	}

	return 0;
}
