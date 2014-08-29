/**
 * @file libperf_events/operf_counter.cpp
 * C++ class implementation that abstracts the user-to-kernel interface
 * for using Linux Performance Events Subsystem.
 *
 * @remark Copyright 2011 OProfile authors
 * @remark Read the file COPYING
 *
 * Created on: Dec 7, 2011
 * @author Maynard Johnson
 * (C) Copyright IBM Corp. 2011
 *
 * Modified by Maynard Johnson <maynardj@us.ibm.com>
 * (C) Copyright IBM Corporation 2012, 2014
 *
*/

#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <signal.h>
#include <errno.h>
#include <string.h>
#include <iostream>
#include <sstream>
#include <stdlib.h>
#include "op_events.h"
#include "operf_counter.h"
#include "op_abi.h"
#include "cverb.h"
#include "operf_process_info.h"
#include "op_libiberty.h"
#include "operf_stats.h"
#include "op_pe_utils.h"


using namespace std;
using namespace OP_perf_utils;


volatile bool quit;
int sample_reads;
int num_mmap_pages;
unsigned int pagesize;
verbose vrecord("record");
verbose vconvert("convert");

extern bool first_time_processing;
extern bool throttled;
extern size_t mmap_size;
extern size_t pg_sz;
extern bool use_cpu_minus_one;
extern bool track_new_forks;

namespace {

vector<string> event_names;

static const char *__op_magic = "OPFILE";

#define OP_MAGIC	(*(u64 *)__op_magic)

static bool _print_pp_progress(int fd)
{
	int msg;
	if (read(fd, &msg, sizeof(msg)) > 0)
		return true;
	else
		return false;
}

/* This function for reading an event from the sample data pipe must
 * be robust enough to handle the situation where the operf_record process
 * writes an event record to the pipe in multiple chunks.
 */
#define OP_PIPE_READ_OK 0
#define OP_PIPE_CLOSED -1
static int _get_perf_event_from_pipe(event_t * event, int sample_data_fd)
{
	static size_t pe_header_size = sizeof(perf_event_header);
	size_t read_size = pe_header_size;
	int rc = OP_PIPE_READ_OK;
	char * evt = (char *)event;
	ssize_t num_read;
	perf_event_header * header = (perf_event_header *)event;

	memset(header, '\0', pe_header_size);

	/* A signal handler was setup for the operf_read process to handle interrupts
	 * (i.e., from ctrl-C), so the read syscalls below may get interrupted.  But the
	 * operf_read process should ignore the interrupt and continue processing
	 * until there's no more data to read or until the parent operf process
	 * forces us to stop.  So we must try the read operation again if it was
	 * interrupted.
	 */
again:
	errno = 0;
	if ((num_read = read(sample_data_fd, header, read_size)) < 0) {
		cverb << vdebug << "Read 1 of sample data pipe returned with " << strerror(errno) << endl;
		if (errno == EINTR) {
			goto again;
		} else {
			rc = OP_PIPE_CLOSED;
			goto out;
		}
	} else if (num_read == 0) {
		// Implies pipe has been closed on the write end, so return -1 to quit reading
		rc = OP_PIPE_CLOSED;
		goto out;
	} else if (num_read != (ssize_t)read_size) {
		header += num_read;
		read_size -= num_read;
		goto again;
	}

	read_size = header->size - pe_header_size;
	if (read_size == 0)
		/* This is technically a valid record -- it's just empty. I'm not
		 * sure if this can happen (i.e., if the kernel ever creates empty
		 * records), but we'll handle it just in case.
		 */
		goto again;

	if (!header->size || (header->size < pe_header_size))
		/* Bogus header size detected. In this case, we don't set rc to -1,
		 * because the caller will catch this error when it calls is_header_valid().
		 * I've seen such bogus stuff occur when profiling lots of processes at
		 * a very high sampling frequency. This issue is still being investigated,
		 * so for now, we'll just do our best to detect and handle gracefully.
		 */
		goto out;

	evt += pe_header_size;

again2:
	if ((num_read = read(sample_data_fd, evt, read_size)) < 0) {
		cverb << vdebug << "Read 2 of sample data pipe returned with " << strerror(errno) << endl;
		if (errno == EINTR) {
			goto again2;
		} else {
			rc = OP_PIPE_CLOSED;
			if (errno == EFAULT)
				cerr << "Size of event record: " << header->size << endl;
			goto out;
		}
	} else if (num_read == 0) {
		// Implies pipe has been closed on the write end, so return -1 to quit reading
		rc = OP_PIPE_CLOSED;
		goto out;
	} else if (num_read != (ssize_t)read_size) {
		evt += num_read;
		read_size -= num_read;
		goto again2;
	}

out:
	return rc;
}

static event_t * _get_perf_event_from_file(struct mmap_info & info)
{
	uint32_t size = 0;
	static int num_remaps = 0;
	event_t * event;
	size_t pe_header_size = sizeof(struct perf_event_header);

try_again:
	event = NULL;
	if (info.offset + info.head + pe_header_size > info.file_data_size)
		goto out;

	if (info.head + pe_header_size <= mmap_size)
		event = (event_t *)(info.buf + info.head);

	if (unlikely(!event || (info.head + event->header.size > mmap_size))) {
		int ret;
		u64 shift = pg_sz * (info.head / pg_sz);
		cverb << vdebug << "Remapping perf data file: " << dec << ++num_remaps << endl;
		ret = munmap(info.buf, mmap_size);
		if (ret) {
			string errmsg = "Internal error:  munmap of perf data file failed with errno: ";
			errmsg += strerror(errno);
			throw runtime_error(errmsg);
		}

		info.offset += shift;
		info.head -= shift;
		ret = op_mmap_trace_file(info, false);
		if (ret) {
			string errmsg = "Internal error:  mmap of perf data file failed with errno: ";
			errmsg += strerror(errno);
			throw runtime_error(errmsg);
		}
		goto try_again;
	}

	size = event->header.size;
	info.head += size;
out:
	if (unlikely(!event)) {
		cverb << vdebug << "No more event records in file.  info.offset: " << dec << info.offset
		      << "; info.head: " << info.head << "; info.file_data_size: " << info.file_data_size
		      << endl << "; mmap_size: " << mmap_size  << "; current record size: " << size << endl;
	}
	return event;
}

}  // end anonymous namespace

