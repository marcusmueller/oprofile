/**
 * @file name_storage.cpp
 * Storage of global names (filenames and symbols)
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author Philippe Elie
 * @author John Levon
 */

#include <stdexcept>

#include "name_storage.h"
#include "demangle_symbol.h"
#include "file_manip.h"
#include "string_manip.h"

using namespace std;

image_name_storage image_names;
debug_name_storage debug_names;
symbol_name_storage symbol_names;

name_storage::name_storage()
{
}


size_t name_storage::do_create(string const & name)
{
	id_map::const_iterator cit = ids.find(name);
	if (cit == ids.end()) {
		names.push_back(name);
		ids[name] = names.size();
		return names.size();
	}
	return cit->second;
}


string const & name_storage::get_name(size_t id) const
{
	// some stl lack at(), so we emulate it
	if (id > 0 && id <= names.size())
		return names[id - 1].name;

	throw out_of_range("name_storage::get_name(size_t): out of bound index");
}


name_storage::stored_name const & name_storage::processed_name(size_t id) const
{
	// some stl lack of at(), we emulate it
	if (id > 0 && id <= names.size())
		return names[id - 1];

	throw out_of_range("name_storage::processed_name(size_t): out of bound index");
}


string const & filename_storage::basename(size_t id) const
{
	static string empty;
	if (id == 0) {
		return empty;
	}

	stored_name const & n = processed_name(id);
	if (n.name_processed.empty()) {
		n.name_processed = ::basename(n.name);
	}
	return n.name_processed;
}


image_name_id image_name_storage::create(string const & name)
{
	image_name_id name_id;
	name_id.id = do_create(name);
	return name_id;
}


string const & image_name_storage::name(image_name_id image_id) const
{
	return get_name(image_id.id);
}


string const & image_name_storage::basename(image_name_id image_id) const
{
	return filename_storage::basename(image_id.id);
}


debug_name_id debug_name_storage::create(string const & name)
{
	debug_name_id debug_id;
	debug_id.id = do_create(name);
	return debug_id;
}


string const & debug_name_storage::name(debug_name_id debug_id) const
{
	static string empty;
	if (!debug_id.id) {
		return empty;
	}

	return get_name(debug_id.id);
}


string const & debug_name_storage::basename(debug_name_id debug_id) const
{
	return filename_storage::basename(debug_id.id);
}


symbol_name_id symbol_name_storage::create(string const & name)
{
	symbol_name_id symbol_id;
	symbol_id.id = do_create(name);
	return symbol_id;
}


string const & symbol_name_storage::name(symbol_name_id symbol_id) const
{
	return get_name(symbol_id.id);
}


string const & symbol_name_storage::demangle(symbol_name_id symb_id) const
{
	stored_name const & n = processed_name(symb_id.id);
	if (!n.name_processed.empty() || n.name.empty())
		return n.name_processed;

	if (n.name[0] != '?') {
		n.name_processed = demangle_symbol(n.name);
		return n.name_processed;
	}

	if (n.name.length() < 2 || n.name[1] != '?') {
		n.name_processed = "(no symbols)";
		return n.name_processed;
	}
	
	n.name_processed = "anonymous symbol from section ";
	n.name_processed += ltrim(n.name, "?");
	return n.name_processed;
}
