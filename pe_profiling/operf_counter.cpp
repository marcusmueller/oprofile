/**
 * @file pe_profiling/operf_counter.cpp
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
 */

#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <signal.h>
#include <errno.h>
#include <string.h>
#include <iostream>
#ifdef HAVE_LIBPFM
#include <perfmon/pfmlib.h>
#endif
#include <stdlib.h>
#include "op_events.h"
#include "operf_counter.h"
#include "cverb.h"
#include "operf_process_info.h"
#include "operf.h"

using namespace std;
using namespace OP_perf_utils;


volatile bool quit;
int sample_reads;
unsigned int pagesize;
verbose vperf("perf_events");

namespace {

vector<string> event_names;

static const char *__op_magic = "OPFILE";

#define OP_MAGIC	(*(u64 *)__op_magic)

}  // end anonymous namespace

operf_counter::operf_counter(operf_event_t evt) {
	memset(&attr, 0, sizeof(attr));
	attr.size = sizeof(attr);
	attr.sample_type = OP_BASIC_SAMPLE_FORMAT;
	attr.type = PERF_TYPE_RAW;
	attr.config = evt.evt_code;
	attr.sample_period = evt.count;
	attr.inherit = 1;
	attr.enable_on_exec = 1;
	attr.disabled  = 1;
	attr.exclude_idle = 1;
	attr.exclude_kernel = evt.no_kernel;
	attr.exclude_hv = evt.no_hv;
	attr.sample_id_all = 0;
	attr.read_format = PERF_FORMAT_ID;
	event_name = evt.name;
}

operf_counter::~operf_counter() {
}


int operf_counter::perf_event_open(pid_t ppid, int cpu, unsigned event, operf_record * rec)
{
	struct {
		u64 count;
		u64 id;
	} read_data;

	if (event == 0) {
		attr.mmap = 1;
		attr.comm = 1;
		attr.mmap_data = 1;
	}

	fd = op_perf_event_open(&attr, ppid, cpu, -1, 0);
	if (fd < 0) {
		int ret = -1;
		cverb << vperf << "perf_event_open failed: " << strerror(errno) << endl;
		if (errno == EBUSY) {
			cerr << "The performance monitoring hardware reports EBUSY. Is another profiling tool in use?" << endl
			     << "On some architectures, tools such as oprofile and perf being used in system-wide "
			     << "mode can cause this problem." << endl;
			ret = OP_PERF_HANDLED_ERROR;
		} else if (errno == ESRCH) {
			cerr << "!!!! No samples collected !!!" << endl;
			cerr << "The target program/command ended before profiling was started." << endl;
			ret = OP_PERF_HANDLED_ERROR;
		}
		return ret;
	}
	if (read(fd, &read_data, sizeof(read_data)) == -1) {
		perror("Error reading perf_event fd");
		return -1;
	}
	rec->register_perf_event_id(event, read_data.id, attr);

	cverb << vperf << "perf_event_open returned fd " << fd << endl;
	return fd;
}

operf_record::~operf_record()
{
	cverb << vperf << "operf_record::~operf_record()" << endl;
	opHeader.data_size = total_bytes_recorded;
	if (total_bytes_recorded)
		write_op_header_info();
	delete[] poll_data;
	close(outputFile);
	samples_array.clear();
	evts.clear();
	perfCounters.clear();
}