operf_counter::operf_counter(operf_event_t & evt,  bool enable_on_exec, bool do_cg,
                             bool separate_cpu, bool inherit, int event_number)
{
	memset(&attr, 0, sizeof(attr));
	attr.size = sizeof(attr);
	attr.sample_type = OP_BASIC_SAMPLE_FORMAT;
	if (do_cg)
		attr.sample_type |= PERF_SAMPLE_CALLCHAIN;
	if (separate_cpu)
		attr.sample_type |= PERF_SAMPLE_CPU;

#ifdef __s390__
	attr.type = PERF_TYPE_HARDWARE;
#else
	attr.type = PERF_TYPE_RAW;
#endif
#if ((defined(__i386__) || defined(__x86_64__)) && (HAVE_PERF_PRECISE_IP))
	if (evt.evt_code & EXTRA_PEBS) {
		attr.precise_ip = 2;
		evt.evt_code ^= EXTRA_PEBS;
	}
#endif
	attr.exclude_hv = evt.no_hv;
	attr.config = evt.evt_code;
	attr.sample_period = evt.count;
	attr.inherit = inherit ? 1 : 0;
	attr.enable_on_exec = enable_on_exec ? 1 : 0;
	attr.disabled  = 1;
	attr.exclude_idle = 0;
	attr.exclude_kernel = evt.no_kernel;
	attr.read_format = PERF_FORMAT_ID;
	event_name = evt.name;
	fd = id = -1;
	evt_num = event_number;
}

operf_counter::~operf_counter() {
}


int operf_counter::perf_event_open(pid_t pid, int cpu, operf_record * rec, bool print_error)
{
	struct {
		u64 count;
		u64 id;
	} read_data;

	if (evt_num == 0) {
		attr.mmap = 1;
		attr.comm = 1;
	}
	fd = op_perf_event_open(&attr, pid, cpu, -1, 0);
	if (fd < 0) {
		int ret = -1;
		cverb << vrecord << "perf_event_open failed: " << strerror(errno) << endl;
		if (errno == EBUSY) {
			if (print_error) {
				cerr << "The performance monitoring hardware reports EBUSY. Is another profiling tool in use?" << endl
				     << "On some architectures, tools such as oprofile and perf being used in system-wide "
				     << "mode can cause this problem." << endl;
			}
			ret = OP_PERF_HANDLED_ERROR;
		} else if (errno == ESRCH) {
			if (print_error) {
				cerr << "!!!! No samples collected !!!" << endl;
				cerr << "The target program/command ended before profiling was started." << endl;
			}
			ret = OP_PERF_HANDLED_ERROR;
		} else {
			if (print_error)
				cerr << "perf_event_open failed with " << strerror(errno) << endl;
		}
		return ret;
	}
	if (read(fd, &read_data, sizeof(read_data)) == -1) {
		perror("Error reading perf_event fd");
		return -1;
	}
	rec->register_perf_event_id(evt_num, read_data.id, attr);

	cverb << vrecord << "perf_event_open returning fd " << fd << endl;
	return fd;
}

operf_record::~operf_record()
{
	cverb << vrecord << "operf_record::~operf_record()" << endl;
	opHeader.data_size = total_bytes_recorded;
	// If recording to a file, we re-write the op_header info
	// in order to update the data_size field.
	if (total_bytes_recorded && write_to_file)
		write_op_header_info();

	if (poll_data)
		delete[] poll_data;
	for (size_t i = 0; i < samples_array.size(); i++) {
		struct mmap_data *md = &samples_array[i];
		munmap(md->base, (num_mmap_pages + 1) * pagesize);
	}
	samples_array.clear();
	evts.clear();
	perfCounters.clear();
	/* Close output_fd last. If sample data was being written to a pipe, we want
	 * to give the pipe reader (i.e., operf_read::convertPerfData) as much time
	 * as possible in order to drain the pipe of any remaining data.
	 */
	close(output_fd);
}

