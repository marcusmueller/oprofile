/* $Id: oprofpp.c,v 1.25 2001/02/05 11:37:59 movement Exp $ */
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
 
static char const *samplefile;
static char *basedir="/var/opd";
static const char *imagefile;
static char *gproffile;
static char *symbol;
static int list_symbols;
static int demangle;

static struct opd_fentry *samples;
static uint nr_samples; 
static struct opd_footer footer;
static char *ctr0_name; 
static char *ctr0_desc; 
static char *ctr0_um_desc;
static char *ctr1_name; 
static char *ctr1_desc; 
static char *ctr1_um_desc;
static int ctr;
static u32 sect_offset; 

static struct poptOption options[] = {
	{ "samples-file", 'f', POPT_ARG_STRING, &samplefile, 0, "image sample file", "file", },
	{ "image-file", 'i', POPT_ARG_STRING, &imagefile, 0, "image file", "file", },
	{ "list-symbols", 'l', POPT_ARG_NONE, &list_symbols, 0, "list samples by symbol", NULL, },
	{ "dump-gprof-file", 'g', POPT_ARG_STRING, &gproffile, 0, "dump gprof format file", "file", },
	{ "list-symbol", 's', POPT_ARG_STRING, &symbol, 0, "give detailed samples for a symbol", "symbol", },
	{ "demangle", 'd', POPT_ARG_NONE, &demangle, 0, "demangle GNU C++ symbol names", NULL, },
	{ "counter", 'c', POPT_ARG_INT, &ctr, 0, "which counter to use", "0|1", }, 
	{ "version", 'v', POPT_ARG_NONE, &showvers, 0, "show version", NULL, },
	{ "verbose", 'V', POPT_ARG_NONE, &verbose, 0, "verbose output", NULL, },
	{ "base-dir", 'b', POPT_ARG_STRING, &basedir, 0, "base directory of profile daemon", NULL, }, 
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
	
	optcon = poptGetContext(NULL, argc, argv, options, 0);

	c=poptGetNextOpt(optcon);

	if (c<-1) {
		fprintf(stderr, "oprofpp: %s: %s\n",
	                poptBadOption(optcon, POPT_BADOPTION_NOALIAS),
	                poptStrerror(c));
	        poptPrintHelp(optcon, stderr, 0);
	        exit(1);
	}

	if (showvers) {
		printf(VERSION_STRING " compiled on " __DATE__ " " __TIME__ "\n");
		exit(0);
	}
 
	if (!list_symbols && !gproffile && !symbol) {
		fprintf(stderr, "oprofpp: no mode specified. What do you want from me ?\n");
		poptPrintHelp(optcon, stderr, 0);
		exit(1);
	}

	/* non-option file, either a sample or binary image file */
	file = poptGetArg(optcon);

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
			exit(1);
		} else {
			/* we'll "leak" this memory */
			samplefile = remangle(imagefile);
		}
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
		char *cp=(char *)name;
		char *unmangled=cp;

		while (*cp && *cp=='_')
			cp++;

		if (*cp) {
			unmangled = cplus_demangle(cp,DMGL_PARAMS|DMGL_ANSI);
			if (unmangled) {
				/* FIXME: print leading underscores ? */
				printf("%s",unmangled);
				free(unmangled);
				return;
			}
		}
	}
	printf("%s",name);
}

