/* $Id: oprofpp_util.cpp,v 1.32 2002/03/04 18:56:22 movement Exp $ */
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
#include <cstdarg>
#include <algorithm>
#include <sstream>

#include <elf.h>

#include "oprofpp.h"
#include "opp_symbol.h"
#include "../util/file_manip.h"
#include "../util/string_manip.h"
 
using std::string;

int verbose;
char const *samplefile;
char *basedir="/var/opd";
const char *imagefile;
int demangle;
int list_all_symbols_details;
const char * exclude_symbols_str;
static std::vector<std::string> exclude_symbols;

/**
 * verbprintf
 */
void verbprintf(const char * fmt, ...)
{
	if (verbose) {
		va_list va;
		va_start(va, fmt);

		vprintf(fmt, va);

		va_end(va);
	}
}

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
 * is_excluded_symbol - check if the symbol is in the exclude list
 * @symbol: symbol name to check
 *
 * return true if @symbol is in the list of excluded symbol
 */
bool is_excluded_symbol(const std::string & symbol)
{
	return std::find(exclude_symbols.begin(), exclude_symbols.end(),
			 symbol) != exclude_symbols.end();
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
 * @image_file: where to store the image file name
 * @sample_file: ditto for sample filename
 * @counter: where to put the counter command line argument
 *
 * Process the arguments, fatally complaining on
 * error. 
 *
 * Most of the complexity here is to process
 * filename. @file is considered as a sample file
 * if it contains at least one OPD_MANGLE_CHAR else
 * it is an image file. If no image file is given
 * on command line the sample file name is un-mangled
 * -after- stripping the optionnal "#d" suffixe. This
 * give some limitations on the image filename.
 *
 * all filename checking is made here only with a
 * syntactical approch. (ie existence of filename is
 * not tested)
 *
 * post-condition: @sample_file and @image_file are setup
 */
void opp_treat_options(const char* file, poptContext optcon,
		       string & image_file, string & sample_file,
		       int & counter)
{
	char *file_ctr_str;
	int temp_counter;

	/* add to the exclude symbol list the symbols contained in the comma
	 * separated list of symbols */
	if (exclude_symbols_str)
		separate_token(exclude_symbols, exclude_symbols_str, ',');

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
	 * and chop optionnal suffixe "#%d" first */

	/* check for a valid counter suffix in a given sample file */
	temp_counter = -1;
	file_ctr_str = strrchr(samplefile, '#');
	if (file_ctr_str) {
		sscanf(file_ctr_str + 1, "%d", &temp_counter);
	}

	if (counter != temp_counter) {
		/* a --counter=x have priority on the # suffixe of filename */
		if (counter != -1 && temp_counter != -1)
			quit_error(optcon, "oprofpp: conflict between given counter and counter of samples file.\n");
	}

	if (counter == -1)
		counter = temp_counter;

	if (counter == -1) {
		/* list_all_symbols_details always output all counter */
		if (!list_all_symbols_details)
			counter = 0;
	}

	/* chop suffixes */
	if (file_ctr_str)
		file_ctr_str[0] = '\0';

	/* check we have a valid ctr */
	if (counter != -1 && (counter < 0 || counter >= OP_MAX_COUNTERS)) {
		fprintf(stderr, "oprofpp: invalid counter number %u\n", counter);
		exit(EXIT_FAILURE);
	}

	sample_file = samplefile;

	if (!imagefile) {
		/* we allow for user to specify a sample filename on the form
		 * /var/opd/samples/}bin}nash}}}lib}libc.so so we need to
		 * check against this form of mangled filename */
		string lib_name;
		string app_name = extract_app_name(sample_file, lib_name);
		if (lib_name.length())
			app_name = lib_name;
		image_file = demangle_filename(app_name);
	}
	else
		image_file = imagefile;
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
 * @nr_samples: the number of samples location for
 * this image ie the number of bytes memory mapped
 * for this image with EXEC right
 * @image_file: the name of the image file
 *
 * All error are fatal.
 *
 */
opp_bfd::opp_bfd(const opd_header* header, uint nr_samples_, const string & filename)
	:
	ibfd(0),
	bfd_syms(0),
	sect_offset(0),
	nr_samples(nr_samples_)
{
	if (filename.length() == 0) {
		fprintf(stderr,"oprofpp: oppp_bfd() empty image filename.\n");
		exit(EXIT_FAILURE);
	} 

	open_bfd_image(filename, header->is_kernel);

	time_t newmtime = opd_get_mtime(filename.c_str());
	if (newmtime != header->mtime) {
		fprintf(stderr, "oprofpp: WARNING: the last modified time of the binary file %s does not match\n"
			"that of the sample file. Either this is the wrong binary or the binary\n"
			"has been modified since the sample file was created.\n", filename.c_str());
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
 * Failure to open the image a fatal
 * gettings zero symbols from the image is not an error
 */
void opp_bfd::open_bfd_image(const string & filename, bool is_kernel)
{
	char **matching;

	ibfd = bfd_openr(filename.c_str(), NULL);
 
	if (!ibfd) {
		fprintf(stderr,"oprofpp: bfd_openr of %s failed.\n", filename.c_str());
		exit(EXIT_FAILURE);
	}
	 
	if (!bfd_check_format_matches(ibfd, bfd_object, &matching)) { 
		fprintf(stderr,"oprofpp: BFD format failure for %s.\n", filename.c_str());
		exit(EXIT_FAILURE);
	}

	if (is_kernel) {
		asection *sect;
		sect = bfd_get_section_by_name(ibfd, ".text");
		sect_offset = OPD_KERNEL_OFFSET - sect->filepos;
		verbprintf("Adjusting kernel samples by 0x%x, .text filepos 0x%lx\n", sect_offset, sect->filepos); 
	}

	get_symbols();
}

/**
 * symcomp - comparator
 *
 */
static bool symcomp(const asymbol * a, const asymbol * b)
{
	return a->value + a->section->vma < b->value + b->section->vma;
}

namespace { 
 
// only add symbols that would /never/ be
// worth examining
static const char *boring_symbols[] = {
	"gcc2_compiled.",
};

static const size_t nr_boring_symbols = (sizeof(boring_symbols) / sizeof(char *));
 
/**
 * Return true if the symbol is worth looking at
 */
static bool interesting_symbol(asymbol *sym)
{
	if (!(sym->section->flags & SEC_CODE))
		return 0;

	if (streq("", sym->name))
		return 0;

	if (streq("_init", sym->name))
		return 0;

	for (size_t i = 0; i < nr_boring_symbols; ++i) {
		if (streq(boring_symbols[i], sym->name))
			return 0;
	}
	 
	return 1;
}

} // namespace anon
 
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

	std::stable_sort(syms.begin(), syms.end(), symcomp);

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
			const char*& filename, unsigned int& linenr) const
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

// #define USE_ELF_INTERNAL

#ifdef USE_ELF_INTERNAL
struct elf_internal_sym {
  bfd_vma	st_value;		/* Value of the symbol */
  bfd_vma	st_size;		/* Associated symbol size */
  unsigned long	st_name;		/* Symbol name, index in string tbl */
  unsigned char	st_info;		/* Type and binding attributes */
  unsigned char	st_other;		/* No defined meaning, 0 */
  unsigned short st_shndx;		/* Associated section index */
};

typedef struct elf_internal_sym Elf_Internal_Sym;

typedef struct
{
  /* The BFD symbol.  */
  asymbol symbol;
  /* ELF symbol information.  */
  Elf_Internal_Sym internal_elf_sym;
} elf_symbol_type;

#endif /* USE_ELF_INTERNAL */

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

	if (is_excluded_symbol(sym->name)) {
		end = start;
	} else {
#ifndef USE_ELF_INTERNAL
		if (next) {
			end = next->value;
			/* offset of section */
			end += next->section->filepos;
			/* adjust for kernel image */
			end += sect_offset;
		} else
			end = nr_samples;
#else /* !USE_ELF_INTERNAL */
		size_t length =
			((elf_symbol_type *)sym)->internal_elf_sym.st_size;

		// some asm symbol can have a zero length such system_call
		// entry point in vmlinux. Calculate the length from the next
		// symbol vma
		if (length == 0) {
			u32 next_offset = start;
			if (next) {
				next_offset = next->value +
					next->section->filepos + sect_offset;
			} else {
				next_offset = nr_samples;
			}
			length = next_offset - start;
		}
		end = start + length;
#endif /* USE_ELF_INTERNAL */
	}
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
void check_headers(const opd_header * f1, const opd_header * f2)
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

	if (f1->separate_samples != f2->separate_samples) {
		fprintf(stderr, "oprofpp: header separate_samples are different (%d, %d)",
			f2->separate_samples, f2->separate_samples);
		exit(EXIT_FAILURE);
	}
}

/**
 * opp_samples_files - construct an opp_samples_files object
 * @sample_file: the base name of sample file
 * @counter: which samples files to open, -1 means try to open
 * all samples files.
 *
 * at least one sample file (based on @sample_file name)
 * must be opened. If more than one sample file is open
 * their header must be coherent. Each header is also
 * sanitized.
 *
 * all error are fatal
 */
opp_samples_files::opp_samples_files(const std::string & sample_file,
				     int counter_)
	:
	nr_counters(2),
	first_file(-1),
	sample_filename(sample_file),
	counter(counter_)
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
		if (counter == -1 || counter == (int)i) {
			/* if counter == i, this means than we open only one
			 * samples file so don't allow opening failure to get
			 * a more precise error message */
			open_samples_file(i, counter != (int)i);
		}
	}

	/* find first open file */
	for (first_file = 0; first_file < OP_MAX_COUNTERS ; ++first_file) {
		if (fd[first_file] != -1)
			break;
	}

	if (first_file == OP_MAX_COUNTERS) {
		fprintf(stderr, "Can not open any samples files for %s last error %s\n", sample_filename.c_str(), strerror(errno));
		exit(EXIT_FAILURE);
	}

	nr_samples = size[first_file] / sizeof(opd_fentry);
	mtime = header[first_file]->mtime;

	/* determine how many counters are possible via the sample file */
	op_cpu cpu = static_cast<op_cpu>(header[first_file]->cpu_type);
	nr_counters = op_get_cpu_nr_counters(cpu);

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
 * open_samples_file - helper function to open a samples files
 * @fd: the file descriptor of file to mmap
 * @size: where to store the samples files size not counting the header
 * @fentry: where to store the opd_fentry pointer
 * @header: where to store the opd_header pointer
 * @filename: the filename of fd used for error message only
 *
 * open and mmap the given samples files,
 * the param @samples, @header[@counter]
 * etc. are updated.
 * all error are fatal
 */
static void open_samples_file(fd_t fd, size_t & size, opd_fentry * & fentry,
			      opd_header * & header, const std::string & filename)
{
	size_t sz_file = opd_get_fsize(filename.c_str(), 1);
	if (sz_file < sizeof(opd_header)) {
		fprintf(stderr, "op_merge: sample file %s is not the right "
			"size: got %d, expect at least %d\n", 
			filename.c_str(), sz_file, sizeof(opd_header));
		exit(EXIT_FAILURE);
	}
	size = sz_file - sizeof(opd_header); 

	header = (opd_header*)mmap(0, sz_file, 
				   PROT_READ, MAP_PRIVATE, fd, 0);
	if (header == (void *)-1) {
		fprintf(stderr, "op_merge: mmap of %s failed. %s\n", filename.c_str(), strerror(errno));
		exit(EXIT_FAILURE);
	}

	fentry = (opd_fentry *)(header + 1);

	if (memcmp(header->magic, OPD_MAGIC, sizeof(header->magic))) {
		/* FIXME: is 4.4 ok : there is no zero terminator */
		fprintf(stderr, "op_merge: wrong magic %4.4s, expected %s.\n", header->magic, OPD_MAGIC);
		exit(EXIT_FAILURE);
	}

	if (header->version != OPD_VERSION) {
		fprintf(stderr, "op_merge: wrong version 0x%x, expected 0x%x.\n", header->version, OPD_VERSION);
		exit(EXIT_FAILURE);
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
	filename << sample_filename << "#" << counter;
	std::string temp = filename.str();

	fd[counter] = open(temp.c_str(), O_RDONLY);
	if (fd[counter] == -1) {
		if (can_fail == false)	{
			/* FIXME: nicer message if e.g. wrong counter */ 
			fprintf(stderr, "oprofpp: Opening %s failed. %s\n", temp.c_str(), strerror(errno));
			exit(EXIT_FAILURE);
		}
		header[counter] = NULL;
		samples[counter] = NULL;
		size[counter] = 0;
		return;
	}

	::open_samples_file(fd[counter], size[counter],
			    samples[counter], header[counter], temp);
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
	op_cpu cpu = static_cast<op_cpu>(header[i]->cpu_type);
	op_get_event_desc(cpu, header[i]->ctr_event,
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
	bool found_samples = false;

	for (uint k = 0; k < nr_counters; ++k) {
		if (samples_count(k, index)) {
			found_samples = true;
			counter[k] += samples_count(k, index);
		}
	}

	return found_samples;
}

/**
 * accumulate_samples - lookup samples from a range of vma address
 * @counter: where to accumulate the samples
 * @start: start index of the samples.
 * @end: end index of the samples.
 *
 * return false if no samples has been found
 */
bool opp_samples_files::accumulate_samples(counter_array_t& counter,
					   uint start, uint end) const
{
	bool found_samples = false;

	for (uint k = 0; k < nr_counters; ++k) {
		if (is_open(k)) {
			for (uint j = start ; j < end; ++j) {
				if (samples[k][j].count) {
					counter[k] += samples[k][j].count;
					found_samples = true;
				}
			}
		}
	}

	return found_samples;
}

/**
 * samples_file_t - construct a samples_file_t object
 * @filename: the full path of sample file
 *
 * open and mmap the samples file specified by @filename
 * samples file header is checked
 *
 * all error are fatal
 *
 */
samples_file_t::samples_file_t(const string & filename)
	:
	samples(0),
	header(0),
	fd(-1),
	size(0)
{
	fd = open(filename.c_str(), O_RDONLY);
	if (fd == -1) {
		fprintf(stderr, "op_merge: Opening %s failed. %s\n", filename.c_str(), strerror(errno));
		exit(EXIT_FAILURE);
	}

	open_samples_file(fd, size, samples, header, filename);

	nr_samples = size / sizeof(opd_fentry);
}

/**
 * ~samples_file_t - destroy a samples_file_t object
 *
 * close and unmap the samples file
 *
 */
samples_file_t::~samples_file_t()
{
	if (header)
		munmap(header, size + sizeof(opd_header));
	if (fd != -1)
		close(fd);
}

/**
 * check_headers - check than the lhs and rhs headers are
 * coherent (same size, same mtime etc.)
 * @rhs: the other samples_file_t
 *
 * all error are fatal
 *
 */
bool samples_file_t::check_headers(const samples_file_t & rhs) const
{
	::check_headers(header, rhs.header);
	
	if (size != rhs.size) {
		fprintf(stderr, "op_merge: mapping file size "
			"are different (%d, %d)\n", size, rhs.size);
		exit(EXIT_FAILURE);		
	}

	return true;
}

/**
 * count - return the number of samples in given range
 * @start: start samples nr of range
 * @end: end samples br of range
 *
 * return the number of samples in the the range [@start, @end]
 * no range checking is performed.
 *
 * This actually code duplicate partially accumulate member of
 * opp_samples_files which in future must use this as it internal
 * implementation
 */
u32 samples_file_t::count(uint start, uint end) const
{
	u32 count = 0;
	for ( ; start < end ; ++start)
		count += samples[start].count;

	return count;
}
