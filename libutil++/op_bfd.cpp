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
#include "op_config.h"

#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>

#include <cerrno>
#include <cstring>
#include <cstdlib>

#include <algorithm>
#include <iostream>
#include <fstream>
#include <iomanip>
#include <sstream>

#include "op_exception.h"
#include "op_bfd.h"
#include "string_filter.h"
#include "stream_util.h"
#include "cverb.h"
#include "op_fileio.h"

using namespace std;

namespace {

verbose vbfd("bfd");

void check_format(string const & file, bfd ** ibfd)
{
	if (!bfd_check_format_matches(*ibfd, bfd_object, NULL)) {
		cverb << vbfd << "BFD format failure for " << file << endl;
		bfd_close(*ibfd);
		*ibfd = NULL;
	}
}


bfd * open_bfd(string const & file)
{
	/* bfd keeps its own reference to the filename char *,
	 * so it must have a lifetime longer than the ibfd */
	bfd * ibfd = bfd_openr(file.c_str(), NULL);
	if (!ibfd) {
		cverb << vbfd << "bfd_openr failed for " << file << endl;
		return NULL;
	}

	check_format(file, &ibfd);

	return ibfd;
}


bfd * fdopen_bfd(string const & file, int fd)
{
	/* bfd keeps its own reference to the filename char *,
	 * so it must have a lifetime longer than the ibfd */
	bfd * ibfd = bfd_fdopenr(file.c_str(), NULL, fd);
	if (!ibfd) {
		cverb << vbfd << "bfd_openr failed for " << file << endl;
		return NULL;
	}

	check_format(file, &ibfd);

	return ibfd;
}


bool
separate_debug_file_exists(string const & name, 
                           unsigned long const crc)
{
	unsigned long file_crc = 0;
	// The size of 8*1024 element for the buffer is arbitrary.
	char buffer[8*1024];
	
	ifstream file(name.c_str());
	if (!file)
		return false;

	cverb << vbfd << "found " << name;
	while (file) {
		file.read(buffer, sizeof(buffer));
		file_crc = calc_crc32(file_crc, 
				      reinterpret_cast<unsigned char *>(&buffer[0]),
				      file.gcount());
	}
	cverb << vbfd << " with crc32 = " << hex << file_crc << endl;
	return crc == file_crc;
}


bool
get_debug_link_info(bfd * ibfd, 
                    string & filename,
                    unsigned long & crc32)
{
	asection * sect;

	cverb << vbfd << "fetching .gnu_debuglink section" << endl;
	sect = bfd_get_section_by_name(ibfd, ".gnu_debuglink");
	
	if (sect == NULL)
		return false;
	
	bfd_size_type debuglink_size = bfd_section_size(ibfd, sect);  
	char contents[debuglink_size];
	cverb << vbfd
	      << ".gnu_debuglink section has size " << debuglink_size << endl;
	
	bfd_get_section_contents(ibfd, sect, 
				 reinterpret_cast<unsigned char *>(contents), 
				 static_cast<file_ptr>(0), debuglink_size);
	
	/* CRC value is stored after the filename, aligned up to 4 bytes. */
	size_t filename_len = strlen(contents);
	size_t crc_offset = filename_len + 1;
	crc_offset = (crc_offset + 3) & ~3;
	
	crc32 = bfd_get_32(ibfd, 
			       reinterpret_cast<bfd_byte *>(contents + crc_offset));
	filename = string(contents, filename_len);
	cverb << vbfd << ".gnu_debuglink filename is " << filename << endl;
	return true;
}

} // namespace anon


