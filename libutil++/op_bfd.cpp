/**
 * @file op_bfd.cpp
 * Encapsulation of bfd objects
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author Philippe Elie <phil_el@wanadoo.fr>
 * @author John Levon <moz@compsoc.man.ac.uk>
 */

#include "op_file.h"

#include <algorithm>
#include <iostream>
#include <iomanip>
#include <string>

#include <cstdlib>

#include "op_bfd.h"

using std::find;
using std::vector;
using std::string;
using std::cout;
using std::cerr;
using std::hex;
using std::dec;
using std::endl;

extern std::ostream cverb;
 
op_bfd::op_bfd(string const & filename, vector<string> exclude_symbols)
	:
	file_size(0),
	ibfd(0),
	bfd_syms(0),
	text_offset(0)
{
	if (filename.empty()) {
		cerr << "op_bfd() empty image filename." << endl;
		exit(EXIT_FAILURE);
	}

	op_get_fsize(filename.c_str(), &file_size);

	ibfd = bfd_openr(filename.c_str(), NULL);

	if (!ibfd) {
		cerr << "bfd_openr of " << filename << " failed." << endl;
		exit(EXIT_FAILURE);
	}

	char ** matching;

	if (!bfd_check_format_matches(ibfd, bfd_object, &matching)) {
		cerr << "BFD format failure for " << filename << endl;
		exit(EXIT_FAILURE);
	}

	/* Kernel / kernel modules are calculated as offsets against
	 * the .text section, so they need special handling
	 */
	asection * sect = bfd_get_section_by_name(ibfd, ".text");
	if (sect) {
		text_offset = sect->filepos;
		cverb << ".text filepos " << hex << text_offset << endl;
	}

	get_symbols(exclude_symbols);

	if (syms.size() == 0) {
		u32 start, end;
		get_vma_range(start, end);
		create_artificial_symbol(start, end);
	}
}


op_bfd::~op_bfd()
{
	bfd_close(ibfd);
	delete [] bfd_syms;
}


/**
 * symcomp - comparator
 *
 */
static bool symcomp(op_bfd_symbol const & a, op_bfd_symbol const & b)
{
	return a.vma() < b.vma();
}

namespace {

// only add symbols that would /never/ be
// worth examining
static char const * boring_symbols[] = {
	"gcc2_compiled.",
	"_init"
};

static size_t const nr_boring_symbols =
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
 * get_symbols - op_bfd ctor helper
 *
 * Parse and sort in ascending order all symbols
 * in the file pointed to by abfd that reside in
 * a %SEC_CODE section. Returns true if symbol(s)
 * are found. The symbols are filtered through
 * the interesting_symbol() predicate and sorted
 * with the symcomp() comparator.
 */
bool op_bfd::get_symbols(vector<string> excluded)
{
	uint nr_all_syms;
	symbol_index_t i;
	size_t size;

	if (!(bfd_get_file_flags(ibfd) & HAS_SYMS))
		return false;

	size = bfd_get_symtab_upper_bound(ibfd);

	/* HAS_SYMS can be set with no symbols */
	if (size < 1)
		return false;

	bfd_syms = new asymbol * [size];
	nr_all_syms = bfd_canonicalize_symtab(ibfd, bfd_syms);
	if (nr_all_syms < 1)
		return false;

	for (i = 0; i < nr_all_syms; i++) {
		if (interesting_symbol(bfd_syms[i])) {
			// we can't fill the size member for now, because in
			// some case it is calculated from the vma of the
			// next symbol
			asymbol const * symbol = bfd_syms[i];
			op_bfd_symbol symb(symbol,
				symbol->value,
				symbol->section->filepos,
				symbol->section->vma,
				0,
				symbol->name);
			syms.push_back(symb);
		}
	}

	std::stable_sort(syms.begin(), syms.end(), symcomp);

	// now we can calculate the symbol size
	for (i = 0 ; i < syms.size() ; ++i) {
		syms[i].size(symbol_size(i));
	}

	// we need to ensure than for a given vma only one symbol exist else
	// we read more than one time some samples. Fix #526098
	// ELF symbols size : potential bogosity here because when using
	// elf symbol size we need to check than two symbols does not overlap.
	for (i =  1 ; i < syms.size() ; ++i) {
		if (syms[i].vma() == syms[i-1].vma()) {
			// TODO: choose more carefully the symbol we drop.
			// If once have FUNCTION flag and not the other keep
			// it etc.
			syms.erase(syms.begin() + i);
			i--;
		}
	}

	cverb << "number of symbols before excluding " << dec << syms.size() << endl;

	// it's time to remove the excluded symbols
	for (i = 0 ; i < syms.size() ; ) {
		vector<string>::const_iterator it =
			find(excluded.begin(), excluded.end(), syms[i].name());
		if (it != excluded.end()) {
			cout << "excluding symbol " << syms[i].name() << endl;
			syms.erase(syms.begin() + i);
		} else {
			++i;
		}
	}

	cverb << "number of symbols now " << dec << syms.size() << endl;

	if (syms.empty())
		return false;

	return true;
}


u32 op_bfd::sym_offset(symbol_index_t sym_index, u32 num) const
{
	/* take off section offset and symb value */
	return num - syms[sym_index].filepos();
}

bool op_bfd::have_debug_info() const
{
	sec* section;
	for (section = ibfd->sections; section; section = section->next)
		if (section->flags & SEC_DEBUGGING)
			break;

	return section != NULL;
}

bool op_bfd::get_linenr(symbol_index_t sym_idx, uint offset,
			string & filename, unsigned int & linenr) const
{
	char const * functionname;
	bfd_vma pc;

	char const * cfilename = "";
	linenr = 0;

	// take care about artificial symbol
	if (syms[sym_idx].symbol() == 0)
		return false;

	asection* section = syms[sym_idx].symbol()->section;

	if ((bfd_get_section_flags (ibfd, section) & SEC_ALLOC) == 0)
		return false;

	pc = sym_offset(sym_idx, offset) + syms[sym_idx].value();

	if (pc >= bfd_section_size(ibfd, section))
		return false;

	bool ret = bfd_find_nearest_line(ibfd, section, bfd_syms, pc,
					 &cfilename, &functionname, &linenr);

	if (cfilename == 0 || ret == false) {
		cfilename = "";
		linenr = 0;
		ret = false;
	}

	// functioname and symbol name can be different if we query linenr info
	// if we accept it we can get samples for the wrong symbol (#484660)
	if (ret == true && functionname
		&& syms[sym_idx].name() != string(functionname)) {
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
	if (linenr == 0) {
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
							 &cfilename,
							 &functionname,
							 &linenr);

			if (ret && linenr != 0
				&& syms[sym_idx].name() == string(functionname)) {
				return true;	// we win
			}
		}

		// We lose it's worthwhile to try more.

		// bfd_find_nearest_line clobber the memory pointed by filename
		// from a previous call when the filename change across
		// multiple calls. The more easy way to recover is to reissue
		// the first call, we don't need to recheck return value, we
		// know that the call will succeed.
		bfd_find_nearest_line(ibfd, section, bfd_syms, pc,
				      &cfilename, &functionname, &linenr);
	}

