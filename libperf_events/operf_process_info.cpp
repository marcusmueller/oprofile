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
 *
 * Modified by Maynard Johnson <maynardj@us.ibm.com>
 * (C) Copyright IBM Corporation 2013
 *
 */

#include <stdio.h>
#include <unistd.h>
#include <iostream>
#include <sstream>
#include <map>
#include <string.h>
#include <errno.h>
#include "operf_process_info.h"
#include "file_manip.h"
#include "operf_utils.h"

using namespace std;
using namespace OP_perf_utils;

operf_process_info::operf_process_info(pid_t tgid, const char * appname,
                                       bool app_arg_is_fullname, bool is_valid)
: pid(tgid), valid(is_valid), appname_valid(false), look_for_appname_match(false),
  forked(false), appname_is_fullname(NOT_FULLNAME), num_app_chars_matched(-1)
{
	_appname = "";
	set_appname(appname, app_arg_is_fullname);
	parent_of_fork = NULL;
}

operf_process_info::~operf_process_info()
{
	map<u64, struct operf_mmap *>::iterator it;
	map<u64, struct operf_mmap *>::iterator end;

	if (valid) {
		it = mmappings.begin();
		end = mmappings.end();
	}
	mmappings.clear();
}

void operf_process_info::set_appname(const char * appname, bool app_arg_is_fullname)
{
	char exe_symlink[64];
	char exe_realpath[PATH_MAX];
	/* A combination of non-null appname and app_arg_is_fullname==true may be passed
	 * from various locations.  But a non-null appname and app_arg_is_fullname==false
	 * may only be passed as a result of a PERF_RECORD_COMM event.
	 */
	bool from_COMM_event = (appname && !app_arg_is_fullname);

	if (appname_valid)
		return;
	/* If stored _appname is not empty, it implies we've been through this function before
	 * (and would have tried the readlink method or, perhaps, fallen back to some other
	 * method to set the stored _appname).  If we're here because of something other than
	 * a COMM event (e.g. MMAP event), then we should compare our stored _appname with our
	 * collection of mmapping basenames to see if we can find an appname match; otherwise,
	 * if the passed appname is NULL, we just return, since a NULL appname won't help us here.
	 */
	if (_appname.length()) {
		if (look_for_appname_match && !from_COMM_event)
			return find_best_match_appname_all_mappings();
		else if (!appname)
			return;
	}

	snprintf(exe_symlink, 64, "/proc/%d/exe", pid);
	memset(exe_realpath, '\0', PATH_MAX);

	/* If the user is running a command via taskset, the kernel will send us a PERF_RECORD_COMM
	 * for both comm=taskset and comm=<user_command> for the same process ID !!
	 * The user will not be interested in taskset samples; thus, we ignore such COMM events.
	 * This is a hack, but there doesn't seem to be a better way around the possibility of having
	 * application samples attributed to "taskset" instead of the application.
	 */
	if (readlink(exe_symlink, exe_realpath, sizeof(exe_realpath)-1) > 0) {
		_appname = exe_realpath;
		app_basename = op_basename(_appname);
		if (!strncmp(app_basename.c_str(), "taskset", strlen("taskset"))) {
			_appname = "unknown";
			app_basename = "unknown";
		} else {
			appname_valid = true;
		}
	} else {
		/* Most likely that the process has ended already, so we'll need to determine
		 * the appname through different means.
		 */
		if (cverb << vmisc) {
			ostringstream message;
			message << "PID: " << hex << pid << " Unable to obtain appname from " << exe_symlink << endl
			        <<  "\t" << strerror(errno) << endl;
			cout << message.str();
		}
		if (appname && strcmp(appname, "taskset")) {
			_appname = appname;
			if (app_arg_is_fullname) {
				appname_valid = true;
			} else {
				look_for_appname_match = true;
			}
		} else {
			_appname = "unknown";
		}
		app_basename = _appname;
	}
	ostringstream message;
	message << "PID: " << hex << pid << " appname is set to "
	        << _appname << endl;
	cverb << vmisc << message.str();
	if (look_for_appname_match)
		find_best_match_appname_all_mappings();
}

/* This operf_process_info object may be a parent to processes that it has forked.
 * If the forked process has not done an 'exec' yet (i.e., we've not received a
 * COMM event for it), then it's still a dependent process of its parent.
 * If so, it will be in the parent's collection of forked processes.  So,
 * when adding a new mapping, we should copy that mapping to each forked
 * child's operf_process_info object.  Then, if samples are taken for that
 * mapping for that forked process, the samples can be correctly attributed.
 */
