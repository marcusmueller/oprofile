/**
 * @file operf_utils.cpp
 * Helper methods for perf_events-based OProfile.
 *
 * @remark Copyright 2011 OProfile authors
 * @remark Read the file COPYING
 *
 * Created on: Dec 7, 2011
 * @author Maynard Johnson
 * (C) Copyright IBM Corp. 2011
 *
 * Modified by Maynard Johnson <maynardj@us.ibm.com>
 * (C) Copyright IBM Corporation 2012
 *
 */

#include <errno.h>
#include <dirent.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include <cverb.h>
#include <iostream>
#include "operf_counter.h"
#include "operf_utils.h"
#ifdef HAVE_LIBPFM
#include <perfmon/pfmlib.h>
#endif
#include "op_types.h"
#include "operf_process_info.h"
#include "file_manip.h"
#include "operf_kernel.h"
#include "operf_sfile.h"
#include "op_fileio.h"
#include "op_libiberty.h"


extern verbose vmisc;
extern volatile bool quit;
extern volatile bool read_quit;
extern operf_read operfRead;
extern int sample_reads;
extern unsigned int pagesize;
extern char * app_name;
extern verbose vperf;

using namespace std;

map<pid_t, operf_process_info *> process_map;
multimap<string, struct operf_mmap *> all_images_map;
map<u64, struct operf_mmap *> kernel_modules;
struct operf_mmap * kernel_mmap;
bool first_time_processing;

static list<event_t *> unresolved_events;
static struct operf_transient trans;
static bool sfile_init_done;

/* The handling of mmap's for a process was a bit tricky to get right, in particular,
 * the handling of what I refer to as "deferred mmap's" -- i.e., when we receive an
 * mmap event for which we've not yet received a comm event (so we don't know app name
 * for the process).  I have left in some debugging code here (compiled out via #ifdef)
 * so we can easily test and validate any changes we ever may need to make to this code.
 */
//#define _TEST_DEFERRED_MAPPING
#ifdef _TEST_DEFERRED_MAPPING
static bool do_comm_event;
static event_t comm_event;
#endif


/* Some architectures (e.g., ppc64) do not use the same event value (code) for oprofile
 * and for perf_events.  The operf-record process requires event values that perf_events
 * understands, but the operf-read process requires oprofile event values.  The purpose of
 * the following method is to map the operf-record event value to a value that
 * opreport can understand.
 */
#if (defined(__powerpc__) || defined(__powerpc64__))
#define NIL_CODE ~0U
static bool _get_codes_for_match(unsigned int pfm_idx, const char name[],
                                 vector<operf_event_t> * evt_vec)
{
	unsigned int num_events = evt_vec->size();
	int tmp_code, ret;
	char evt_name[OP_MAX_EVT_NAME_LEN];
	char * grp_name;
	unsigned int events_converted = 0;
	for (unsigned int i = 0; i < num_events; i++) {
		operf_event_t event = (*evt_vec)[i];
		if (event.evt_code != NIL_CODE) {
			events_converted++;
			continue;
		}
		memset(evt_name, 0, OP_MAX_EVT_NAME_LEN);
		if (!strcmp(event.name, "CYCLES")) {
			strcpy(evt_name ,"PM_CYC") ;
		} else if ((grp_name = strstr(event.name, "_GRP"))) {
			strncpy(evt_name, event.name, grp_name - event.name);
		} else {
			strncpy(evt_name, event.name, strlen(event.name));
		}
		if (strncmp(name, evt_name, OP_MAX_EVT_NAME_LEN))
			continue;
		ret = pfm_get_event_code(pfm_idx, &tmp_code);
		if (ret != PFMLIB_SUCCESS) {
			string evt_name_str = event.name;
			string msg = "libpfm cannot find event code for " + evt_name_str +
					"; cannot continue";
			throw runtime_error(msg);
		}
		event.evt_code = tmp_code;
		(*evt_vec)[i] = event;
		events_converted++;
		cverb << vperf << "Successfully converted " << event.name << " to perf_event code "
		      << hex << tmp_code << endl;
	}
	return (events_converted == num_events);
}

