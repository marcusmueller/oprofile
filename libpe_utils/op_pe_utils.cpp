/**
 * @file op_pe_utils.cpp
 * General utility functions for tools using Linux Performance Events Subsystem.
 *
 * @remark Copyright 2013 OProfile authors
 * @remark Read the file COPYING
 *
 * Created on: May 21, 2013
 * @author Maynard Johnson
 * (C) Copyright IBM Corp. 2013
 *
 */

#include <linux/perf_event.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <asm/unistd.h>
#include <string.h>
#include <signal.h>

#include <iostream>
#include <set>
#include <stdexcept>
#include <sstream>
#include <string>

#include "config.h"
// HAVE_LIBPFM is defined in config.h
#ifdef HAVE_LIBPFM
#include <perfmon/pfmlib.h>
#endif
#include "op_config.h"
#include "op_types.h"
#include "op_pe_utils.h"
#include "operf_event.h"
#include "op_libiberty.h"
#include "cverb.h"
#include "op_string.h"
#include "op_netburst.h"
#include "op_events.h"


extern verbose vdebug;
extern std::vector<operf_event_t> events;
extern op_cpu cpu_type;


using namespace std;

// Global functions

int op_pe_utils::op_get_next_online_cpu(DIR * dir, struct dirent *entry)
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

	errno = 0;
	snprintf(cpu_online_pathname, 40, "/sys/devices/system/cpu/cpu%u/online", cpu_num);
	if ((online = fopen(cpu_online_pathname, "r")) == NULL) {
		cerr << "Unable to open " << cpu_online_pathname << endl;
		if (errno)
			cerr << strerror(errno) << endl;
		return -1;
	}
	res = fgetc(online);
	fclose(online);
	if (res == OFFLINE)
		goto again;
	else
		return cpu_num;
}

int op_pe_utils::op_get_sys_value(const char * filename)
{
	char str[10];
	int _val = -999;
	FILE * fp = fopen(filename, "r");
	if (fp == NULL)
		return _val;
	if (fgets(str, 9, fp))
		sscanf(str, "%d", &_val);
	fclose(fp);
	return _val;
}

int op_pe_utils::op_get_cpu_for_perf_events_cap(void)
{
	int retval;
	string err_msg;
	char cpus_online[257];
	FILE * online_cpus;
	DIR *dir = NULL;

	int total_cpus = sysconf(_SC_NPROCESSORS_ONLN);
	if (!total_cpus) {
		err_msg = "Internal Error (1): Number of online cpus cannot be determined.";
		retval = -1;
		goto error;
	}

	online_cpus = fopen("/sys/devices/system/cpu/online", "r");
	if (!online_cpus) {
		err_msg = "Internal Error (2): Number of online cpus cannot be determined.";
		retval = -1;
		goto error;
	}
	memset(cpus_online, 0, sizeof(cpus_online));

	if ( fgets(cpus_online, sizeof(cpus_online), online_cpus) == NULL) {
		fclose(online_cpus);
		err_msg = "Internal Error (3): Number of online cpus cannot be determined.";
		retval = -1;
		goto error;
	}

	if (!cpus_online[0]) {
		fclose(online_cpus);
		err_msg = "Internal Error (4): Number of online cpus cannot be determined.";
		retval = -1;
		goto error;

	}
	if (index(cpus_online, ',') || cpus_online[0] != '0') {
		// A comma in cpus_online implies a gap, which in turn implies that not all
		// CPUs are online.
		if ((dir = opendir("/sys/devices/system/cpu")) == NULL) {
			fclose(online_cpus);
			err_msg = "Internal Error (5): Number of online cpus cannot be determined.";
			retval = -1;
			goto error;
		} else {
			struct dirent *entry = NULL;
			retval = op_get_next_online_cpu(dir, entry);
			closedir(dir);
		}
	} else {
		// All CPUs are available, so we just arbitrarily choose CPU 0.
		retval = 0;
	}
	fclose(online_cpus);
error:
	return retval;
}