/**
 * open_image_file - open associated binary image
 * @mangled: name of samples file
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
bfd *open_image_file(char const *mangled)
{
	char *file;
	char **matching;
	bfd *ibfd;
	 
	file = (char *)imagefile;

	if (!mangled) {
		if (!file)
			return NULL;
	} else if (!imagefile) {
		char *mang;
		char *c;

		mang = strdup(mangled); 
		if (!mang)
			return NULL;
		 
		c = &mang[strlen(mang)];
		/* strip leading dirs */
		while (c!=mang && *c!='/')
			c--;

		c++;

		file = strdup(c);

		if (!file) {
			fprintf(stderr, "oprofpp: strdup() failed.\n");
			exit(1);
		}

		c=file;

		do {
			if (*c==OPD_MANGLE_CHAR)
				*c='/';
		} while (*c++);

		free(mang);
	}

	ibfd = bfd_openr(file, NULL);
 
	if (!ibfd) {
		fprintf(stderr,"oprofpp: bfd_openr of %s failed.\n",file);
		exit(1);
	}
	 
	if (!bfd_check_format_matches(ibfd, bfd_object, &matching)) { 
		fprintf(stderr,"oprofpp: BFD format failure for %s.\n",file);
		exit(1);
	}
 
 	if (!imagefile)
		free(file);

	if (footer.is_kernel) { 
		asection *sect; 
 
		sect = bfd_get_section_by_name(ibfd, ".text");
		sect_offset = OPD_KERNEL_OFFSET - sect->filepos;
		verbprintf("Adjusting kernel samples by 0x%x, .text filepos 0x%lx\n",sect_offset,sect->filepos); 
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
		exit(1); 
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

	if (va<vb)
		return -1;
	return (va>vb);
}

/* need a better filter, but only this gets rid of _start
 * and other crud easily. 
 */