bool OP_perf_utils::op_convert_event_vals(vector<operf_event_t> * evt_vec)
{
	unsigned int i, count;
	char name[256];
	int ret;
	for (unsigned int i = 0; i < evt_vec->size(); i++) {
		operf_event_t event = (*evt_vec)[i];
		event.evt_code = NIL_CODE;
		(*evt_vec)[i] = event;
	}
	pfm_initialize();
	ret = pfm_get_num_events(&count);
	if (ret != PFMLIB_SUCCESS)
		throw runtime_error("Unable to use libpfm to obtain event code; cannot continue");
	for(i =0 ; i < count; i++)
	{
		ret = pfm_get_event_name(i, name, 256);
		if (ret != PFMLIB_SUCCESS)
			continue;
		if (_get_codes_for_match(i, name, evt_vec))
			break;
	}
	return (i != count);
}

#endif


static inline void update_trans_last(struct operf_transient * trans)
{
	trans->last = trans->current;
	trans->is_anon = trans->is_anon;
	trans->last_pc = trans->pc;
}


static void __handle_comm_event(event_t * event)
{
#ifdef _TEST_DEFERRED_MAPPING
	if (!do_comm_event) {
		comm_event = event;
		return;
	}
#endif
	if (cverb << vperf)
		cout << "PERF_RECORD_COMM for " << event->comm.comm << endl;
	map<pid_t, operf_process_info *>::iterator it;
	it = process_map.find(event->comm.pid);
	if (it == process_map.end()) {
		operf_process_info * proc = new operf_process_info(event->comm.pid,
		                                                   app_name ? app_name : event->comm.comm,
		                                                   app_name != NULL, true);
		if (cverb << vperf)
			cout << "Adding new proc info to collection for PID " << event->comm.pid << endl;
		process_map[event->comm.pid] = proc;
	} else {
		// sanity check -- should not get a second COMM event for same PID
		if (it->second->is_valid()) {
			if (cverb << vperf)
				cout << "Received extraneous COMM event for " << event->comm.comm
				      << ", PID " << event->comm.pid << endl;
		} else {
			if (cverb << vperf)
				cout << "Processing deferred mappings" << endl;
			it->second->process_deferred_mappings(event->comm.comm);
		}
	}
}

static void __handle_mmap_event(event_t * event)
{
	string image_basename = op_basename(event->mmap.filename);
	struct operf_mmap * mapping = NULL;
	multimap<string, struct operf_mmap *>::iterator it;
	pair<multimap<string, struct operf_mmap *>::iterator,
	     multimap<string, struct operf_mmap *>::iterator> range;

	range = all_images_map.equal_range(image_basename);
	for (it = range.first; it != range.second; it++) {
		if (((strcmp((*it).second->filename, image_basename.c_str())) == 0)
				&& ((*it).second->start_addr == event->mmap.start)) {
			mapping = (*it).second;
			break;
		}
	}
	if (!mapping) {
		mapping = new struct operf_mmap;
		memset(mapping, 0, sizeof(struct operf_mmap));
		mapping->start_addr = event->mmap.start;
	        strcpy(mapping->filename, event->mmap.filename);
		/* Mappings starting with "/" are for either a file or shared memory object.
		 * From the kernel's perf_events subsystem, anon maps have labels like:
		 *     [heap], [stack], [vdso], //anon
		 */
		if (mapping->filename[0] == '[') {
			mapping->is_anon_mapping = true;
		} else if ((strncmp(mapping->filename, "//anon",
		                    strlen("//anon")) == 0)) {
			mapping->is_anon_mapping = true;
			strcpy(mapping->filename, "anon");
		}
		mapping->end_addr = (event->mmap.len == 0ULL)? 0ULL : mapping->start_addr + event->mmap.len - 1;
		mapping->pgoff = event->mmap.pgoff;

		if (cverb << vperf) {
			cout << "PERF_RECORD_MMAP for " << event->mmap.filename << endl;
			cout << "\tstart_addr: " << hex << mapping->start_addr;
			cout << "; end addr: " << mapping->end_addr << endl;
		}

		if (event->header.misc & PERF_RECORD_MISC_USER)
			all_images_map.insert(pair<string, struct operf_mmap *>(image_basename, mapping));
	}

	if (event->header.misc & PERF_RECORD_MISC_KERNEL) {
		if (!strncmp(mapping->filename, operf_get_vmlinux_name(),
		            strlen(mapping->filename))) {
			/* The kernel_mmap is just a convenience variable
			 * for use when mapping samples to kernel space, since
			 * most of the kernel samples will be attributable to
			 * the vmlinux file versus kernel modules.
			 */
			kernel_mmap = mapping;
		} else {
			operf_create_module(mapping->filename,
			                    mapping->start_addr,
			                    mapping->end_addr);
			kernel_modules[mapping->start_addr] = mapping;
		}
	} else {
		map<pid_t, operf_process_info *>::iterator it;
		it = process_map.find(event->mmap.pid);
		if (it == process_map.end()) {
			// Create a new proc info object, but mark it invalid since we have
			// not yet received a COMM event for this PID.
			operf_process_info * proc = new operf_process_info(event->mmap.pid, app_name ? app_name : NULL,
			                                                                             app_name != NULL, false);
			proc->add_deferred_mapping(mapping);
			if (cverb << vperf)
				cout << "Added deferred mapping " << event->mmap.filename
				      << " for new process_info object" << endl;
			process_map[event->mmap.pid] = proc;
#ifdef _TEST_DEFERRED_MAPPING
			if (!do_comm_event) {
				do_comm_event = true;
				__handle_comm_event(comm_event, out);
			}
#endif
		} else if (!it->second->is_valid()) {
			it->second->add_deferred_mapping(mapping);
			if (cverb << vperf)
				cout << "Added deferred mapping " << event->mmap.filename
				      << " for existing but incomplete process_info object" << endl;
		} else {
			it->second->process_new_mapping(mapping);
		}
	}
}

