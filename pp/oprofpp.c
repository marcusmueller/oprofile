/* $Id: oprofpp.c,v 1.54 2001/09/24 18:14:57 movement Exp $ */
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

// FIXME: rename to oprofpp.cpp
 
// FIXME: sprintf -> sstream (and elsewhere) 
#include <algorithm>

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
static int list_all_symbols_details;
static int output_linenr_info;

static uint nr_samples; 
/* if != -1 appended to samples files with "-%d" format */
static int session_number = -1;
/* PHE FIXME: check the logic and this comment */
/* counter k is selected if (ctr == -1 || ctr == k), if ctr == -1 counter 0
 * is used for sort purpose */
static int ctr = -1;

static poptOption options[] = {
	{ "samples-file", 'f', POPT_ARG_STRING, &samplefile, 0, "image sample file", "file", },
	{ "image-file", 'i', POPT_ARG_STRING, &imagefile, 0, "image file", "file", },
	{ "list-symbols", 'l', POPT_ARG_NONE, &list_symbols, 0, "list samples by symbol", NULL, },
	{ "dump-gprof-file", 'g', POPT_ARG_STRING, &gproffile, 0, "dump gprof format file", "file", },
	{ "list-symbol", 's', POPT_ARG_STRING, &symbol, 0, "give detailed samples for a symbol", "symbol", },
	{ "demangle", 'd', POPT_ARG_NONE, &demangle, 0, "demangle GNU C++ symbol names", NULL, },
	{ "counter", 'c', POPT_ARG_INT, &ctr, 0, "which counter to use", "counter number", }, 
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

/**
 * remangle - convert a filename into the related sample file name
 * @image: the image filename
 */
char *remangle(const char *image)
{
	char *file;
	char *c; 

	if (!basedir || !*basedir)
		basedir = "/var/opd";
	else if (basedir[strlen(basedir)-1] == '/')
		basedir[strlen(basedir)-1] = '\0';

	file = (char*)opd_malloc(strlen(basedir) + strlen("/samples/") + strlen(image) + 1);
	
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
 * quit_error - quit with error
 * @optcon: popt context
 * @err: error to show
 *
 * err may be NULL
 */
static void quit_error(poptContext * optcon, char const *err)
{
	if (err)
		fprintf(stderr, err); 
	poptPrintHelp(*optcon, stderr, 0);
	exit(EXIT_FAILURE);
}
 
/**
 * get_options - process command line
 * @argc: program arg count
 * @argv: program arg array
 *
 * Process the arguments, fatally complaining on
 * error. 
 *
 * Most of the complexity here is to process
 * filename. argument precedeed with an option -f
 * (-i) is considered as a sample filename (image 
 * filename). filename without preceding option -i or
 * -f is considered as a sample file if it contains
 * at least one OPD_MANGLE_CHAR else it is an image
 * file. If no image file is given on command line the
 * sample file name is un-mangled -after- stripping
 * the optionnal "#d-d" suffixe. This give some
 * limitations on the image filename.
 *
 * all filename checking is made here only with a
 * syntactical approch. (ie existence of filename is
 * not tested)
 *
 * post-condition: samplefile and imagefile are setup
 */
void opp_get_options(int argc, const char **argv)
{
	poptContext optcon;
	char c; 
	const char *file;
	const char *file_session_str;
	char *file_ctr_str;
	int counter;
	
	optcon = opd_poptGetContext(NULL, argc, argv, options, 0);

	c=poptGetNextOpt(optcon);

	if (c < -1) {
		fprintf(stderr, "oprofpp: %s: %s\n",
			poptBadOption(optcon, POPT_BADOPTION_NOALIAS),
			poptStrerror(c));
		quit_error(&optcon, NULL);
	}

	if (showvers) {
		printf(VERSION_STRING " compiled on " __DATE__ " " __TIME__ "\n");
		exit(EXIT_SUCCESS);
	}
 
	if (!list_all_symbols_details && !list_symbols && 
	    !gproffile && !symbol)
		quit_error(&optcon, "oprofpp: no mode specified. What do you want from me ?\n");

	/* check only one major mode specified */
	if ((list_all_symbols_details + list_symbols + (gproffile != 0) + (symbol != 0)) > 1)
		quit_error(&optcon, "oprofpp: must specify only one output type.\n");

	if (output_linenr_info && !list_all_symbols_details && !symbol)
		quit_error(&optcon, "oprofpp: cannot list debug info without -L or -s option.\n");
 
	/* non-option file, either a sample or binary image file */
	file = poptGetArg(optcon);

	/* some minor memory leak from the next calls */
	if (imagefile)
		imagefile = opd_relative_to_absolute_path(imagefile, NULL);

	if (samplefile)
		samplefile = opd_relative_to_absolute_path(samplefile, NULL);

	if (file) {
		if (imagefile && samplefile) {
			quit_error(&optcon, "too many filename on command line: you can specify at most one sample filename and one image filename.\n");
		}

		file = opd_relative_to_absolute_path(file, NULL);
		if (strchr(file, OPD_MANGLE_CHAR))
			samplefile = file;
		else
			imagefile = file;
	}

	if (!samplefile) { 
		if (!imagefile) { 
			quit_error(&optcon, "oprofpp: no samples file specified.\n");
		} else {
			/* we'll "leak" this memory */
			samplefile = remangle(imagefile);
		}
	} 

	/* we can not complete filename checking of imagefile because
	 * it can be derived from the sample filename, we must process
	 * and chop optionnal suffixe "#%d-%d" first */

	/* check for a valid counter suffix in a given sample file */
	counter = -1;
	file_ctr_str = strrchr(samplefile, '#');
	if (file_ctr_str) {
		sscanf(file_ctr_str + 1, "%d", &counter);
	}

	if (ctr != counter) {
		/* a --counter=x have priority on the # suffixe of filename */
		if (ctr != -1 && counter != -1)
			quit_error(&optcon, "oprofpp: conflict between given counter and counter of samples file.\n");
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

	/* chop suffixes */
	if (file_ctr_str)
		file_ctr_str[0] = '\0';

	/* check we have a valid ctr */
	if (ctr != -1 && (ctr < 0 || ctr >= OP_MAX_COUNTERS)) {
		fprintf(stderr, "oprofpp: invalid counter number %u\n", ctr);
		exit(EXIT_FAILURE);
	}

	/* memory leak */
	if (!imagefile) {
		char *mang;
		char *c;
		char *file;

		mang = opd_strdup(samplefile); 
		 
		c = &mang[strlen(mang)];
		/* strip leading dirs */
		while (c != mang && *c != '/')
			c--;

		c++;

		file = opd_strdup(c);

		c=file;

		while (*c) {
			if (*c == OPD_MANGLE_CHAR)
				*c='/';
			c++;
		}

		free(mang);

		imagefile = file;
	}
}

// FIXME: only use char arrays and pointers if you MUST. Otherwise std::string
// and references everywhere please.

/**
 * demangle_symbol - demangle a symbol
 * @symbol: the symbol name
 *
 * demangle the symbol name @name. if the global
 * global variable demangle is %TRUE. No error can
 * occur. Caller must deallocate returned pointer
 *
 * The demangled name lists the parameters and type
 * qualifiers such as "const".
 *
 * return a pointer to the un-mangled name
 */
char* demangle_symbol(const char* name)
{
	if (demangle) {
		char *cp = (char *)name;
		char *unmangled = cp;

		while (*cp && *cp == '_')
			cp++;

		if (*cp) {
			unmangled = cplus_demangle(cp, DMGL_PARAMS | DMGL_ANSI);
			if (unmangled) {
				/* FIXME: leading underscores ? */
				return unmangled;
			}
		}
	}

	return opd_strdup(name);
}

/**
 * printf_symbol - output a symbol name
 * @name: verbatim symbol name
 *
 * Print the symbol name to stdout, demangling
 * if if necessary.
 */
void printf_symbol(const char *name)
{
	char* unmangled = demangle_symbol(name);

	printf("%s", unmangled);

	opd_free(unmangled);
}

/**
 * counter_array_t - construct a counter_array_t
 *
 * set count to zero for all counter
 */
counter_array_t::counter_array_t()
{
	for (size_t i = 0 ; i < OP_MAX_COUNTERS ; ++i)
		value[i] = 0;
}

/**
 * operator+= - vectorized += operator
 *
 * accumulate samples in this object
 */
counter_array_t & counter_array_t::operator+=(const counter_array_t & rhs)
{
	for (size_t i = 0 ; i < OP_MAX_COUNTERS ; ++i)
		value[i] += rhs.value[i];

	return *this;
}

/**
 * opp_bfd - construct an opp_bfd object
 * @footer: a valid samples file opd_footer
 *
 * The global variable imagefile is used to
 * open the binary file. After construction
 * the data member are setup.
 *
 * All error are fatal.
 *
 */
opp_bfd::opp_bfd(const opd_footer* footer)
{
	time_t newmtime;

	sect_offset = 0;

	if (!imagefile) {
		fprintf(stderr,"oprofpp: oppp_bfd() imagefile is NULL.\n");
		exit(EXIT_FAILURE);
	} 

	newmtime = opd_get_mtime(imagefile);
	if (newmtime != footer->mtime) {
		fprintf(stderr, "oprofpp: WARNING: the last modified time of the binary file %s does not match\n"
			"that of the sample file. Either this is the wrong binary or the binary\n"
			"has been modified since the sample file was created.\n", imagefile);
	}

	open_bfd_image(imagefile, footer->is_kernel);
}

/**
 * close_bfd_image - open a bfd image
 *
 * This function will close an opended a bfd image
 * and free all related resource.
 */
opp_bfd::~opp_bfd()
{
	bfd_close(ibfd);
}

/**
 * open_bfd_image - opp_bfd ctor helper
 * @file: name of image file
 * @is_kernel: true if the image is the kernel
 *
 * This function will open a bfd image and process symbols
 * within this image file
 *
 * @file mut be a valid image filename
 * @is_kernel must be true if the image is
 * the linux kernel.
 *
 * Failure to open the image or to get symbol from
 * the image is fatal.
 */
void opp_bfd::open_bfd_image(const char* file, bool is_kernel)
{
	char **matching;

	ibfd = bfd_openr(file, NULL);
 
	if (!ibfd) {
		fprintf(stderr,"oprofpp: bfd_openr of %s failed.\n", file);
		exit(EXIT_FAILURE);
	}
	 
	if (!bfd_check_format_matches(ibfd, bfd_object, &matching)) { 
		fprintf(stderr,"oprofpp: BFD format failure for %s.\n", file);
		exit(EXIT_FAILURE);
	}

	if (get_symbols() == false) {
		fprintf(stderr, "oprofpp: couldn't get any symbols from image file.\n");
		exit(EXIT_FAILURE);
	}

	if (is_kernel) {
		asection *sect;
		sect = bfd_get_section_by_name(ibfd, ".text");
		sect_offset = OPD_KERNEL_OFFSET - sect->filepos;
		verbprintf("Adjusting kernel samples by 0x%x, .text filepos 0x%lx\n", sect_offset, sect->filepos); 
	}
}

/**
 * symcomp - comparator
 *
 * FIXME: this is not sufficient because it compare
 * incorrectly symbol coming from different section
 */
static bool symcomp(const asymbol * a, const asymbol * b)
{
	return a->value <= b->value;
}

/* need a better filter, but only this gets rid of _start
 * and other crud easily. 
 */
static bool interesting_symbol(asymbol *sym)
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
 * get_symbols - opp_bfd ctor helper
 *
 * Parse and sort in ascending order all symbols
 * in the file pointed to by @abfd that reside in
 * a %SEC_CODE section. Returns true if symbol(s)
 * are found. The symbols are filtered through
 * the interesting_symbol() predicate and sorted
 * with the symcomp() comparator.
 */
bool opp_bfd::get_symbols()
{
	uint nr_all_syms;
	uint i; 
	size_t size;
	asymbol **temp_syms; 

	if (!(bfd_get_file_flags(ibfd) & HAS_SYMS))
		return false;

	size = bfd_get_symtab_upper_bound(ibfd);

	/* HAS_SYMS can be set with no symbols */
	if (size < 1)
		return false;

	temp_syms = (asymbol**)opd_malloc(size);
	nr_all_syms = bfd_canonicalize_symtab(ibfd, temp_syms);
	if (nr_all_syms < 1) {
		opd_free(temp_syms);
		return false;
	}

	for (i = 0; i < nr_all_syms; i++) {
		if (interesting_symbol(temp_syms[i])) {
			syms.push_back(temp_syms[i]);
		}
	}

	std::sort(syms.begin(), syms.end(), symcomp);

	verbprintf("nr symbols %u\n", syms.size());

	if (syms.empty())
		return false;

	return true;
}

/**
 * sym_offset - return offset from a symbol's start
 * @sym_index: symbol number
 * @num: number of fentry
 *
 * Returns the offset of a sample at position @num
 * in the samples file from the start of symbol @sym_idx.
 */
u32 opp_bfd::sym_offset(uint sym_index, u32 num) const
{
	if (num - sect_offset > num) {
		fprintf(stderr,"oprofpp: less than zero offset ? \n");
		exit(EXIT_FAILURE); 
	}
	 
	/* adjust for kernel images */
	num -= sect_offset;
	/* take off section offset */
	num -= syms[sym_index]->section->filepos;
	/* and take off symbol offset from section */
	num -= syms[sym_index]->value;

	return num;
}
 
/**
 * get_linenr - lookup linenr info from a vma address
 * @sym_idx: the symbol number
 * @offset: the offset
 * @filename: source filename from where come the vma
 * @linenr: linenr corresponding to this vma
 *
 * lookup for for filename::linenr info from a @sym_idx
 * symbol at offset @offset.
 * if the lookup success return 0. in this case filename
 * and/or linenr can be set to NULL/0 if debug info are
 * not available in the image file. If lookup fail @filename
 * and @linenr have undefined value.
 */
bool opp_bfd::get_linenr(uint sym_idx, uint offset, 
			const char** filename, unsigned int* linenr)
{
	const char *functionname;
	bfd_vma pc;

	asection* section = syms[sym_idx]->section;

	if ((bfd_get_section_flags (ibfd, section) & SEC_ALLOC) == 0)
		return false;

	pc = sym_offset(sym_idx, offset) + syms[sym_idx]->value;

	if (pc >= bfd_section_size(ibfd, section))
		return false;

	return bfd_find_nearest_line(ibfd, section, &syms[0], pc,
				     filename, &functionname, linenr);
}

/**
 * ouput_linenr - lookup and output linenr info from a vma address
 * in a given section to standard output.
 * @sym_idx: the symbol number
 * @offset: offset
 *
 */
void opp_bfd::output_linenr(uint sym_idx, uint offset)
{
	const char *filename;
	unsigned int line;

	if (!output_linenr_info)
		return;

	if (get_linenr(sym_idx, offset, &filename, &line))
		printf ("%s:%u ", filename, line);
	else
		printf ("??:0 ");
}

/**
 * get_symbol_range - get range of symbol
 * @sym_idx: symbol index
 * @start: pointer to start var
 * @end: pointer to end var
 *
 * Calculates the range of sample file entries covered
 * by @sym. @start and @end will be filled in appropriately.
 * If index is the last entry in symbol table, all entries up
 * to the end of the sample file will be used.  After
 * calculating @start and @end they are sanitized
 *
 * All error are fatal.
 */
// FIXME: references not pointers ! 
void opp_bfd::get_symbol_range(uint sym_idx, u32 *start, u32 *end) const
{
	asymbol *sym, *next;

	sym = syms[sym_idx];
	next = (sym_idx == syms.size() - 1) ? NULL : syms[sym_idx + 1];

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

	if (*start >= nr_samples) {
		fprintf(stderr,"oprofpp: start 0x%x out of range (max 0x%x)\n", *start, nr_samples);
		exit(EXIT_FAILURE);
	}

	if (*end > nr_samples) {
		fprintf(stderr,"oprofpp: end 0x%x out of range (max 0x%x)\n", *end, nr_samples);
		exit(EXIT_FAILURE);
	}

	/* FIXME: this do not work */
#if 0
	if (*start > *end) {
		fprintf(stderr,"oprofpp: start 0x%x overflow or end 0x%x underflow\n", *start, *end);
		exit(EXIT_FAILURE);
	}
#endif
}

/**
 * symbol_index - find a symbol
 * @name: the symbol name
 *
 * find and return the index of a symbol.
 * if the @name is not found -1 is returned
 */

int opp_bfd::symbol_index(const char* symbol) const
{
	for (size_t i = 0; i < syms.size(); i++) {
		if (streq(syms[i]->name, symbol))
			return i;
	}

	return -1;
}

/**
 * check_footers - check coherence between two footers.
 * @f1: first footer
 * @f2: second footer
 *
 * verify that footer @f1 and @f2 are coherent.
 * all error are fatal
 */
static void check_footers(opd_footer* f1, opd_footer* f2)
{
	if (f1->mtime != f2->mtime) {
		fprintf(stderr, "oprofpp: footer timestamps are different (%ld, %ld)\n", f1->mtime, f2->mtime);
		exit(EXIT_FAILURE);
	}

	if (f1->is_kernel != f2->is_kernel) {
		fprintf(stderr, "oprofpp: footer is_kernel flags are different\n");
		exit(EXIT_FAILURE);
	}

	if (f1->cpu_speed != f2->cpu_speed) {
		fprintf(stderr, "oprofpp: footer cpu speeds are different (%f, %f)",
			f2->cpu_speed, f2->cpu_speed);
		exit(EXIT_FAILURE);
	}
}

/**
 * opp_samples_files - construct an opp_samples_files object
 *
 * the global variable @samplefile is used to locate the samples
 * filename.
 *
 * at least one sample file (based on @samplefile name)
 * must be opened. If more than one sample file is open
 * their footer must be coherent. Each footer is also
 * sanitized.
 *
 * all error are fatal
 */
opp_samples_files::opp_samples_files()
	:
	nr_counters(2),
	first_file(-1)
{
	uint i, j;
	time_t mtime = 0;

	/* no samplefiles open initially */
	for (i = 0; i < OP_MAX_COUNTERS; ++i) {
		samples[i] = 0;
		footer[i] = 0;
		ctr_name[i] = 0;
		ctr_desc[i] = 0;
		ctr_um_desc[i] = 0;
		fd[i] = -1;
		size[i] = 0;
	}

	for (i = 0; i < OP_MAX_COUNTERS ; ++i) {
		if (ctr == -1 || ctr == (int)i) {
			/* if ctr == i, this means than we open only one
			 * samples file so don't allow opening failure to get
			 * a more precise error message */
			open_samples_file(i, ctr != (int)i);
		}
	}

	/* find first open file */
	for (first_file = 0; first_file < OP_MAX_COUNTERS ; ++first_file) {
		if (fd[first_file] != -1)
			break;
	}

	if (first_file == OP_MAX_COUNTERS) {
		fprintf(stderr, "Can not open any samples files for %s last error %s\n", samplefile, strerror(errno));
		exit(EXIT_FAILURE);
	}

	nr_samples = size[first_file] / sizeof(opd_fentry);
	mtime = footer[first_file]->mtime;

	/* determine how many counters are possible via the sample file.
	 * allows use on different platform */ 
	nr_counters = 2;
	if (footer[first_file]->cpu_type == CPU_ATHLON)
		nr_counters = 4;

	/* check sample files match */
	for (j = first_file + 1; j < OP_MAX_COUNTERS; ++j) {
		if (fd[j] == -1)
			continue;
		if (size[first_file] != size[j]) {
			fprintf(stderr, "oprofpp: mapping file size for ctr "
				"(%d, %d) are different (%d, %d)\n", 
				first_file, j, size[first_file], size[j]);

			exit(EXIT_FAILURE);
		}
		check_footers(footer[first_file], footer[j]);
	}

	/* sanity check on ctr_um, ctr_event and cpu_type */
	for (i = 0 ; i < OP_MAX_COUNTERS; ++i) {
		if (fd[i] != -1)
			check_event(i);
	}

	verbprintf("nr_samples %d\n", nr_samples); 
}

/**
 * ~opp_samples_files - destroy an object opp_samples
 *
 * close and free all related to the samples file(s)
 */
opp_samples_files::~opp_samples_files()
{
	uint i;

	for (i = 0 ; i < OP_MAX_COUNTERS; ++i) {
		if (footer[i]) {
			munmap(footer[i], size[i] + sizeof(opd_footer));
			close(fd[i]);
		}
	}
}

/**
 * open_samples_file - ctor helper
 * @counter: the counter number
 * @can_fail: allow to fail gracefully
 *
 * open and mmap the given samples files,
 * the member var samples[@counter], footer[@counter]
 * etc. are updated in case of success.
 * The footer is checked but coherence between
 * footer can not be sanitized at this point.
 *
 * if @can_fail == false all error are fatal.
 */
void opp_samples_files::open_samples_file(u32 counter, bool can_fail)
{
	char* temp;

	temp = (char*)opd_malloc(strlen(samplefile) +  32);

	strcpy(temp, samplefile);

	sprintf(temp + strlen(temp), "#%d", counter);
	if (session_number != -1)
		sprintf(temp + strlen(temp), "-%d", session_number);

	fd[counter] = open(temp, O_RDONLY);
	if (fd[counter] == -1) {
		if (can_fail == false)	{
			fprintf(stderr, "oprofpp: Opening %s failed. %s\n", temp, strerror(errno));
			exit(EXIT_FAILURE);
		}
		footer[counter] = NULL;
		samples[counter] = NULL;
		size[counter] = 0;
		return;
	}

	size[counter] = opd_get_fsize(temp, 1) - sizeof(opd_footer); 
	if (size[counter] < sizeof(opd_footer)) {
		fprintf(stderr, "oprofpp: sample file %s is not the right "
			"size: got %d, expected %d\n", 
			temp, size[counter], sizeof(opd_footer));
		exit(EXIT_FAILURE);
	}

	footer[counter] = (opd_footer*)mmap(0, size[counter] + sizeof(opd_footer), 
				      PROT_READ, MAP_PRIVATE, fd[counter], 0);
	if (footer[counter] == (void *)-1) {
		fprintf(stderr, "oprofpp: mmap of %s failed. %s\n", temp, strerror(errno));
		exit(EXIT_FAILURE);
	}

	samples[counter] = (opd_fentry *)(footer[counter] + 1);

	if (memcmp(footer[counter]->magic, OPD_MAGIC, sizeof(footer[0]->magic))) {
		/* FIXME: is 4.4 ok : there is no zero terminator */
		fprintf(stderr, "oprofpp: wrong magic %4.4s, expected %s.\n", footer[counter]->magic, OPD_MAGIC);
		exit(EXIT_FAILURE);
	}

	if (footer[counter]->version != OPD_VERSION) {
		fprintf(stderr, "oprofpp: wrong version 0x%x, expected 0x%x.\n", footer[counter]->version, OPD_VERSION);
		exit(EXIT_FAILURE);
	}

	/* This should be guaranteed by the daemon */
	if (footer[counter]->ctr != counter) {
		fprintf(stderr, "oprofpp: sanity check counter number fail %d, expect %d.\n", footer[counter]->ctr, counter);
		exit(EXIT_FAILURE);
	}

	opd_free(temp);
}

/**
 * check_event - check and translate an event
 * @i: counter number
 *
 * the member variable describing the event are
 * updated.
 *
 * all error are fatal
 */
void opp_samples_files::check_event(int i)
{
	op_get_event_desc(footer[i]->cpu_type, footer[i]->ctr_event,
			  footer[i]->ctr_um, &ctr_name[i],
			  &ctr_desc[i], &ctr_um_desc[i]);
}

/**
 * is_open - test if a samples file is open
 * @index: index of the samples file to check.
 *
 * return true if the samples file @index is open
 */ 
bool opp_samples_files::is_open(int index) const
{
	return samples[index] != 0;
}

/**
 * samples_count - check if samples are available
 * @index: index of the samples files
 * @samples_nr: number of the samples to test.
 *
 * return the number of samples for samples file @index
 * at position @sample_nr. return 0 if the samples file
 * is close
 */
uint opp_samples_files::samples_count(int index, int sample_nr) const
{
	return is_open(index) ? samples[index][sample_nr].count : 0;
}

/**
 * accumulate_samples - lookup samples from a vma address
 * @counter: where to accumulate the samples
 * @index: index of the samples.
 *
 * return false if no samples has been found
 */
bool opp_samples_files::accumulate_samples(counter_array_t& counter, uint index) const
{
	uint k;
	bool found_samples = false;

	for (k = 0; k < nr_counters; ++k) {
		if (samples_count(k, index)) {
			found_samples = true;
			counter[k] += samples_count(k, index);
		}
	}

	return found_samples;
}
 
struct opp_count {
	asymbol *sym;
	counter_array_t count;
};

/**
 * countcomp - comparator
 *
 * predicate to sort samples by samples count
 */
static bool countcomp(const opp_count& a, const opp_count& b)
{
	/* note than ctr must be sanitized before calling qsort */
	return a.count[ctr] < b.count[ctr];
}
 
/**
 * do_list_symbols - list symbol samples for an image
 * @abfd: the bfd object from where come the samples
 *
 * Lists all the symbols in decreasing sample count
 * order, to standard out.
 */
void opp_samples_files::do_list_symbols(opp_bfd* abfd) const
{
	std::vector<opp_count> scounts(abfd->syms.size());
	u32 start, end;
	counter_array_t tot;
	uint i,j;
	bool found_samples;
	uint k;

	for (i = 0; i < abfd->syms.size(); i++) {
		scounts[i].sym = abfd->syms[i];

		abfd->get_symbol_range(i, &start, &end); 
		for (j = start; j < end; j++)
			accumulate_samples(scounts[i].count, j);

		tot += scounts[i].count;
	}

	std::sort(scounts.begin(), scounts.end(), countcomp);

	for (i = 0; i < abfd->syms.size(); i++) {
		printf_symbol(scounts[i].sym->name);

		found_samples = false;
		for (k = 0; k < nr_counters ; ++k) {
			if (scounts[i].count[k]) {
				printf("[0x%.8lx]: %2.4f%% (%u samples)\n", 
					scounts[i].sym->value+scounts[i].sym->section->vma,
					(((double)scounts[i].count[k]) / tot[k])*100.0, 
					scounts[i].count[k]);
				found_samples = true;
			}
			
		}
		if (found_samples == 0 && !streq("", scounts[i].sym->name)) {
			printf(" (0 samples)\n");
		}
	}
}
 
/**
 * do_list_symbol - list detailed samples for a symbol
 * @abfd: the bfd object from where come the samples
  *
 * the global variable @symbol is used to list all
 * the samples for this symbol from the image 
 * specified by @abfd.
 */
void opp_samples_files::do_list_symbol(opp_bfd* abfd) const
{
	u32 start, end;
	u32 j;
	uint k;

	int i = abfd->symbol_index(symbol);
	if (i < 0) {
		fprintf(stderr, "oprofpp: symbol \"%s\" not found in image file.\n", symbol);
		return;
	}

	printf("Samples for symbol \"%s\" in image %s\n", symbol, abfd->ibfd->filename);

	abfd->get_symbol_range(i, &start, &end);
	for (j = start; j < end; j++) {
		for (k = 0 ; k < nr_counters ; ++k) {
			if (samples_count(k, j))
				break;
		}

		/* all counters are empty at this address */ 
		if (k == nr_counters)
			continue;
 
		abfd->output_linenr(i, j);
		printf("%s+%x/%x:", symbol, abfd->sym_offset(i, j), end-start);
 
		for (k = 0 ; k < nr_counters ; ++k) {
			printf("\t%u", samples_count(k, j));
		}
		printf("\n");
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
 * @abfd: the bfd object from where come the samples
 *
 * Dump gprof-format samples for the image specified by samplefile to
 * the file specified by gproffile.
 */
// FIXME: too slow (and others) 
void opp_samples_files::do_dump_gprof(struct opp_bfd* abfd) const
{
	static gmon_hdr hdr = { { 'g', 'm', 'o', 'n' }, GMON_VERSION, {0,0,0,},}; 
	FILE *fp; 
	u32 start, end;
	uint i, j;
	u32 low_pc = (u32)-1;
	u32 high_pc = 0;
	u16 * hist;
	u32 histsize;

	fp=opd_open_file(gproffile, "w");

	opd_write_file(fp,&hdr, sizeof(gmon_hdr));

	opd_write_u8(fp, GMON_TAG_TIME_HIST);

	for (i = 0; i < abfd->syms.size(); i++) {
		start = abfd->syms[i]->value + abfd->syms[i]->section->vma;
		if (i == abfd->syms.size() - 1) {
			abfd->get_symbol_range(i, &start, &end);
			end -= start;
			start = abfd->syms[i]->value + abfd->syms[i]->section->vma;
			end += start;
		} else
			end = abfd->syms[i+1]->value + abfd->syms[i+1]->section->vma;
 
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

	hist = (u16*)opd_calloc0(histsize, sizeof(u16)); 
 
	for (i = 0; i < abfd->syms.size(); i++) {
		abfd->get_symbol_range(i, &start, &end); 
		for (j=start; j < end; j++) {
			u32 count;
			u32 pos;
			pos = (abfd->sym_offset(i, j) + abfd->syms[i]->value + abfd->syms[i]->section->vma - low_pc) / MULTIPLIER; 

			/* opp_get_options have set ctr to one value != -1 */
			count = samples_count(ctr, j);

			if (pos >= histsize) {
				fprintf(stderr, "Bogus histogram bin %u, larger than %u !", pos, histsize);
				continue;
			}

			if (hist[pos] + count > (u16)-1) {
				printf("Warning: capping sample count !\n");
				hist[pos] = (u16)-1;
			} else {
				hist[pos] += (u16)count;
			}
		}
	}

	opd_write_file(fp, hist, histsize * sizeof(u16));
	opd_close_file(fp);
}

/**
 * do_list_symbol_details - list all samples for a given symbol
 * @abfd: the bfd object from where come the samples
 * @sym_idx: the symbol index
 *
 * detail samples for the symbol @sym_idx to stdout in
 * increasing order of vma. Nothing occur if the the symbol
 * do not have any sample associated with it.
 */
void opp_samples_files::do_list_symbol_details(opp_bfd* abfd, uint sym_idx) const
{
	counter_array_t counter;
	uint j, k;
	bool found_samples;
	bfd_vma vma, base_vma;
	u32 start, end;
	asymbol * sym;

	sym = abfd->syms[sym_idx];

	abfd->get_symbol_range(sym_idx, &start, &end);

	/* To avoid outputing 0 samples symbols */
	found_samples = false;
	for (j = start; j < end; ++j)
		found_samples |= accumulate_samples(counter, j);

	if (found_samples == false)
		return;

	base_vma = sym->value + sym->section->vma;
	vma = abfd->sym_offset(sym_idx, start) + base_vma;

	abfd->output_linenr(sym_idx, start);
	printf("%.8lx ", vma);
	for (k = 0 ; k < nr_counters ; ++k)
		printf("%u ", counter[k]);
	printf_symbol(sym->name);
	printf("\n");

	for (j = start; j < end; j++) {
		for (k = 0; k < nr_counters; ++k)
			counter[k] = 0;

		found_samples = accumulate_samples(counter, j);
		if (found_samples == false)
			continue;

		vma = abfd->sym_offset(sym_idx, j) + base_vma;

		printf(" ");
		abfd->output_linenr(sym_idx, j);
		printf("%.8lx", vma);

		for (k = 0; k < nr_counters; ++k)
			printf(" %u", counter[k]);
		printf("\n");
	}
}

/**
 * do_list_all_symbols_details - list all samples for all symbols.
 * @abfd: the bfd object from where come the samples
 *
 * Lists all the samples for all the symbols, from the image specified by
 * @abfd, in increasing order of vma, to standard out.
 */
void opp_samples_files::do_list_all_symbols_details(opp_bfd* abfd) const
{
	size_t i;

	for (i = 0 ; i < abfd->syms.size(); ++i) {
		do_list_symbol_details(abfd, i);
	} 
}

#ifndef OPROFPP_NO_MAIN

/**
 * output_event - output a counter setup
 * @i: counter number
 *
 * output to stdout a description of an event.
 */
void opp_samples_files::output_event(int i) const
{
	printf("Counter %d counted %s events (%s) with a unit mask of "
	       "0x%.2x (%s) count %u\n", 
	       i, ctr_name[i], ctr_desc[i], footer[i]->ctr_um,
	       ctr_um_desc[i] ? ctr_um_desc[i] : "Not set", 
	       footer[i]->ctr_count);
}

/**
 * output_header() - output counter setup
 *
 * output to stdout the cpu type, cpu speed
 * and all counter description available
 */
void opp_samples_files::output_header() const
{
	uint i;

	printf("Cpu type: %s\n", op_get_cpu_type_str(footer[first_file]->cpu_type));

	printf("Cpu speed was (MHz estimation) : %f\n", footer[first_file]->cpu_speed);

	for (i = 0 ; i < OP_MAX_COUNTERS; ++i) {
		if (fd[i] != -1)
			output_event(i);
	}
}

/**
 * main - output "hello, world" to stdout
 */
int main(int argc, char const *argv[])
{
	opp_get_options(argc, argv);

	opp_samples_files samples_files;
	opp_bfd abfd(samples_files.footer[samples_files.first_file]);

	samples_files.output_header();

	if (list_symbols) {
		samples_files.do_list_symbols(&abfd);
	} else if (symbol) {
		samples_files.do_list_symbol(&abfd);
	} else if (gproffile) {
		samples_files.do_dump_gprof(&abfd);
	} else if (list_all_symbols_details) {
		samples_files.do_list_all_symbols_details(&abfd);
	}

	return 0;
}

#endif /* !OPROFPP_NO_MAIN */