void operf_process_info::process_mapping(struct operf_mmap * mapping, bool do_self)
{
	if (!appname_valid && !is_forked()) {
		if (look_for_appname_match)
			check_mapping_for_appname(mapping);
		else
			set_appname(NULL, false);
	}
	set_new_mapping_recursive(mapping, do_self);
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

/* If we do not know the full pathname of our app yet,
 * let's try to determine if the passed filename is a good
 * candidate appname.
 * ASSUMPTION: This function is called only when look_for_appname_match==true.
 */
void operf_process_info::check_mapping_for_appname(struct operf_mmap * mapping)
{
	if (!mapping->is_anon_mapping) {
		string basename;
		int num_matched_chars = get_num_matching_chars(mapping->filename, basename);
		if (num_matched_chars > num_app_chars_matched) {
			if (num_matched_chars == (int)app_basename.length()) {
				appname_is_fullname = YES_FULLNAME;
				look_for_appname_match = false;
				appname_valid = true;
			} else {
				appname_is_fullname = MAYBE_FULLNAME;
			}
			_appname = mapping->filename;
			app_basename = basename;
			num_app_chars_matched = num_matched_chars;
			cverb << vmisc << "Best appname match is " << _appname << endl;
		}
	}
}

void operf_process_info::find_best_match_appname_all_mappings(void)
{
	map<u64, struct operf_mmap *>::iterator it;

	// We may not even have a candidate shortname (from a COMM event) for the app yet
	if (_appname == "unknown")
		return;

	it = mmappings.begin();
	while (it != mmappings.end()) {
		check_mapping_for_appname(it->second);
		it++;
	}

}

const struct operf_mmap * operf_process_info::find_mapping_for_sample(u64 sample_addr, bool hypervisor_sample)
{
	map<u64, struct operf_mmap *>::iterator it = mmappings.begin();
	while (it != mmappings.end()) {
		if (sample_addr >= it->second->start_addr && sample_addr <= it->second->end_addr &&
				it->second->is_hypervisor == hypervisor_sample)
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
	it = mmappings.begin();
	end = mmappings.end();
	while (it != end) {
		if (it->second->is_hypervisor) {
			struct operf_mmap * _mmap = it->second;
			curr_start = _mmap->start_addr;
			curr_end = _mmap->end_addr;
			if (curr_start > ip) {
				mmappings.erase(it);
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
			ostringstream message;
			message << "Synthesize mmapping for " << hypervisor_mmap->filename << endl;
			message << "\tstart_addr: " << hex << hypervisor_mmap->start_addr;
			message << "; end addr: " << hypervisor_mmap->end_addr << endl;
			cout << message.str();
		}
		process_mapping(hypervisor_mmap, false);
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
	        forked_pid->process_mapping(mapping, true);
	        it++;
	}
}

void operf_process_info::set_fork_info(operf_process_info * parent)
{
	forked = true;
	parent_of_fork = parent;
	parent_of_fork->add_forked_pid_association(this);
	parent_of_fork->copy_mappings_to_forked_process(this);
}

/* ASSUMPTION: This function should only be called during reprocessing phase
 * since we blindly set the _appname to that of the parent.  If this function
 * were called from elsewhere, the parent's _appname might not yet be fully baked.
 */
void operf_process_info::connect_forked_process_to_parent(void)
{
	if (cverb << vmisc)
		cout << "Connecting forked proc " << pid << " to parent " << parent_of_fork << endl;
	valid = true;
	_appname = parent_of_fork->get_app_name();
	app_basename = op_basename(_appname);
	appname_valid = true;
}


void operf_process_info::remove_forked_process(pid_t forked_pid)
{
	std::vector<operf_process_info *>::iterator it = forked_processes.begin();
	while (it != forked_processes.end()) {
		operf_process_info * p = *it;
		if (p->pid == forked_pid) {
			forked_processes.erase(it);
			break;
		}
		it++;
	}
}

/* See comment in operf_utils::__handle_comm_event for conditions under
 * which this function is called.
 */
void operf_process_info::try_disassociate_from_parent(char * app_shortname)
{
	if (parent_of_fork && (parent_of_fork->pid == this->pid))
		return;

	if (cverb << vmisc && parent_of_fork)
		cout << "Dis-associating forked proc " << pid
		     << " from parent " << parent_of_fork->pid << endl;

	valid = true;
	set_appname(app_shortname, false);

	map<u64, struct operf_mmap *>::iterator it = mmappings.begin();
	while (it != mmappings.end()) {
		operf_mmap * cur = it->second;
		/* mmappings from the parent may have been added to this proc info prior
		 * to this proc info becoming valid since we could not know at the time if
		 * this proc would ever be valid. But now we know it's valid (which is why
		 * we're dis-associating from the parent), so we remove these unnecessary
		 * parent mmappings.
		 */
		if (mmappings_from_parent[cur->start_addr]) {
			mmappings_from_parent[cur->start_addr] = false;
			mmappings.erase(it++);
		} else {
			process_mapping(cur, false);
			it++;
		}
	}
	if (parent_of_fork) {
		parent_of_fork->remove_forked_process(this->pid);
		parent_of_fork = NULL;
	}
	forked = false;
}

/* This function adds a new mapping to the current operf_process_info
 * and then calls the same function on each of its forked children.
 * If do_self==true, it means this function is being called by a parent
 * on a forked child's operf_process_info.  Then, if the mapping already
 * exists, we do not set the corresponding mmappings_from_parent since we
 * want to retain the knowledge that the mapping had already been added for
 * this process versus from the parent. If do_self==false, it means this
 * operf_process_info is the top-level parent and should set the corresponding
 * mmappings_from_parent to false. The mmappings_from_parent map allows us to
 * know whether to keep or discard the mapping if/when we dis-associate from
 * the parent,
 */
void operf_process_info::set_new_mapping_recursive(struct operf_mmap * mapping, bool do_self)
{
	if (do_self) {
		map<u64, struct operf_mmap *>::iterator it = mmappings.find(mapping->start_addr);
		if (it == mmappings.end())
			mmappings_from_parent[mapping->start_addr] = true;
		else
			mmappings_from_parent[mapping->start_addr] = false;
	} else {
		mmappings_from_parent[mapping->start_addr] = false;
	}
	mmappings[mapping->start_addr] = mapping;
	std::vector<operf_process_info *>::iterator it = forked_processes.begin();
	while (it != forked_processes.end()) {
		operf_process_info * fp = *it;
		fp->set_new_mapping_recursive(mapping, true);
		cverb << vmisc << "Copied new parent mapping for " << mapping->filename
		      << " for forked process " << fp->pid << endl;
		it++;
	}
}
