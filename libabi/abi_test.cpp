#include "abi.h"
#include "db.h"
#include "popt_options.h"
#include "op_sample_file.h"
#include "op_cpu_type.h"
#include "op_config.h"

#include <fstream>
#include <iostream>

using namespace std;

namespace {
	string db_filename;
	string abi_filename;
}


popt::option options_array[] = {
	popt::option(db_filename, "db", 'd', "output db to file", "filename"),
	popt::option(abi_filename, "abi", 'a', "output abi to file", "filename")
};


int main(int argc, char const ** argv)
{
	vector<string> rest;
	popt::parse_options(argc, argv, rest);
	bool file_processed = false;

	Abi curr;
	if (abi_filename.size() > 0) {
		ofstream file(abi_filename.c_str());
		if (!file) {
			cerr << "error: cannot open " << abi_filename << " for writing" << endl;
			exit(1);
		}
		file << curr;
		file_processed = true;
	}

	if (db_filename.size() > 0) {
		samples_db_t dest;
		db_open(&dest, db_filename.c_str(), DB_RDWR, sizeof(struct opd_header));

		struct opd_header * header;
		header = static_cast<struct opd_header *>(dest.base_memory);
		memset(header, '\0', sizeof(struct opd_header));
		header->version = OPD_VERSION;
		memcpy(header->magic, OPD_MAGIC, sizeof(header->magic));
		header->is_kernel = 1;
		header->ctr_event = 0x80; /* ICACHE_FETCHES */
		header->ctr_um = 0x0;
		header->ctr = 0x0; 
		header->cpu_type = CPU_ATHLON;
		header->ctr_count = 0xdeadbeef;
		header->cpu_speed = 1500.0;
		header->mtime = 1034790063;
		header->separate_samples = 0;

    
		for(int i = 0; i < 76543; ++i) {
			db_insert(&dest, ((i*i) ^ (i+i)), ((i*i) ^ i));
		}
		db_close(&dest);
		file_processed = true;
	}

	if (!file_processed) {
		cerr << "error: no file processed" << endl;
		exit(1);
	}

}