int op_pe_utils::op_check_perf_events_cap(bool use_cpu_minus_one)
{
	/* If perf_events syscall is not implemented, the syscall below will fail
	 * with ENOSYS (38).  If implemented, but the processor type on which this
	 * program is running is not supported by perf_events, the syscall returns
	 * ENOENT (2).
	 */
	struct perf_event_attr attr;
	pid_t pid ;
	int cpu_to_try = use_cpu_minus_one ? -1 : op_get_cpu_for_perf_events_cap();
	errno = 0;
        memset(&attr, 0, sizeof(attr));
        attr.size = sizeof(attr);
        attr.sample_type = PERF_SAMPLE_IP;

	pid = getpid();
	syscall(__NR_perf_event_open, &attr, pid, cpu_to_try, -1, 0);
	return errno;
}

static const char * appname;
static int find_app_file_in_dir(const struct dirent * d)
{
	if (!strcmp(d->d_name, appname))
		return 1;
	else
		return 0;
}

static char full_pathname[PATH_MAX];
static int _get_PATH_based_pathname(const char * app_name)
{
	int retval = -1;

	char * real_path = getenv("PATH");
	char * path = (char *) xstrdup(real_path);
	char * segment = strtok(path, ":");
	appname = app_name;
	while (segment) {
		struct dirent ** namelist;
		int rc = scandir(segment, &namelist, find_app_file_in_dir, NULL);
		if (rc < 0) {
			if (errno != ENOENT) {
				cerr << strerror(errno) << endl;
				cerr << app_name << " cannot be found in your PATH." << endl;
				break;
			}
		} else if (rc == 1) {
			size_t applen = strlen(app_name);
			size_t dirlen = strlen(segment);

			if (applen + dirlen + 2 > PATH_MAX) {
				cerr << "Path segment " << segment
				     << " prepended to the passed app name is too long"
				     << endl;
				retval = -1;
				break;
			}

			if (!strcmp(segment, ".")) {
				if (getcwd(full_pathname, PATH_MAX) == NULL) {
					retval = -1;
					cerr << "getcwd [3] failed when processing <cur-dir>/" << app_name << " found via PATH. Aborting."
							<< endl;
					break;
				}
			} else {
				strncpy(full_pathname, segment, dirlen);
			}
			strcat(full_pathname, "/");
			strncat(full_pathname, app_name, applen);
			retval = 0;
			free(namelist[0]);
			free(namelist);

			break;
		}
		segment = strtok(NULL, ":");
	}
	free(path);
	return retval;
}

int op_pe_utils::op_validate_app_name(char ** app, char ** save_appname)
{
	int rc = 0;
	struct stat filestat;
	char * app_name = *app;
	size_t len = strlen(app_name);

	if (len > (size_t) (OP_APPNAME_LEN - 1)) {
		cerr << "app name longer than max allowed (" << OP_APPNAME_LEN
		     << " chars)\n";
		cerr << app_name << endl;
		rc = -1;
		goto out;
	}

	if (index(app_name, '/') == app_name) {
		// Full pathname of app was specified, starting with "/".
		strncpy(full_pathname, app_name, len);
	} else if ((app_name[0] == '.') && (app_name[1] == '/')) {
		// Passed app is in current directory; e.g., "./myApp"
		if (getcwd(full_pathname, PATH_MAX) == NULL) {
			rc = -1;
			cerr << "getcwd [1] failed when trying to find app name " << app_name << ". Aborting."
			     << endl;
			goto out;
		}
		strcat(full_pathname, "/");
		if ((strlen(full_pathname) + strlen(app_name + 2) + 1) > PATH_MAX) {
			rc = -1;
			cerr << "Length of current dir (" << full_pathname << ") and app name ("
			     << (app_name + 2) << ") exceeds max allowed (" << PATH_MAX << "). Aborting."
			     << endl;
			goto out;
		}
		strcat(full_pathname, (app_name + 2));
	} else if (index(app_name, '/')) {
		// Passed app is in a subdirectory of cur dir; e.g., "test-stuff/myApp"
		if (getcwd(full_pathname, PATH_MAX) == NULL) {
			rc = -1;
			cerr << "getcwd [2] failed when trying to find app name " << app_name << ". Aborting."
			     << endl;
			goto out;
		}
		strcat(full_pathname, "/");
		strcat(full_pathname, app_name);
	} else {
		// Passed app name, at this point, MUST be found in PATH
		rc = _get_PATH_based_pathname(app_name);
	}

	if (rc) {
		cerr << "Problem finding app name " << app_name << ". Aborting."
		     << endl;
		goto out;
	}
	*save_appname = app_name;
	*app = full_pathname;
	if (stat(*app, &filestat)) {
		char msg[OP_APPNAME_LEN + 50];
		snprintf(msg, OP_APPNAME_LEN + 50, "Non-existent app name \"%s\"",
		         *app);
		perror(msg);
		rc = -1;
	}

	out: return rc;
}