bool
find_separate_debug_file(bfd * ibfd, 
                         string const & dir_in,
                         string const & global_in,
                         string & filename)
{
	string dir(dir_in);
	string global(global_in);
	string basename;
	unsigned long crc32;
	
	if (!get_debug_link_info(ibfd, basename, crc32))
		return false;
	
	if (dir.size() > 0 && dir.at(dir.size() - 1) != '/')
		dir += '/';
	
	if (global.size() > 0 && global.at(global.size() - 1) != '/')
		global += '/';

	cverb << vbfd << "looking for debugging file " << basename 
	      << " with crc32 = " << hex << crc32 << endl;
	
	string first_try(dir + basename);
	string second_try(dir + ".debug/" + basename);

	if (dir.size() > 0 && dir[0] == '/')
		dir = dir.substr(1);

	string third_try(global + dir + basename);
	
	if (separate_debug_file_exists(first_try, crc32)) 
		filename = first_try; 
	else if (separate_debug_file_exists(second_try, crc32))
		filename = second_try;
	else if (separate_debug_file_exists(third_try, crc32))
		filename = third_try;
	else
		return false;
	
	return true;
}


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
	dbfd(0),
	text_offset(0),
	debug_info(false)
{
	int fd;
	struct stat st;
	// after creating all symbol it's convenient for user code to access
	// symbols through a vector. We use an intermediate list to avoid a
	// O(N²) behavior when we will filter vector element below
	symbols_found_t symbols;
	asection const * sect;

	// if there's a problem already, don't try to open it
	if (!ok)
		goto out_fail;

	fd = open(filename.c_str(), O_RDONLY);
	if (fd == -1) {
		cverb << vbfd << "open failed for " << filename << endl;
		ok = false;
		goto out_fail;
	}

	if (fstat(fd, &st)) {
		cverb << vbfd << "stat failed for " << filename << endl;
		ok = false;
		goto out_fail;
	}

	file_size = st.st_size;

	ibfd = fdopen_bfd(filename, fd);

	if (!ibfd) {
		cverb << vbfd << "fdopen_bfd failed for " << filename << endl;
		ok = false;
		goto out_fail;
	}

	// find the first text section as use that as text_offset
	for (sect = ibfd->sections; sect; sect = sect->next) {
		if (sect->flags & SEC_CODE) {
			text_offset = sect->filepos;
			io_state state(cverb << vbfd);
			cverb << vbfd << sect->name << " filepos "
				<< hex << text_offset << endl;
			break;
		}
	}

	for (sect = ibfd->sections; sect; sect = sect->next) {
		if (sect->flags & SEC_DEBUGGING) {
			debug_info = true;
			break;
		}
	}

	// if no debugging section check to see if there is an .debug file
	if (!debug_info) {
		string global(DEBUGDIR);
		string dirname(filename.substr(0, filename.rfind('/')));
		if (find_separate_debug_file (ibfd, dirname, global,
					      debug_filename)) {
			cverb << vbfd
			      << "now loading: " << debug_filename << endl;
			dbfd = open_bfd(debug_filename);
			if (dbfd) {
				for (sect = dbfd->sections; sect; 
				     sect = sect->next) {
					if (sect->flags & SEC_DEBUGGING) {
						debug_info = true;
						break;
					}
				}
			} else {
				// .debug is optional, so will not fail if
				// problem opening file.
				cverb << vbfd << "unable to open: "
				      << debug_filename << endl;
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
	if (dbfd)
		bfd_close(dbfd);
	dbfd = NULL;
	// make the fake symbol fit within the fake file
	file_size = -1;
	goto out;
}


op_bfd::~op_bfd()
{
	if (ibfd)
		bfd_close(ibfd);
	if (dbfd)
		bfd_close(dbfd);
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

void op_bfd::get_symbols_from_file(bfd * ibfd, size_t start,
   op_bfd::symbols_found_t & symbols, bool debug_file)
{
	uint nr_all_syms;

	nr_all_syms = bfd_canonicalize_symtab(ibfd, bfd_syms.get()+start);
	if (nr_all_syms < 1)
		return;

	for (symbol_index_t i = start; i < start+nr_all_syms; i++) {
		if (interesting_symbol(bfd_syms[i])) {
			// need to use filepos of original file for debug
			// file symbs
			if (debug_file)
				// FIXME: this is not enough, we must get the
				// offset where this symbol live in the
				// original file.
				bfd_syms[i]->section->filepos = text_offset;
			symbols.push_back(op_bfd_symbol(bfd_syms[i]));
		}
	}

}


void op_bfd::get_symbols(op_bfd::symbols_found_t & symbols)
{
	size_t size;
	size_t size_binary = 0;
	size_t size_debug = 0;

	if (bfd_get_file_flags(ibfd) & HAS_SYMS)
		size_binary = bfd_get_symtab_upper_bound(ibfd);

	if (dbfd && (bfd_get_file_flags(dbfd) & HAS_SYMS))
		size_debug += bfd_get_symtab_upper_bound(dbfd);

	size = size_binary + size_debug;

	/* HAS_SYMS can be set with no symbols */
	if (size < 1)
		return;

	bfd_syms.reset(new asymbol*[size]);

	if (size_binary > 0)
		get_symbols_from_file(ibfd, 0, symbols, false);

	if (size_debug > 0)
		get_symbols_from_file(dbfd, size_binary, symbols, true);

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

	cverb << vbfd << "number of symbols before filtering "
	      << symbols.size() << endl;

	symbols_found_t::iterator it;
	it = remove_if(symbols.begin(), symbols.end(),
	               remove_filter(symbol_filter));
	symbols.erase(it, symbols.end());

	copy(symbols.begin(), symbols.end(), back_inserter(syms));

	cverb << vbfd << "number of symbols now " << syms.size() << endl;
}


unsigned long op_bfd::sym_offset(symbol_index_t sym_index, u32 num) const
{
	/* take off section offset and symb value */
	return num - syms[sym_index].filepos();
}


bfd_vma op_bfd::offset_to_pc(bfd_vma offset) const
{
	asection const * sect = ibfd->sections;

	for (; sect; sect = sect->next) {
		if (offset >= bfd_vma(sect->filepos) &&
		    (!sect->next || offset < bfd_vma(sect->next->filepos))) {
			return sect->vma + (offset - sect->filepos);
		}
	}

	return 0;
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

	// FIXME: to test, I'm unsure if from this point we must use abfd
	// or the check if (pc >= bfd_section_size(abfd, section)) must be done
	// with ibfd.
	bfd * abfd = dbfd ? dbfd : ibfd;

	if (pc >= bfd_section_size(abfd, section))
		return false;

	bool ret = bfd_find_nearest_line(abfd, section, bfd_syms.get(), pc,
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
		size_t section_size = bfd_section_size(abfd, section);
		if (pc + max_search > section_size)
			max_search = section_size - pc;

		for (size_t i = 1 ; i < max_search ; ++i) {
			bool ret = bfd_find_nearest_line(abfd, section,
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
		bfd_find_nearest_line(abfd, section, bfd_syms.get(), pc,
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
	unsigned long start = sym.filepos();
	unsigned long end = next ? next->filepos() : file_size;

	return end - start;
}


void op_bfd::get_symbol_range(symbol_index_t sym_idx,
			      unsigned long & start, unsigned long & end) const
{
	op_bfd_symbol const & sym = syms[sym_idx];

	io_state state(cverb << (vbfd&vlevel1));

	cverb << (vbfd&vlevel1) << "symbol " << sym.name()
	      << ", value " << hex << sym.value() << endl;

	start = sym.filepos();
	if (sym.symbol()) {
		cverb << (vbfd&vlevel1) << "in section "
		      << sym.symbol()->section->name << ", filepos "
		      << hex << sym.symbol()->section->filepos << endl;
	}

	end = start + syms[sym_idx].size();
	cverb << (vbfd & vlevel1)
	      << "start " << hex << start << ", end " << end << endl;

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
