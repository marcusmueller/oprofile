/**
 * @file name_storage.h
 * Storage of global names (filenames and symbols)
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
#include <vector>
#include <map>

/**
 * Base class to holds shared names. Each ID identifies a unique string,
 * and IDs can be shared across all users.
 */
class name_storage {

public:
	name_storage();

protected:
	/// allocate or re-use an ID for this name
	size_t do_create(std::string const & name);

	/// return the original name form of the given ID
	std::string const & get_name(size_t id) const;

	struct stored_name {
		stored_name(std::string const & n = std::string())
			: name(n) {}

		bool operator<(stored_name const & rhs) const {
			return name < rhs.name;
		}
		std::string name;
		mutable std::string name_processed;
	};

	/// return a reference to the storage used for the given ID
	stored_name const & processed_name(size_t id) const;
private:
	typedef std::vector<stored_name> stored_names;
	typedef std::map<std::string, size_t> id_map;

	stored_names names;
	id_map ids;
};


/**
 * base class for shared filename storage
 */
class filename_storage : public name_storage {
protected:
	/// return the basename name for the given ID
	std::string const & basename(size_t id) const;
};


struct image_name_id {
	image_name_id() : id(0) {}
	size_t id;
};


class image_name_storage : public filename_storage {
public:
	/// allocate or re-use an ID for this name
	image_name_id create(std::string const & name);

	/// return the original name form of the given ID
	std::string const & name(image_name_id id) const;
	/// return the basename name for the given ID
	std::string const & basename(image_name_id id) const;
};


struct debug_name_id {
	debug_name_id() : id(0) {}
	size_t id;
};


class debug_name_storage : public filename_storage {
public:
	/// allocate or re-use an ID for this name
	debug_name_id create(std::string const & name);

	/// return the original name form of the given ID
	std::string const & name(debug_name_id id) const;
	/// return the basename for the given ID
	std::string const & basename(debug_name_id id) const;
};


struct symbol_name_id {
	symbol_name_id() : id(0) {}
	size_t id;
};


class symbol_name_storage : public name_storage {
public:
	/// allocate or re-use an ID for this name
	symbol_name_id create(std::string const & name);

	/// return the original name form of the given ID
	std::string const & name(symbol_name_id id) const;
	/// return the demangled name for the given ID
	std::string const & demangle(symbol_name_id id) const;
};

/// for images
extern image_name_storage image_names;

/// for debug filenames
extern debug_name_storage debug_names;

/// for symbols
extern symbol_name_storage symbol_names;

#endif /* !NAME_STORAGE_H */