static struct operf_transient * __get_operf_trans(struct sample_data * data)
{
	operf_process_info * proc = NULL;
	const struct operf_mmap * op_mmap = NULL;
	struct operf_transient * retval = NULL;

	if (trans.tgid == data->pid) {
		proc = trans.cur_procinfo;
		if (cverb << vmisc)
			cout << "trans.tgid == data->pid : " << data->pid << endl;

	} else {
		// Find operf_process info for data.tgid.
		std::map<pid_t, operf_process_info *>::const_iterator it = process_map.find(data->pid);
		if (it != process_map.end() && (it->second->is_appname_valid())) {
			proc = it->second;
			trans.cur_procinfo = proc;
		} else {
			/* This can happen for the following reasons:
			 *   - We get a sample before getting a COMM or MMAP
			 *     event for the process being profiled
			 *   - The COMM event has been processed, but since that
			 *     only gives 16 chars of the app name, we don't
			 *     have a valid app name yet
			 *   - The kernel incorrectly records a sample for a
			 *     process other than the one we requested (not
			 *     likely -- this would be a kernel bug if it did)
			 *
			 * TODO: log the lost sample if !first_time_processing
			*/
			if ((cverb << vmisc) && !first_time_processing)
				cerr << "Dropping sample -- process info unavailable" << endl;
			goto out;
		}
	}

	// Now find mmapping that contains the data.ip address.
	// Use that mmapping to set fields in trans.
	if (trans.in_kernel) {
		if (data->ip >= kernel_mmap->start_addr &&
				data->ip < kernel_mmap->end_addr) {
			op_mmap = kernel_mmap;
		} else {
			map<u64, struct operf_mmap *>::iterator it;
			it = kernel_modules.begin();
			while (it != kernel_modules.end()) {
				if (data->ip >= it->second->start_addr &&
						data->ip < it->second->end_addr) {
					op_mmap = it->second;
					break;
				}
				it++;
			}
		} if (!op_mmap) {
			if ((kernel_mmap->start_addr == 0ULL) &&
					(kernel_mmap->end_addr == 0ULL))
				op_mmap = kernel_mmap;
		}
		if (!op_mmap) {
			/* This can happen if a kernel module is loaded after profiling
			 * starts, and then we get samples for that kernel module.
			 * TODO:  Fix this.
			 */
		}
	} else {
		op_mmap = proc->find_mapping_for_sample(data->ip);
	}
	if (op_mmap) {
		if (cverb << vperf)
			cout << "Found mmap for sample; image_name is " << op_mmap->filename << endl;
		trans.image_name = op_mmap->filename;
		trans.app_filename = proc->get_app_name().c_str();
		trans.image_len = strlen(trans.image_name);
		trans.app_len = strlen(trans.app_filename);
		trans.start_addr = op_mmap->start_addr;
		trans.end_addr = op_mmap->end_addr;
		trans.tgid = data->pid;
		trans.tid = data->tid;
		trans.cpu = data->cpu;
		trans.is_anon = op_mmap->is_anon_mapping;
		if (trans.in_kernel || trans.is_anon)
			trans.pc = data->ip;
		else
			trans.pc = data->ip - trans.start_addr;

		trans.sample_id = data->id;
		retval = &trans;
	} else {
		// TODO: log lost sample due to no mapping
		/* Ditto the comment above -- i.e., we need to look into caching these
		 * discarded samples and trying to process them later.
		 */
		if ((cverb << vmisc) && !first_time_processing) {
			string domain = trans.in_kernel ? "kernel" : "userspace";
			cerr << "Discarding " << domain
			     << " sample where no mapping was found. (pc=0x"
			     << hex << data->ip <<")" << endl;
		}
		retval = NULL;
	}
out:
	return retval;
}

