/* COPYRIGHT (C) 2001 Philippe Elie
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59 Temple
 * Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef PERSISTENT_CONFIG_H
#define PERSISTENT_CONFIG_H

#include <iostream>
#include <string>
#include <map>

// a simple association between a string and a type T. try to handle
// also a dirty flag to track non const access. a typical use:
// define a struct T:
//
// persistent_cfg_t<T> cfg;
//
// cfg[string].field_name = XXXX;
//
// if (cfg[string].field_name == yyyy) then ...
//
// the behavior try to mimic script language with associative array.

template <typename T>
class persistent_config_t
{
public:
	persistent_config_t() : is_dirty(false) {}

	bool dirty() const { return is_dirty; }
	void set_dirty(bool dirty) { is_dirty = dirty; }

	// set dirty flag
	T& operator[](const std::string& key);
	// does not change dirty flag.
	// if you try to:
	// if (cfg[string].member == xxxx) you call the non-const [] operator
	const T& operator[](const std::string& key) const;

	// these two function reset the dirty flag
	// save and load use the folloing file format
	// key value1 value2 value3 ... valueN through the overload of
	// operator>> [<<] (ostream&[istream&], [const] T&);
	void save(std::ostream& out) const;
	// take care: this do not reset the current recorded value. It only
	// override or create new key/value.
	void load(std::istream& out);

private:
	typedef std::map<std::string, T> map_t;
	
	map_t map;
	bool is_dirty;
};

template <typename T>
T& persistent_config_t<T>::operator[](const std::string& key)
{
	is_dirty = true;

	return map[key];
}

template <typename T>
const T& persistent_config_t<T>::operator[](const std::string& key) const
{
	return const_cast<map_t&>(map)[key];
}

template <typename T>
void persistent_config_t<T>::save(std::ostream& out) const
{
	typename map_t::const_iterator it;
	for (it = map.begin() ; it != map.end() ; ++it) {
		out << it->first;
		out << " ";
		out << it->second << std::endl;
	}
}

template <typename T>
void persistent_config_t<T>::load(std::istream& in)
{
	typename map_t::const_iterator it;
	for (it = map.begin() ; it != map.end() ; ++it) {
		std::string name;

		in >> name;
		in >> map[name];
	}
}

template <typename T>
std::istream& operator>>(std::istream& in, persistent_config_t<T>& cfg)
{
	cfg.load(in);

	return in;
}

template <typename T>
std::ostream& operator<<(std::ostream& out, const persistent_config_t<T>& cfg)
{
	cfg.save(out);

	return out;
}

#endif // !PERSISTENT_CONFIG_H
