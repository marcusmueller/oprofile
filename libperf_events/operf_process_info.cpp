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
#include <string.h>
#include "operf_process_info.h"
#include "file_manip.h"
#include "operf_utils.h"

using namespace std;
using namespace OP_perf_utils;

operf_process_info::operf_process_info(pid_t tgid, const char * appname, bool app_arg_is_fullname, bool is_valid)
: pid(tgid), _appname(appname ? appname : ""), valid(is_valid)
{
	if (app_arg_is_fullname && appname) {
		appname_is_fullname = YES_FULLNAME;
		app_basename = op_basename(appname);
		num_app_chars_matched = (int)app_basename.length();
	} else if (appname) {
		appname_is_fullname = MAYBE_FULLNAME;
		num_app_chars_matched = -1;
		app_basename = appname;
	} else {
		appname_is_fullname = NOT_FULLNAME;
		num_app_chars_matched = -1;
		app_basename = "";
	}
	forked = false;
	parent_of_fork = NULL;
}

operf_process_info::~operf_process_info()
{
	map<u64, struct operf_mmap *>::iterator it;
	map<u64, struct operf_mmap *>::iterator end;

	if (valid) {
		it = mmappings.begin();
		end = mmappings.end();
	} else {
		it = deferred_mmappings.begin();
		end = deferred_mmappings.end();
	}
	mmappings.clear();
	deferred_mmappings.clear();
}

void operf_process_info::process_new_mapping(struct operf_mmap * mapping)
{
	// If we do not know the full pathname of our app yet,
	// let's try to determine if the passed filename is a good
	// candidate appname.

	if (!mapping->is_anon_mapping && (appname_is_fullname < YES_FULLNAME) && (num_app_chars_matched < (int)app_basename.length())) {
		string basename;
		int num_matched_chars = get_num_matching_chars(mapping->filename, basename);
		if (num_matched_chars > num_app_chars_matched) {
			appname_is_fullname = MAYBE_FULLNAME;
			_appname = mapping->filename;
			app_basename = basename;
			num_app_chars_matched = num_matched_chars;
			cverb << vmisc << "Best appname match is " << _appname << endl;
		}
	}
	mmappings[mapping->start_addr] = mapping;
	vector<operf_process_info *>::iterator it = forked_processes.begin();
	while (it != forked_processes.end()) {
		operf_process_info * p = *it;
		p->copy_new_parent_mapping(mapping);
		cverb << vmisc << "Copied new parent mapping for " << mapping->filename
		      << " for forked process " << p->pid << endl;
		it++;
	}

}

/* This method should only be invoked when a "delayed" COMM event is processed.
 * By "delayed", I mean that we have already received MMAP events for the associated
 * process, for which we've had to create a partial operf_process_info object -- one
 * that has no _appname yet and is marked invalid.
 *
 * Given the above statement, the passed app_shortname "must" come from a comm.comm
 * field, which is 16 chars in length (thus the name of the arg).
 */
void operf_process_info::process_deferred_mappings(string app_shortname)
{
	_appname = app_shortname;
	app_basename = app_shortname;
	valid = true;
	map<u64, struct operf_mmap *>::iterator it = deferred_mmappings.begin();
	while (it != deferred_mmappings.end()) {
		process_new_mapping(it->second);
		cverb << vmisc << "Processed deferred mapping for " << it->second->filename << endl;
		it++;
	}
	deferred_mmappings.clear();
	process_deferred_forked_processes();
}