operf_record::operf_record(string outfile, pid_t the_pid, vector<operf_event_t> & events)
{
	int flags = O_CREAT|O_RDWR|O_TRUNC;
	struct sigaction sa;
	sigset_t ss;

	pid = the_pid;
	total_bytes_recorded = 0;
	poll_count = 0;
	evts = events;
	valid = false;

	opHeader.data_size = 0;
	outputFile = open(outfile.c_str(), flags, S_IRUSR|S_IWUSR);
	if (outputFile < 0) {
		string errmsg = "Internal error:  Could not create output file. errno is ";
		errmsg += strerror(errno);
		throw runtime_error(errmsg);
	}
	cverb << vperf << "operf_record ctor: successfully opened output file " << outfile << endl;

	memset(&sa, 0, sizeof(struct sigaction));
	sa.sa_sigaction = op_perfrecord_sigusr1_handler;
	sigemptyset(&sa.sa_mask);
	sigemptyset(&ss);
	sigaddset(&ss, SIGUSR1);
	sigprocmask(SIG_UNBLOCK, &ss, NULL);
	sa.sa_mask = ss;
	sa.sa_flags = SA_NOCLDSTOP | SA_SIGINFO;
	cverb << vperf << "calling sigaction" << endl;
	if (sigaction(SIGUSR1, &sa, NULL) == -1) {
		cverb << vperf << "operf_record ctor: sigaction failed; errno is: "
		      << strerror(errno) << endl;
		_exit(EXIT_FAILURE);
	}
	cverb << vperf << "calling setup" << endl;
	setup();
}


void operf_record::register_perf_event_id(unsigned event, u64 id, perf_event_attr attr)
{
	// It's overkill to blindly do this assignment below every time, since this function
	// is invoked once for each event for each cpu; but it's not worth the bother of trying
	// to avoid it.
	opHeader.h_attrs[event].attr = attr;
	cverb << vperf << "Perf header: id = " << hex << (unsigned long long)id << " for event num "
			<< event << ", code " << attr.config <<  endl;
	opHeader.h_attrs[event].ids.push_back(id);
}

void operf_record::write_op_header_info()
{
	struct OP_file_header f_header;
	struct op_file_attr f_attr;

	lseek(outputFile, sizeof(f_header), SEEK_SET);

	for (unsigned i = 0; i < evts.size(); i++) {
		opHeader.h_attrs[i].id_offset = lseek(outputFile, 0, SEEK_CUR);
		add_to_total(op_write_output(outputFile, &opHeader.h_attrs[i].ids[0],
				opHeader.h_attrs[i].ids.size() * sizeof(u64)));
	}

	opHeader.attr_offset = lseek(outputFile, 0, SEEK_CUR);

	for (unsigned i = 0; i < evts.size(); i++) {
		struct op_header_evt_info attr = opHeader.h_attrs[i];
		f_attr.attr = attr.attr;
		f_attr.ids.offset = attr.id_offset;
		f_attr.ids.size =attr.ids.size() * sizeof(u64);
		add_to_total(op_write_output(outputFile, &f_attr, sizeof(f_attr)));
	}

	opHeader.data_offset = lseek(outputFile, 0, SEEK_CUR);

	f_header.magic = OP_MAGIC;
	f_header.size = sizeof(f_header);
	f_header.attr_size = sizeof(f_attr);
	f_header.attrs.offset = opHeader.attr_offset;
	f_header.attrs.size = evts.size() * sizeof(f_attr);
	f_header.data.offset = opHeader.data_offset;
	f_header.data.size = opHeader.data_size;

	lseek(outputFile, 0, SEEK_SET);
	add_to_total(op_write_output(outputFile, &f_header, sizeof(f_header)));
	lseek(outputFile, opHeader.data_offset + opHeader.data_size, SEEK_SET);
}

