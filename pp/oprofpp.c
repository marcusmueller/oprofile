/* $Id: oprofpp.c,v 1.4 2000/08/03 21:25:01 moz Exp $ */

#include "oprofpp.h"
 
static char *version = VERSION_STRING; 
 
static char *samplefile;
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
	{ "list-symbols", 'l', POPT_ARG_NONE, &list_symbols, 0, "list samples by symbol", NULL, },
	{ "dump-gprof-file", 'g', POPT_ARG_STRING, &gproffile, 0, "dump gprof format file", "file", },
	{ "list-symbol", 's', POPT_ARG_STRING, &symbol, 0, "give detailed samples for a symbol", "symbol", },
	{ "gcc-demangle", 'd', POPT_ARG_NONE, &demangle, 0, "demangle GNU C++ symbol names", NULL, },
	{ "counter", 'c', POPT_ARG_INT, &ctr, 0, "which counter to use", "0|1", }, 
	POPT_AUTOHELP
	{ NULL, 0, 0, NULL, 0, NULL, NULL, },
};

/**
 * get_options - process command line
 * @argc: program arg count
 * @argv: program arg array
 *
 * Process the arguments, fatally complaining on
 * error. samplefile is guaranteed to have some
 * non-NULL value after this function.
 */
static void get_options(int argc, char *argv[])
{
	poptContext optcon;
	char c; 
	
	optcon = poptGetContext(NULL, argc, argv, options, 0);

	c=poptGetNextOpt(optcon);

	if (c<-1) {
		fprintf(stderr, "oprofpp: %s: %s\n",
	                poptBadOption(optcon, POPT_BADOPTION_NOALIAS),
	                poptStrerror(c));
	        poptPrintHelp(optcon, stderr, 0);
	        exit(1);
	}

	if (!list_symbols && !gproffile && !symbol) {
		fprintf(stderr, "oprofpp: no mode specified. What do you want from me ?\n");
		poptPrintHelp(optcon, stderr, 0);
		exit(1);
	}

	if (!samplefile) {
		samplefile = poptGetArg(optcon);

		if (!samplefile) {
			fprintf(stderr, "oprofpp: no samples file specified.\n");
			poptPrintHelp(optcon, stderr, 0);
			exit(1);
		}
	}
}

/**
 * printf_symbol - output a symbol name
 * @name: verbatim symbol name
 *
 * Print the symbol name to stdout, demangling
 * the symbol if necessary, and "demangle" is %TRUE.
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
				/* FIXME: print underscores ? */
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
 * image associated with the samples file @mangled.
 * @mangled may be a relative or absolute pathname.
 *
 * Failure to open the image is fatal. This function
 * returns a pointer to the bfd descriptor for the
 * image.
 *
 * The global struct footer must be valid. sect_offset
 * will be set as appropriate.
 */
