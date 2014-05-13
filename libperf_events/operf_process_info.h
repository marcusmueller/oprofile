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

extern verbose vmisc;

struct operf_mmap {
	u64 start_addr;
	u64 end_addr;
	u64 pgoff;
	bool is_anon_mapping;
	bool is_hypervisor;
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
 * prior to the COMM event.
 */
class operf_process_info {
public:
	operf_process_info(pid_t tgid, const char * appname, bool app_arg_is_fullname,
	                   bool is_valid);
	~operf_process_info(void);
	bool is_valid(void) { return (valid); }
	bool is_appname_valid(void) { return (valid && appname_valid); }
	void set_valid(void) { valid = true; }
	void set_appname_valid(void) { appname_valid = true; }
	bool is_forked(void) { return forked; }
	void process_mapping(struct operf_mmap * mapping, bool do_self);
	void process_hypervisor_mapping(u64 ip);
	void connect_forked_process_to_parent(void);
	void set_fork_info(operf_process_info * parent);
	void add_forked_pid_association(operf_process_info * forked_pid)
	{ forked_processes.push_back(forked_pid); }
	void copy_mappings_to_forked_process(operf_process_info * forked_pid);
	void try_disassociate_from_parent(char * appname);
	void remove_forked_process(pid_t forked_pid);
	std::string get_app_name(void) { return _appname; }
	const struct operf_mmap * find_mapping_for_sample(u64 sample_addr, bool hypervisor_sample);
	void set_appname(const char * appname, bool app_arg_is_fullname);
	void check_mapping_for_appname(struct operf_mmap * mapping);


private:
	typedef enum {
		NOT_FULLNAME,
		MAYBE_FULLNAME,
		YES_FULLNAME
	} op_fullname_t;
	pid_t pid;
	std::string _appname;
	bool valid, appname_valid, look_for_appname_match;
	bool forked;
	op_fullname_t appname_is_fullname;
	std::string app_basename;
	int  num_app_chars_matched;
	std::map<u64, struct operf_mmap *> mmappings;
	std::map<u64, bool> mmappings_from_parent;
	/* When a FORK event is received, we associate that forked process
	 * with its parent by adding it to the parent's forked_processes
	 * collection. The main reason we need this collection is because
	 * PERF_RECORD_MMAP events may arrive for the parent out of order,
	 * after a PERF_RECORD_FORK.  Since forked processes inherit their
	 * parent's mmappings, we want to make sure those mmappings exist
	 * for the forked process so that samples may be properly attributed.
	 * Therefore, the various paths of adding mmapings to a parent, will
	 * also result in adding those mmappings to forked children.
	 */
	std::vector<operf_process_info *> forked_processes;
	operf_process_info * parent_of_fork;
	void set_new_mapping_recursive(struct operf_mmap * mapping, bool do_self);
	int get_num_matching_chars(std::string mapped_filename, std::string & basename);
	void find_best_match_appname_all_mappings(void);
};


#endif /* OPERF_PROCESS_INFO_H_ */
