/**
 * @file abi.cpp
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author Graydon Hoare
 * @author John Levon
 */
 
#include "abi.h"
#include "db-hash.h"
#include "op_sample_file.h"

#include <iostream>

using namespace std;

typedef unsigned char byte;
typedef byte * bytep;
typedef unsigned long long octobyte;
typedef map<string,int> abi_map;
typedef map<string,int>::const_iterator abi_iter;

#define byte_addr(x) ((bytep)(&(x)))
#define field_offset(s,f) (byte_addr(s.f) - byte_addr(s))


Abi_exception::Abi_exception(string const d) : desc(d) {}


Abi::Abi() 
{
	
	db_node_t node;
	db_descr_t descr;
	struct opd_header header;
	
	slots["sizeof_double"] = sizeof(double);
	slots["sizeof_time_t"] = sizeof(time_t);
	slots["sizeof_u8"] = sizeof(u8);
	slots["sizeof_u32"] = sizeof(u32);
	slots["sizeof_int"] = sizeof(int);
	slots["sizeof_unsigned_int"] = sizeof(unsigned int);
	slots["sizeof_db_key_t"] = sizeof(db_key_t);
	slots["sizeof_db_index_t"] = sizeof(db_index_t);
	slots["sizeof_db_value_t"] = sizeof(db_value_t);
	slots["sizeof_db_node_nr_t"] = sizeof(db_node_nr_t);
	slots["sizeof_db_descr_t"] = sizeof(db_descr_t);
	slots["sizeof_struct_opd_header"] = sizeof(struct opd_header);		
	
	slots["offsetof_node_key"] = field_offset(node, key);
	slots["offsetof_node_value"] = field_offset(node, value);
	slots["offsetof_node_next"] = field_offset(node, next);
	
	slots["offsetof_descr_size"] = field_offset(descr, size);
	slots["offsetof_descr_current_size"] = field_offset(descr, current_size);
	
	slots["offsetof_header_magic"] = field_offset(header, magic);
	slots["offsetof_header_version"] = field_offset(header, version);
	slots["offsetof_header_is_kernel"] = field_offset(header, is_kernel);
	slots["offsetof_header_ctr_event"] = field_offset(header, ctr_event);
	slots["offsetof_header_ctr_um"] = field_offset(header, ctr_um);
	slots["offsetof_header_ctr"] = field_offset(header, ctr);
	slots["offsetof_header_cpu_type"] = field_offset(header, cpu_type);
	slots["offsetof_header_ctr_count"] = field_offset(header, ctr_count);
	slots["offsetof_header_cpu_speed"] = field_offset(header, cpu_speed);
	slots["offsetof_header_mtime"] = field_offset(header, mtime);
	slots["offsetof_header_separate_samples"] = field_offset(header, separate_samples);
	
	for (int i = 0; i < 8; ++i) {
		octobyte probe = 0LL;
		bytep probe_byte = static_cast<bytep>(static_cast<void *>(&probe));
		probe |= 0xff;
		probe <<= (i*8);
		int j = 0;
		for (j = 0; j < 8; ++j) {
			if (probe_byte[j] == 0xff) {
				string key("endian_byte_");
				key += ('0' + i);
				slots[key] = j;
				break;
			}
		}
		assert(j < 8);
	}
}


Abi::Abi(Abi const & other) 
{
	slots.clear();
	slots.insert(other.slots.begin(), other.slots.end());
}


int Abi::need(string const key) const throw (Abi_exception)
{
	if (slots.find(key) != slots.end())
		return slots.find(key)->second;
	else
		throw Abi_exception(string("missing ABI key: ") + key);
}


bool Abi::operator==(Abi const & other) const
{
	abi_iter i = slots.begin();
	abi_iter e = slots.end();
	abi_map const & theirs = other.slots;

	for (; i != e; ++i) {
		if (theirs.find(i->first) == theirs.end() ||
		    theirs.find(i->first)->second != i->second)
			return false;
	}
	return true;		
}


ostream & operator<<(ostream & o, Abi const & abi)
{
	abi_iter i = abi.slots.begin();
	abi_iter e = abi.slots.end();

	for (; i != e; ++i) {
		o << i->first << " " << i->second << endl;
	}
	return o;
}


istream & operator>>(istream & i, Abi & abi)
{
	string key;
	int val;
	abi.slots.clear();
	while(i >> key >> val) {
		abi.slots[key] = val;
	}
	return i;
}