static void __handle_callchain(u64 * array, struct sample_data * data)
{
	data->callchain = (struct ip_callchain *) array;
	if (data->callchain->nr) {
		if (cverb << vperf)
			cout << "Processing callchain" << endl;
		for (int i = 0; i < data->callchain->nr; i++) {
			data->ip = data->callchain->ips[i];
			if (data->ip >= PERF_CONTEXT_MAX) {
				switch (data->ip) {
					case PERF_CONTEXT_HV:
						// hypervisor samples currently unsupported
						// TODO: log lost callgraph arc
						break;
					case PERF_CONTEXT_KERNEL:
						trans.in_kernel = 1;
						break;
					case PERF_CONTEXT_USER:
						trans.in_kernel = 0;
						break;
					default:
						break;
				}
				continue;
			}
			if (data->ip && __get_operf_trans(data)) {
				if ((trans.current = operf_sfile_find(&trans))) {
					operf_sfile_log_arc(&trans);
					update_trans_last(&trans);
				}
			} else {
				// TODO: log lost callgraph arc, but only for non-zero data->ip
			}
		}
	}
}

static void __handle_sample_event(event_t * event, u64 sample_type)
{
	struct sample_data data;
	bool found_trans = false;
	const struct operf_mmap * op_mmap = NULL;

	u64 *array = event->sample.array;

	if (sample_type & PERF_SAMPLE_IP) {
		data.ip = event->ip.ip;
		array++;
	}

	if (sample_type & PERF_SAMPLE_TID) {
		u_int32_t *p = (u_int32_t *)array;
		data.pid = p[0];
		data.tid = p[1];
		array++;
	}

	data.id = ~0ULL;
	if (sample_type & PERF_SAMPLE_ID) {
		data.id = *array;
		array++;
	}

	if (sample_type & PERF_SAMPLE_CPU) {
		u_int32_t *p = (u_int32_t *)array;
		data.cpu = *p;
		array++;
	}
	if (event->header.misc == PERF_RECORD_MISC_KERNEL) {
		trans.in_kernel = 1;
	} else if (event->header.misc == PERF_RECORD_MISC_USER) {
		trans.in_kernel = 0;
	} else {
		// TODO: For now, we'll discard hypervisor and guest kernel/
		// guest user samples.  We should at least log what we're
		// throwing away.
		cverb << vmisc << "Discarding sample that is neither user nor kernel domain" << endl;
		goto out;
	}


	/* Early versions of the Linux Performance Events Subsystem were a bit
	 * buggy and would record samples for processes other than the requested
	 * process ID.  The code below basically ignores these superfluous samples
	 * except for the case of PID 0.  If this case occurs right away when
	 * the static variable trans.tgid is still holding its initial value of 0,
	 * then we would incorrectly find trans.tgid and data.pid matching, and
	 * and make wrong assumptions from that match -- ending seg fault.  So we
	 * will bail out early if we see a sample for PID 0 coming in.
	 */
	if (data.pid == 0) {
		cverb << vmisc << "Discarding sample for PID 0" << endl;
		goto out;
	}

	if (cverb << vperf)
		cout << "(IP, " <<  event->header.misc << "): " << dec << data.pid << "/"
		      << data.tid << ": " << hex << (unsigned long long)data.ip
		      << endl << "\tdata ID: " << data.id << endl;

	// Verify the sample.
	trans.event = operfRead.get_eventnum_by_perf_event_id(data.id);
	if (trans.event < 0) {
		cerr << "Event num " << trans.event << " for id " << data.id
		     << " is invalid. Skipping sample." << endl;
		goto out;
	}
	/* Check for the common case first -- i.e., where the current sample is from
	 * the same context as the previous sample.  For the "no-vmlinux" case, start_addr
	 * and end_addr will be zero, so need to make sure we detect that.
	 * The last resort (and most expensive) is to call __get_operf_trans() if the
	 * sample cannot be matched up with a previous tran object.
	 */
	if (trans.in_kernel) {
		if (trans.image_name && trans.tgid == data.pid) {
			// For the no-vmlinux case . . .
			if ((trans.start_addr == 0ULL) && (trans.end_addr == 0ULL)) {
				trans.pc = data.ip;
				found_trans = true;
			// For the case where a vmlinux file is passed in . . .
			} else if (data.ip >= trans.start_addr && data.ip < trans.end_addr) {
				trans.pc = data.ip;
				found_trans = true;
			}
		}
	} else {
		if (trans.tgid == data.pid && data.ip >= trans.start_addr && data.ip < trans.end_addr) {
			trans.tid = data.tid;
			trans.pc = data.ip - trans.start_addr;
			found_trans = true;
		} else if (__get_operf_trans(&data)) {
			trans.current = operf_sfile_find(&trans);
			found_trans = true;
		}

	}
	/*
	 * trans.current may be NULL if a kernel sample falls through
	 * the cracks, or if it's a sample from an anon region we couldn't find
	 */
	if (found_trans && trans.current) {
		/* log the sample or arc */
		operf_sfile_log_sample(&trans);

		update_trans_last(&trans);
		if (sample_type & PERF_SAMPLE_CALLCHAIN)
			__handle_callchain(array, &data);
		goto out;
	}

	if (first_time_processing) {
		event_t * ev = (event_t *)xmalloc(event->header.size);
		memcpy(ev, event, event->header.size);
		unresolved_events.push_back(ev);
	}

out:
	return;
}