bfd *open_image_file(char *mangled)
{
	char *c = &mangled[strlen(mangled)];
	char *file;
	char **matching;
	bfd *ibfd; 

	/* strip leading dirs */
	while (c!=mangled && *c!='/')
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

	ibfd = bfd_openr(file, NULL);

	if (footer.is_kernel) { 
		asection *sect; 
 
		sect = bfd_get_section_by_name(ibfd, ".text");
		sect_offset = OPD_KERNEL_OFFSET - sect->filepos;
	}
 
	if (!ibfd) {
		fprintf(stderr,"oprofpp: bfd_openr of %s failed.\n",file);
		exit(1);
	}
	 
	if (!bfd_check_format_matches(ibfd, bfd_object, &matching)) { 
		free(matching); 
		fprintf(stderr,"oprofpp: BFD format failure for %s.\n",file);
		exit(1);
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
	num -= sym->value - sym->section->vma;

	return num;
}
 
int symcomp(const void *a, const void *b)
{
	asymbol **sa= (asymbol **)a;
	asymbol **sb= (asymbol **)b;

	if ((*sa)->value<(*sb)->value)
		return -1;
	return ((*sa)->value>(*sb)->value);
}
 
/**
 * get_symbols - get symbols from bfd
 * @ibfd: bfd to read from
 * @symsp: pointer to array of symbol pointers
 *
 * Parse and sort in ascending order all symbols
 * in the file pointed to by @ibfd. Returns the number
 * of symbols found. @symsp will be set to point
 * to the symbol pointer array.
 */
uint get_symbols(bfd *ibfd, asymbol ***symsp)
{
	uint nr_syms;
	size_t size;

	if (!(bfd_get_file_flags(ibfd) & HAS_SYMS))
		return 0;

	size = bfd_get_symtab_upper_bound(ibfd);

	/* HAS_SYMS can be set with no symbols */
	if (size<1)
		return 0;

	*symsp = opd_malloc(size);
	nr_syms = bfd_canonicalize_symtab(ibfd, *symsp);
	if (nr_syms < 1) {
	        opd_free(*symsp);
	        return 0;
	}

	qsort(*symsp, nr_syms, sizeof(asymbol *), symcomp);
 
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
	/* offset from section */ 
	*start = sym->value - sym->section->vma;	 
	/* offset of section */
	*start += sym->section->filepos;
	/* adjust for kernel image */
	*start += sect_offset;
	if (next) {
		/* offset from section */ 
		*end = next->value - next->section->vma;	 
		/* offset of section */
		*end += next->section->filepos;
		/* adjust for kernel image */
		*end += sect_offset;
	} else
		*end = nr_samples;
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
		for (j=start; j < end; j++) {
			scounts[i].count0 += samples[j].count0;
			scounts[i].count1 += samples[j].count1;
			tot0 += samples[j].count0;
			tot1 += samples[j].count1;
		}
	}
 
	qsort(scounts, num, sizeof(struct opp_count), countcomp);

	for (i=0; i < num; i++) {
		printf_symbol(scounts[i].sym->name);
		if (ctr) { 
			printf("[0x%.8lx]: %2.4f%% (%d samples)\n",scounts[i].sym->value,
				((double)scounts[i].count1)/tot1, scounts[i].count1);
		} else {
			printf("[0x%.8lx]: %2.4f%% (%d samples)\n",scounts[i].sym->value,
				((double)scounts[i].count0)/tot0, scounts[i].count0);
		} 
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
			printf("+0x%.8x: %s:%u %s:%u\n", sym_offset(syms[i], j),
				ctr0_name, samples[j].count0, ctr1_name, samples[j].count1);
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

 
int main(int argc, char *argv[])
{
	fd_t fd;
	FILE *fp;
	size_t size;
 
	get_options(argc, argv);

	fp = fopen(samplefile,"r");
	if (!fp) {
		fprintf(stderr, "oprofpp: fopen of %s failed. %s", samplefile, strerror(errno));
		exit(1);
	}
 
	if (fseek(fp, -sizeof(struct opd_footer), SEEK_END)==-1) {
		fprintf(stderr, "oprofpp: fseek of %s failed. %s", samplefile, strerror(errno));
		exit(1);
	}
	 
	if (fread(&footer, sizeof(struct opd_footer), 1, fp)!=1) {
		fprintf(stderr, "oprofpp: fread of %s failed. %s", samplefile, strerror(errno));
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
 
	op_get_event_desc(footer.ctr0_type_val, footer.ctr0_um, &ctr0_name, &ctr0_desc, &ctr0_um_desc);
	op_get_event_desc(footer.ctr1_type_val, footer.ctr1_um, &ctr1_name, &ctr1_desc, &ctr1_um_desc);

	fd = open(samplefile, O_RDONLY);

	if (fd==-1) {
		fprintf(stderr, "oprofpp: Opening %s failed. %s",samplefile, strerror(errno));
		exit(1);
	}

	size = opd_get_fsize(samplefile) - sizeof(struct opd_footer); 
	samples = (struct opd_fentry *)mmap(0, size, PROT_READ, MAP_PRIVATE, fd, 0);

	if (samples==(void *)-1) {
		fprintf(stderr, "oprofpp: mmap of %s failed. %s", samplefile, strerror(errno));
		exit(1);
	}

	nr_samples = size/sizeof(struct opd_fentry);

	printf("Counter 0 counted %s events (%s) with a unit mask of 0x%.2x (%s)\n",ctr0_name, ctr0_desc, 
		 footer.ctr0_um, ctr0_um_desc);
	printf("Counter 1 counted %s events (%s) with a unit mask of 0x%.2x (%s)\n",ctr1_name, ctr1_desc, 
		 footer.ctr1_um, ctr1_um_desc);
 
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