int operf_process_info::get_num_matching_chars(string mapped_filename, string & basename)
{
	size_t app_length;
	size_t basename_length;
	const char * app_cstr, * basename_cstr;
	string app_basename;
	basename = op_basename(mapped_filename);
	if (appname_is_fullname == NOT_FULLNAME) {
		// This implies _appname is storing a short name from a COMM event
		app_length = _appname.length();
		app_cstr = _appname.c_str();
	} else {
		app_basename = op_basename(_appname);
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
	return num_matched_chars ? num_matched_chars : -1;
}

const struct operf_mmap * operf_process_info::find_mapping_for_sample(u64 sample_addr)
{
	map<u64, struct operf_mmap *>::iterator it = mmappings.begin();
	while (it != mmappings.end()) {
		if (sample_addr >= it->second->start_addr && sample_addr <= it->second->end_addr)
			return it->second;
		it++;
	}
	return NULL;
}

/**
 * Hypervisor samples cannot be attributed to any real binary, so we synthesize
 * an operf_mmap object with the name of "[hypervisor_bucket]".  We mark this
 * mmaping as "is_anon" so that hypervisor samples are handled in the same way as
 * anon samples (and vdso, heap, and stack) -- i.e., a sample file is created
 * with the following pieces of information in its name:
 *   - [hypervisor_bucket]
 *   - PID
 *   - address range
 *
 * The address range part is problematic for hypervisor samples, since we don't
 * know the range of sample addresses until we process all the samples.  This is
 * why we need to adjust the hypervisor_mmaping when we detect an ip that's
 * outside of the current address range.  This is also why we defer processing
 * hypervisor samples the first time through the processing of sample
 * data.  See operf_utils::__handle_sample_event for details relating to how we
 * defer processing of such samples.
 */
void operf_process_info::process_hypervisor_mapping(u64 ip)
{
	bool create_new_hyperv_mmap = true;
	u64 curr_start, curr_end;
	map<u64, struct operf_mmap *>::iterator it;
	map<u64, struct operf_mmap *>::iterator end;

	curr_end = curr_start = ~0ULL;
	if (valid) {
		it = mmappings.begin();
		end = mmappings.end();
	} else {
		it = deferred_mmappings.begin();
		end = deferred_mmappings.end();
	}
	while (it != end) {
		if (it->second->is_hypervisor) {
			struct operf_mmap * _mmap = it->second;
			curr_start = _mmap->start_addr;
			curr_end = _mmap->end_addr;
			if (curr_start > ip) {
				if (valid)
					mmappings.erase(it);
				else
					deferred_mmappings.erase(it);
				delete _mmap;
			} else {
				create_new_hyperv_mmap = false;
				if (curr_end <= ip)
					_mmap->end_addr = ip;
			}
			break;
		}
		it++;
	}

	if (create_new_hyperv_mmap) {
		struct operf_mmap * hypervisor_mmap = new struct operf_mmap;
		memset(hypervisor_mmap, 0, sizeof(struct operf_mmap));
		hypervisor_mmap->start_addr = ip;
		hypervisor_mmap->end_addr = ((curr_end == ~0ULL) || (curr_end < ip)) ? ip : curr_end;
		strcpy(hypervisor_mmap->filename, "[hypervisor_bucket]");
		hypervisor_mmap->is_anon_mapping = true;
		hypervisor_mmap->pgoff = 0;
		hypervisor_mmap->is_hypervisor = true;
		if (cverb << vmisc) {
			cout << "Synthesize mmapping for " << hypervisor_mmap->filename << endl;
			cout << "\tstart_addr: " << hex << hypervisor_mmap->start_addr;
			cout << "; end addr: " << hypervisor_mmap->end_addr << endl;
		}
		if (valid)
			process_new_mapping(hypervisor_mmap);
		else
			add_deferred_mapping(hypervisor_mmap);
	}
}

void operf_process_info::copy_mappings_to_forked_process(operf_process_info * forked_pid)
{
	map<u64, struct operf_mmap *>::iterator it = mmappings.begin();
	while (it != mmappings.end()) {
		struct operf_mmap * mapping = it->second;
		/* We can pass just the pointer of the operf_mmap object because the
		 * original object is created in operf_utils:__handle_mmap_event and
		 * is saved in the global all_images_map.
		 */
	        forked_pid->process_new_mapping(mapping);
	        it++;
	}
}

void operf_process_info::connect_forked_process_to_parent(operf_process_info * parent)
{
	forked = true;
	parent_of_fork = parent;
	if (parent->is_valid()) {
		valid = true;
		_appname = parent->get_app_name();
		if (parent->is_appname_valid() && !_appname.empty()) {
			appname_is_fullname = YES_FULLNAME;
			app_basename = op_basename(_appname);
			num_app_chars_matched = (int)app_basename.length();
		} else if (!_appname.empty()) {
			appname_is_fullname = MAYBE_FULLNAME;
			num_app_chars_matched = -1;
			app_basename = _appname;
		} else {
			appname_is_fullname = NOT_FULLNAME;
			num_app_chars_matched = -1;
			app_basename = "";
		}
		parent->copy_mappings_to_forked_process(this);
	}
}

void operf_process_info::process_deferred_forked_processes(void)
{
	vector<operf_process_info *>::iterator it = forked_processes.begin();
	while (it != forked_processes.end()) {
		operf_process_info * p = *it;
		p->connect_forked_process_to_parent(this);
		cverb << vmisc << "Processed deferred forked process " << p->pid << endl;
		it++;
	}
}

void operf_process_info::remove_forked_process(pid_t forked_pid)
{
	std::vector<operf_process_info *>::iterator it = forked_processes.begin();
	while (it != forked_processes.end()) {
		if ((*it)->pid == forked_pid) {
			forked_processes.erase(it);
			break;
		}
		it++;
	}
}

/* This function is called as a result of the following scenario:
 *   1. An operf_process_info was created for a FORK event
 *   2. The forked process was connected to (associated with) its parent,
 *      adding the parent's mmappings to the forked process's operf_process_info.
 *   3. Then the forked process does an exec, which results in a COMM
 *      event. The forked process is now considered completely separate
 *      from its parent, so we need to disassociate it from the parent.
 */
void operf_process_info::disassociate_from_parent(char * app_shortname)
{
	_appname = app_shortname;
	app_basename = app_shortname;
	appname_is_fullname = NOT_FULLNAME;
	valid = true;
	/* Now that we have a valid app shortname (from the COMM event data),
	 * let's spin through our mmappings and process them -- see if we can
	 * find one that has a good appname candidate.
	 */
	num_app_chars_matched = 0;
	map<u64, struct operf_mmap *>::iterator it = mmappings.begin();
	while (it != mmappings.end()) {
		process_new_mapping(it->second);
		it++;
	}
	parent_of_fork->remove_forked_process(this->pid);
}
