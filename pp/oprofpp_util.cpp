/* $Id: oprofpp_util.cpp,v 1.11 2001/12/05 04:31:17 phil_e Exp $ */
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

// FIXME: printf -> ostream (and elsewhere) 
#include <algorithm>
#include <sstream>

#include "oprofpp.h"
#include "../util/file_manip.h"
 
int verbose;
char const *samplefile;
char *basedir="/var/opd";
const char *imagefile;
int demangle;
int list_all_symbols_details;
/* counter k is selected if (ctr == -1 || ctr == k), if ctr == -1 counter 0
 * is used for sort purpose */
int ctr = -1;

/**
 * remangle - convert a filename into the related sample file name
 * @image: the image filename
 */
static char *remangle(const char *image)
{
	char *file;
	char *c; 

	if (!basedir || !*basedir)
		basedir = "/var/opd";
	else if (basedir[strlen(basedir)-1] == '/')
		basedir[strlen(basedir)-1] = '\0';

	file = (char*)xmalloc(strlen(basedir) + strlen("/samples/") + strlen(image) + 1);
	
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
 * demangle_filename - convert a sample filenames into the related
 * image file name
 * @sample_filename: the samples image filename
 *
 * if samples_filename does not contain any %OPD_MANGLE_CHAR
 * the string samples_filename itself is returned.
 */
std::string demangle_filename(const std::string & samples_filename)
{
	std::string result(samples_filename);
	size_t pos = samples_filename.find_first_of(OPD_MANGLE_CHAR);
	if (pos != std::string::npos) {
		result.erase(0, pos);
		std::replace(result.begin(), result.end(), OPD_MANGLE_CHAR, '/');
	}

	return result;
}

/**
 * quit_error - quit with error
 * @err: error to show
 *
 * err may be NULL
 */
void quit_error(poptContext optcon, char const *err)
{
	if (err)
		fprintf(stderr, err); 
	poptPrintHelp(optcon, stderr, 0);
	exit(EXIT_FAILURE);
}
 
/**
 * opp_treat_options - process command line options
 * @file: a filename passed on the command line, can be %NULL
 * @optcon: poptContext to allow better message handling
 *
 * Process the arguments, fatally complaining on
 * error. 
 *
 * Most of the complexity here is to process
 * filename. @file is considered as a sample file
 * if it contains at least one OPD_MANGLE_CHAR else
 * it is an image file. If no image file is given
 * on command line the sample file name is un-mangled
 * -after- stripping the optionnal "#d-d" suffixe. This
 * give some limitations on the image filename.
 *
 * all filename checking is made here only with a
 * syntactical approch. (ie existence of filename is
 * not tested)
 *
 * post-condition: samplefile and imagefile are setup
 */
void opp_treat_options(const char* file, poptContext optcon)
{
	char *file_ctr_str;
	int counter;

	/* some minor memory leak from the next calls */
	if (imagefile)
		imagefile = opd_relative_to_absolute_path(imagefile, NULL);

	if (samplefile)
		samplefile = opd_relative_to_absolute_path(samplefile, NULL);

	if (file) {
		if (imagefile && samplefile) {
			quit_error(optcon, "oprofpp: too many filenames given on command line:" 
				"you can specify at most one sample filename"
				" and one image filename.\n");
		}

		file = opd_relative_to_absolute_path(file, NULL);
		if (strchr(file, OPD_MANGLE_CHAR))
			samplefile = file;
		else
			imagefile = file;
	}

	if (!samplefile) { 
		if (!imagefile) { 
			quit_error(optcon, "oprofpp: no samples file specified.\n");
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
			quit_error(optcon, "oprofpp: conflict between given counter and counter of samples file.\n");
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

	/* chop suffixes */
	if (file_ctr_str)
		file_ctr_str[0] = '\0';

	/* check we have a valid ctr */
	if (ctr != -1 && (ctr < 0 || ctr >= OP_MAX_COUNTERS)) {
		fprintf(stderr, "oprofpp: invalid counter number %u\n", ctr);
		exit(EXIT_FAILURE);
	}

	if (!imagefile) {
		std::string temp = demangle_filename(samplefile);
		/* memory leak */
		imagefile = xstrdup(temp.c_str());
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
 * occur.
 *
 * The demangled name lists the parameters and type
 * qualifiers such as "const".
 *
 * return the un-mangled name
 */
std::string demangle_symbol(const char* name)
{
	if (demangle && *name) {
		// Do not try to strip leading underscore, this leads to many
		// C++ demangling failure.
		char *unmangled = cplus_demangle(name, DMGL_PARAMS | DMGL_ANSI);
		if (unmangled) {
			std::string result(unmangled);
			free(unmangled);
			return result;
		}
	}

	return name;
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
 * @header: a valid samples file opd_header
 *
 * The global variable imagefile is used to
 * open the binary file. After construction
 * the data member are setup.
 *
 * All error are fatal.
 *
 */
opp_bfd::opp_bfd(const opd_header* header, uint nr_samples_)
	:
	ibfd(0),
	bfd_syms(0),
	sect_offset(0),
	nr_samples(nr_samples_)
{
	if (!imagefile) {
		fprintf(stderr,"oprofpp: oppp_bfd() imagefile is NULL.\n");
		exit(EXIT_FAILURE);
	} 

	open_bfd_image(imagefile, header->is_kernel);

	time_t newmtime = opd_get_mtime(imagefile);
	if (newmtime != header->mtime) {
		fprintf(stderr, "oprofpp: WARNING: the last modified time of the binary file %s does not match\n"
			"that of the sample file. Either this is the wrong binary or the binary\n"
			"has been modified since the sample file was created.\n", imagefile);
	}
}

/**
 * close_bfd_image - open a bfd image
 *
 * This function will close an opended a bfd image
 * and free all related resource.
 */
opp_bfd::~opp_bfd()
{
	if (bfd_syms) free(bfd_syms);
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
 */
static bool symcomp(const asymbol * a, const asymbol * b)
{
	return a->value + a->section->vma <= b->value + b->section->vma;
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

	if (!(bfd_get_file_flags(ibfd) & HAS_SYMS))
		return false;

	size = bfd_get_symtab_upper_bound(ibfd);

	/* HAS_SYMS can be set with no symbols */
	if (size < 1)
		return false;

	bfd_syms = (asymbol**)xmalloc(size);
	nr_all_syms = bfd_canonicalize_symtab(ibfd, bfd_syms);
	if (nr_all_syms < 1) {
		return false;
	}

	for (i = 0; i < nr_all_syms; i++) {
		if (interesting_symbol(bfd_syms[i])) {
			syms.push_back(bfd_syms[i]);
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
 * have_debug_info - check if the ibfd object contains debug info
 *
 * Returns true if the underlined bfd object contains debug info
 */
bool opp_bfd::have_debug_info() const
{
	sec* section;
	for (section = ibfd->sections; section; section = section->next)
		if (section->flags & SEC_DEBUGGING)
			break;

	return section != NULL;
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
 * return true if the lookup succeed. In any case @filename
 * is never set to NULL.
 */
bool opp_bfd::get_linenr(uint sym_idx, uint offset, 
			const char*& filename, unsigned int& linenr)
{
	const char *functionname;
	bfd_vma pc;

	filename = 0;
	linenr = 0;

	asection* section = syms[sym_idx]->section;

	if ((bfd_get_section_flags (ibfd, section) & SEC_ALLOC) == 0)
		return false;

	pc = sym_offset(sym_idx, offset) + syms[sym_idx]->value;

	if (pc >= bfd_section_size(ibfd, section))
		return false;

	bool ret = bfd_find_nearest_line(ibfd, section, bfd_syms, pc,
					 &filename, &functionname, &linenr);

	if (filename == NULL || ret == false) {
		filename = "";
		linenr = 0;
	}

	// functioname and symbol name can be different if we query linenr info
	// if we accept it we can get samples for the wrong symbol (#484660)
	if (ret == true && functionname && 
	    strcmp(functionname, syms[sym_idx]->name)) {
		ret = false;
	}

	return ret;
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
void opp_bfd::get_symbol_range(uint sym_idx, u32 & start, u32 & end) const
{
	asymbol *sym, *next;

	sym = syms[sym_idx];
	next = (sym_idx == syms.size() - 1) ? NULL : syms[sym_idx + 1];

	verbprintf("Symbol %s, value 0x%lx\n", sym->name, sym->value); 
	start = sym->value;
	/* offset of section */
	start += sym->section->filepos;
	verbprintf("in section %s, filepos 0x%lx\n", sym->section->name, sym->section->filepos);
	/* adjust for kernel image */
	start += sect_offset;
	if (next) {
		end = next->value;
		/* offset of section */
		end += next->section->filepos;
		/* adjust for kernel image */
		end += sect_offset;
	} else
		end = nr_samples;
	verbprintf("start 0x%x, end 0x%x\n", start, end); 

	if (start >= nr_samples) {
		fprintf(stderr,"oprofpp: start 0x%x out of range (max 0x%x)\n", start, nr_samples);
		exit(EXIT_FAILURE);
	}

	if (end > nr_samples) {
		fprintf(stderr,"oprofpp: end 0x%x out of range (max 0x%x)\n", end, nr_samples);
		exit(EXIT_FAILURE);
	}

	if (start > end) {
		fprintf(stderr,"oprofpp: start 0x%x overflow or end 0x%x underflow\n", start, end);
		exit(EXIT_FAILURE);
	}
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
 * check_headers - check coherence between two headers.
 * @f1: first header
 * @f2: second header
 *
 * verify that header @f1 and @f2 are coherent.
 * all error are fatal
 */
static void check_headers(const opd_header * f1, const opd_header * f2)
{
	if (f1->mtime != f2->mtime) {
		fprintf(stderr, "oprofpp: header timestamps are different (%ld, %ld)\n", f1->mtime, f2->mtime);
		exit(EXIT_FAILURE);
	}

	if (f1->is_kernel != f2->is_kernel) {
		fprintf(stderr, "oprofpp: header is_kernel flags are different\n");
		exit(EXIT_FAILURE);
	}

	if (f1->cpu_speed != f2->cpu_speed) {
		fprintf(stderr, "oprofpp: header cpu speeds are different (%f, %f)",
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
 * their header must be coherent. Each header is also
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
		header[i] = 0;
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
	mtime = header[first_file]->mtime;

	/* determine how many counters are possible via the sample file.
	 * allows use on different platform */ 
	nr_counters = 2;
	if (header[first_file]->cpu_type == CPU_ATHLON)
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
		check_headers(header[first_file], header[j]);
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
 * close and free all related resource to the samples file(s)
 */
opp_samples_files::~opp_samples_files()
{
	uint i;

	for (i = 0 ; i < OP_MAX_COUNTERS; ++i) {
		if (header[i]) {
			munmap(header[i], size[i] + sizeof(opd_header));
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
 * the member var samples[@counter], header[@counter]
 * etc. are updated in case of success.
 * The header is checked but coherence between
 * header can not be sanitized at this point.
 *
 * if @can_fail == false all error are fatal.
 */
void opp_samples_files::open_samples_file(u32 counter, bool can_fail)
{
	std::ostringstream filename;
	filename << samplefile << "#" << counter;
	std::string temp = filename.str();

	fd[counter] = open(temp.c_str(), O_RDONLY);
	if (fd[counter] == -1) {
		if (can_fail == false)	{
			fprintf(stderr, "oprofpp: Opening %s failed. %s\n", temp.c_str(), strerror(errno));
			exit(EXIT_FAILURE);
		}
		header[counter] = NULL;
		samples[counter] = NULL;
		size[counter] = 0;
		return;
	}

	size_t sz_file = opd_get_fsize(temp.c_str(), 1);
	if (sz_file < sizeof(opd_header)) {
		fprintf(stderr, "oprofpp: sample file %s is not the right "
			"size: got %d, expect at least %d\n", 
			temp.c_str(), sz_file, sizeof(opd_header));
		exit(EXIT_FAILURE);
	}
	size[counter] = sz_file - sizeof(opd_header); 

	header[counter] = (opd_header*)mmap(0, sz_file, 
				      PROT_READ, MAP_PRIVATE, fd[counter], 0);
	if (header[counter] == (void *)-1) {
		fprintf(stderr, "oprofpp: mmap of %s failed. %s\n", temp.c_str(), strerror(errno));
		exit(EXIT_FAILURE);
	}

	samples[counter] = (opd_fentry *)(header[counter] + 1);

	if (memcmp(header[counter]->magic, OPD_MAGIC, sizeof(header[0]->magic))) {
		/* FIXME: is 4.4 ok : there is no zero terminator */
		fprintf(stderr, "oprofpp: wrong magic %4.4s, expected %s.\n", header[counter]->magic, OPD_MAGIC);
		exit(EXIT_FAILURE);
	}

	if (header[counter]->version != OPD_VERSION) {
		fprintf(stderr, "oprofpp: wrong version 0x%x, expected 0x%x.\n", header[counter]->version, OPD_VERSION);
		exit(EXIT_FAILURE);
	}

	/* This should be guaranteed by the daemon */
	if (header[counter]->ctr != counter) {
		fprintf(stderr, "oprofpp: sanity check counter number fail %d, expect %d.\n", header[counter]->ctr, counter);
		exit(EXIT_FAILURE);
	}
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
	op_get_event_desc(header[i]->cpu_type, header[i]->ctr_event,
			  header[i]->ctr_um, &ctr_name[i],
			  &ctr_desc[i], &ctr_um_desc[i]);
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