int operf_record::prepareToRecord(int counter, int cpu, int fd)
{
	struct mmap_data md;;

	md.counter = counter;
	md.prev = 0;
	md.mask = NUM_MMAP_PAGES * pagesize - 1;

	fcntl(fd, F_SETFL, O_NONBLOCK);

	poll_data[cpu * evts.size() + counter].fd = fd;
	poll_data[cpu * evts.size() + counter].events = POLLIN;
	poll_count++;

	md.base = mmap(NULL, (NUM_MMAP_PAGES + 1) * pagesize,
			PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
	if (md.base == MAP_FAILED) {
		perror("failed to mmap");
		return -1;
	}
	samples_array[cpu].push_back(md);
	ioctl(fd, PERF_EVENT_IOC_ENABLE);

	return 0;
}


void operf_record::setup()
{
	bool all_cpus_avail = true;
	int rc = 0;
	struct dirent *entry = NULL;
	DIR *dir = NULL;
	string err_msg;
	char cpus_online[129];

	cverb << vperf << "operf_record::setup()" << endl;
	pagesize = sysconf(_SC_PAGE_SIZE);
	num_cpus = sysconf(_SC_NPROCESSORS_ONLN);
	if (!num_cpus)
		throw runtime_error("Number of online CPUs is zero; cannot continue");;

	poll_data = new struct pollfd [evts.size() * num_cpus];

	cverb << vperf << "calling perf_event_open for pid " << pid << " on "
	      << num_cpus << " cpus" << endl;
	FILE * online_cpus = fopen("/sys/devices/system/cpu/online", "r");
	if (!online_cpus) {
		fclose(online_cpus);
		err_msg = "Internal Error: Number of online cpus cannot be determined.";
		rc = -1;
		goto error;
	}
	memset(cpus_online, 0, sizeof(cpus_online));
	fgets(cpus_online, sizeof(cpus_online), online_cpus);
	if (!cpus_online[0]) {
		fclose(online_cpus);
		err_msg = "Internal Error: Number of online cpus cannot be determined.";
		rc = -1;
		goto error;

	}
	if (index(cpus_online, ',')) {
		all_cpus_avail = false;
		dir = opendir("/sys/devices/system/cpu");
	}
	fclose(online_cpus);

	for (int cpu = 0; cpu < num_cpus; cpu++) {
		int real_cpu;
		if (all_cpus_avail) {
			real_cpu = cpu;
		} else {
			real_cpu = op_get_next_online_cpu(dir, entry);
			if (real_cpu < 0) {
				closedir(dir);
				err_msg = "Internal Error: Number of online cpus cannot be determined.";
				rc = -1;
				goto error;
			}
		}

		// Create new row to hold operf_counter objects since we need one
		// row for each cpu. Do the same for samples_array.
		vector<struct mmap_data> tmp_mdvec;
		vector<operf_counter> tmp_pcvec;

		samples_array.push_back(tmp_mdvec);
		perfCounters.push_back(tmp_pcvec);
		for (unsigned event = 0; event < evts.size(); event++) {
			evts[event].counter = event;
			perfCounters[cpu].push_back(operf_counter(evts[event]));
			if ((rc = perfCounters[cpu][event].perf_event_open(pid, real_cpu, event, this)) < 0) {
				err_msg = "Internal Error.  Perf event setup failed.";
				goto error;
			}
			if (((rc = prepareToRecord(event, cpu, perfCounters[cpu][event].get_fd()))) < 0) {
				err_msg = "Internal Error.  Perf event setup failed.";
				goto error;
			}
		}
	}
	if (!all_cpus_avail)
		closedir(dir);
	write_op_header_info();

	op_record_process_info(pid, this, outputFile);
	// Set bit to indicate we're set to go.
	valid = true;
	return;

error:
	delete[] poll_data;
	close(outputFile);
	if (rc != OP_PERF_HANDLED_ERROR)
		throw runtime_error(err_msg);
}

void operf_record::recordPerfData(void)
{
	while (1) {
		int prev = sample_reads;

		for (int cpu = 0; cpu < num_cpus; cpu++) {
			for (unsigned int evt = 0; evt < evts.size(); evt++) {
				if (samples_array[cpu][evt].base)
					op_get_kernel_event_data(&samples_array[cpu][evt], this);
			}
		}
		if (quit)
			break;

		if (prev == sample_reads) {
			poll(poll_data, poll_count, -1);
		}

		if (quit) {
			for (int i = 0; i < num_cpus; i++) {
				for (unsigned int evt = 0; evt < evts.size(); evt++)
					ioctl(perfCounters[i][evt].get_fd(), PERF_EVENT_IOC_DISABLE);
			}
		}
	}
	cverb << vdebug << "operf recording finished." << endl;
}

void operf_read::init(string infilename, string samples_loc,  op_cpu cputype, vector<operf_event_t> & events)
{
	inputFname = infilename;
	sampledir = samples_loc;
	evts = events;
	cpu_type = cputype;
}

operf_read::~operf_read()
{
	evts.clear();
}

int operf_read::readPerfHeader(void)
{
	int ret = 0;

	opHeader.data_size = 0;
	istrm.open(inputFname.c_str(), ios_base::in);
	if (!istrm.good()) {
		return -1;
	}
	istrm.peek();
	if (istrm.eof()) {
		cverb << vperf << "operf_read::readPerfHeader:  Empty profile data file." << endl;
		valid = false;
		return OP_PERF_HANDLED_ERROR;
	}
	cverb << vperf << "operf_read: successfully opened input file " << inputFname << endl;
	read_op_header_info_with_ifstream();
	valid = true;
	cverb << vperf << "Successfully read perf header" << endl;

	return ret;
}

void operf_read::read_op_header_info_with_ifstream(void)
{
	struct OP_file_header fheader;
	istrm.seekg(0, ios_base::beg);

	if (op_read_from_stream(istrm, (char *)&fheader, sizeof(fheader)) != sizeof(fheader)) {
		throw runtime_error("Error: input file " + inputFname + " does not have enough data for header");
	}

	if (memcmp(&fheader.magic, __op_magic, sizeof(fheader.magic)))
		throw runtime_error("Error: input file " + inputFname + " does not have expected header data");

	cverb << vperf << "operf magic number " << (char *)&fheader.magic << " matches expected __op_magic " << __op_magic << endl;
	opHeader.attr_offset = fheader.attrs.offset;
	opHeader.data_offset = fheader.data.offset;
	opHeader.data_size = fheader.data.size;
	size_t fattr_size = sizeof(struct op_file_attr);
	if (fattr_size != fheader.attr_size) {
		string msg = "Error: perf_events binary incompatibility. Event data collection was apparently "
				"performed under a different kernel version than current.";
		throw runtime_error(msg);
	}
	int num_fattrs = fheader.attrs.size/fheader.attr_size;
	cverb << vperf << "num_fattrs  is " << num_fattrs << endl;
	istrm.seekg(opHeader.attr_offset, ios_base::beg);
	for (int i = 0; i < num_fattrs; i++) {
		struct op_file_attr f_attr;
		streamsize fattr_size = sizeof(f_attr);
		if (op_read_from_stream(istrm, (char *)&f_attr, fattr_size) != fattr_size)
			throw runtime_error("Error: Unexpected end of input file " + inputFname + ".");
		opHeader.h_attrs[i].attr = f_attr.attr;
		streampos next_f_attr = istrm.tellg();
		int num_ids = f_attr.ids.size/sizeof(u64);
		istrm.seekg(f_attr.ids.offset, ios_base::beg);
		for (int id = 0; id < num_ids; id++) {
			u64 perf_id;
			streamsize perfid_size = sizeof(perf_id);
			if (op_read_from_stream(istrm, (char *)& perf_id, perfid_size) != perfid_size)
				throw runtime_error("Error: Unexpected end of input file " + inputFname + ".");
			cverb << vperf << "Perf header: id = " << hex << (unsigned long long)perf_id << endl;
			opHeader.h_attrs[i].ids.push_back(perf_id);
		}
		istrm.seekg(next_f_attr, ios_base::beg);
	}
	istrm.close();
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


int operf_read::convertPerfData(void)
{
	int num_bytes = 0;
	struct mmap_info info;
	info.file_data_offset = opHeader.data_offset;
	info.file_data_size = opHeader.data_size;
	info.traceFD = open(inputFname.c_str(), O_RDONLY);
	if (op_mmap_trace_file(info) < 0) {
		close(info.traceFD);
		throw runtime_error("Error: Unable to mmap operf data file");
	}

	cverb << vdebug << "Converting operf.data to oprofile sample data format" << endl;
	while (1) {
		streamsize rec_size = 0;
		event_t * event = op_get_perf_event(info);
		if (event == NULL) {
			break;
		}
		rec_size = event->header.size;
		op_write_event(event);
		num_bytes += rec_size;
	}


	map<pid_t, operf_process_info *>::iterator it = process_map.begin();
	while (it != process_map.end())
		delete it++->second;

	process_map.clear();
	close(info.traceFD);
	return num_bytes;
}
