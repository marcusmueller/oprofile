/**
 * @file oprofpp_util.cpp
 * Helpers for post-profiling analysis
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 * 
 * @author Philippe Elie <phil_el@wanadoo.fr>
 * @author John Levon <moz@compsoc.man.ac.uk>
 */

// FIXME: printf -> ostream (and elsewhere) 
#include <cstdarg>
#include <algorithm>
#include <sstream>
#include <iomanip>

#include <elf.h>

#include "oprofpp.h"
#include "opp_symbol.h"
#include "op_libiberty.h"
#include "op_file.h"
#include "file_manip.h"
#include "string_manip.h"
#include "op_events.h"
#include "op_events_desc.h"
 
using std::string;
using std::vector;
using std::ostream;

int verbose;
char const *samplefile;
char const * imagefile;
int demangle;
char const * exclude_symbols_str;
static vector<string> exclude_symbols;

void op_print_event(ostream & out, int i, op_cpu cpu_type,
		    u8 type, u8 um, u32 count)
{
	char * typenamep;
	char * typedescp;
	char * umdescp;

	op_get_event_desc(cpu_type, type, um, 
			  &typenamep, &typedescp, &umdescp);

	out << "Counter " << i << " counted " << typenamep << " events ("
	    << typedescp << ")";
	if (cpu_type != CPU_RTC) {
		out << " with a unit mask of 0x"
		    << hex << setw(2) << setfill('0') << unsigned(um) << " ("
		    << (umdescp ? umdescp : "Not set") << ")";
	}
	out << " count " << dec << count << endl;
}

/**
 * verbprintf
 */
void verbprintf(char const * fmt, ...)
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
 * @param image the image filename
 */
