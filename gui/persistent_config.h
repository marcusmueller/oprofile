/**
 * @file persistent_config.h
 * Templates for management of config files
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author Philippe Elie
 * @author John Levon
 */

#ifndef PERSISTENT_CONFIG_H
#define PERSISTENT_CONFIG_H

#include <iostream>
#include <string>
#include <map>

// a simple association between a string and a type T. a typical use:
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
	persistent_config_t() {}

	T & operator[](std::string const & key);
	T const & operator[](std::string const & key) const;

	// save and load using the file format
	// key value1 value2 value3 ... valueN through the overload of
	// operator>> [<<] (ostream&[istream&], [const] T&);
	void save(std::ostream& out) const;
	// take care: this do not reset the current recorded value. It only
	// override or create new key/value.
	void load(std::istream& out);

private:
	typedef std::map<std::string, T> map_t;

	map_t map;
};

template <typename T>
T & persistent_config_t<T>::operator[](std::string const & key)
{
	return map[key];
}

template <typename T>
T const & persistent_config_t<T>::operator[](std::string const & key) const
{
	return const_cast<map_t &>(map)[key];
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
std::ostream& operator<<(std::ostream& out, persistent_config_t<T> const & cfg)
{
	cfg.save(out);

	return out;
}

#endif // !PERSISTENT_CONFIG_H