set<int> op_pe_utils::op_get_available_cpus(int max_num_cpus)
{
	struct dirent *entry = NULL;
	int rc = 0;
	bool all_cpus_avail = true;
	DIR *dir = NULL;
	string err_msg;
	char cpus_online[257];
	set<int> available_cpus;
	FILE * online_cpus = fopen("/sys/devices/system/cpu/online", "r");

	if (max_num_cpus == -1) {
		if (online_cpus)
			fclose(online_cpus);
		return available_cpus;
	}

	if (!online_cpus) {
		err_msg = "Internal Error: Number of online cpus cannot be determined.";
		rc = -1;
		goto out;
	}
	memset(cpus_online, 0, sizeof(cpus_online));
	if (fgets(cpus_online, sizeof(cpus_online), online_cpus) == NULL) {
		fclose(online_cpus);
		err_msg = "Internal Error: Number of online cpus cannot be determined.";
		rc = -1;
		goto out;

	}
	if (index(cpus_online, ',') || cpus_online[0] != '0') {
		all_cpus_avail = false;
		if ((dir = opendir("/sys/devices/system/cpu")) == NULL) {
			fclose(online_cpus);
			err_msg = "Internal Error: Number of online cpus cannot be determined.";
			rc = -1;
			goto out;
		}
	}
	fclose(online_cpus);

	for (int cpu = 0; cpu < max_num_cpus; cpu++) {
		int real_cpu;
		if (all_cpus_avail) {
			available_cpus.insert(cpu);
		} else {
			real_cpu = op_get_next_online_cpu(dir, entry);
			if (real_cpu < 0) {
				err_msg = "Internal Error: Number of online cpus cannot be determined.";
				rc = -1;
				goto out;
			}
			available_cpus.insert(real_cpu);
		}
	}
out:
	if (dir)
		closedir(dir);
	if (rc)
		throw runtime_error(err_msg);
	return available_cpus;

}