static char *remangle(char const * image)
{
	char *file;
	char *c; 

	file = (char *)xmalloc(strlen(OP_SAMPLES_DIR) + strlen(image) + 1);
	
	strcpy(file, OP_SAMPLES_DIR);
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
 * @param samples_filename the samples image filename
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
 * @param symbol symbol name to check
 *
 * return true if symbol is in the list of excluded symbol
 */
bool is_excluded_symbol(const std::string & symbol)
{
	return std::find(exclude_symbols.begin(), exclude_symbols.end(),
			 symbol) != exclude_symbols.end();
}

/**
 * quit_error - quit with error
 * @param err error to show
 * @param optcon the popt context
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
 * validate_counter - validate the counter nr
 * @param counter_mask bit mask specifying the counter nr to use
 * @param sort_by_counter the counter nr from which we sort
 *
 * all error are fatal
 */
void validate_counter(int counter_mask, int & sort_by_counter)
{
	if (counter_mask + 1 > 1 << OP_MAX_COUNTERS) {
		cerr << "invalid counter mask " << counter_mask << "\n";
		exit(EXIT_FAILURE);
	}

	if (sort_by_counter == -1) {
		// get the first counter selected and use it as sort order
		for (size_t i = 0 ; i < OP_MAX_COUNTERS ; ++i) {
			if ((counter_mask & (1 << i)) != 0)
				sort_by_counter = i;
		}
	}

	if ((counter_mask & (1 << sort_by_counter)) == 0) {
		cerr << "invalid sort counter nr " << sort_by_counter << "\n";
		exit(EXIT_FAILURE);
	}
}

/**
 * add to the exclude symbol list the symbols contained in the comma
 * separated list of symbols through the gloval var exclude_symbols_str
 */
void handle_exclude_symbol_option()
{
	if (exclude_symbols_str)
		separate_token(exclude_symbols, exclude_symbols_str, ',');  
}
 
/**
 * opp_treat_options - process command line options
 * @param file a filename passed on the command line, can be %NULL
 * @param optcon poptContext to allow better message handling
 * @param image_file where to store the image file name
 * @param sample_file ditto for sample filename
 * @param counter where to put the counter command line argument
 * @param sort_by_counter FIXME
 *
 * Process the arguments, fatally complaining on
 * error. 
 *
 * Most of the complexity here is to process
 * filename. file is considered as a sample file
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
 * post-condition: sample_file and image_file are setup
 */
void opp_treat_options(char const * file, poptContext optcon,
		       string & image_file, string & sample_file,
		       int & counter, int & sort_by_counter)
{
	char *file_ctr_str;
	int temp_counter;

	/* add to the exclude symbol list the symbols contained in the comma
	 * separated list of symbols */
	handle_exclude_symbol_option();

	/* some minor memory leak from the next calls */
	if (imagefile)
		imagefile = op_relative_to_absolute_path(imagefile, NULL);

	if (samplefile)
		samplefile = op_relative_to_absolute_path(samplefile, NULL);

	if (file) {
		if (imagefile && samplefile) {
			quit_error(optcon, "oprofpp: too many filenames given on command line:" 
				"you can specify at most one sample filename"
				" and one image filename.\n");
		}

		file = op_relative_to_absolute_path(file, NULL);
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

	if (temp_counter != -1 && counter != -1 && counter != 0) {
		if ((counter & (1 << temp_counter)) == 0)
			quit_error(optcon, "oprofpp: conflict between given counter and counter of samples file.\n");
	}

	if (counter == -1 || counter == 0) {
		if (temp_counter != -1)
			counter = 1 << temp_counter;
		else
			counter = 1 << 0;	// use counter 0
	}

	/* chop suffixes */
	if (file_ctr_str)
		file_ctr_str[0] = '\0';

	sample_file = samplefile;

	if (!imagefile) {
		/* we allow for user to specify a sample filename on the form
		 * /var/lib/oprofile/samples/}bin}nash}}}lib}libc.so so we need to
		 * check against this form of mangled filename */
		string lib_name;
		string app_name = extract_app_name(sample_file, lib_name);
		if (lib_name.length())
			app_name = lib_name;
		image_file = demangle_filename(app_name);
	}
	else
		image_file = imagefile;

	validate_counter(counter, sort_by_counter);
}

// FIXME: only use char arrays and pointers if you MUST. Otherwise std::string
// and references everywhere please.

/**
 * counter_mask -  given a --counter=0,1,..., option parameter return a mask
 * representing each counter. Bit i is on if counter i was specified.
 * So we allow up to sizeof(uint) * CHAR_BIT different counter
 */
uint counter_mask(const std::string & str)
{
	vector<string> result;
	separate_token(result, str, ',');

	uint mask = 0;
	for (size_t i = 0 ; i < result.size(); ++i) {
		istringstream stream(result[i]);
		int counter;
		stream >> counter;
		mask |= 1 << counter;
	}

	return mask;
}

counter_array_t::counter_array_t()
{
	for (size_t i = 0 ; i < OP_MAX_COUNTERS ; ++i)
		value[i] = 0;
}

counter_array_t & counter_array_t::operator+=(const counter_array_t & rhs)
{
	for (size_t i = 0 ; i < OP_MAX_COUNTERS ; ++i)
		value[i] += rhs.value[i];

	return *this;
}

opp_bfd::opp_bfd(opp_samples_files& samples, const std::string & filename)
	:
	ibfd(0),
	bfd_syms(0),
	sect_offset(0)
{
	if (filename.length() == 0) {
		fprintf(stderr,"oprofpp: oppp_bfd() empty image filename.\n");
		exit(EXIT_FAILURE);
	}

	nr_samples = op_get_fsize(filename.c_str(), 0);

	open_bfd_image(filename, samples.first_header()->is_kernel);

	time_t newmtime = op_get_mtime(filename.c_str());
	if (newmtime != samples.first_header()->mtime) {
		fprintf(stderr, "oprofpp: WARNING: the last modified time of the binary file %s does not match\n"
			"that of the sample file. Either this is the wrong binary or the binary\n"
			"has been modified since the sample file was created.\n", filename.c_str());
	}

	samples.set_sect_offset(sect_offset);
}

opp_bfd::~opp_bfd()
{
	if (bfd_syms) free(bfd_syms);
	bfd_close(ibfd);
}

/**
 * open_bfd_image - opp_bfd ctor helper
 * @param file name of a valid image file
 * @param is_kernel true if the image is the kernel or a module
 *
 * This function will open a bfd image and process symbols
 * within this image file
 *
 * Failure to open the image a fatal
 * gettings zero symbols from the image is not an error
 */
void opp_bfd::open_bfd_image(const std::string & filename, bool is_kernel)
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

	/* Kernel / kernel modules are calculated as offsets against 
	 * the .text section, so they need special handling
	 */
	if (is_kernel) {
		asection *sect;
		sect = bfd_get_section_by_name(ibfd, ".text");
		sect_offset = sect->filepos;
		verbprintf("Adjusting kernel samples by 0x%x, .text filepos 0x%lx\n", 
			sect_offset, sect->filepos); 
	}

	get_symbols();
}

/**
 * symcomp - comparator
 *
 */
static bool symcomp(const op_bfd_symbol & a, const op_bfd_symbol & b)
{
	return a.vma < b.vma;
}

namespace { 
 
// only add symbols that would /never/ be
// worth examining
static char const * boring_symbols[] = {
	"gcc2_compiled.",
	"_init"
};

static const size_t nr_boring_symbols =
			sizeof(boring_symbols) / sizeof(boring_symbols[0]);
 
/**
 * Return true if the symbol is worth looking at
 */
static bool interesting_symbol(asymbol *sym)
{
	if (!(sym->section->flags & SEC_CODE))
		return 0;

	if (!sym->name || sym->name[0] == '\0')
		return 0;

	// C++ exception stuff
	if (sym->name[0] == '.' && sym->name[1] == 'L')
		return 0;

	for (size_t i = 0; i < nr_boring_symbols; ++i) {
		if (!strcmp(boring_symbols[i], sym->name))
			return 0;
	}
	 
	return 1;
}

} // namespace anon
 
/**
 * get_symbols - opp_bfd ctor helper
 *
 * Parse and sort in ascending order all symbols
 * in the file pointed to by abfd that reside in
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
			// we can't fill the size member for now, because in
			// some case it is calculated from the vma of the
			// next symbol
			struct op_bfd_symbol symb = { bfd_syms[i], 
			  bfd_syms[i]->value + bfd_syms[i]->section->vma, 0 };
			syms.push_back(symb);
		}
	}

	std::stable_sort(syms.begin(), syms.end(), symcomp);

	// now we can calculate the symbol size
	for (i = 0 ; i < syms.size() ; ++i) {
		syms[i].size = symbol_size(i);
	}

	// we need to ensure than for a given vma only one symbol exist else
	// we read more than one time some samples. Fix #526098
	// ELF symbols size : potential bogosity here because when using
	// elf symbol size we need to check than two symbols does not overlap.
	for (i =  1 ; i < syms.size() ; ++i) {
		if (syms[i].vma == syms[i-1].vma) {
			// TODO: choose more carefully the symbol we drop.
			// If once have FUNCTION flag and not the other keep
			// it etc.
			syms.erase(syms.begin() + i);
			i--;
		}
	}

	verbprintf("nr symbols before excluding symbols%u\n", syms.size());

	// it's time to remove the excluded symbol.
	for (i = 0 ; i < syms.size() ; ) {
		if (is_excluded_symbol(syms[i].symbol->name)) {
			printf("excluding symbold %s\n", syms[i].symbol->name);
			syms.erase(syms.begin() + i);
		} else {
			++i;
		}
	}

	verbprintf("nr symbols %u\n", syms.size());

	if (syms.empty())
		return false;

	return true;
}

u32 opp_bfd::sym_offset(uint sym_index, u32 num) const
{
	/* take off section offset */
	num -= syms[sym_index].symbol->section->filepos;
	/* and take off symbol offset from section */
	num -= syms[sym_index].symbol->value;

	return num;
}

