/**
 * @file op_import.cpp
 * Import sample files from other ABI
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author Graydon Hoare 
 */

#include "abi.h"
#include "db.h"
#include "popt_options.h"
#include "op_sample_file.h"

#include <fstream>
#include <iostream>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>

#define MAX_ENDIAN_CHUNK_SIZE 8

using namespace std;
 
struct Extractor 
{
	Abi const & abi;
	unsigned char const * first;
	unsigned char const * last;
	int endian[MAX_ENDIAN_CHUNK_SIZE];

	explicit Extractor(Abi const & a, unsigned char const * src, size_t len) : 
		abi(a), 
		first(src),
		last(src + len) 
	{
		for (int i = 0; i < 8; ++i) {
			string key("endian_byte_");
			key += ('0' + i);
			endian[i] = abi.need(key);
		}
	}

	void extract(void * targ_, void const * src_, 
		     const char * sz, const char * off) throw (Abi_exception);
};


void Extractor::extract(void * targ_, void const * src_, 
	const char * sz, const char * off) throw (Abi_exception)
{
	unsigned char * targ = static_cast<unsigned char *>(targ_);
	unsigned char const * src = static_cast<unsigned char const *>(src_);
		
	int offset = abi.need(off);
	int count = abi.need(sz);

	if (count == 0) 
		return;

	if (src + offset + count > last) {
		throw Abi_exception("input file too short");
	} 
		
	int chunk = (count < MAX_ENDIAN_CHUNK_SIZE 
		     ? count 
		     : MAX_ENDIAN_CHUNK_SIZE);
	src += offset;
 
	for (; count > 0; count -= chunk) {
		int k = (src-first) % chunk;
		for (int i = 0; i < chunk; i++) {
			targ[i] = src[endian[k] % chunk];
			k = (k + 1) % chunk;
		}
		targ += chunk;
		src += chunk;
	}
}
 

void import_from_abi(Abi const & abi, 
		     void const * srcv, 
		     size_t len, 
		     db_tree_t * dest) throw (Abi_exception)
{
	struct opd_header * head;
	head = static_cast<opd_header *>(dest->base_memory);	
	unsigned char const * src = static_cast<unsigned char const *>(srcv);
	Extractor ext(abi, src, len);	

	memcpy(head->magic, src + abi.need("offsetof_header_magic"), 4);

	// begin extracting opd header
	ext.extract(&(head->version), src, "sizeof_u32", "offsetof_header_version");
	ext.extract(&(head->is_kernel), src, "sizeof_u8", "offsetof_header_is_kernel");
	ext.extract(&(head->ctr_event), src, "sizeof_u32", "offsetof_header_ctr_event");
	ext.extract(&(head->ctr_um), src, "sizeof_u32", "offsetof_header_ctr_um");
	ext.extract(&(head->ctr), src, "sizeof_u32", "offsetof_header_ctr");
	ext.extract(&(head->cpu_type), src, "sizeof_u32", "offsetof_header_cpu_type");
	ext.extract(&(head->ctr_count), src, "sizeof_u32", "offsetof_header_ctr_count");
	ext.extract(&(head->cpu_speed), src, "sizeof_double", "offsetof_header_cpu_speed");
	ext.extract(&(head->mtime), src, "sizeof_time_t", "offsetof_header_mtime");
	ext.extract(&(head->separate_samples), src, "sizeof_int", "offsetof_header_separate_samples");
	src += abi.need("sizeof_struct_opd_header");
	// done extracting opd header
	       
	// begin extracting necessary parts of descr
	db_page_count_t page_count;
	ext.extract(&page_count, src, "sizeof_db_page_count_t", "offsetof_descr_current_size");
	src += abi.need("sizeof_db_descr_t");
	// done extracting descr

	// begin extracting pages
	for(db_page_count_t i = 0; i < page_count; ++i, src += abi.need("sizeof_db_page_t")) {
		unsigned int item_count;
		ext.extract(&item_count, src, "sizeof_unsigned_int", "offsetof_page_count");
		unsigned char const * item = src + abi.need("offsetof_page_page_table_offset");
		for (unsigned int j = 0; j < item_count; ++j, item += abi.need("sizeof_db_item_t")) {
			db_key_t key;
			db_value_t val;
			ext.extract(&key, item, "sizeof_db_key_t", "offsetof_item_key");
			ext.extract(&val, item, "sizeof_db_value_t", "offsetof_item_info");
			db_insert(dest, key, val);
		}
	}
	// done extracting pages
}


namespace {
	string output_filename;
	string abi_filename;
	bool verbose;
};

popt::option options_array[] = {
	popt::option(verbose, "verbose", 'V', "verbose output"),
	popt::option(output_filename, "output", 'o', "output to file", "filename"),
	popt::option(abi_filename, "abi", 'a', "abi description", "filename")
};


int 
main(int argc, char const ** argv) 
{

	vector<string> inputs;
	popt::parse_options(argc, argv, inputs);

	if (inputs.size() != 1) {
		cerr << "error: must specify exactly 1 input file" << endl;
		exit(1);
	}

	Abi current_abi, input_abi;
	{
		ifstream abi_file(abi_filename.c_str());
		if (!abi_file) {
			cerr << "error: cannot open abi file " 
			     << abi_filename << endl;
			exit(1);
		}
		abi_file >> input_abi;
	}

	if (current_abi == input_abi) {
		cerr << "input abi is identical to native. "
		     << "no conversion necessary." << endl;
		exit(1);
	} else {
		int in_fd;
		struct stat statb;
		void * in;
		db_tree_t dest;

		assert((in_fd = open(inputs[0].c_str(), O_RDONLY)) > 0);		
		assert(fstat(in_fd, &statb)==0);
		assert((in = mmap(0, statb.st_size, PROT_READ, 
				  MAP_PRIVATE, in_fd, 0)) != (void *)-1);

		db_open(&dest, output_filename.c_str(), DB_RDWR, 
			sizeof(struct opd_header));

		try {
			import_from_abi(input_abi, in, statb.st_size, &dest);
		} catch (Abi_exception &e) {
			cerr << "caught abi exception: " << e.desc << endl;
		}

		db_close(&dest);

		assert(munmap(in, statb.st_size)==0);		
	}
}