	filename = (cfilename) ? cfilename : "";

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

size_t op_bfd::symbol_size(symbol_index_t sym_idx) const
{
	op_bfd_symbol const * next;
	op_bfd_symbol const & sym = syms[sym_idx];

	next = (sym_idx == syms.size() - 1) ? NULL : &syms[sym_idx + 1];

	u32 start = sym.filepos();
	size_t length;

#ifndef USE_ELF_INTERNAL
	u32 end;
	if (next) {
		end = next->filepos();
	} else
		end = file_size;

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
			next_offset = next->filepos();
		} else {
			next_offset = file_size;
		}
		length = next_offset - start;
	}
#endif /* USE_ELF_INTERNAL */

	return length;
}

void op_bfd::get_symbol_range(symbol_index_t sym_idx,
			      u32 & start, u32 & end) const
{
	op_bfd_symbol const & sym = syms[sym_idx];

	cverb << "symbol " << sym.name() << ", value " << hex << sym.value() << endl;

	start = sym.filepos();
	if (sym.symbol()) {
		cverb << "in section " << sym.symbol()->section->name
			<< ", filepos " << hex << sym.symbol()->section->filepos << endl;
	}

	end = start + syms[sym_idx].size();
	cverb << "start " << hex << start << ", end " << end << endl;

	if (start >= file_size + text_offset) {
		cerr << "start " << hex << start
			<< " out of range (max " << file_size << ")" << endl;
		exit(EXIT_FAILURE);
	}

	if (end > file_size + text_offset) {
		cerr << "end " << hex << end
			<< " out of range (max " << file_size << ")" << endl;
		exit(EXIT_FAILURE);
	}

	if (start > end) {
		cerr << "start " << hex << start
			<< " is more than end " << end << endl;
		exit(EXIT_FAILURE);
	}
}

void op_bfd::get_vma_range(u32 & start, u32 & end) const
{
	if (syms.size()) {
		// syms are sorted by vma so vma of the first symbol and vma +
		// size of the last symbol give the vma range for gprof output
		const op_bfd_symbol & last_symb = syms[syms.size() - 1];
		start = syms[0].vma();
		end = last_symb.vma() + last_symb.size();
	} else {
		start = 0;
		end = file_size;
	}
}

void op_bfd::create_artificial_symbol(u32 start, u32 end)
{
	// FIXME: prefer a bool artificial; to this ??
	string symname = "?";

	symname += get_filename();

	op_bfd_symbol symbol(0, 0, start, 0, end - start, symname);

	syms.push_back(symbol);
}

string op_bfd::get_filename() const
{
	return bfd_get_filename(ibfd);
}