static void _get_event_code(operf_event_t * event, op_cpu cpu_type)
{
	FILE * fp;
	char oprof_event_code[11];
	string command;
	u64 base_code, config;
	char buf[20];
	if ((snprintf(buf, 20, "%lu", event->count)) < 0) {
		cerr << "Error parsing event count of " << event->count << endl;
		exit(EXIT_FAILURE);
	}
	base_code = config = 0ULL;

	command = OP_BINDIR;
	command += "ophelp ";
	command += event->name;

	fp = popen(command.c_str(), "r");
	if (fp == NULL) {
		cerr << "Unable to execute ophelp to get info for event "
		     << event->name << endl;
		exit(EXIT_FAILURE);
	}
	if (fgets(oprof_event_code, sizeof(oprof_event_code), fp) == NULL) {
		pclose(fp);
		cerr << "Unable to find info for event "
		     << event->name << endl;
		exit(EXIT_FAILURE);
	}

	pclose(fp);

	base_code = strtoull(oprof_event_code, (char **) NULL, 10);


#if defined(__i386__) || defined(__x86_64__)
	// Setup EventSelct[11:8] field for AMD
	const char * vendor_AMD = "AuthenticAMD";
	if (op_is_cpu_vendor((char *)vendor_AMD)) {
		config = base_code & 0xF00ULL;
		config = config << 32;
	}

	// Setup EventSelct[7:0] field
	config |= base_code & 0xFFULL;
#endif
#if defined(__i386__) || defined(__x86_64__) || defined(__powerpc64__)
	char mask[OP_MAX_UM_NAME_LEN];
// Setup unitmask field
handle_named_um:
	if (event->um_name[0]) {
		command = OP_BINDIR;
		command += "ophelp ";
		command += "--extra-mask ";
		command += event->name;
		command += ":";
		command += buf;
		command += ":";
		command += event->um_name;
		fp = popen(command.c_str(), "r");
		if (fp == NULL) {
			cerr << "Unable to execute ophelp to get info for event "
			     << event->name << endl;
			exit(EXIT_FAILURE);
		}
		if (fgets(mask, sizeof(mask), fp) == NULL) {
			pclose(fp);
			cerr << "Unable to find unit mask info for " << event->um_name << " for event "
			     << event->name << endl;
			exit(EXIT_FAILURE);
		}
		pclose(fp);
		// FIXME:  The mask value here is the extra bits from the named unit mask.  It's not
		// ideal to put that value into the UM's mask, since that's what will show up in
		// opreport.  It would be better if we could somehow have the unit mask name that the
		// user passed to us show up in opreort.
		event->evt_um = strtoull(mask, (char **) NULL, 10);
		/* A value >= EXTRA_MIN_VAL returned by 'ophelp --extra-mask' is interpreted as a
		 * valid extra value; otherwise we interpret it as a simple unit mask value
		 * for a named unit mask with EXTRA_NONE.
		 */
		if (event->evt_um >= EXTRA_MIN_VAL)
			config |= event->evt_um;
		else
			config |= ((event->evt_um & 0xFFULL) << 8);
	} else if (!event->evt_um) {
		char * endptr;
		command.clear();
		command = OP_BINDIR;
		command += "ophelp ";
		command += "--unit-mask ";
		command += event->name;
		command += ":";
		command += buf;
		fp = popen(command.c_str(), "r");
		if (fp == NULL) {
			cerr << "Unable to execute ophelp to get unit mask for event "
			     << event->name << endl;
			exit(EXIT_FAILURE);
		}
		if (fgets(mask, sizeof(mask), fp) == NULL) {
			pclose(fp);
			cerr << "Unable to find unit mask info for event " << event->name << endl;
			exit(EXIT_FAILURE);
		}
		pclose(fp);
		event->evt_um = strtoull(mask, &endptr, 10);
		if ((endptr >= mask) &&
				(endptr <= (mask + strlen(mask) - 2))) { // '- 2' to account for linefeed and '\0'

			// Must be a default named unit mask
			strncpy(event->um_name, mask, OP_MAX_UM_NAME_LEN - 1);
			goto handle_named_um;
		}
#if defined(__powerpc64__)
		config = base_code;
		config |= ((event->evt_um & 0xFFULL) << 32);
#else
		config |= ((event->evt_um & 0xFFULL) << 8);
#endif
	} else {
		config |= ((event->evt_um & 0xFFULL) << 8);
	}
#else
	config = base_code;
#endif

	event->op_evt_code = base_code;
	if (cpu_type == CPU_P4 || cpu_type == CPU_P4_HT2) {
		if (op_netburst_get_perf_encoding(event->name, event->evt_um, 1, 1, &config)) {
			cerr << "Unable to get event encoding for " << event->name << endl;
			exit(EXIT_FAILURE);
		}
	}
	event->evt_code = config;
	cverb << vdebug << "Final event code is " << hex << event->evt_code << endl;
}

#if PPC64_ARCH
/* All ppc64 events (except CYCLES) have a _GRP<n> suffix.  This is
 * because the legacy opcontrol profiler can only profile events in
 * the same group (i.e., having the same _GRP<n> suffix).  But operf
 * can multiplex events, so we should allow the user to pass event
 * names without the _GRP<n> suffix.
 *
 * If event name is not CYCLES or does not have a _GRP<n> suffix,
 * we'll call ophelp and scan the list of events, searching for one
 * that matches up to the _GRP<n> suffix.  If we don't find a match,
 * then we'll exit with the expected error message for invalid event name.
 */
