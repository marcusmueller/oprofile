/**
 * @file op_bfd.h
 * Encapsulation of bfd objects
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author Philippe Elie
 * @author John Levon
 */

#ifndef OP_BFD_H
#define OP_BFD_H

#include "config.h"

#include <bfd.h>

#include <vector>
#include <string>
#include <list>

#include "utility.h"
#include "op_types.h"

class op_bfd;
class string_filter;

/// all symbol vector indexing uses this type
typedef size_t symbol_index_t;

/**
 * A symbol description from a bfd point of view. This duplicate
 * information pointed by an asymbol, we need this duplication in case
 * the symbol is an artificial symbol
 */
class op_bfd_symbol {
public:

	/// ctor for real symbols
	op_bfd_symbol(asymbol const * a);

	/// ctor for artificial symbols
	op_bfd_symbol(bfd_vma vma, size_t size, std::string const & name);

	bfd_vma vma() const { return symb_value + section_vma; }
	u32 value() const { return symb_value; }
	u32 filepos() const { return symb_value + section_filepos; }
	std::string const & name() const { return symb_name; }
	asymbol const * symbol() const { return bfd_symbol; }
	size_t size() const { return symb_size; }
	void size(size_t s) { symb_size = s; }

	/// compare two symbols by their filepos()
	bool operator<(op_bfd_symbol const & lhs) const;

private:
	/// the original bfd symbol, this can be null if the symbol is an
	/// artificial symbol
	asymbol const * bfd_symbol;
	/// the offset of this symbol relative to the begin of the section's
	/// symbol
	u32 symb_value;
	/// the section filepos for this symbol
	u32 section_filepos;
	/// the section vma for this symbol
	bfd_vma section_vma;
	/// the size of this symbol
	size_t symb_size;
	/// the name of the symbol
	std::string symb_name;
};

/**
 * Encapsulation of a bfd object. Simplifies open/close of bfd, enumerating
 * symbols and retrieving informations for symbols or vma.
 *
 * Use of this class relies on a std::ostream cverb
 */
class op_bfd {
public:
	/**
	 * @param filename  the name of the image file
	 * @param symbol_filter  filter to apply to symbols
	 * @param create_fake  Don't try to bfd_openr the file, only create
	 *  a fake op_bfd
	 *
	 * All errors are fatal.
	 */
	op_bfd(std::string const & filename,
	       string_filter const & symbol_filter,
	       bool create_fake);

	/// close an opened bfd image and free all related resources
	~op_bfd();

	/**
	 * @param sym_idx index of the symbol
	 * @param offset fentry number
	 * @param filename output parameter to store filename
	 * @param linenr output parameter to store linenr.
	 *
	 * Retrieve the relevant finename:linenr information for the sym_idx
	 * at offset. If the lookup fails, return false. In some cases this
	 * function can retrieve the filename and return true but fail to
	 * retrieve the linenr and so can return zero in linenr
	 */
	bool get_linenr(symbol_index_t sym_idx, unsigned int offset,
			std::string & filename, unsigned int & linenr) const;

	/**
	 * @param sym_idx symbol index
	 * @param start reference to start var
	 * @param end reference to end var
	 *
	 * Calculates the range of sample file entries covered by sym. start
	 * and end will be filled in appropriately. If index is the last entry
	 * in symbol table, all entries up to the end of the sample file will
	 * be used.  After calculating start and end they are sanitized
	 *
	 * All errors are fatal.
	 */
	void get_symbol_range(symbol_index_t sym_idx,
			      u32 & start, u32 & end) const;

	/**
	 * sym_offset - return offset from a symbol's start
	 * @param num_symbols symbol number
	 * @param num number of fentry
	 *
	 * Returns the offset of a sample at position num
	 * in the samples file from the start of symbol sym_idx.
	 */
	u32 sym_offset(symbol_index_t num_symbols, u32 num) const;

	/**
	 * @param start reference to the start vma
	 * @param end reference to the end vma
	 *
	 * return in start, end the vma range for this binary object.
	 */
	void get_vma_range(bfd_vma & start, bfd_vma & end) const;

	/** return the text section filepos. */
	u32 const get_start_offset() const { return text_offset; }

	/// return the image name of the underlying binary image
	std::string get_filename() const;

	/// sorted vector by vma of interesting symbol.
	std::vector<op_bfd_symbol> syms;

	/// return in bits the bfd_vma size for this binary. This is needed
	/// because gprof output depend on the bfd_vma for *this* binary
	/// and do not depend on sizeof(bfd_vma)
	size_t bfd_arch_bits_per_address() const;

private:
	/// filename we open
	std::string filename;

	/// file size in bytes
	off_t file_size;

	// the bfd object.
	bfd * ibfd;

	// vector of symbol filled by the bfd lib.
	scoped_array<asymbol*> bfd_syms;
	// image file such the linux kernel need than all vma are offset
	// by this value.
	u32 text_offset;

	/// true if at least one section has (flags & SEC_DEBUGGING) != 0
	bool debug_info;

	/// temporary container for getting symbols
	typedef std::list<op_bfd_symbol> symbols_found_t;

	/**
	 * Collect the symbols excluding any in the given vector.
	 *
	 * Returns false if there were no symbols in the binary
	 * before filtering.
	 */
	void get_symbols(symbols_found_t & symbols);

	/**
	 * Add the symbols in the binary, applying filtering,
	 * and handling artificial symbol.
	 */
	void add_symbols(symbols_found_t & symbols,
	                 string_filter const & symbol_filter);

	/**
	 * symbol_size - return the size of a symbol
	 * @param sym_idx symbol index
	 */
	size_t symbol_size(op_bfd_symbol const & sym,
			   op_bfd_symbol const * next) const;

	/// create an artificial symbol for a symbolless binary
	op_bfd_symbol const create_artificial_symbol();
};

#endif /* !OP_BFD_H*/
