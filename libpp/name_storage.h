/**
 * @file name_storage.h
 * Type-safe unique storage of global names (filenames and symbols)
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author Philippe Elie
 * @author John Levon
 */

#ifndef NAME_STORAGE_H
#define NAME_STORAGE_H

#include <string>

#include "unique_storage.h"

/// store original name and processed name
struct stored_name {
	stored_name(std::string const & n = std::string())
		: name(n) {}

	bool operator<(stored_name const & rhs) const {
		return name < rhs.name;
	}

	std::string name;
	mutable std::string name_processed;
};


/// partial specialization for unique storage of names
template<typename I> class name_storage
	: public unique_storage<I, stored_name> {
	
public:
	std::string const & name(I const & id) const {
		return unique_storage<I, stored_name>::get(id).name;
	};
};


/**
 * template class of storage IDs. The only reason for the
 * templatization is so different containers are typesafe (you cannot look
 * up with the wrong ID type).
 */
template<typename T> struct name_id {
public:
	typedef std::vector<stored_name>::size_type size_type;

	explicit name_id(size_type s) : id(s) {}

	name_id() : id(0) {}

	operator size_type() const { return id; }

	size_type id;

};


/// store an image name identifier
struct image_name_tag;
typedef name_id<image_name_tag> image_name_id;


/// store a debug name identifier (a source filename)
struct debug_name_tag;
typedef name_id<debug_name_tag> debug_name_id;


/// store a symbol name identifier
struct symbol_name_tag;
typedef name_id<symbol_name_tag> symbol_name_id;


/// comparator for debug names is different
bool operator<(debug_name_id const & lhs, debug_name_id const & rhs);


/// class storing a set of shared debug name (source filename)
class debug_name_storage : public name_storage<debug_name_id> {
public:
	/// return the basename for the given ID
	std::string const & basename(debug_name_id id) const;
};


/// class storing a set of shared image name
class image_name_storage : public name_storage<image_name_id> {
public:
	/// return the basename name for the given ID
	std::string const & basename(image_name_id) const;
};


/// class storing a set of shared symbol name
class symbol_name_storage : public name_storage<symbol_name_id> {
public:
	/// return the demangled name for the given ID
	std::string const & demangle(symbol_name_id id) const;
};


/// for images
extern image_name_storage image_names;

/// for debug filenames i.e. source filename
extern debug_name_storage debug_names;

/// for symbols
extern symbol_name_storage symbol_names;

#endif /* !NAME_STORAGE_H */
