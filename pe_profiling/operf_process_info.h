/*
 * @file pe_profiling/operf_process_info.h
 * This file contains functions for storing process information,
 * handling exectuable mmappings, etc.
 *
 * @remark Copyright 2011 OProfile authors
 * @remark Read the file COPYING
 *
 * Created on: Dec 7, 2011
 * @author Maynard Johnson
 * (C) Copyright IBM Corp. 2011
 */

#ifndef OPERF_PROCESS_INFO_H_
#define OPERF_PROCESS_INFO_H_

#include <map>
#include <limits.h>
#include "op_types.h"
#include "cverb.h"

extern verbose vperf;

using namespace std;
#define BUILD_ID_SIZE 20

struct operf_mmap {
	u32 pid;
	u64 start_addr;
	u64 end_addr;
	u64 pgoff;
	char * buildid;
	bool buildid_valid;
	u64 checksum;
	char filename[PATH_MAX];
};

/* This class is designed to hold information about a process for which a COMM event
 * has been recorded in the profile data: application name, process ID, and a map
 * containing all of the libraries and executable anonymous memory mappings used by this
 * process.
 *
 * The COMM event provides only a 16-char (possibly abbreviated) "comm" field for the
 * executable's basename.  If operf is being run in single-process mode vs system-wide,
 * then we will know what the full pathname of the executable is, in which case, that
 * will be the value stored in the app_name field; otherwise, as MMAP events are
 * processed, we compare their basenames to the short name we got from the COMM event.
 * The mmap'ing whose basename has the most matching characters is chosen to use as
 * the full pathname of the application.  TODO: It's possible that this choice may be wrong;
 * we should verify the choice by looking at the ELF data (ELF header e_type field should
 * be "ET_EXEC").
 *
 * This class is designed to handle the possibility that MMAP events may occur for a process
 * prior to the COMM event.  I don't know if this is possible, but it didn't take much to
 * add code to handle this exigency.
 */
class operf_process_info {
public:
	operf_process_info(pid_t tgid, const char * appname, bool app_arg_is_fullname, bool is_valid);
	bool is_valid(void) { return valid; }
	void process_new_mapping(struct operf_mmap mapping);
	void process_deferred_mappings(string app_shortname);
	string get_app_name(void) { return app_name; }
	void add_deferred_mapping(struct operf_mmap mapping)
	{ deferred_mmappings[mapping.start_addr] = mapping; }

	const struct operf_mmap * find_mapping_for_sample(u64 sample_addr);
private:
	typedef enum {
		NOT_FULLNAME,
		MAYBE_FULLNAME,
		YES_FULLNAME
	} op_fullname_t;
	pid_t pid;
	string app_name;

	// The valid bit is set when a COMM event has been received for the process
	// represented by this object.
	bool valid;
	op_fullname_t appname_is_fullname;
	string app_basename;
	int  num_app_chars_matched;
	map<u64, struct operf_mmap> mmappings;
	map<u64, struct operf_mmap> deferred_mmappings;
	int get_num_matching_chars(string mapped_filename, string & basename);
};

extern map<pid_t, operf_process_info *> process_map;

#endif /* OPERF_PROCESS_INFO_H_ */