bool opp_bfd::have_debug_info() const
{
	sec* section;
	for (section = ibfd->sections; section; section = section->next)
		if (section->flags & SEC_DEBUGGING)
			break;

	return section != NULL;
}
 
bool opp_bfd::get_linenr(uint sym_idx, uint offset, 
			char const * & filename, unsigned int & linenr) const
{
	char const * functionname;
	bfd_vma pc;

	filename = 0;
	linenr = 0;

	asection* section = syms[sym_idx].symbol->section;

	if ((bfd_get_section_flags (ibfd, section) & SEC_ALLOC) == 0)
		return false;

	pc = sym_offset(sym_idx, offset) + syms[sym_idx].symbol->value;

	if (pc >= bfd_section_size(ibfd, section))
		return false;

	bool ret = bfd_find_nearest_line(ibfd, section, bfd_syms, pc,
					 &filename, &functionname, &linenr);

	if (filename == NULL || ret == false) {
		filename = "";
		linenr = 0;
		ret = false;
	}

	// functioname and symbol name can be different if we query linenr info
	// if we accept it we can get samples for the wrong symbol (#484660)
	if (ret == true && functionname && 
	    strcmp(functionname, syms[sym_idx].symbol->name)) {
		ret = false;
	}

	/* binutils 2.12 and below have a small bug where functions without a
	 * debug entry at the prologue start do not give a useful line number
	 * from bfd_find_nearest_line(). This can happen with certain gcc
	 * versions such as 2.95.
	 *
	 * We work around this problem by scanning forward for a vma with 
	 * valid linenr info, if we can't get a valid line number.
	 * Problem uncovered by Norbert Kaufmann. The work-around decreases,
	 * on the tincas application, the number of failure to retrieve linenr
	 * info from 835 to 173. Most of the remaining are c++ inline functions
	 * mainly from the STL library. Fix #529622
	 */
	if (/*ret == false || */linenr == 0) {
		// FIXME: looking at debug info for all gcc version shows
		// than the same problems can -perhaps- occur for epilog code:
		// find a samples files with samples in epilog and try oprofpp
		// -L -o on it, check it also with op_to_source.

		// first restrict the search on a sensible range of vma,
		// 16 is an intuitive value based on epilog code look
		size_t max_search = 16;
		size_t section_size = bfd_section_size(ibfd, section);
		if (pc + max_search > section_size)
			max_search = section_size - pc;

		for (size_t i = 1 ; i < max_search ; ++i) {
			bool ret = bfd_find_nearest_line(ibfd, section,
							 bfd_syms, pc+i,
							 &filename,
							 &functionname,
							 &linenr);

			if (ret == true && linenr != 0 &&
			    strcmp(functionname,
				   syms[sym_idx].symbol->name) == 0) {
				return ret;	// we win
			}
		}

		// We lose it's worthwhile to try more.

		// bfd_find_nearest_line clobber the memory pointed by filename
		// from a previous call when the filename change across
		// multiple calls. The more easy way to recover is to reissue
		// the first call, we don't need to recheck return value, we
		// know that the call will succeed.
		bfd_find_nearest_line(ibfd, section, bfd_syms, pc,
				      &filename, &functionname, &linenr);
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

size_t opp_bfd::symbol_size(uint sym_idx) const
{
	asymbol * next, *sym;

	sym = syms[sym_idx].symbol;
	next = (sym_idx == syms.size() - 1) ? NULL : syms[sym_idx + 1].symbol;

	u32 start = sym->section->filepos + sym->value;
	size_t length;

#ifndef USE_ELF_INTERNAL
	u32 end;
	if (next) {
		end = next->value;
		/* offset of section */
		end += next->section->filepos;
	} else
		end = nr_samples;

	length = end - start;
#else /* !USE_ELF_INTERNAL */
	size_t length =
		((elf_symbol_type *)sym)->internal_elf_sym.st_size;

	// some asm symbol can have a zero length such system_call
	// entry point in vmlinux. Calculate the length from the next
	// symbol vma
	if (length == 0) {
		u32 next_offset = start;
		if (next) {
			next_offset = next->value + next->section->filepos;
		} else {
			next_offset = nr_samples;
		}
		length = next_offset - start;
	}
#endif /* USE_ELF_INTERNAL */

	return length;
}

void opp_bfd::get_symbol_range(uint sym_idx, u32 & start, u32 & end) const
{
	asymbol *sym = syms[sym_idx].symbol;

	verbprintf("Symbol %s, value 0x%lx\n", sym->name, sym->value); 
	start = sym->value;
	/* offset of section */
	start += sym->section->filepos;
	verbprintf("in section %s, filepos 0x%lx\n", sym->section->name, sym->section->filepos);

	end = start + syms[sym_idx].size;
	verbprintf("start 0x%x, end 0x%x\n", start, end); 

	if (start >= nr_samples + sect_offset) {
		fprintf(stderr,"oprofpp: start 0x%x out of range (max 0x%x)\n", start, nr_samples);
		exit(EXIT_FAILURE);
	}

	if (end > nr_samples + sect_offset) {
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
 * @param name the symbol name
 *
 * find and return the index of a symbol.
 * if the name is not found -1 is returned
 */
int opp_bfd::symbol_index(char const * symbol) const
{
	for (size_t i = 0; i < syms.size(); i++) {
		if (!strcmp(syms[i].symbol->name, symbol))
			return i;
	}

	return -1;
}

/**
 * check_headers - check coherence between two headers.
 * @param f1 first header
 * @param f2 second header
 *
 * verify that header f1 and f2 are coherent.
 * all error are fatal
 */
void check_headers(opd_header const * f1, opd_header const * f2)
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

void check_event(const struct opd_header * header)
{
	char * ctr_name;
	char * ctr_desc;
	char * ctr_um_desc;

	op_cpu cpu = static_cast<op_cpu>(header->cpu_type);
	op_get_event_desc(cpu, header->ctr_event, header->ctr_um,
			  &ctr_name, &ctr_desc, &ctr_um_desc);
}

/**
 * opp_samples_files - construct an opp_samples_files object
 * @param sample_file the base name of sample file
 * @param counter which samples files to open, -1 means try to open
 * all samples files.
 *
 * at least one sample file (based on sample_file name)
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
	sample_filename(sample_file),
	counter_mask(counter_),
	first_file(-1)
{
	uint i, j;
	time_t mtime = 0;

	/* no samplefiles open initially */
	for (i = 0; i < OP_MAX_COUNTERS; ++i) {
		samples[i] = 0;
	}

	for (i = 0; i < OP_MAX_COUNTERS ; ++i) {
		if ((counter_mask &  (1 << i)) != 0) {
			/* if only the i th bit is set in counter spec we do
			 * not allow opening failure to get a more precise
			 * error message */
			open_samples_file(i, (counter_mask & ~(1 << i)) != 0);
		}
	}

	/* find first open file */
	for (first_file = 0; first_file < OP_MAX_COUNTERS ; ++first_file) {
		if (samples[first_file] != 0)
			break;
	}

	if (first_file == OP_MAX_COUNTERS) {
		fprintf(stderr, "Can not open any samples files for %s last error %s\n", sample_filename.c_str(), strerror(errno));
		exit(EXIT_FAILURE);
	}

	const struct opd_header * header = samples[first_file]->header();
	mtime = header->mtime;

	/* determine how many counters are possible via the sample file */
	op_cpu cpu = static_cast<op_cpu>(header->cpu_type);
	nr_counters = op_get_cpu_nr_counters(cpu);

	/* check sample files match */
	for (j = first_file + 1; j < OP_MAX_COUNTERS; ++j) {
		if (samples[j] == 0)
			continue;
		samples[first_file]->check_headers(*samples[j]);
	}

	/* sanity check on ctr_um, ctr_event and cpu_type */
	for (i = 0 ; i < OP_MAX_COUNTERS; ++i) {
		if (samples[i] != 0)
			check_event(samples[i]->header());
	}
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
		delete samples[i];
	}
}

/**
 * open_samples_file - ctor helper
 * @param counter the counter number
 * @param can_fail allow to fail gracefully
 *
 * open and mmap the given samples files,
 * the member var samples[counter], header[counter]
 * etc. are updated in case of success.
 * The header is checked but coherence between
 * header can not be sanitized at this point.
 *
 * if can_fail == false all error are fatal.
 */
void opp_samples_files::open_samples_file(u32 counter, bool can_fail)
{
	std::ostringstream filename;
	filename << sample_filename << "#" << counter;
	std::string temp = filename.str();

	if (access(temp.c_str(), R_OK) == 0) {
		samples[counter] = new samples_file_t(temp);
	}
	else {
		if (can_fail == false) {
			/* FIXME: nicer message if e.g. wrong counter */ 
			fprintf(stderr, "oprofpp: Opening %s failed. %s\n", temp.c_str(), strerror(errno));
			exit(EXIT_FAILURE);
		}
	}
}

/**
 * accumulate_samples - lookup samples from a vma address
 * @param counter where to accumulate the samples
 * @param index index of the samples.
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
 * @param counter where to accumulate the samples
 * @param start start index of the samples.
 * @param end end index of the samples.
 *
 * return false if no samples has been found
 */
bool opp_samples_files::accumulate_samples(counter_array_t& counter,
					   uint start, uint end) const
{
	bool found_samples = false;

	for (uint k = 0; k < nr_counters; ++k) {
		if (is_open(k)) {
			counter[k] += samples[k]->count(start, end);
			if (counter[k])
				found_samples = true;
		}
	}

	return found_samples;
}

void opp_samples_files::set_sect_offset(u32 sect_offset)
{
	for (uint k = 0; k < nr_counters; ++k) {
		if (is_open(k)) {
			samples[k]->sect_offset = sect_offset;
		}
	}
}

/**
 * samples_file_t - construct a samples_file_t object
 * @param filename the full path of sample file
 *
 * open and mmap the samples file specified by filename
 * samples file header coherence are checked
 *
 * all error are fatal
 *
 */
samples_file_t::samples_file_t(const std::string & filename)
	:
	sect_offset(0)
{
	db_open(&db_tree, filename.c_str(), DB_RDONLY, sizeof(struct opd_header));
}

/**
 * ~samples_file_t - destroy a samples_file_t object
 *
 * close and unmap the samples file
 *
 */
samples_file_t::~samples_file_t()
{
	if (db_tree.base_memory)
		db_close(&db_tree);
}

/**
 * check_headers - check than the lhs and rhs headers are
 * coherent (same size, same mtime etc.)
 * @param rhs the other samples_file_t
 *
 * all error are fatal
 *
 */
bool samples_file_t::check_headers(const samples_file_t & rhs) const
{
	::check_headers(header(), rhs.header());

	return true;
}

void db_tree_callback(db_key_t, db_value_t value, void * data)
{
	u32 * count = (u32 *)data;

	*count += value;
}

/**
 * count - return the number of samples in given range
 * @param start start samples nr of range
 * @param end end samples br of range
 *
 * return the number of samples in the the range [start, end]
 * no range checking is performed.
 *
 * This actually code duplicate partially accumulate member of
 * opp_samples_files which in future must use this as it internal
 * implementation
 */
u32 samples_file_t::count(uint start, uint end) const
{
	u32 count = 0;

	db_travel(&db_tree, start - sect_offset, end - sect_offset,
		  db_tree_callback, &count);

	return count;
}