operf_record::operf_record(int out_fd, bool sys_wide, pid_t the_pid, bool pid_running,
                           vector<operf_event_t> & events, vmlinux_info_t vi, bool do_cg,
                           bool separate_by_cpu, bool out_fd_is_file,
                           int _convert_read_pipe, int _convert_write_pipe)
{
	struct sigaction sa;
	sigset_t ss;
	vmlinux_file = vi.image_name;
	kernel_start = vi.start;
	kernel_end = vi.end;
	pid_to_profile = the_pid;
	pid_started = pid_running;
	system_wide = sys_wide;
	callgraph = do_cg;
	separate_cpu = separate_by_cpu;
	total_bytes_recorded = 0;
	poll_count = 0;
	evts = events;
	valid = false;
	poll_data = NULL;
	output_fd = out_fd;
	read_comm_pipe = _convert_read_pipe;
	write_comm_pipe = _convert_write_pipe;
	write_to_file = out_fd_is_file;
	opHeader.data_size = 0;
	num_cpus = -1;

	if (system_wide && (pid_to_profile != -1 || pid_started))
		return;  // object is not valid

	cverb << vrecord << "operf_record ctor using output fd " << output_fd << endl;

	memset(&sa, 0, sizeof(struct sigaction));
	sa.sa_sigaction = op_perfrecord_sigusr1_handler;
	sigemptyset(&sa.sa_mask);
	sigemptyset(&ss);
	sigaddset(&ss, SIGUSR1);
	sigprocmask(SIG_UNBLOCK, &ss, NULL);
	sa.sa_mask = ss;
	sa.sa_flags = SA_NOCLDSTOP | SA_SIGINFO;
	cverb << vrecord << "calling sigaction" << endl;
	if (sigaction(SIGUSR1, &sa, NULL) == -1) {
		cverb << vrecord << "operf_record ctor: sigaction failed; errno is: "
		      << strerror(errno) << endl;
		_exit(EXIT_FAILURE);
	}
	cverb << vrecord << "calling setup" << endl;
	setup();
}

int operf_record::_write_header_to_file(void)
{
	struct OP_file_header f_header;
	struct op_file_attr f_attr;
	int total = 0;

	if (lseek(output_fd, sizeof(f_header), SEEK_SET) == (off_t)-1)
		goto err_out;


	for (unsigned i = 0; i < evts.size(); i++) {
		opHeader.h_attrs[i].id_offset = lseek(output_fd, 0, SEEK_CUR);
		if (opHeader.h_attrs[i].id_offset == (off_t)-1)
			goto err_out;
		total += op_write_output(output_fd, &opHeader.h_attrs[i].ids[0],
		                         opHeader.h_attrs[i].ids.size() * sizeof(u64));
	}

	opHeader.attr_offset = lseek(output_fd, 0, SEEK_CUR);
	if (opHeader.attr_offset == (off_t)-1)
		goto err_out;

	for (unsigned i = 0; i < evts.size(); i++) {
		struct op_header_evt_info attr = opHeader.h_attrs[i];
		f_attr.attr = attr.attr;
		f_attr.ids.offset = attr.id_offset;
		f_attr.ids.size = attr.ids.size() * sizeof(u64);
		total += op_write_output(output_fd, &f_attr, sizeof(f_attr));
	}

	opHeader.data_offset = lseek(output_fd, 0, SEEK_CUR);
	if (opHeader.data_offset == (off_t)-1)
		goto err_out;


	f_header.magic = OP_MAGIC;
	f_header.size = sizeof(f_header);
	f_header.attr_size = sizeof(f_attr);
	f_header.attrs.offset = opHeader.attr_offset;
	f_header.attrs.size = evts.size() * sizeof(f_attr);
	f_header.data.offset = opHeader.data_offset;
	f_header.data.size = opHeader.data_size;

	if (lseek(output_fd, 0, SEEK_SET) == (off_t)-1)
		goto err_out;
	total += op_write_output(output_fd, &f_header, sizeof(f_header));
	if (lseek(output_fd, opHeader.data_offset + opHeader.data_size, SEEK_SET) == (off_t)-1)
		goto err_out;
	return total;

err_out:
	string errmsg = "Internal error doing lseek: ";
	errmsg += strerror(errno);
	throw runtime_error(errmsg);
}

int operf_record::_write_header_to_pipe(void)
{
	struct OP_file_header f_header;
	struct op_file_attr f_attr;
	int total;

	f_header.magic = OP_MAGIC;
	f_header.size = sizeof(f_header);
	f_header.attr_size = sizeof(f_attr);
	f_header.attrs.size = evts.size() * sizeof(f_attr);
	f_header.data.size = 0;

	total = op_write_output(output_fd, &f_header, sizeof(f_header));

	for (unsigned i = 0; i < evts.size(); i++) {
		struct op_header_evt_info attr = opHeader.h_attrs[i];
		f_attr.attr = attr.attr;
		f_attr.ids.size = attr.ids.size() * sizeof(u64);
		total += op_write_output(output_fd, &f_attr, sizeof(f_attr));
	}

	for (unsigned i = 0; i < evts.size(); i++) {
		total += op_write_output(output_fd, &opHeader.h_attrs[i].ids[0],
		                         opHeader.h_attrs[i].ids.size() * sizeof(u64));
	}
	return total;
}

void operf_record::register_perf_event_id(unsigned event, u64 id, perf_event_attr attr)
{
	// It's overkill to blindly do this assignment below every time, since this function
	// is invoked once for each event for each cpu; but it's not worth the bother of trying
	// to avoid it.
	opHeader.h_attrs[event].attr = attr;
	ostringstream message;
	message  << "Perf header: id = " << hex << (unsigned long long)id << " for event num "
	         << event << ", code " << attr.config <<  endl;
	cverb << vrecord << message.str();
	opHeader.h_attrs[event].ids.push_back(id);
}

void operf_record::write_op_header_info()
{
	if (write_to_file)
		add_to_total(_write_header_to_file());
	else
		add_to_total(_write_header_to_pipe());
}