/* This function is used by operf_read::convertPerfData() to convert perf-formatted
 * data to oprofile sample data files.  After the header information in the perf sample data,
 * the next piece of data is typically the PERF_RECORD_COMM record which tells us the name of the
 * application/command being profiled.  This is followed by PERF_RECORD_MMAP records
 * which indicate what binary executables and libraries were mmap'ed into process memory
 * when profiling began.  Additional PERF_RECORD_MMAP records may appear later in the data
 * stream (e.g., dlopen for single-process profiling or new process startup for system-wide
 * profiling.
 */
int OP_perf_utils::op_write_event(event_t * event, u64 sample_type)
{
#if 0
	if (event->header.type < PERF_RECORD_MAX) {
		cverb << vperf << "PERF_RECORD type " << hex << event->header.type << endl;
	}
#endif

	switch (event->header.type) {
	case PERF_RECORD_SAMPLE:
		__handle_sample_event(event, sample_type);
		return 0;
	case PERF_RECORD_MMAP:
		__handle_mmap_event(event);
		return 0;
	case PERF_RECORD_COMM:
		if (!sfile_init_done) {
			operf_sfile_init();
			sfile_init_done = true;
		}
		__handle_comm_event(event);
		return 0;
	case PERF_RECORD_EXIT:
		return 0;
	default:
		// OK, ignore all other header types.
		cverb << vperf << "No matching event type for " << hex << event->header.type << endl;
		return 0;
	}
}

void OP_perf_utils::op_reprocess_unresolved_events(u64 sample_type)
{
	cverb << vperf << "Reprocessing samples" << endl;
	list<event_t *>::const_iterator it = unresolved_events.begin();
	for (; it != unresolved_events.end(); it++) {
		event_t * evt = (*it);
		// This is just a sanity check, since all events in this list
		// are unresolved sample events.
		if (evt->header.type == PERF_RECORD_SAMPLE) {
			__handle_sample_event(evt, sample_type);
			free(evt);
		}
	}
}


void OP_perf_utils::op_perfrecord_sigusr1_handler(int sig __attribute__((unused)),
		siginfo_t * siginfo __attribute__((unused)),
		void *u_context __attribute__((unused)))
{
	quit = true;
}

