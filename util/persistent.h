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
#ifndef PERSISTENT_H
#define PERSISTENT_H

/** 
 * A simple persistence implementation, do not support inheritance,
 * pointer and reference to object.
 * The basic idea is to associate an uid (string) and a pointer to data member.
 * The data are manipulated through a virtual function member of a template
 * class.
 * This leads to many issue but it is sufficient for our current need:
 * one object by file and type of field initializable through overloading
 * of <<(istream &) and >>(ostream &)
 * Additionnaly we support all stl like container type at top level of the
 * saved hierarchy.
 */

#include <map>
#include <string>
#include <iostream>
#include <iterator>

template <typename Container>
void SaveContainer(const Container & x, std::ostream & out);
template <typename Container>
void LoadContainer(Container & x, std::istream & in);

template <typename T> class Persistent;

/* haveIO<type>::val must be true if type have overloaded >>() and <<().
 * This allow to select save/load specialization */
/* These probably needs further specialization for const base_type */
template <typename T> struct haveIO	 { static const bool val = false; };
template <> struct haveIO<int>		 { static const bool val = true;  };
template <> struct haveIO<unsigned int>  { static const bool val = true;  };
template <> struct haveIO<short>	 { static const bool val = true;  };
template <> struct haveIO<unsigned short>{ static const bool val = true;  };
template <> struct haveIO<long>		 { static const bool val = true;  };
template <> struct haveIO<unsigned long> { static const bool val = true;  };
template <> struct haveIO<char>		 { static const bool val = true;  };
template <> struct haveIO<unsigned char> { static const bool val = true;  };
template <> struct haveIO<signed char>   { static const bool val = true;  };
template <> struct haveIO<wchar_t>	 { static const bool val = true;  };
template <> struct haveIO<float>	 { static const bool val = true;  };
template <> struct haveIO<double>	 { static const bool val = true;  };
template <> struct haveIO<long double>	 { static const bool val = true;  };
template <> struct haveIO<std::string>	 { static const bool val = true;  };
template <> struct haveIO<const std::string> { static const bool val = true; };

/* A built-in to describe std::pair<>, so we can support std::map/multimap */
template<typename T1, typename T2>
void describe(const std::pair<T1, T2> *)
{
	typedef std::pair<T1, T2> p;
	Persistent<p>::addField(&p::first, "first");
	Persistent<p>::addField(&p::second, "second");
}

template<typename T>
inline void describe(const T *)
{
	T::describe();
}

template <typename V, bool b> struct Save;
template <typename V, bool b> struct Load;

/* For all V which does not have >>() and <<() we assume than user provide a
 * static V::describe() function so we dispach to Persistent<V> to get the
 * relevant description, through Persistent<V>::initialize */
template <typename V> struct Save<V, false> {
	static void save(const V & x, std::ostream & out) {
		Persistent<V>::save(x, out);
	}
};

template <typename V> struct Save<V, true> {
	static void save(const V & x, std::ostream & out) {
		out << x << "\n";
	}
};

/* std::string needs special handling mainly to quote the string */
template <> struct Save<const std::string, true> {
	static void save(const std::string & x, std::ostream & out) {
		out << '"';
		for(std::string::const_iterator it = x.begin(); 
		    it != x.end();
		    ++it) {
			if (*it == '\\' || *it == '"')
				out << '\\';
			out << *it;
		}
		out << "\"\n";
	}
};

template <> struct Save<std::string, true> {
	static void save(const std::string & x, std::ostream & out) {
		Save<const std::string, true>::save(x, out);
	}
};

template <typename V> struct Load<V, false> {
	static void load(V & x, std::istream & in) {
		Persistent<V>::load(x, in);
	}
};

template <typename V> struct Load<V, true> {
	static void load(V & x, std::istream & in) {
		in >> x;
	}
};

// to rebuild struct with const member. Typically for the member first of
// a std::map which is a pair<const T, Key>.
template <typename V> struct Load<const V, true> {
	static void load(const V & x, std::istream & in) {
		in >> const_cast<V &>(x);
	}
};

/* std::string needs special handling mainly to quote the string */
template <> struct Load<std::string, true> {
	static void load(std::string & x, std::istream & in) {
		x.resize(0);

		in >> std::ws;
		if (in.peek() == '"')
			in.get();
		else {
			in.setstate(in.failbit);
			return;
		}

		if (!in)
			return;
		for(;;) {
			char c = in.get();
			if(!in)
				break;
			if (c == '"') {           // end of string
				in >> std::ws;
				return; 
			}
			if (c == '\\') {
				if (in.peek() == '\\' || in.peek() == '"') {
					c = in.get();
					if (!in)
						break;
				}
			}
			x += c;
		}
		in.setstate(in.failbit);
	}
};

// to rebuild struct with const member. Typically for the member first of
// a std::map which is a pair<const T, Key>.
template <> struct Load<const std::string, true> {
	static void load(const std::string & x, std::istream & in) {
		Load<std::string, true>::load(const_cast<std::string&>(x), in);
	}
};

/**
 * Abstract base class for recorded field. Persistent<T> store a
 * map<string, pointer to these object> and use it to save/load type T.
 */
template <typename T>
struct PersistentFieldBase
{
	virtual bool load(T & object, std::istream &) = 0;
	virtual bool save(const T & object, std::ostream &) = 0;
};

