/**
 * @file op_bfd.cpp
 * Encapsulation of bfd objects
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author Philippe Elie
 * @author John Levon
 */

#include "op_file.h"

#include <cerrno>
#include <cstring>
#include <cstdlib>

#include <algorithm>
#include <iostream>
#include <iomanip>
#include <sstream>

#include "op_exception.h"
#include "op_bfd.h"
#include "string_filter.h"
#include "stream_util.h"
#include "cverb.h"

using namespace std;

op_bfd_symbol::op_bfd_symbol(asymbol const * a)
	: bfd_symbol(a), symb_value(a->value),
	  section_filepos(a->section->filepos),
	  section_vma(a->section->vma),
	  symb_size(0)
{
	// Some sections have unnamed symbols in them. If
	// we just ignore them then we end up sticking
	// things like .plt hits inside of _init. So instead
	// we name the symbol after the section.
	if (a->name && a->name[0] != '\0') {
		symb_name = a->name;
	} else {
		symb_name = string("??") + a->section->name;
	}
}


op_bfd_symbol::op_bfd_symbol(bfd_vma vma, size_t size, string const & name)
	: bfd_symbol(0), symb_value(vma),
	  section_filepos(0), section_vma(0),
	  symb_size(size), symb_name(name)
{
}


op_bfd::op_bfd(string const & fname, string_filter const & symbol_filter,
               bool & ok)
	:
	filename(fname),
	file_size(-1),
	ibfd(0),
	text_offset(0),
	debug_info(false)
{
	// after creating all symbol it's convenient for user code to access
	// symbols through a vector. We use an intermediate list to avoid a
	// O(N²) behavior when we will filter vector element below
	symbols_found_t symbols;

	// if there's a problem already, don't try to open it
	if (!ok)
		goto out_fail;

	op_get_fsize(filename.c_str(), &file_size);

	/* bfd keeps its own reference to the filename char *,
	 * so it must have a lifetime longer than the ibfd */
	ibfd = bfd_openr(filename.c_str(), NULL);

	if (!ibfd) {
		cverb << "bfd_openr failed for " << filename << endl;
		ok = false;
		goto out_fail;
	}

	{

	char ** matching;

	if (!bfd_check_format_matches(ibfd, bfd_object, &matching)) {
		cverb << "BFD format failure for " << filename << endl;
		ok = false;
		goto out_fail;
	}

	asection const * sect = bfd_get_section_by_name(ibfd, ".text");
	if (sect) {
		text_offset = sect->filepos;
		io_state state(cverb);
		cverb << ".text filepos " << hex << text_offset << endl;
	}

	for (sect = ibfd->sections; sect; sect = sect->next) {
		if (sect->flags & SEC_DEBUGGING) {
			debug_info = true;
			break;
		}
	}

	}

	get_symbols(symbols);

out:
	add_symbols(symbols, symbol_filter);
	return;
out_fail:
	if (ibfd)
		bfd_close(ibfd);
	ibfd = NULL;
	// make the fake symbol fit within the fake file
	file_size = -1;
	goto out;
}


op_bfd::~op_bfd()
{
	if (ibfd)
		bfd_close(ibfd);
}


bool op_bfd_symbol::operator<(op_bfd_symbol const & rhs) const
{
	return filepos() < rhs.filepos();
}

namespace {

/**
 * Return true if the symbol is worth looking at
 */
bool interesting_symbol(asymbol * sym)
{
	// #717720 some binutils are miscompiled by gcc 2.95, one of the
	// typical symptom can be catched here.
	if (!sym->section) {
		ostringstream os;
		os << "Your version of binutils seems to have a bug.\n"
		   << "Read http://oprofile.sf.net/faq/#binutilsbug\n";
		throw op_runtime_error(os.str());
	}

	if (!(sym->section->flags & SEC_CODE))
		return false;

	// returning true for fix up in op_bfd_symbol()
	if (!sym->name || sym->name[0] == '\0')
		return true;

	// C++ exception stuff
	if (sym->name[0] == '.' && sym->name[1] == 'L')
		return false;

	/* This case cannot be moved to boring_symbol(),
	 * because that's only used for duplicate VMAs,
	 * and sometimes this symbol appears at an address
	 * different from all other symbols.
	 */
	if (!strcmp("gcc2_compiled.", sym->name))
		return false;

	return true;
}

/**
 * return true if the symbol is boring, boring symbol are eliminated
 * when multiple symbol exist at the same vma
 */
bool boring_symbol(string const & name)
{
	// FIXME: check if we can't do a better job, this heuristic fix all
	// case I'm aware
	if (name == "Letext")
		return true;

	if (name.substr(0, 2) == "??")
		return true;

	return false;
}


/// function object for filtering symbols to remove
struct remove_filter {
	remove_filter(string_filter const & filter)
		: filter_(filter) {}