static string _handle_powerpc_event_spec(string event_spec)
{
	FILE * fp;
	char line[MAX_INPUT];
	size_t grp_pos;
	string evt, err_msg;
	size_t evt_name_len;
	bool first_non_cyc_evt_found = false;
	bool event_found = false;
	char event_name[OP_MAX_EVT_NAME_LEN], * remaining_evt_spec, * colon_start;
	string cmd = OP_BINDIR;
	cmd += "/ophelp";

	colon_start = (char *)index(event_spec.c_str(), ':');
	if (colon_start)
		evt_name_len = colon_start - event_spec.c_str();
	else
		evt_name_len = event_spec.length();
	strncpy(event_name, event_spec.c_str(), evt_name_len);
	event_name[evt_name_len] = '\0';
	remaining_evt_spec = colon_start ?
	                                  ((char *)event_spec.c_str() + strlen(event_name) + 1)
	                                  : NULL;
	if (!strcmp("CYCLES", event_name)) {
		event_found = true;
		goto out;
	}

	evt = event_name;
	// Need to make sure the event name truly has a _GRP<n> suffix.
	grp_pos = evt.rfind("_GRP");
	if ((grp_pos != string::npos) && ((evt = evt.substr(grp_pos, string::npos))).length() > 4) {
		char * end;
		strtoul(evt.substr(4, string::npos).c_str(), &end, 0);
		if (end && (*end == '\0')) {
		// Valid group number found after _GRP, so we can skip to the end.
			event_found = true;
			goto out;
		}
	}

	// If we get here, it implies the user passed a non-CYCLES event without a GRP suffix.
	// Lets try to find a valid suffix for it.
	fp = popen(cmd.c_str(), "r");
	if (fp == NULL) {
		cerr << "Unable to execute ophelp to get info for event "
		     << event_spec << endl;
		exit(EXIT_FAILURE);
	}

	err_msg = "Cannot find event ";
	while (fgets(line, MAX_INPUT, fp)) {
		if (!first_non_cyc_evt_found) {
			if (!strncmp(line, "PM_", 3))
				first_non_cyc_evt_found = true;
			else
				continue;
		}
		if (line[0] == ' ' || line[0] == '\t')
			continue;
		if (!strncmp(line, event_name, evt_name_len)) {
			// Found a potential match.  Check if it's a perfect match.
			string save_event_name = event_name;
			size_t full_evt_len = index(line, ':') - line;
			memset(event_name, '\0', OP_MAX_EVT_NAME_LEN);
			strncpy(event_name, line, full_evt_len);
			string candidate = event_name;
			if (candidate.rfind("_GRP") == evt_name_len) {
				event_found = true;
				break;
			} else {
				memset(event_name, '\0', OP_MAX_EVT_NAME_LEN);
				strncpy(event_name, save_event_name.c_str(), evt_name_len);
			}
		}
	}
	pclose(fp);

out:
	if (!event_found) {
		cerr << err_msg << event_name << endl;
		cerr << "Error retrieving info for event "
				<< event_spec << endl;
		exit(EXIT_FAILURE);
	}
	ostringstream ret_strm;
	if (remaining_evt_spec)
		ret_strm << event_name << ":" << remaining_evt_spec;
	else
		ret_strm << event_name;
	return ret_strm.str();
}


/* Some architectures (e.g., ppc64) do not use the same event value (code) for oprofile
 * and for perf_events.  The operf-record process requires event values that perf_events
 * understands, but the operf-read process requires oprofile event values.  The purpose of
 * the following method is to map the operf-record event value to a value that
 * opreport can understand.
 */

extern op_cpu cpu_type;
#define NIL_CODE ~0U