void OP_perf_utils::op_perfread_sigusr1_handler(int sig __attribute__((unused)),
		siginfo_t * siginfo __attribute__((unused)),
		void *u_context __attribute__((unused)))
{
	read_quit = true;
}

int OP_perf_utils::op_write_output(int output, void *buf, size_t size)
{
	int sum = 0;
	while (size) {
		int ret = write(output, buf, size);

		if (ret < 0) {
			string errmsg = "Internal error:  Failed to write sample data to pipe. errno is ";
			errmsg += strerror(errno);
			throw runtime_error(errmsg);
		}

		size -= ret;
		buf = (char *)buf + ret;
		sum  += ret;
	}
	return sum;
}


static void op_record_process_exec_mmaps(pid_t pid, pid_t tgid, int output_fd, operf_record * pr)
{
	char fname[PATH_MAX];
	FILE *fp;

	snprintf(fname, sizeof(fname), "/proc/%d/maps", tgid);

	fp = fopen(fname, "r");
	if (fp == NULL) {
		// Process must have exited already or invalid pid.
		cverb << vperf << "couldn't open " << fname << endl;
		return;
	}

	while (1) {
		char line_buffer[BUFSIZ];
		char perms[5], pathname[PATH_MAX], dev[16];
		unsigned long long start_addr, end_addr, offset;
		u_int32_t inode;

		memset(pathname, '\0', sizeof(pathname));
		struct mmap_event mmap;
		size_t size;
		mmap.pgoff = 0;
		mmap.header.type = PERF_RECORD_MMAP;
		mmap.header.misc = PERF_RECORD_MISC_USER;

		if (fgets(line_buffer, sizeof(line_buffer), fp) == NULL)
			break;

		sscanf(line_buffer, "%llx-%llx %s %llx %s %d %s",
				&start_addr, &end_addr, perms, &offset, dev, &inode, pathname);
		if (perms[2] == 'x') {
			char *imagename = strchr(pathname, '/');

			if (imagename == NULL)
				imagename = strstr(pathname, "[vdso]");

			if (imagename == NULL)
				continue;

			size = strlen(imagename) + 1;
			strcpy(mmap.filename, imagename);
			size = align_64bit(size);
			mmap.start = start_addr;
			mmap.len = end_addr - mmap.start;
			mmap.pid = tgid;
			mmap.tid = pid;
			mmap.header.size = (sizeof(mmap) -
					(sizeof(mmap.filename) - size));
			int num = OP_perf_utils::op_write_output(output_fd, &mmap, mmap.header.size);
			if (cverb << vperf)
				cout << "Created MMAP event for " << imagename << endl;
			pr->add_to_total(num);
		}
	}

	fclose(fp);
	return;
}

static int _record_one_process_info(pid_t pid, bool sys_wide, operf_record * pr,
                                    int output_fd)
{
	struct comm_event comm;
	char fname[PATH_MAX];
	char buff[BUFSIZ];
	FILE *fp;
	pid_t tgid = 0;
	size_t size = 0;
	DIR *tids;
	struct dirent dirent, *next;
	int ret = 0;

	snprintf(fname, sizeof(fname), "/proc/%d/status", pid);
	fp = fopen(fname, "r");
	if (fp == NULL) {
		/* Process must have finished or invalid PID passed into us.
		 * If we're doing system-wide profiling, this case can naturally
		 * occur, and it's not an error.  But if profiling on a single
		 * application, we can't continue after this, so we'll bail out now.
		 */
		if (!sys_wide) {
			cerr << "Unable to find process information for process " << pid << "." << endl;
			cverb << vperf << "couldn't open " << fname << endl;
			return -1;
		}
	}

	memset(&comm, 0, sizeof(comm));
	while (!comm.comm[0] || !comm.pid) {
		if (fgets(buff, sizeof(buff), fp) == NULL) {
			ret = -1;
			cverb << vperf << "Did not find Name or PID field in status file." << endl;
			goto out;
		}
		if (!strncmp(buff, "Name:", 5)) {
			char *name = buff + 5;
			while (*name && isspace(*name))
				++name;
			size = strlen(name) - 1;
			// The "Name" field in /proc/pid/status currently only allows for 16 characters,
			// but I'm not going to count on that being stable.  We'll ensure we copy no more
			// than 16 chars  since the comm.comm char array only holds 16.
			size = size > 16 ? 16 : size;
			memcpy(comm.comm, name, size++);
		} else if (memcmp(buff, "Tgid:", 5) == 0) {
			char *tgids = buff + 5;
			while (*tgids && isspace(*tgids))
				++tgids;
			tgid = comm.pid = atoi(tgids);
		}
	}

	comm.header.type = PERF_RECORD_COMM;
	size = align_64bit(size);
	comm.header.size = sizeof(comm) - (sizeof(comm.comm) - size);
	if (tgid != pid) {
		// passed pid must have been a secondary thread
		comm.tid = pid;
		int num = OP_perf_utils::op_write_output(output_fd, &comm, comm.header.size);
		pr->add_to_total(num);
		goto out;
	}

	snprintf(fname, sizeof(fname), "/proc/%d/task", pid);
	tids = opendir(fname);
	if (tids == NULL) {
		// process must have exited
		ret = -1;
		cverb << vperf << "opendir returned NULL" << endl;
		goto out;
	}

	while (!readdir_r(tids, &dirent, &next) && next) {
		char *end;
		pid = strtol(dirent.d_name, &end, 10);
		if (*end)
			continue;

		comm.tid = pid;

		int num = OP_perf_utils::op_write_output(output_fd, &comm, comm.header.size);
		pr->add_to_total(num);
	}
	closedir(tids);

out:
	op_record_process_exec_mmaps(pid, tgid, output_fd, pr);

	fclose(fp);
	if (ret) {
		cverb << vperf << "couldn't get app name and tgid for pid "
		      << dec << pid << " from /proc fs." << endl;
	} else {
		if (cverb << vperf)
			cout << "Created COMM event for " << comm.comm << endl;
	}
	return ret;

}