	bool operator()(op_bfd_symbol const & symbol) {
		return !filter_.match(symbol.name());
	}

	string_filter filter_;
};


} // namespace anon


void op_bfd::get_symbols(op_bfd::symbols_found_t & symbols)
{
	uint nr_all_syms;
	size_t size;

	if (!(bfd_get_file_flags(ibfd) & HAS_SYMS))
		return;

	size = bfd_get_symtab_upper_bound(ibfd);

	/* HAS_SYMS can be set with no symbols */
	if (size < 1)
		return;

	bfd_syms.reset(new asymbol*[size]);
	nr_all_syms = bfd_canonicalize_symtab(ibfd, bfd_syms.get());
	if (nr_all_syms < 1)
		return;

	for (symbol_index_t i = 0; i < nr_all_syms; i++) {
		if (interesting_symbol(bfd_syms[i])) {
			symbols.push_back(op_bfd_symbol(bfd_syms[i]));
		}
	}

	symbols.sort();

	symbols_found_t::iterator it = symbols.begin();

	// we need to ensure than for a given vma only one symbol exist else
	// we read more than one time some samples. Fix #526098
	for (; it != symbols.end();) {
		symbols_found_t::iterator temp = it;
		++temp;
		if (temp != symbols.end() && (it->vma() == temp->vma())) {
			if (boring_symbol(it->name())) {
				it = symbols.erase(it);
			} else {
				symbols.erase(temp);
			}
		} else {
			++it;
		}
	}

	// now we can calculate the symbol size, we can't first include/exclude
	// symbols because the size of symbol is calculated from the difference
	// between the vma of a symbol and the next one.
	for (it = symbols.begin() ; it != symbols.end(); ++it) {
		op_bfd_symbol const * next = 0;
		symbols_found_t::iterator temp = it;
		++temp;
		if (temp != symbols.end())
			next = &*temp;
		it->size(symbol_size(*it, next));
	}
}


void op_bfd::add_symbols(op_bfd::symbols_found_t & symbols,
                         string_filter const & symbol_filter)
{
	// images with no symbols debug info available get a placeholder symbol
	if (symbols.empty())
		symbols.push_back(create_artificial_symbol());

	cverb << "number of symbols before filtering " << symbols.size() << endl;

	symbols_found_t::iterator it;
	it = remove_if(symbols.begin(), symbols.end(), remove_filter(symbol_filter));
	symbols.erase(it, symbols.end());

	copy(symbols.begin(), symbols.end(), back_inserter(syms));

	cverb << "number of symbols now " << syms.size() << endl;
}


u32 op_bfd::sym_offset(symbol_index_t sym_index, u32 num) const
{
	/* take off section offset and symb value */
	return num - syms[sym_index].filepos();
}