#if HAVE_LIBPFM3
static bool _get_codes_for_match(unsigned int pfm_idx, const char name[],
                                 vector<operf_event_t> * evt_vec)
{
	unsigned int num_events = evt_vec->size();
	int tmp_code, ret;
	bool edge_detect = false;
	char evt_name[OP_MAX_EVT_NAME_LEN];
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
		} else if (strstr(event.name, "_GRP")) {
			string str = event.name;
			strncpy(evt_name, event.name, str.rfind("_GRP"));
		} else {
			strncpy(evt_name, event.name, strlen(event.name));
		}

		/* Events where the "_EDGE_COUNT" suffix has been appended to a
		 * real native event name are pseudo events (events that have
		 * not been formally defined in processor documentation), where
		 * we wish to detect the rising edge of the real native event.
		 * This "edge detection" technique is useful for events that normally
		 * count the number of cycles that a particular condition is true.
		 * Since libpfm does not know about pseudo events, we need to
		 * convert them to their real native event equivalent, and then
		 * set the "edge detect" bit (the LSB) in the event code.
		 */
		string evt = evt_name;
		size_t edge_suffix_pos = evt.rfind("_EDGE_COUNT");
		if (edge_suffix_pos != string::npos) {
			evt = evt.substr(0, edge_suffix_pos);
			strncpy(evt_name, evt.c_str(), evt.length() + 1);
			edge_detect = true;
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
		// Setting LSB of code makes this a "rising edge detection" type of event
		if (edge_detect)
			event.evt_code |= 1;
		(*evt_vec)[i] = event;
		events_converted++;
		cverb << vdebug << "Successfully converted " << event.name << " to perf_event code "
		      << hex << event.evt_code << endl;
	}
	return (events_converted == num_events);
}
#else
static bool _op_get_event_codes(vector<operf_event_t> * evt_vec)
{
	int ret;
	unsigned int num_events = evt_vec->size();
	bool edge_detect = false;
	char evt_name[OP_MAX_EVT_NAME_LEN];
	unsigned int events_converted = 0;
	u64 code[1];

	typedef struct {
		u64    *codes;
		char        **fstr;
		size_t      size;
		int         count;
		int         idx;
	} pfm_raw_pmu_encode_t;

	pfm_raw_pmu_encode_t raw;
	raw.codes = code;
	raw.count = 1;
	raw.fstr = NULL;

	if (pfm_initialize() != PFM_SUCCESS)
		throw runtime_error("Unable to initialize libpfm; cannot continue");

	for (unsigned int i = 0; i < num_events; i++) {
		operf_event_t event = (*evt_vec)[i];
		if (event.evt_code != NIL_CODE) {
			events_converted++;
			continue;
		}
		memset(evt_name, 0, OP_MAX_EVT_NAME_LEN);
		if (!strcmp(event.name, "CYCLES")) {
			strcpy(evt_name ,"PM_CYC") ;
		} else if (strstr(event.name, "_GRP")) {
			string str = event.name;
			strncpy(evt_name, event.name, str.rfind("_GRP"));
		} else {
			strncpy(evt_name, event.name, strlen(event.name));
		}

		/* Events where the "_EDGE_COUNT" suffix has been appended to a
		 * real native event name are pseudo events (events that have
		 * not been formally defined in processor documentation), where
		 * we wish to detect the rising edge of the real native event.
		 * This "edge detection" technique is useful for events that normally
		 * count the number of cycles that a particular condition is true.
		 * Since libpfm does not know about pseudo events, we need to
		 * convert them to their real native event equivalent, and then
		 * set the "edge detect" bit (the LSB) in the event code.
		 */
		string evt = evt_name;
		size_t edge_suffix_pos = evt.rfind("_EDGE_COUNT");
		if (edge_suffix_pos != string::npos) {
			evt = evt.substr(0, edge_suffix_pos);
			strncpy(evt_name, evt.c_str(), evt.length() + 1);
			edge_detect = true;
		}

		memset(&raw, 0, sizeof(raw));
		ret = pfm_get_os_event_encoding(evt_name, PFM_PLM3, PFM_OS_NONE, &raw);
		if (ret != PFM_SUCCESS) {
			string evt_name_str = event.name;
			string msg = "libpfm cannot find event code for " + evt_name_str +
					"; cannot continue";
			throw runtime_error(msg);
		}
		event.evt_code = raw.codes[0];
		// Setting LSB of code makes this a "rising edge detection" type of event
		if (edge_detect)
			event.evt_code |= 1;
		(*evt_vec)[i] = event;
		events_converted++;
		cverb << vdebug << "Successfully converted " << event.name << " to perf_event code "
		      << hex << event.evt_code << endl;
	}
	return (events_converted == num_events);
}
#endif