int operf_record::_prepare_to_record_one_fd(int idx, int fd)
{
	struct mmap_data md;
	md.prev = 0;
	md.mask = num_mmap_pages * pagesize - 1;

	if (fcntl(fd, F_SETFL, O_NONBLOCK) < 0) {
		perror("fcntl failed");
		return -1;
	}

	poll_data[idx].fd = fd;
	poll_data[idx].events = POLLIN;
	poll_count++;

	md.base = mmap(NULL, (num_mmap_pages + 1) * pagesize,
			PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
	if (md.base == MAP_FAILED) {
		if (errno == EPERM) {
			cerr << "Failed to mmap kernel profile data." << endl;
			cerr << "This issue may be caused by a non-root user running multiple operf" << endl;
			cerr << "sessions simultaneously. Try running as root or increasing the value of" << endl;
			cerr << "/proc/sys/kernel/perf_event_mlock_kb to resolve the problem." << endl << endl;
			return OP_PERF_HANDLED_ERROR;
		} else {
			perror("failed to mmap");
		}
		return -1;
	}
	samples_array.push_back(md);

	return 0;
}


int operf_record::prepareToRecord(void)
{
	int op_ctr_idx = 0;
	int rc = 0;
	errno = 0;
	if (pid_started && (procs.size() > 1)) {
		/* Implies we're profiling a thread group, where we call perf_event_open
		 * on each thread (process) in the group, passing cpu=-1.  So we'll do
		 * one mmap per thread (by way of the _prepare_to_record_one_fd function).
		 * If more than one event has been specified to profile on, we just do an
		 * ioctl PERF_EVENT_IOC_SET_OUTPUT to tie that perf_event fd with the fd
		 * of the first event of the thread.
		 */

		// Sanity check
		if ((procs.size() * evts.size()) != perfCounters.size()) {
			cerr << "Internal error: Number of fds[] (" << perfCounters.size()
			     << ") != number of processes x number of events ("
			     << procs.size() << " x " << evts.size() << ")." << endl;
			return -1;
		}
		for (unsigned int proc_idx = 0; proc_idx < procs.size(); proc_idx++) {
			int fd_for_set_output = perfCounters[op_ctr_idx].get_fd();
			for (unsigned event = 0; event < evts.size(); event++) {
				int fd =  perfCounters[op_ctr_idx].get_fd();
				if (event == 0) {
					rc = _prepare_to_record_one_fd(proc_idx, fd);
				} else {
					if ((rc = ioctl(fd, PERF_EVENT_IOC_SET_OUTPUT,
					                fd_for_set_output)) < 0)
						perror("prepareToRecord: ioctl #1 failed");
				}

				if (rc < 0)
					return rc;

				if ((rc = ioctl(fd, PERF_EVENT_IOC_ENABLE)) < 0) {
					perror("prepareToRecord: ioctl #2 failed");
					return rc;
				}
				op_ctr_idx++;
			}
		}
	} else {
		/* We're either doing a system-wide profile or a profile of a single process.
		 * We'll do one mmap per cpu.  If more than one event has been specified
		 * to profile on, we just do an ioctl PERF_EVENT_IOC_SET_OUTPUT to tie
		 * that perf_event fd with the fd of the first event of the cpu.
		 */
		if ((num_cpus * evts.size()) != perfCounters.size()) {
			cerr << "Internal error: Number of fds[] (" << perfCounters.size()
			     << ") != number of cpus x number of events ("
			     << num_cpus << " x " << evts.size() << ")." << endl;
			return -1;
		}
		for (int cpu = 0; cpu < num_cpus; cpu++) {
			int fd_for_set_output = perfCounters[op_ctr_idx].get_fd();
			for (unsigned event = 0; event < evts.size(); event++) {
				int fd = perfCounters[op_ctr_idx].get_fd();
				if (event == 0) {
					rc = _prepare_to_record_one_fd(cpu, fd);
				} else {
					if ((rc = ioctl(fd, PERF_EVENT_IOC_SET_OUTPUT,
					                fd_for_set_output)) < 0)
						perror("prepareToRecord: ioctl #3 failed");
				}

				if (rc < 0)
					return rc;

				if ((rc = ioctl(fd, PERF_EVENT_IOC_ENABLE)) < 0) {
					perror("prepareToRecord: ioctl #4 failed");
					return rc;
				}
				op_ctr_idx++;
			}
		}
	}
	return rc;
}


void operf_record::setup()
{
	bool all_cpus_avail = true;
	int rc = 0;
	struct dirent *entry = NULL;
	DIR *dir = NULL;
	string err_msg;
	char cpus_online[257];
	bool profile_process_group = false;


	if (system_wide)
		cverb << vrecord << "operf_record::setup() for system-wide profiling" << endl;
	else
		cverb << vrecord << "operf_record::setup() with pid_started = " << pid_started << endl;

	if (pid_started || system_wide) {
		if ((rc = op_get_process_info(system_wide, pid_to_profile, this)) < 0) {
			if (rc == OP_PERF_HANDLED_ERROR)
				return;
			else
				throw runtime_error("Unexpected error in operf_record setup");
		}
		// 'pid_started && (procs.size() > 1)' implies the process that the user
		// has requested us to profile has cloned one or more children.
		profile_process_group = pid_started && (procs.size() > 1);
	}

	pagesize = sysconf(_SC_PAGE_SIZE);
	// If profiling a process group, use a smaller mmap length to avoid EINVAL.
	num_mmap_pages = profile_process_group ? 1 : (512 * 1024)/pagesize;

	/* To set up to profile an existing thread group, we need call perf_event_open
	 * for each thread, and we need to pass cpu=-1 on the syscall.
	 */
	use_cpu_minus_one = use_cpu_minus_one ? true : profile_process_group;
	num_cpus = use_cpu_minus_one ? 1 : sysconf(_SC_NPROCESSORS_ONLN);
	if (num_cpus < 1) {
		char int_str[256];
		sprintf(int_str, "Number of online CPUs is %d; cannot continue", num_cpus);
		throw runtime_error(int_str);
	}

	cverb << vrecord << "calling perf_event_open for pid " << pid_to_profile << " on "
	      << num_cpus << " cpus" << endl;
	FILE * online_cpus = fopen("/sys/devices/system/cpu/online", "r");
	if (!online_cpus) {
		err_msg = "Internal Error: Number of online cpus cannot be determined.";
		rc = -1;
		goto error;
	}
	memset(cpus_online, 0, sizeof(cpus_online));
	if (fgets(cpus_online, sizeof(cpus_online), online_cpus) == NULL) {
		fclose(online_cpus);
		err_msg = "Internal Error: Number of online cpus cannot be determined.";
		rc = -1;
		goto error;

	}
	if (index(cpus_online, ',') || cpus_online[0] != '0') {
		all_cpus_avail = false;
		if ((dir = opendir("/sys/devices/system/cpu")) == NULL) {
			fclose(online_cpus);
			err_msg = "Internal Error: Number of online cpus cannot be determined.";
			rc = -1;
			goto error;
		}
	}
	fclose(online_cpus);

	for (int cpu = 0; cpu < num_cpus; cpu++) {
		int real_cpu;
		if (use_cpu_minus_one) {
			real_cpu = -1;
		} else if (all_cpus_avail) {
			real_cpu = cpu;
		} else {
			real_cpu = op_pe_utils::op_get_next_online_cpu(dir, entry);
			if (real_cpu < 0) {
				err_msg = "Internal Error: Number of online cpus cannot be determined.";
				rc = -1;
				goto error;
			}
		}
		size_t num_procs = profile_process_group ? procs.size() : 1;
		/* To profile a parent and its children, the perf_events kernel subsystem
		 * requires us to use cpu=-1 on the perf_event_open call for each of the
		 * processes in the group.  But perf_events also prevents us from specifying
		 * "inherit" on the perf_event_attr we pass to perf_event_open when cpu is '-1'.
		 */
		bool inherit = !profile_process_group;
		std::map<u32, struct comm_event>::iterator proc_it = procs.begin();
		for (unsigned proc_idx = 0; proc_idx < num_procs; proc_idx++) {
			for (unsigned event = 0; event < evts.size(); event++) {
				/* For a parent process, comm.tid==comm.pid, but for child
				 * processes in a process group, comm.pid is the parent, so
				 * we must use comm.tid for the perf_event_open call.  So
				 * we can use comm.tid for all cases.
				 */
				pid_t pid_for_open;
				if (profile_process_group)
					pid_for_open = proc_it++->second.tid;
				else
					pid_for_open = pid_to_profile;
				operf_counter op_ctr(operf_counter(evts[event],
				                                   (!pid_started && !system_wide),
				                                   callgraph, separate_cpu,
				                                   inherit, event));
				if ((rc = op_ctr.perf_event_open(pid_for_open,
				                                 real_cpu, this, true)) < 0) {
					err_msg = "Internal Error.  Perf event setup failed.";
					goto error;
				}
				perfCounters.push_back(op_ctr);
			}
		}
	}
	int num_mmaps;
	if (pid_started && (procs.size() > 1))
		num_mmaps = procs.size();
	else
		num_mmaps = num_cpus;
	poll_data = new struct pollfd [num_mmaps];
	if ((rc = prepareToRecord()) < 0) {
		err_msg = "Internal Error.  Perf event setup failed.";
		goto error;
	}
	write_op_header_info();

	// Set bit to indicate we're set to go.
	valid = true;
	if (dir)
		closedir(dir);
	return;

error:
	delete[] poll_data;
	poll_data = NULL;
	for (size_t i = 0; i < samples_array.size(); i++) {
		struct mmap_data *md = &samples_array[i];
		munmap(md->base, (num_mmap_pages + 1) * pagesize);
	}
	samples_array.clear();
	if (dir)
		closedir(dir);
	close(output_fd);
	if (rc != OP_PERF_HANDLED_ERROR)
		throw runtime_error(err_msg);
}

void operf_record::record_process_info(void)
{
	map<unsigned int, unsigned int> pids_mapped;
	pid_t last_tgid = -1;
	std::map<u32, struct comm_event>::iterator proc_it = procs.begin();
	for (unsigned int proc_idx = 0; proc_idx < procs.size(); proc_idx++, proc_it++)
	{
		struct comm_event ce = proc_it->second;
		int num = OP_perf_utils::op_write_output(output_fd, &ce, ce.header.size);
		add_to_total(num);
		if (cverb << vrecord)
			cout << "Created COMM event for " << ce.comm << endl;

		if (((pid_t)(ce.pid) == last_tgid) ||
				(pids_mapped.find(ce.pid) != pids_mapped.end()))
			continue;
		OP_perf_utils::op_record_process_exec_mmaps(ce.tid,
		                                            ce.pid,
		                                            output_fd, this);
		pids_mapped[ce.pid] = last_tgid = ce.pid;
	}
}

int operf_record::_start_recoding_new_thread(pid_t id)
{
	string err_msg;
	int num_mmaps, rc, fd_for_set_output = -1;
	struct comm_event ce;
	u64 sample_id;
	struct pollfd * old_polldata = poll_data;

	num_mmaps = sizeof(poll_data)/sizeof(poll_data[0]);
	num_mmaps++;
	poll_data = new struct pollfd [num_mmaps];
	// Copy only the existing pollfd objects from the array.  The new pollfd will
	// be filled in via the call to _prepare_to_record_one_fd.
	for (int i = 0; i < num_mmaps - 1; i++)
		poll_data[i] = old_polldata[i];
	delete[] old_polldata;
	// Make a pseudo comm_event object.  At this point, the
	// only field we need to set is tid.
	memset(&ce, 0, sizeof(ce));
	ce.tid = id;
	add_process(ce);

	for (unsigned event = 0; event < evts.size(); event++) {
		operf_counter op_ctr(operf_counter(evts[event],
		                                   (!pid_started && !system_wide),
		                                   callgraph, separate_cpu,
		                                   false, event));
		if (op_ctr.perf_event_open(id, -1, this, false) < 0) {
			sample_id = OP_PERF_NO_SAMPLE_ID;
			// Send special value to convert process to indicate failure
			ssize_t len = write(write_comm_pipe, &sample_id, sizeof(sample_id));
			if (len < 0)
				perror("Internal error on convert write_comm_pipe");
			return -1;
		}
		perfCounters.push_back(op_ctr);
		int fd = op_ctr.get_fd();
		if (event == 0) {
			rc = _prepare_to_record_one_fd(num_mmaps - 1, fd);
			fd_for_set_output = fd;
		} else {
			if ((rc = ioctl(fd, PERF_EVENT_IOC_SET_OUTPUT,
			                fd_for_set_output)) < 0)
				perror("_start_recoding_new_thread: ioctl #1 failed");
		}

		if (rc < 0)
			return rc;

		if ((rc = ioctl(fd, PERF_EVENT_IOC_ENABLE)) < 0) {
			perror("_start_recoding_new_thread: ioctl #2 failed");
			return rc;
		}

		sample_id = opHeader.h_attrs[event].ids.back();
		ssize_t len = write(write_comm_pipe, &sample_id, sizeof(sample_id));
		if (len < 0)
			perror("Internal error on convert write_comm_pipe");
		else if (len != sizeof(sample_id))
			cerr << "Incomplete write convert to write_comm_pipe" << endl;
		else
			cverb << vrecord << "Sent sample_id " << sample_id << " to convert process" << endl;
	}

	return 0;
}


void operf_record::recordPerfData(void)
{
	bool disabled = false;
	if (pid_started || system_wide)
		record_process_info();
	else
		op_get_vsyscall_mapping(pid_to_profile, output_fd, this);

	op_record_kernel_info(vmlinux_file, kernel_start, kernel_end, output_fd, this);
	cerr << "operf: Profiler started" << endl;
	while (1) {
		int prev = sample_reads;
		pid_t pi;
		ssize_t len;

		for (size_t i = 0; i < samples_array.size(); i++) {
			if (samples_array[i].base)
				op_get_kernel_event_data(&samples_array[i], this);
		}
		if (quit && disabled)
			break;

		if (prev == sample_reads) {
			(void)poll(poll_data, poll_count, -1);
		}
		if (!quit && track_new_forks && procs.size() > 1) {
			len = read(read_comm_pipe, &pi, sizeof(pi));

			if (len < 0 && errno != EAGAIN) {
				cverb << vrecord << "Non-fatal error: read_comm_pipe returned too few bytes" << endl;
			} else if (len == sizeof(pi) && (procs.find(pi) == procs.end())) {
				// Start profiling this new thread
				cverb << vrecord << "Start recording for new thread " << pi << endl;
				// Don't treat as fatal error if it doesn't work
				if (_start_recoding_new_thread(pi) < 0)
					cerr << "Unable to collect samples for forked process " << pi
					     << ". Process may have ended before recording could be started." << endl;
			}
		}

		if (quit) {
			for (unsigned int i = 0; i < perfCounters.size(); i++)
				ioctl(perfCounters[i].get_fd(), PERF_EVENT_IOC_DISABLE);
			disabled = true;
			cverb << vrecord << "operf_record::recordPerfData received signal to quit." << endl;
		}
	}

	cverb << vdebug << "operf recording finished." << endl;
}

void operf_read::init(int sample_data_pipe_fd, string input_filename, string samples_loc, op_cpu cputype,
                      bool systemwide, int _record_write_pipe, int _record_read_pipe,
                      int _post_profiling_pipe)
{
	sample_data_fd = sample_data_pipe_fd;
	read_comm_pipe = _record_read_pipe;
	write_comm_pipe = _record_write_pipe;
	post_profiling_pipe = _post_profiling_pipe;
	inputFname = input_filename;
	sampledir = samples_loc;
	cpu_type = cputype;
	syswide = systemwide;
}

operf_read::~operf_read()
{
	evts.clear();
}

void operf_read::add_sample_id_to_opHeader(u64 sample_id)
{
	for (unsigned int i = 0; i < evts.size(); i++)
		opHeader.h_attrs[i].ids.push_back(sample_id);
}

int operf_read::_read_header_info_with_ifstream(void)
{
	struct OP_file_header fheader;
	int num_fattrs, ret = 0;
	size_t fattr_size;
	istrm.seekg(0, ios_base::beg);

	if (op_read_from_stream(istrm, (char *)&fheader, sizeof(fheader)) != sizeof(fheader)) {
		cerr << "Error: input file " << inputFname << " does not have enough data for header" << endl;
		ret = OP_PERF_HANDLED_ERROR;
		goto out;
	}

	if (memcmp(&fheader.magic, __op_magic, sizeof(fheader.magic))) {
		cerr << "Error: input file " << inputFname << " does not have expected header data" << endl;
		ret = OP_PERF_HANDLED_ERROR;
		goto out;
	}

	cverb << vconvert << "operf magic number " << (char *)&fheader.magic << " matches expected __op_magic " << __op_magic << endl;
	opHeader.attr_offset = fheader.attrs.offset;
	opHeader.data_offset = fheader.data.offset;
	opHeader.data_size = fheader.data.size;
	fattr_size = sizeof(struct op_file_attr);
	if (fattr_size != fheader.attr_size) {
		cerr << "Error: perf_events binary incompatibility. Event data collection was apparently "
		     << endl << "performed under a different kernel version than current." << endl;
		ret = OP_PERF_HANDLED_ERROR;
		goto out;
	}
	num_fattrs = fheader.attrs.size/fheader.attr_size;
	cverb << vconvert << "num_fattrs  is " << num_fattrs << endl;
	istrm.seekg(opHeader.attr_offset, ios_base::beg);
	for (int i = 0; i < num_fattrs; i++) {
		struct op_file_attr f_attr;
		streamsize fattr_size = sizeof(f_attr);
		if (op_read_from_stream(istrm, (char *)&f_attr, fattr_size) != fattr_size) {
			cerr << "Error: Unexpected end of input file " << inputFname << "." << endl;
			ret = OP_PERF_HANDLED_ERROR;
			goto out;
		}
		opHeader.h_attrs[i].attr = f_attr.attr;
		streampos next_f_attr = istrm.tellg();
		int num_ids = f_attr.ids.size/sizeof(u64);
		istrm.seekg(f_attr.ids.offset, ios_base::beg);
		for (int id = 0; id < num_ids; id++) {
			u64 perf_id;
			streamsize perfid_size = sizeof(perf_id);
			if (op_read_from_stream(istrm, (char *)& perf_id, perfid_size) != perfid_size) {
				cerr << "Error: Unexpected end of input file " << inputFname << "." << endl;
				ret = OP_PERF_HANDLED_ERROR;
				goto out;
			}
			ostringstream message;
			message << "Perf header: id = " << hex << (unsigned long long)perf_id << endl;
			cverb << vconvert << message.str();
			opHeader.h_attrs[i].ids.push_back(perf_id);
		}
		istrm.seekg(next_f_attr, ios_base::beg);
	}
out:
	istrm.close();
	return ret;
}

int operf_read::_read_perf_header_from_file(void)
{
	int ret = 0;

	opHeader.data_size = 0;
	istrm.open(inputFname.c_str(), ios_base::in);
	if (!istrm.good()) {
		valid = false;
		cerr << "Input stream bad for " << inputFname << endl;
		ret = OP_PERF_HANDLED_ERROR;
		goto out;
	}
	istrm.peek();
	if (istrm.eof()) {
		cverb << vconvert << "operf_read::readPerfHeader:  Empty profile data file." << endl;
		valid = false;
		ret = OP_PERF_HANDLED_ERROR;
		goto out;
	}
	cverb << vconvert << "operf_read: successfully opened input file " << inputFname << endl;
	if ((ret = _read_header_info_with_ifstream()) == 0) {
		valid = true;
		cverb << vconvert << "Successfully read perf header" << endl;
	} else {
		valid = false;
	}
out:
	return ret;
}

int operf_read::_read_perf_header_from_pipe(void)
{
	struct OP_file_header fheader;
	string errmsg;
	int num_fattrs;
	size_t fattr_size;
	vector<struct op_file_attr> f_attr_cache;

	errno = 0;
	if (read(sample_data_fd, &fheader, sizeof(fheader)) != sizeof(fheader)) {
		errmsg = "Error reading header on sample data pipe: " + string(strerror(errno));
		goto fail;
	}

	if (memcmp(&fheader.magic, __op_magic, sizeof(fheader.magic))) {
		errmsg = "Error: operf sample data does not have expected header data";
		goto fail;
	}

	cverb << vconvert << "operf magic number " << (char *)&fheader.magic << " matches expected __op_magic " << __op_magic << endl;
	fattr_size = sizeof(struct op_file_attr);
	if (fattr_size != fheader.attr_size) {
		errmsg = "Error: perf_events binary incompatibility. Event data collection was apparently "
				"performed under a different kernel version than current.";
		goto fail;
	}
	num_fattrs = fheader.attrs.size/fheader.attr_size;
	cverb << vconvert << "num_fattrs  is " << num_fattrs << endl;
	for (int i = 0; i < num_fattrs; i++) {
		struct op_file_attr f_attr;
		streamsize fattr_size = sizeof(f_attr);
		if (read(sample_data_fd, (char *)&f_attr, fattr_size) != fattr_size) {
			errmsg = "Error reading file attr on sample data pipe: " + string(strerror(errno));
			goto fail;
		}
		opHeader.h_attrs[i].attr = f_attr.attr;
		f_attr_cache.push_back(f_attr);
	}
	for (int i = 0; i < num_fattrs; i++) {
		vector<struct op_file_attr>::iterator it = f_attr_cache.begin();
		struct op_file_attr f_attr = *(it);
		int num_ids = f_attr.ids.size/sizeof(u64);

		for (int id = 0; id < num_ids; id++) {
			u64 perf_id;
			streamsize perfid_size = sizeof(perf_id);
			if (read(sample_data_fd, (char *)& perf_id, perfid_size) != perfid_size) {
				errmsg = "Error reading perf ID on sample data pipe: " + string(strerror(errno));
				goto fail;
			}
			ostringstream message;
			message << "Perf header: id = " << hex << (unsigned long long)perf_id << endl;
			cverb << vconvert << message.str();
			opHeader.h_attrs[i].ids.push_back(perf_id);
		}

	}
	valid = true;
	cverb << vconvert << "Successfully read perf header" << endl;
	return 0;

fail:
	cerr << errmsg;
	return OP_PERF_HANDLED_ERROR;
}

int operf_read::readPerfHeader(void)
{
	if (!inputFname.empty())
		return _read_perf_header_from_file();
	else
		return _read_perf_header_from_pipe();
}

int operf_read::get_eventnum_by_perf_event_id(u64 id) const
{
	for (unsigned i = 0; i < evts.size(); i++) {
		struct op_header_evt_info attr = opHeader.h_attrs[i];
		for (unsigned j = 0; j < attr.ids.size(); j++) {
			if (attr.ids[j] == id)
				return i;
		}
	}
	return -1;
}


unsigned int operf_read::convertPerfData(void)
{
	unsigned int num_bytes = 0;
	struct mmap_info info;
	bool error = false;
	event_t * event = NULL;

	if (fcntl(post_profiling_pipe, F_SETFL, O_NONBLOCK) < 0) {
		cerr << "Error: fcntl failed with errno:\n\t" << strerror(errno) << endl;
		throw runtime_error("Error: Unable to set post_profiling_pipe to non blocking");
	}

	if (!inputFname.empty()) {
		info.file_data_offset = opHeader.data_offset;
		info.file_data_size = opHeader.data_size;
		cverb << vdebug << "Expecting to read approximately " << dec
		      << info.file_data_size - info.file_data_offset
		      << " bytes from operf sample data file." << endl;
		info.traceFD = open(inputFname.c_str(), O_RDONLY);
		if (info.traceFD == -1) {
			cerr << "Error: open failed with errno:\n\t" << strerror(errno) << endl;
			throw runtime_error("Error: Unable to open operf data file");
		}
		cverb << vdebug << "operf_read opened " << inputFname << endl;
		pg_sz = sysconf(_SC_PAGESIZE);
		if (op_mmap_trace_file(info, true) < 0) {
			close(info.traceFD);
			throw runtime_error("Error: Unable to mmap operf data file");
		}
	} else {
		// Allocate way more than enough space for a really big event with a long callchain
		event = (event_t *)xmalloc(65536);
		memset(event, '\0', 65536);
	}

	for (int i = 0; i < OPERF_MAX_STATS; i++)
		operf_stats[i] = 0;

	ostringstream message;
	message << "Converting operf data to oprofile sample data format" << endl;
	message << "sample type is " << hex <<  opHeader.h_attrs[0].attr.sample_type << endl;
	cverb << vdebug << message.str();
	first_time_processing = true;
	int num_recs = 0;
	struct perf_event_header last_header;
	bool print_progress = !inputFname.empty() && syswide;
	bool printed_progress_msg = false;
	while (1) {
		streamsize rec_size = 0;
		if (!inputFname.empty()) {
			event = _get_perf_event_from_file(info);
			if (event == NULL)
				break;
		} else {
			if (_get_perf_event_from_pipe(event, sample_data_fd) < 0)
				break;
		}
		rec_size = event->header.size;

		if ((!is_header_valid(event->header)) ||
				((op_write_event(event, opHeader.h_attrs[0].attr.sample_type)) < 0)) {
			error = true;
			last_header = event->header;
			break;
		}
		num_bytes += rec_size;
		num_recs++;
		if ((num_recs % 1000000 == 0) && (print_progress || _print_pp_progress(post_profiling_pipe))) {
			if (!printed_progress_msg) {
				cerr << "\nConverting profile data to OProfile format " << endl;
				printed_progress_msg = true;
			}
			cerr << ".";
		}
	}
	if (unlikely(error)) {
		if (!inputFname.empty()) {
			cerr << "ERROR: operf_read::convertPerfData quitting. Bad data read from file." << endl;
		} else {
			cerr << "ERROR: operf_read::convertPerfData quitting. Bad data read from pipe." << endl;
			cerr << "Closing read end of data pipe. operf-record process will stop with SIGPIPE (13)."
			     << endl;
		}
		cerr << "Try lowering the sample frequency to avoid this error; e.g., double the 'count'"
		     << endl << "value in your event specification." << endl;
		cverb << vdebug << "Event header type: " << last_header.type << "; size: " << last_header.size << endl;
	}

	first_time_processing = false;
	if (!error)
		op_reprocess_unresolved_events(opHeader.h_attrs[0].attr.sample_type, print_progress);

	if (printed_progress_msg)
		cerr << endl;

	op_release_resources();
	operf_print_stats(operf_options::session_dir, start_time_human_readable, throttled, evts);

	char * cbuf;
	cbuf = (char *)xmalloc(operf_options::session_dir.length() + 5);
	strcpy(cbuf, operf_options::session_dir.c_str());
	strcat(cbuf, "/abi");
	op_write_abi_to_file(cbuf);
	free(cbuf);
	if (!inputFname.empty())
		close(info.traceFD);
	else
		free(event);
	return num_bytes;
}