bool op_bfd::get_linenr(symbol_index_t sym_idx, unsigned int offset,
			string & source_filename, unsigned int & linenr) const
{
	linenr = 0;

	if (!debug_info)
		return false;

	char const * functionname;
	char const * cfilename = "";
	bfd_vma pc;

	op_bfd_symbol const & sym = syms[sym_idx];

	// take care about artificial symbol
	if (sym.symbol() == 0)
		return false;

	asection* section = sym.symbol()->section;

	if ((bfd_get_section_flags (ibfd, section) & SEC_ALLOC) == 0)
		return false;

	pc = sym_offset(sym_idx, offset) + sym.value();

	if (pc >= bfd_section_size(ibfd, section))
		return false;

	bool ret = bfd_find_nearest_line(ibfd, section, bfd_syms.get(), pc,
					 &cfilename, &functionname, &linenr);

	if (cfilename == 0 || !ret) {
		cfilename = "";
		linenr = 0;
		ret = false;
	}

	// functionname and symbol name can be different if we accept it we
	// can get samples for the wrong symbol (#484660)
	// Note this break static inline function, since for these functions we
	// get a different symbol name than symbol name but we recover later.
	if (ret && functionname && sym.name() != string(functionname)) {
		// gcc doesn't emit mangled name for C++ static function so we
		// try to recover by accepting this linenr info if functionname
		// is a substring of sym.name, this is not a bug see gcc
		// bugzilla #11774. Check agaisnt the filename part of the
		// is error prone error (e.g. namespace A { static int f1(); })
		// so we check only for a substring and warn the user.
		static bool warned = false;
		if (!warned) {
			// FIXME: enough precise message ? We will get this
			// message for static C++ function too, must we
			// warn only if the following check fails ?
			cerr << "warning: \"" << get_filename() << "\" some "
			     << "functions compiled without debug information "
			     << "may have incorrect source line attributions"
			     << endl;
			warned = true;
		}
		if (sym.name().find(functionname) == string::npos)
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
		// find a samples files with samples in epilog and try oreport
		// -l -g on it, check it also with opannotate.

		// first restrict the search on a sensible range of vma,
		// 16 is an intuitive value based on epilog code look
		size_t max_search = 16;
		size_t section_size = bfd_section_size(ibfd, section);
		if (pc + max_search > section_size)
			max_search = section_size - pc;

		for (size_t i = 1 ; i < max_search ; ++i) {
			bool ret = bfd_find_nearest_line(ibfd, section,
							 bfd_syms.get(), pc+i,
							 &cfilename,
							 &functionname,
							 &linenr);

			if (ret && linenr != 0
				&& sym.name() == string(functionname)) {
				return true;
			}
		}

		// We lose it's pointless to try more.

		// bfd_find_nearest_line clobber the memory pointed by filename
		// from a previous call when the filename change across
		// multiple calls. The more easy way to recover is to reissue
		// the first call, we don't need to recheck return value, we
		// know that the call will succeed.
		// As mentionned above a previous work-around break static
		// inline function. We recover here by not checking than
		// functionname == sym.name
		bfd_find_nearest_line(ibfd, section, bfd_syms.get(), pc,
				      &cfilename, &functionname, &linenr);
	}

	if (cfilename) {
		source_filename = cfilename;
	} else {
		source_filename = "";
		linenr = 0;
	}

	return ret;
}


size_t op_bfd::symbol_size(op_bfd_symbol const & sym,
			   op_bfd_symbol const * next) const
{
	u32 start = sym.filepos();
	u32 end = next ? next->filepos() : file_size;

	return end - start;
}


void op_bfd::get_symbol_range(symbol_index_t sym_idx,
			      u32 & start, u32 & end) const
{
	op_bfd_symbol const & sym = syms[sym_idx];

	io_state state(cverb);

	cverb << "symbol " << sym.name() << ", value " << hex << sym.value() << endl;

	start = sym.filepos();
	if (sym.symbol()) {
		cverb << "in section " << sym.symbol()->section->name
			<< ", filepos " << hex << sym.symbol()->section->filepos << endl;
	}

	end = start + syms[sym_idx].size();
	cverb << "start " << hex << start << ", end " << end << endl;

	if (start >= file_size + text_offset) {
		ostringstream os;
		os << "start " << hex << start
		   << " out of range (max " << file_size << ")\n";
		throw op_runtime_error(os.str());
	}

	if (end > file_size + text_offset) {
		ostringstream os;
		os << "end " << hex << end
		   << " out of range (max " << file_size << ")\n";
		throw op_runtime_error(os.str());
	}

	if (start > end) {
		ostringstream os;
		os << "start " << hex << start
		   << " is more than end" << end << endl;
		throw op_runtime_error(os.str());
	}
}


void op_bfd::get_vma_range(bfd_vma & start, bfd_vma & end) const
{
	if (!syms.empty()) {
		// syms are sorted by vma so vma of the first symbol and vma +
		// size of the last symbol give the vma range for gprof output
		op_bfd_symbol const & last_symb = syms[syms.size() - 1];
		start = syms[0].vma();
		// end is excluded from range so + 1 *if* last_symb.size() != 0
		end = last_symb.vma() + last_symb.size() + (last_symb.size() != 0);
	} else {
		start = 0;
		end = file_size;
	}
}


op_bfd_symbol const op_bfd::create_artificial_symbol()
{
	// FIXME: prefer a bool artificial; to this ??
	string symname = "?";

	symname += get_filename();

	bfd_vma start, end;
	get_vma_range(start, end);
	return op_bfd_symbol(start, end - start, symname);
}


string op_bfd::get_filename() const
{
	return filename;
}


size_t op_bfd::bfd_arch_bits_per_address() const
{
	if (ibfd)
		return ::bfd_arch_bits_per_address(ibfd);
	// FIXME: this function should be called only if the underlined ibfd
	// is ok, must we throw ?
	return sizeof(bfd_vma);
}