static bool convert_event_vals(vector<operf_event_t> * evt_vec)
{
	for (unsigned int i = 0; i < evt_vec->size(); i++) {
		operf_event_t event = (*evt_vec)[i];
		if (cpu_type == CPU_PPC64_POWER7) {
			if (!strncmp(event.name, "PM_RUN_CYC", strlen("PM_RUN_CYC"))) {
				event.evt_code = 0x600f4;
			} else if (!strncmp(event.name, "PM_RUN_INST_CMPL", strlen("PM_RUN_INST_CMPL"))) {
				event.evt_code = 0x500fa;
			} else {
				event.evt_code = NIL_CODE;
			}
		} else {
			event.evt_code = NIL_CODE;
		}
		(*evt_vec)[i] = event;
	}

#if HAVE_LIBPFM3
	unsigned int i, count;
	char name[256];
	int ret;

	if (pfm_initialize() != PFMLIB_SUCCESS)
		throw runtime_error("Unable to initialize libpfm; cannot continue");

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
#else
	return _op_get_event_codes(evt_vec);
#endif
}

#endif // PPC64_ARCH



void op_pe_utils::op_process_events_list(set<string> & passed_evts,
                                         bool do_profiling, bool do_callgraph)
{
	string cmd = OP_BINDIR;

	if (passed_evts.size() > OP_MAX_EVENTS) {
		cerr << "Number of events specified is greater than allowed maximum of "
		     << OP_MAX_EVENTS << "." << endl;
		exit(EXIT_FAILURE);
	}
	cmd += "/ophelp --check-events ";
	if (!do_profiling)
		cmd += "--ignore-count ";
	for (set<string>::iterator it = passed_evts.begin(); it != passed_evts.end(); it++) {
		FILE * fp;
		string full_cmd = cmd;
		string event_spec = *it;

#if PPC64_ARCH
		// Starting with CPU_PPC64_ARCH_V1, ppc64 events files are formatted like
		// other architectures, so no special handling is needed.
		if (cpu_type < CPU_PPC64_ARCH_V1)
			event_spec = _handle_powerpc_event_spec(event_spec);
#endif

		if (do_callgraph)
			full_cmd += " --callgraph=1 ";
		full_cmd += event_spec;
		fp = popen(full_cmd.c_str(), "r");
		if (fp == NULL) {
			cerr << "Unable to execute ophelp to get info for event "
			     << event_spec << endl;
			exit(EXIT_FAILURE);
		}
		if (fgetc(fp) == EOF) {
			pclose(fp);
			cerr << "Error retrieving info for event "
			     << event_spec << endl;
			if (do_callgraph)
				cerr << "Note: When doing callgraph profiling, the sample count must be"
				     << endl << "15 times the minimum count value for the event."  << endl;
			exit(EXIT_FAILURE);
		}
		pclose(fp);
		char * event_str = op_xstrndup(event_spec.c_str(), event_spec.length());
		operf_event_t event;
		memset(&event, 0, sizeof(event));
		strncpy(event.name, strtok(event_str, ":"), OP_MAX_EVT_NAME_LEN - 1);
		if (do_profiling)
			event.count = atoi(strtok(NULL, ":"));
		else
			event.count = 0UL;
		/* Event name is required in the event spec in order for
		 * 'ophelp --check-events' to pass.  But since unit mask
		 *  and domain control bits are optional, we need to ensure the result of
		 *  strtok is valid.
		 */
		char * info;
#define	_OP_UM 1
#define	_OP_KERNEL 2
#define	_OP_USER 3
		int place =  _OP_UM;
		char * endptr = NULL;
		event.evt_um = 0UL;
		event.no_kernel = 0;
		event.no_user = 0;
		event.throttled = false;
		event.mode_specified = false;
		event.umask_specified = false;
		memset(event.um_name, '\0', OP_MAX_UM_NAME_LEN);
		memset(event.um_numeric_val_as_str, '\0', OP_MAX_UM_NAME_STR_LEN);
		while ((info = strtok(NULL, ":"))) {
			switch (place) {
			case _OP_UM:
				event.evt_um = strtoul(info, &endptr, 0);
				event.umask_specified = true;

				// If any of the UM part is not a number, then we
				// consider the entire part a string.
				if (*endptr) {
					event.evt_um = 0;
					strncpy(event.um_name, info, OP_MAX_UM_NAME_LEN - 1);
				} else {
					/* event.evt_um gets modified later,
					 * save the specified number as a
					 * string to output later.
					 */
					stringstream strs;
					strs << "0x" << hex << event.evt_um;
					strncpy(event.um_numeric_val_as_str,
						(char *)strs.str().c_str(),
                                                strs.str().length());
				}
				break;
			case _OP_KERNEL:
				event.mode_specified = true;
				if (atoi(info) == 0)
					event.no_kernel = 1;
				break;
			case _OP_USER:
				event.mode_specified = true;
				if (atoi(info) == 0)
					event.no_user = 1;
				break;
			}
			place++;
		}
		free(event_str);

#ifdef __s390__
		if (do_profiling) {
			if (strncmp(event.name, "CPU_CYCLES", strlen(event.name)) != 0) {
				cerr << "Profiling with " << event.name << " is not supported." << endl
				     << "Only CPU_CYCLES is allowed to use with operf." << endl;
				exit(EXIT_FAILURE);
			}
		} else {
			if (!event.no_kernel && event.no_user) {
				cerr << "Counting for just the kernel is not supported." << endl
				     << "Re-run the command and simply pass the event name " << endl
				     << "(" << event.name << ") for the event spec, without" << endl
				     << "unit mask/kernel/user bits." << endl;
				exit(EXIT_FAILURE);
			}
		}
#endif

#ifdef __alpha__
		// Alpha arch does not support any mode exclusion, so if either user or kernel
		// mode are excluded by the user, we'll exit with an error message.
		if (event.no_kernel || event.no_user) {
			cerr << "Mode exclusion is not supported on Alpha." << endl
			     << "Re-run the command and simply pass the event name " << endl
			     << "(" << event.name << ") for the event spec, without" << endl
			     << "unit mask/kernel/user bits." << endl;
			exit(EXIT_FAILURE);
		}
#endif

		_get_event_code(&event, cpu_type);
		events.push_back(event);
	}
#if PPC64_ARCH
	{
		/* For ppc64 architecture processors prior to the introduction of
		 * architected_events_v1, the oprofile event code needs to be converted
		 * to the appropriate event code to pass to the perf_event_open syscall.
		 * But as of the introduction of architected_events_v1, the events
		 * file contains the necessary event code information, so this conversion
		 * step is no longer needed.
		 */

		using namespace op_pe_utils;
		if ((cpu_type < CPU_PPC64_ARCH_V1) && !convert_event_vals(&events)) {
			cerr << "Unable to convert all oprofile event values to perf_event values" << endl;
			exit(EXIT_FAILURE);
		}
	}
#endif
}