static int interesting_symbol(asymbol *sym)
{
	if (!(sym->section->flags & SEC_CODE))
		return 0;

	if (streq("",sym->name))
		return 0;

	if (streq("_init",sym->name))
		return 0;

	if (!(sym->flags & BSF_FUNCTION))
		return 0;

	return 1;
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
	uint nr_syms=0; 
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
 
	filt_syms = opd_malloc(sizeof(asymbol *)*nr_syms);

	for (nr_syms=0,i=0; i < nr_all_syms; i++) {
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
	verbprintf("Symbol %s, value 0x%lx\n",sym->name, sym->value); 
	*start = sym->value;
	/* offset of section */
	*start += sym->section->filepos;
	verbprintf("in section %s, filepos 0x%lx\n",sym->section->name, sym->section->filepos);
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
	verbprintf("start 0x%x, end 0x%x\n",*start,*end); 
}
 
struct opp_count {
	asymbol *sym;
	u32 count0;
	u32 count1;
};
 
int countcomp(const void *a, const void *b)
{
	struct opp_count *ca= (struct opp_count *)a;	 
	struct opp_count *cb= (struct opp_count *)b;	 

	if (ctr) {
		if (ca->count1 < cb->count1)
			return -1;
		return (ca->count1 > cb->count1);
	} else {
		if (ca->count0 < cb->count0)
			return -1;
		return (ca->count0 > cb->count0);
	} 
}
 
/**
 * do_list_symbols - list symbol samples for an image
 *
 * Lists all the symbols from the image specified by samplefile,
 * in decreasing sample count order, to standard out.
 */
void do_list_symbols(void)
{	
	bfd *ibfd; 
	asymbol **syms;
	struct opp_count *scounts;
	u32 start, end;
	uint num,tot0=0,tot1=0,i,j;
 
	ibfd = open_image_file(samplefile);
	num = get_symbols(ibfd,&syms);

	if (!num) {
		fprintf(stderr, "oprofpp: couldn't get any symbols from image file.\n");
		exit(1);
	}
	 

	scounts = opd_calloc0(num,sizeof(struct opp_count));

	for (i=0; i < num; i++) {
		scounts[i].sym = syms[i];
		get_symbol_range(syms[i], (i==num-1) ? NULL : syms[i+1], &start, &end); 
		if (start >= nr_samples) {
			fprintf(stderr,"oprofpp: start 0x%x out of range (max 0x%x)\n",start,nr_samples);
			exit(1);
		}
		if (end > nr_samples) {
			fprintf(stderr,"oprofpp: end 0x%x out of range (max 0x%x)\n",end,nr_samples);
			exit(1);
		}

		for (j=start; j < end; j++) {
			if (samples[j].count0)
				verbprintf("Adding %u samples for symbol $%s$ at pos j 0x%x\n",samples[j].count0,syms[i]->name,j);
			scounts[i].count0 += samples[j].count0;
			scounts[i].count1 += samples[j].count1;
			tot0 += samples[j].count0;
			tot1 += samples[j].count1;
		}
	}
 
	qsort(scounts, num, sizeof(struct opp_count), countcomp);

	for (i=0; i < num; i++) {
		printf_symbol(scounts[i].sym->name);
		if (ctr && scounts[i].count1) { 
			printf("[0x%.8lx]: %2.4f%% (%u samples)\n",scounts[i].sym->value+scounts[i].sym->section->vma,
				(((double)scounts[i].count1)/tot1)*100.0, scounts[i].count1);
		} else if (scounts[i].count0) {
			printf("[0x%.8lx]: %2.4f%% (%u samples)\n",scounts[i].sym->value+scounts[i].sym->section->vma,
				(((double)scounts[i].count0)/tot0)*100.0, scounts[i].count0);
		} else if (!streq("",scounts[i].sym->name))
			printf(" (0 samples)\n");
	}
 
	opd_free(scounts);
	opd_free(syms);
	bfd_close(ibfd);
}
 
/**
 * do_list_symbol - list detailed samples for a symbol
 *
 * Lists all the samples for the symbol specified by symbol, from the image 
 * specified by @samplefile, in decreasing sample count order, to standard out.
 */
void do_list_symbol(void)
{
	bfd *ibfd;
	asymbol **syms;
	u32 start, end;
	u32 num,i,j;

	ibfd = open_image_file(samplefile);
	num = get_symbols(ibfd,&syms);

	if (!num) {
		fprintf(stderr, "oprofpp: couldn't get any symbols from image file.\n");
		exit(1);
	}

	for (i=0; i < num; i++) {
		if (streq(syms[i]->name,symbol))
			goto found;
	}

	fprintf(stderr, "oprofpp: symbol \"%s\" not found in image file.\n", symbol);
	return;
found:
	printf("Samples for symbol \"%s\" in image %s\n", symbol, ibfd->filename);
	get_symbol_range(syms[i], (i==num-1) ? NULL : syms[i+1], &start, &end);
	for (j=start; j < end; j++) { 
		if (samples[j].count0 || samples[j].count1) {
			printf("%s+%x/%x:\t%u \t%u\n", symbol, sym_offset(syms[i], j), end-start,
				samples[j].count0, samples[j].count1);
		}
	}
 
	opd_free(syms);
	bfd_close(ibfd);
}

#define GMON_VERSION 1
#define GMON_TAG_BB_COUNT 2

struct gmon_hdr {
	char cookie[4];
	u32 version;
	u32 spare[3];
};
 
/**
 * do_dump_gprof - produce gprof sample output
 *
 * Dump gprof-format samples for the image specified by samplefile to
 * the file specified by gproffile.
 *
 * FIXME: this is totally broken (and never worked or was tested)
 */
void do_dump_gprof(void)
{
	static struct gmon_hdr hdr = { "gmon", GMON_VERSION, {0,0,0,},}; 
	FILE *fp; 
	bfd *ibfd; 
	asymbol **syms;
	u32 start, end; 
	uint nblocks=0;
	uint num,i,j;
 
	ibfd = open_image_file(samplefile);
	num = get_symbols(ibfd,&syms);

	if (!num) {
		fprintf(stderr, "oprofpp: couldn't get any symbols from image file.\n");
		exit(1);
	}
	 
	fp=opd_open_file(gproffile,"w");

	opd_write_file(fp,&hdr, sizeof(struct gmon_hdr));

	opd_write_u8(fp, GMON_TAG_BB_COUNT);

	for (i=0; i < num; i++) {
		get_symbol_range(syms[i], (i==num-1) ? NULL : syms[i+1], &start, &end); 
		for (j=start; j < end; j++) {
			if (ctr && samples[j].count1)
				nblocks++; 
			else if (samples[j].count0)
				nblocks++;
		}
	}
 
	opd_write_u32_he(fp,nblocks);

	for (i=0; i < num; i++) {
		get_symbol_range(syms[i], (i==num-1) ? NULL : syms[i+1], &start, &end); 
		for (j=start; j < end; j++) {
			if (ctr && samples[j].count1) { 
				opd_write_u32_he(fp, sym_offset(syms[i],j) + syms[i]->value);
				opd_write_u32_he(fp, samples[j].count1);
			} else if (samples[j].count0) {
				opd_write_u32_he(fp, sym_offset(syms[i],j) + syms[i]->value);
				opd_write_u32_he(fp, samples[j].count0);
			}
		}
	}
	 
	opd_close_file(fp); 
	opd_free(syms);
	bfd_close(ibfd);
}

 
int main(int argc, char const *argv[])
{
	fd_t fd;
	FILE *fp;
	size_t size;
 
	get_options(argc, argv);

	fp = fopen(samplefile,"r");
	if (!fp) {
		fprintf(stderr, "oprofpp: fopen of %s failed. %s\n", samplefile, strerror(errno));
		exit(1);
	}
 
	if (fseek(fp, -sizeof(struct opd_footer), SEEK_END)==-1) {
		fprintf(stderr, "oprofpp: fseek of %s failed. %s\n", samplefile, strerror(errno));
		exit(1);
	}
	 
	if (fread(&footer, sizeof(struct opd_footer), 1, fp)!=1) {
		fprintf(stderr, "oprofpp: fread of %s failed. %s\n", samplefile, strerror(errno));
		exit(1);
	}
	fclose(fp);

	if (footer.magic!=OPD_MAGIC) {
		fprintf(stderr, "oprofpp: wrong magic 0x%x, expected 0x%x.\n", footer.magic, OPD_MAGIC);
		exit(1);
	}
 
	if (footer.version!=OPD_VERSION) {
		fprintf(stderr, "oprofpp: wrong version 0x%x, expected 0x%x.\n", footer.version, OPD_VERSION);
		exit(1);
	}
 
	if (footer.ctr0_type_val) {
		op_get_event_desc(footer.ctr0_type_val, footer.ctr0_um, &ctr0_name, &ctr0_desc, &ctr0_um_desc);
		printf("Counter 0 counted %s events (%s) with a unit mask of 0x%.2x (%s)\n",ctr0_name, ctr0_desc, 
			 footer.ctr0_um, ctr0_um_desc ? ctr0_um_desc : "Not set");
	}

	if (footer.ctr1_type_val) {
		op_get_event_desc(footer.ctr1_type_val, footer.ctr1_um, &ctr1_name, &ctr1_desc, &ctr1_um_desc);
		printf("Counter 1 counted %s events (%s) with a unit mask of 0x%.2x (%s)\n",ctr1_name, ctr1_desc, 
			 footer.ctr1_um, ctr1_um_desc ? ctr1_um_desc : "Not set");
	}
 
	fd = open(samplefile, O_RDONLY);

	if (fd==-1) {
		fprintf(stderr, "oprofpp: Opening %s failed. %s\n",samplefile, strerror(errno));
		exit(1);
	}

	size = opd_get_fsize(samplefile, 1) - sizeof(struct opd_footer); 
	samples = (struct opd_fentry *)mmap(0, size, PROT_READ, MAP_PRIVATE, fd, 0);

	if (samples==(void *)-1) {
		fprintf(stderr, "oprofpp: mmap of %s failed. %s\n", samplefile, strerror(errno));
		exit(1);
	}

	nr_samples = size/sizeof(struct opd_fentry);

	if (list_symbols) {
		do_list_symbols();
	} else if (symbol) {
		do_list_symbol();
	} else if (gproffile) {
		do_dump_gprof();
	}
 
	munmap(samples, size);
	close(fd);

	return 0;
}