/* Obtain process information for an active process (where the user has
 * passed in a process ID via the --pid option) or all active processes
 * (where system_wide==true).  Then generate the necessary PERF_RECORD_COMM
 * and PERF_RECORD_MMAP entries into the profile data stream.
 */
int OP_perf_utils::op_record_process_info(bool system_wide, pid_t pid, operf_record * pr,
                                          int output_fd)
{
	int ret;
	if (cverb << vperf)
		cout << "op_record_process_info" << endl;
	if (!system_wide) {
		ret = _record_one_process_info(pid, system_wide, pr, output_fd);
	} else {
		char buff[BUFSIZ];
		pid_t tgid = 0;
		size_t size = 0;
		DIR *pids;
		struct dirent dirent, *next;

		pids = opendir("/proc");
		if (pids == NULL) {
			cerr << "Unable to open /proc." << endl;
			return -1;
		}

		while (!readdir_r(pids, &dirent, &next) && next) {
			char *end;
			pid = strtol(dirent.d_name, &end, 10);
			if (((errno == ERANGE && (pid == LONG_MAX || pid == LONG_MIN))
					|| (errno != 0 && pid == 0)) || (end == dirent.d_name)) {
				cverb << vmisc << "/proc entry " << dirent.d_name << " is not a PID" << endl;
				continue;
			}
			if ((ret = _record_one_process_info(pid, system_wide, pr, output_fd)) < 0)
				break;
		}
		closedir(pids);
	}
	return ret;
}


/*
 * each line is in the format:
 *
 * module_name 16480 1 dependencies Live 0xe091e000
 *
 * without any blank space in each field
 */
static void _record_module_info(int output_fd, operf_record * pr)
{
	const char * fname = "/proc/modules";
	FILE *fp;
	char * line;
	struct operf_kernel_image * image;
	int module_size;
	char ref_count[32+1];
	int ret;
	char module_name[256+1];
	char live_info[32+1];
	char dependencies[4096+1];
	unsigned long long start_address;

	fp = fopen(fname, "r");
	if (fp == NULL) {
		cerr << "Error opening /proc/modules. Unable to process module samples" << endl;
		cerr << strerror(errno) << endl;
		return;
	}

	while (1) {
		struct mmap_event mmap;
		size_t size;
		mmap.pgoff = 0;
		line = op_get_line(fp);

		if (!line)
			break;

		if (line[0] == '\0') {
			free(line);
			continue;
		}

		ret = sscanf(line, "%256s %u %32s %4096s %32s %llx",
			     module_name, &module_size, ref_count,
			     dependencies, live_info, &start_address);
		if (ret != 6) {
			cerr << "op_record_kernel_info: Bad /proc/modules entry: \n\t" << line << endl;
			free(line);
			continue;
		}

		mmap.header.type = PERF_RECORD_MMAP;
		mmap.header.misc = PERF_RECORD_MISC_KERNEL;
		size = strlen(module_name) + 1;
		strncpy(mmap.filename, module_name, size);
		size = align_64bit(size);
		mmap.start = start_address;
		mmap.len = module_size;
		mmap.pid = 0;
		mmap.tid = 0;
		mmap.header.size = (sizeof(mmap) -
				(sizeof(mmap.filename) - size));
		int num = OP_perf_utils::op_write_output(output_fd, &mmap, mmap.header.size);
		if (cverb << vperf)
			cout << "Created MMAP event for " << module_name << ". Size: "
			      << module_size << "; start addr: " << start_address << endl;
		pr->add_to_total(num);
		free(line);
	}
	fclose(fp);
	return;
}

