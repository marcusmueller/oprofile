/**
 * @file oprof_start_config.cpp
 * GUI startup config management
 *
 * @remark Copyright 2002 OProfile authors
 * @remark Read the file COPYING
 *
 * @author John Levon
 * @author Philippe Elie
 */

#include <stdio.h>

#include <sstream>
#include <fstream>
#include <iomanip>
#include <sys/utsname.h>

#include "string_manip.h"
#include "oprof_start_config.h"
#include "oprof_start_util.h"
#include "op_config.h"
#include "op_config_24.h"

using namespace std;

event_setting::event_setting()
	:
	count(0),
	umask(0),
	os_ring_count(0),
	user_ring_count(0)
{
}

void event_setting::save(ostream & out) const
{
	out << count << " ";
	out << umask << " ";
	out << os_ring_count << " ";
	out << user_ring_count << " ";
}

void event_setting::load(istream & in)
{
	in >> count;
	in >> umask;
	in >> os_ring_count;
	in >> user_ring_count;
}


ostream & operator<<(ostream & out, const event_setting & object)
{
	object.save(out);
	return out;
}


istream & operator>>(istream & in, event_setting & object)
{
	object.load(in);
	return in;
}


config_setting::config_setting()
	:
	buffer_size(OP_DEFAULT_BUF_SIZE),
	note_table_size(OP_DEFAULT_NOTE_SIZE),
	no_kernel(false),
	kernel_only(false),
	verbose(false),
	pgrp_filter(0),
	pid_filter(0),
	separate_lib_samples(false),
	separate_kernel_samples(false)
{
	struct utsname info;

	/* Guess path to vmlinux based on kernel currently running. */
	if (uname(&info)) {
		perror("oprof_start: Unable to determine OS release.");
	} else {
		string const version(info.release);
		string const vmlinux_path("/lib/modules/" + version
					 + "/build/vmlinux");
		kernel_filename = vmlinux_path;
	}
}


void config_setting::load(istream & in)
{
	buffer_size = OP_DEFAULT_BUF_SIZE;
	note_table_size = OP_DEFAULT_NOTE_SIZE;

	string str;

	while (getline(in, str)) {
		string val = split(str, '=');
		if (str == "BUF_SIZE") {
			buffer_size = touint(val);
			if (buffer_size < OP_DEFAULT_BUF_SIZE)
				buffer_size = OP_DEFAULT_BUF_SIZE;
		} else if (str == "NOTE_SIZE") {
			note_table_size = touint(val);
			if (note_table_size < OP_DEFAULT_NOTE_SIZE)
				note_table_size = OP_DEFAULT_NOTE_SIZE;
		} else if (str == "PID_FILTER") {
			pid_filter = touint(val);
		} else if (str == "PGRP_FILTER") {
			pgrp_filter = touint(val);
		} else if (str == "VMLINUX") {
			if (val == "none") {
				kernel_filename = "";
				no_kernel = true;
			} else if (!val.empty()) {
				no_kernel = false;
				kernel_filename = val;
			}
		} else if (str == "SEPARATE_LIB_SAMPLES") {
			separate_lib_samples = tobool(val);
		} else if (str == "SEPARATE_KERNEL_SAMPLES") {
			separate_kernel_samples = tobool(val);
		} else if (str == "KERNEL_ONLY") {
			kernel_only = tobool(str);
		}
	}
}


istream & operator>>(istream & in, config_setting & object)
{
	object.load(in);
	return in;
}