/**
 * a convenience class to implement saving/restoring object by value. Each
 * object of this class handle a field of type V member of a type T through
 * a (V T::*)
 */
// The right way is to add a V T::* non type template parameter but some
// compiler does not handle it correctly, so the pointer to member is passed as
// parameter of ctor and stored in class object.
template <typename T, typename V>
class PersistentField : public PersistentFieldBase<T>
{
public:
	PersistentField(V T::* f) : field(f) {}

	bool load(T & object, std::istream & in) {
		char ch;
		in >> ch;
		if (!in || ch != '=') {
			in.putback(ch);
			in.setstate(in.failbit);
			return false;
		}

		Load<V, haveIO<V>::val >::load(object.*field, in);
		return in;
	}

	bool save(const T & object, std::ostream & out) {
		out << " = ";
		Save<V, haveIO<V>::val >::save(object.*field, out);
		return out;
	}

private:
	V T::*  field;	// record where to save/load the output/input data
};

/**
 * persistency implementation: this is mainly a container of associative
 * pair (field identifier, pointer to data member). Note than all member
 * function are static except a private ctor: we do not need to build 
 * Persistent<T> object.
 *
 * See persistent-test.cpp
 */
/* template member are "inlined in the class" to work-around compiler bugs */
template <typename T>
class Persistent
{
public:
	// simplify the call to the non template member addField
	template <typename V>
		static void addField(V T::* field, const std::string & id)
		{ addField(new PersistentField<T, V>(field), id); }

	static void addField(PersistentFieldBase<T> *, const std::string &);

	static bool load(T & object, std::istream & in);
	static bool save(const T & object, std::ostream & out);

	template <typename Container>
		static bool loadContainer(Container & x, std::istream & in) {

		initialize();

		if (startRead(in, '[') == false)
			return false;

		LoadContainer<Container>(x, in);

		return true;
	}
	template <typename Container>
		static bool saveContainer(const Container & x, std::ostream & out) {

		initialize();

		startWrite(out, '[');

		SaveContainer<Container>(x, out);

		finishWrite(out, ']');

		return true;
	}

private:
	Persistent();

	static void initialize();

	static void startWrite(std::ostream & out, char sequence_type);
	static void finishWrite(std::ostream & out, char sequence_type);
	static bool startRead(std::istream & in, char expected_sequence_type);
public:
	// used by SaveContainer()/LoadContainer free function. Must be public
	// to work around template friend decl problem with some compiler.
	static bool finishRead(std::istream & in, char expected_sequence_type);
private:
	typedef std::map<std::string, PersistentFieldBase< T > *> Map;

	// lazzily contruct, never destruct so this acts as a singleton object.
	static Map map;
};

template <typename T>
Persistent<T>::Map Persistent<T>::map;

template <typename T>
void Persistent<T>::initialize()
{
	if (map.empty()) {
		describe(static_cast<T *>(0));
	}
}

template <typename T>
void 
Persistent<T>::addField(PersistentFieldBase<T> * field, const std::string & id)
{
	map.insert(typename Map::value_type(id, field));
}

template <typename T>
void Persistent<T>::startWrite(std::ostream & out, char sequence_type)
{
	out << sequence_type << "\n";
}

template <typename T>
void Persistent<T>::finishWrite(std::ostream & out, char sequence_type)
{
	out << sequence_type << "\n";
}

template <typename T>
bool Persistent<T>::startRead(std::istream & in, char expected_sequence_type)
{
	char ch;
	in >> ch;
	if (ch != expected_sequence_type) {
		in.putback(ch);
		return false;
	}

	return true;
}

template <typename T>
bool Persistent<T>::finishRead(std::istream & in, char expected_sequence_type)
{
	char ch;

	if (!in)
		return true;

	in >> ch;
	if (ch == expected_sequence_type)
		return true;

	in.putback(ch);
	return false;
}

template <typename T>
bool Persistent<T>::load(T & object, std::istream & in)
{
	initialize();

	if (startRead(in, '{') == false)
		return false;

	while (finishRead(in, '}') == false) {
		std::string id;
		in >> id;

		typename Map::const_iterator it = map.find(id);
		if (it != map.end())
			it->second->load(object, in);
	}

	return true;
}

template <typename T>
bool Persistent<T>::save(const T & object, std::ostream & out)
{
	initialize();

	startWrite(out, '{');

	for (typename Map::iterator it = map.begin(); it != map.end() ; ++it) {
		out << it->first;
		it->second->save(object, out);
	}

	finishWrite(out, '}');

	return true;
}

template <typename Container>
void SaveContainer(const Container & x, std::ostream & out)
{
	typename Container::const_iterator it;
	for (it = x.begin() ; it != x.end() ; ++it) {
		Persistent<typename Container::value_type>::save(*it, out);
	}
}

template <typename Container>
void LoadContainer(Container & x, std::istream & in)
{
	// We really need an inserter to end here, not all stl container
	// provide a valid back_insert_interator.
	std::insert_iterator<Container> it = std::inserter(x, x.end());
	while (Persistent<Container>::finishRead(in, ']') == false) {
		typename Container::value_type v;
		Persistent<typename Container::value_type>::load(v, in);
		*it++ = v;
	}
}

#endif /* !PERSISTENT_H */
