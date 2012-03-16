/*
 * @file pe_profiling/operf_process_info.cpp
 * This file contains functions for storing process information,
 * handling exectuable mmappings, etc.
 *
 * @remark Copyright 2011 OProfile authors
 * @remark Read the file COPYING
 *
 * Created on: Dec 13, 2011
 * @author Maynard Johnson
 * (C) Copyright IBM Corp. 2011
 */

#include <stdio.h>
#include <iostream>
#include <map>
#include "operf_process_info.h"
#include "file_manip.h"
#include "operf.h"

using namespace std;
using namespace OP_perf_utils;

operf_process_info::operf_process_info(pid_t tgid, const char * appname, bool app_arg_is_fullname, bool is_valid)
: pid(tgid), app_name(appname), valid(is_valid)
{
	if (app_arg_is_fullname) {
		appname_is_fullname = YES_FULLNAME;
		app_basename = op_basename(appname);
	} else if (appname) {
		appname_is_fullname = MAYBE_FULLNAME;
		num_app_chars_matched = -1;
		app_basename = appname;
	} else {
		appname_is_fullname = NOT_FULLNAME;
		num_app_chars_matched = -1;
		app_basename = "";
	}

}

void operf_process_info::process_new_mapping(struct operf_mmap mapping)
{
	// If we do not know the full pathname of our app yet,
	// let's try to determine if the passed filename is a good
	// candidate appname.
	if ((appname_is_fullname < YES_FULLNAME) && (num_app_chars_matched < (int)app_basename.length())) {
		string basename;
		int num_matched_chars = get_num_matching_chars(mapping.filename, basename);
		if (num_matched_chars > num_app_chars_matched) {
			appname_is_fullname = MAYBE_FULLNAME;
			app_name = mapping.filename;
			app_basename = basename;
			num_app_chars_matched = num_matched_chars;
			cverb << vmisc << "Best appname match is " << app_name << endl;
		}
	}
	mapping.buildid_valid = get_build_id(mapping.buildid);
	if (!mapping.buildid_valid) {
		mapping.checksum = get_checksum_for_file(mapping.filename);
		cverb << vmisc << "checksum for file " << mapping.filename << ": "
		      << hex << mapping.checksum << endl;
	} else {
		cverb << vmisc << "buildid for file " << mapping.filename << ": "
		      << mapping.buildid << endl;
	}
	mmappings[mapping.start_addr] = mapping;
}

/* This method should only be invoked when a "delayed" COMM event is processed.
 * By "delayed", I mean that we have already received MMAP events for the associated
 * process, for which we've had to create a partial operf_process_info object -- one
 * that has no app_name yet and is marked invalid.
 *
 * Given the above statement, the passed app_shortname "must" come from a comm.comm
 * field, which is 16 chars in length (thus the name of the arg).
 */
void operf_process_info::process_deferred_mappings(string app_shortname)
{
	app_name = app_shortname;
	app_basename = app_shortname;
	valid = true;
	map<u64, struct operf_mmap>::iterator it = deferred_mmappings.begin();
	while ((it != deferred_mmappings.end()) &&
			(num_app_chars_matched < (int)app_basename.length())) {
		process_new_mapping(it->second);
		cverb << vmisc << "Processed deferred mapping for " << it->second.filename << endl;
		it++;
	}
	deferred_mmappings.clear();
}

int operf_process_info::get_num_matching_chars(string mapped_filename, string & basename)
{
	size_t app_length;
	size_t basename_length;
	const char * app_cstr, * basename_cstr;
	basename = op_basename(mapped_filename);
	if (appname_is_fullname == NOT_FULLNAME) {
		// This implies app_name is storing a short name from a COMM event
		app_length = app_name.length();
		app_cstr = app_name.c_str();
	} else {
		string app_basename = op_basename(app_name);
		app_length = app_basename.length();
		app_cstr = app_basename.c_str();
	}
	basename_length = basename.length();
	if (app_length > basename_length)
		return -1;

	basename_cstr = basename.c_str();
	int num_matched_chars = 0;
	for (size_t i = 0; i < app_length; i++) {
		if (app_cstr[i] == basename_cstr[i])
			num_matched_chars++;
		else
			break;
	}
	return num_matched_chars;
}

const struct operf_mmap * operf_process_info::find_mapping_for_sample(u64 sample_addr)
{
	map<u64, struct operf_mmap>::iterator it = mmappings.begin();
	while (it != mmappings.end()) {
		if (sample_addr >= it->second.start_addr && sample_addr <= it->second.end_addr)
			return &(it->second);
		it++;
	}
	return NULL;
}

