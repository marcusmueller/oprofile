/**
 * @file op_bfd.h
 * Encapsulation of bfd objects
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author Philippe Elie <phil_el@wanadoo.fr>
 * @author John Levon <moz@compsoc.man.ac.uk>
 */

#ifndef OP_BFD_H
#define OP_BFD_H

#include <bfd.h>

#include <vector>
#include <string>

#include "op_types.h"

class opp_samples_files;

/** all symbol vector indexation use this type */
typedef size_t symbol_index_t;
const symbol_index_t nil_symbol_index = symbol_index_t(-1);

/** a symbol description from a bfd point of view */
struct op_bfd_symbol {
	asymbol* symbol;
	bfd_vma vma;
	size_t size;
};

/** Encapsulation of a bfd object. Simplify open/close of bfd, enumerating
 * symbols and retrieving informations for symbols or vma. */
class op_bfd {
public:
	/**
	 * @param samples a valid samples file associated with this image
	 * @param filename the name of the image file
	 *
	 * All error are fatal.
	 *
	 */
	op_bfd(opp_samples_files & samples, const std::string & filename);

	/** close an opended bfd image and free all related resource. */
	~op_bfd();

	/**
	 * @param sym_idx index of the symbol
	 * @param offset fentry number
	 * @param filename output parameter to store filename
	 * @param linenr output parameter to store linenr.
	 *
	 * retrieve the relevant finename:linenr information for the sym_idx
	 * at offset. If the lookup fail return false. In some case this
	 * function can retrieve the filename and return true but fail to
	 * retrieve the linenr and so can return zero in linenr
	 */
	bool get_linenr(symbol_index_t sym_idx, uint offset, 
			char const * & filename, unsigned int & linenr) const;

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
	 * All error are fatal.
	 */
	void get_symbol_range(symbol_index_t sym_idx,
			      u32 & start, u32 & end) const;

	/** 
	 * @param symbol the symbol name
	 *
	 * find and return the index of a symbol else return -1
	 */
	symbol_index_t symbol_index(char const * symbol) const;

	/**
	 * sym_offset - return offset from a symbol's start
	 * @param num_symbols symbol number
	 * @param num number of fentry
	 *
	 * Returns the offset of a sample at position num
	 * in the samples file from the start of symbol sym_idx.
	 */
	u32 sym_offset(symbol_index_t num_symbols, u32 num) const;

	/** Returns true if the underlined bfd object contains debug info */
	bool have_debug_info() const;

	// TODO: avoid this two public data members
	bfd *ibfd;
	// sorted vector by vma of interesting symbol.
	std::vector<op_bfd_symbol> syms;

	// nr of samples.
	uint nr_samples;
private:
	// vector of symbol filled by the bfd lib.
	asymbol **bfd_syms;
	// image file such the linux kernel need than all vma are offset
	// by this value.
	u32 sect_offset;
	// ctor helper
	void open_bfd_image(const std::string & file_name, bool is_kernel);
	bool get_symbols();

	/**
	 * symbol_size - return the size of a symbol
	 * @param sym_idx symbol index
	 */
	size_t symbol_size(symbol_index_t sym_idx) const;
};

#endif /* !OP_BFD_H*/
