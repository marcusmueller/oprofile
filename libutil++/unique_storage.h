/**
 * @file unique_storage.h
 * Unique storage of values
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author Philippe Elie
 * @author John Levon
 */

#ifndef UNIQUE_STORAGE_H
#define UNIQUE_STORAGE_H

#include <vector>
#include <map>
#include <stdexcept>

/**
 * Store values indexed by I such that only one copy
 * of the value is ever stored.
 *
 * The indexer "I" must be integer-compatible.
 *
 * The value type "V" must be default-constructible.
 */
template <typename I, typename V> class unique_storage {

public:
	unique_storage() {
		// id 0
		values.push_back(V());
	}

	virtual ~unique_storage() {}

	/// ensure this value is available
	I const create(V const & value) {
		typename id_map::const_iterator cit = ids.find(value);

		if (cit == ids.end()) {
			// ID must be constructible from
			// std::vector::size_type
			I const id(values.size());
			values.push_back(value);
			ids[value] = id;
			return id;
		}

		return cit->second;
	}

protected:
	/// return the stored value for the given ID
	V const & get(I const & id) const {
		// some stl lack at(), so we emulate it
		if (id < values.size())
			return values[id];

		throw std::out_of_range("unique_storage::get(): out of bounds");
	}

private:
	typedef std::vector<V> stored_values;
	typedef std::map<V, I> id_map;

	/// the contained values
	stored_values values;

	/// map from ID to value
	id_map ids;
};

#endif /* !UNIQUE_STORAGE_H */