void op_pe_utils::op_get_default_event(bool do_callgraph)
{
	operf_event_t dft_evt;
	struct op_default_event_descr descr;
	vector<operf_event_t> tmp_events;


	op_default_event(cpu_type, &descr);
	if (descr.name[0] == '\0') {
		cerr << "Unable to find default event" << endl;
		exit(EXIT_FAILURE);
	}

	memset(&dft_evt, 0, sizeof(dft_evt));
	if (do_callgraph) {
		struct op_event * _event;
		op_events(cpu_type);
		if ((_event = find_event_by_name(descr.name, 0, 0))) {
			dft_evt.count = _event->min_count * CALLGRAPH_MIN_COUNT_SCALE;
		} else {
			cerr << "Error getting event info for " << descr.name << endl;
			exit(EXIT_FAILURE);
		}
	} else {
		dft_evt.count = descr.count;
	}
	dft_evt.evt_um = descr.um;
	strncpy(dft_evt.name, descr.name, OP_MAX_EVT_NAME_LEN - 1);
	_get_event_code(&dft_evt, cpu_type);
	events.push_back(dft_evt);

#if PPC64_ARCH
	{
		/* This section of code is for architectures such as ppc[64] for which
		 * the oprofile event code needs to be converted to the appropriate event
		 * code to pass to the perf_event_open syscall.
		 */

		using namespace op_pe_utils;
		if ((cpu_type < CPU_PPC64_ARCH_V1) && !convert_event_vals(&events)) {
			cerr << "Unable to convert all oprofile event values to perf_event values" << endl;
			exit(EXIT_FAILURE);
		}
	}
#endif
}