void OP_perf_utils::op_record_kernel_info(string vmlinux_file, u64 start_addr, u64 end_addr,
                                          int output_fd, operf_record * pr)
{
	struct mmap_event mmap;
	size_t size;
	mmap.pgoff = 0;
	mmap.header.type = PERF_RECORD_MMAP;
	mmap.header.misc = PERF_RECORD_MISC_KERNEL;
	if (vmlinux_file.empty()) {
		size = strlen( "no_vmlinux") + 1;
		strncpy(mmap.filename, "no-vmlinux", size);
		mmap.start = 0ULL;
		mmap.len = 0ULL;
	} else {
		size = vmlinux_file.length() + 1;
		strncpy(mmap.filename, vmlinux_file.c_str(), size);
		mmap.start = start_addr;
		mmap.len = end_addr - mmap.start;
	}
	size = align_64bit(size);
	mmap.pid = 0;
	mmap.tid = 0;
	mmap.header.size = (sizeof(mmap) -
			(sizeof(mmap.filename) - size));
	int num = op_write_output(output_fd, &mmap, mmap.header.size);
	if (cverb << vperf)
		cout << "Created MMAP event of size " << mmap.header.size << " for " <<mmap.filename << ". length: "
		      << mmap.len << "; start addr: " << mmap.start << endl;
	pr->add_to_total(num);
	_record_module_info(output_fd, pr);
}

void OP_perf_utils::op_get_kernel_event_data(struct mmap_data *md, operf_record * pr)
{
	struct perf_event_mmap_page *pc = (struct perf_event_mmap_page *)md->base;
	int out_fd = pr->out_fd();

	uint64_t head = pc->data_head;
	// Comment in perf_event.h says "User-space reading the @data_head value should issue
	// an rmb(), on SMP capable platforms, after reading this value."
	rmb();

	uint64_t old = md->prev;
	unsigned char *data = ((unsigned char *)md->base) + pagesize;
	uint64_t size;
	void *buf;
	int64_t diff;

	diff = head - old;
	if (diff < 0) {
		throw runtime_error("ERROR: event buffer wrapped, which should NEVER happen.");
	}

	if (old != head)
		sample_reads++;

	size = head - old;

	if ((old & md->mask) + size != (head & md->mask)) {
		buf = &data[old & md->mask];
		size = md->mask + 1 - (old & md->mask);
		old += size;
		pr->add_to_total(op_write_output(out_fd, buf, size));
	}

	buf = &data[old & md->mask];
	size = head - old;
	old += size;
	pr->add_to_total(op_write_output(out_fd, buf, size));
	md->prev = old;
	pc->data_tail = old;
}


int OP_perf_utils::op_get_next_online_cpu(DIR * dir, struct dirent *entry)
{
#define OFFLINE 0x30
	unsigned int cpu_num;
	char cpu_online_pathname[40];
	int res;
	FILE * online;
	again:
	do {
		entry = readdir(dir);
		if (!entry)
			return -1;
	} while (entry->d_type != DT_DIR);

	res = sscanf(entry->d_name, "cpu%u", &cpu_num);
	if (res <= 0)
		goto again;
	snprintf(cpu_online_pathname, 40, "/sys/devices/system/cpu/cpu%u/online", cpu_num);
	online = fopen(cpu_online_pathname, "r");
	res = fgetc(online);
	fclose(online);
	if (res == OFFLINE)
		goto again;
	else
		return cpu_num;
}
