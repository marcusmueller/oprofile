/* $Id: oprofctl.c,v 1.2 2000/12/06 20:39:50 moz Exp $ */
/* COPYRIGHT (C) 2000 THE VICTORIA UNIVERSITY OF MANCHESTER and John Levon
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59 Temple
 * Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include "oprofctl.h"

static int showvers;
static int start;
static int stop;
static int dump;
static int disable; 
static int op_buf_size = -1;
static int op_hash_size = -1;
static pid_t pid_filter = -1;
static pid_t pgrp_filter = -1;
static int cpu = -1;
static int counter = -1;
static int um = 0x0;
static char *event="INST_RETIRED";
static int value = -1;
static int osusr = 0;
static char *devfilename="/var/opd/opdev";

static fd_t devfd;
static u8 type;

static struct poptOption options[] = {
	{ "start", 's', POPT_ARG_NONE, &start, 0, "Start profiling", NULL, },
	{ "stop", 'f', POPT_ARG_NONE, &stop, 0, "Stop profiling", NULL, },
	{ "dump", 'd', POPT_ARG_NONE, &dump, 0, "Dump eviction buffer", NULL, },
	{ "buffer-size", 'b', POPT_ARG_INT, &op_buf_size, 0, "nr. of samples in kernel buffer", "num", },
	{ "hash-size", 'h', POPT_ARG_INT, &op_hash_size, 0, "nr. of entries in kernel hash table", "num", },
	{ "pid-filter", 'i', POPT_ARG_INT, &pid_filter, 0, "pid to filter on", "pid", },
	{ "pgrp-filter", 'g', POPT_ARG_INT, &pgrp_filter, 0, "pgrp to filter on", "pgrp", },
	{ "cpu", 'p', POPT_ARG_INT, &cpu, 0, "which CPU counters to change", "cpu number", },
	{ "counter", 'c', POPT_ARG_INT, &counter, 0, "which counter to change", "[0|1]", },
	{ "disable", 'r', POPT_ARG_NONE, &disable, 0, "stop the specified counter", NULL, },
	{ "unit-mask", 'u', POPT_ARG_INT, &um, 0, "unit mask (default 0)", "val", },
	{ "event", 'e', POPT_ARG_STRING, &event, 0, "symbolic event name", "name", },
	{ "value", 'w', POPT_ARG_INT, &value, 0, "counter reset value", "val", },
	{ "os-usr", 'o', POPT_ARG_INT, &osusr, 0, "profile kernel (1) or userspace (2) only", "[0|1|2]", },
	{ "device-file", 'n', POPT_ARG_STRING, &devfilename, 0, "profile device file", "file", },
	{ "version", 'v', POPT_ARG_NONE, &showvers, 0, "show version", NULL, },
	POPT_AUTOHELP
	{ NULL, 0, 0, NULL, 0, NULL, NULL, },
};

#define DO_IOCTL(ctl, val, output...) do { \
	if (ioctl(devfd, (ctl), (val))) { \
		fprintf(stderr, output); \
		exit(errno); \
	} } while (0)

int main(int argc, char *argv[])
{
	poptContext optcon;
	int ret;
	char c;

	optcon = poptGetContext(NULL, argc, argv, options, 0);

	c=poptGetNextOpt(optcon);

	if (c<-1) {
		fprintf(stderr, "oprofctl: %s: %s\n",
			poptBadOption(optcon, POPT_BADOPTION_NOALIAS),
			poptStrerror(c));
		poptPrintHelp(optcon, stderr, 0);
		exit(1);
	}

	if (showvers) {
		printf(VERSION_STRING " compiled on " __DATE__ " " __TIME__ "\n");
		exit(0);
	} 
	
	opd_open_device(devfilename,1);
	
	if (start) {
		DO_IOCTL(OPROF_START, 0, "oprofctl: attempt to start failed: %s\n", strerror(errno));
		exit(0);
	} else if (stop) {
		uint i;
		DO_IOCTL(OPROF_STOP, 0, "oprofctl: attempt to stop failed: %s\n", strerror(errno));
		/* disable every counter */
		for (i=0; i < NR_CPUS; i++) {
			ioctl(devfd, OPROF_SET_CTR0, i);
			ioctl(devfd, OPROF_SET_CTR1, i);
		}
		exit(0);
	} else if (dump) {
		DO_IOCTL(OPROF_DUMP, 0, "oprofctl: attempt to dump failed: %s\n", strerror(errno));
		exit(0);
	}

	if (pid_filter!=-1) {
		DO_IOCTL(OPROF_SET_PID_FILTER, pid_filter, "oprofctl: attempt to set pid filter to %d failed: %s\n", pid_filter, strerror(errno));
		if (pgrp_filter==-1 && op_buf_size==-1 && op_hash_size==-1 && cpu==-1)
			exit(0);
	}
	
	if (pgrp_filter!=-1) {
		DO_IOCTL(OPROF_SET_PGRP_FILTER, pgrp_filter, "oprofctl: attempt to set pgrp filter to %d failed: %s\n", pgrp_filter, strerror(errno));
		if (op_buf_size==-1 && op_hash_size==-1 && cpu==-1)
			exit(0);
	}
		
	if (op_buf_size!=-1) {
		DO_IOCTL(OPROF_SET_BUF_SIZE, op_buf_size, "oprofctl: attempt to set buffer size %d failed: %s\n", op_buf_size, strerror(errno));
		if (op_hash_size==-1 && cpu==-1)
			exit(0);
	}
	
	if (op_hash_size!=-1) {
		DO_IOCTL(OPROF_SET_HASH_SIZE, op_hash_size, "oprofctl: attempt to set hash size %d failed: %s\n", op_hash_size, strerror(errno));
		if (cpu==-1)
			exit(0);
	}

	if (cpu < 0 || cpu > 31) {
		fprintf(stderr, "oprofctl: you must specify a CPU (first CPU is 0).\n");
		exit(1);
	}
	
	if (counter != 0 || counter != 1) {
		fprintf(stderr, "oprofctl: you must specify either counter 0 or 1.\n");
		exit(1);
	}
	
	if (value==-1) {
		fprintf(stderr, "oprofctl: you must specify a reset value.\n");
		exit(1);
	}

	if (disable) {
		if (counter) 
			DO_IOCTL(OPROF_SET_CTR1, cpu, "oprofctl: attempt to disable counter 1 on CPU %d failed: %s\n", cpu, strerror(errno));
		else 
			DO_IOCTL(OPROF_SET_CTR0, cpu, "oprofctl: attempt to disable counter 0 on CPU %d failed: %s\n", cpu, strerror(errno));
		exit(0);
	}

	if (counter)
		ret = op_check_events_str("", event, 0, um, 2, &type, &type);
	else
		ret = op_check_events_str(event, "", um, 0, 2, &type, &type);

        if (ret&OP_CTR0_NOT_FOUND) fprintf(stderr, "oprofctl: ctr0: no such event\n");
        if (ret&OP_CTR1_NOT_FOUND) fprintf(stderr, "oprofctl: ctr1: no such event\n");
        if (ret&OP_CTR0_NO_UM) fprintf(stderr, "oprofctl: ctr0: invalid unit mask\n");
        if (ret&OP_CTR1_NO_UM) fprintf(stderr, "oprofctl: ctr1: invalid unit mask\n");
        if (ret&OP_CTR0_NOT_ALLOWED) fprintf(stderr, "oprofctl: ctr0: can't count event\n");
        if (ret&OP_CTR1_NOT_ALLOWED) fprintf(stderr, "oprofctl: ctr1: can't count event\n");
        if (ret&OP_CTR0_PII_EVENT) fprintf(stderr, "oprofctl: ctr0: event only available on PII\n");
        if (ret&OP_CTR1_PII_EVENT) fprintf(stderr, "oprofctl: ctr1: event only available on PII\n");
        if (ret&OP_CTR0_PIII_EVENT) fprintf(stderr, "oprofctl: ctr0: event only available on PIII\n");
        if (ret&OP_CTR1_PIII_EVENT) fprintf(stderr, "oprofctl: ctr1: event only available on PIII\n");

	if (ret!=OP_EVENTS_OK) {
		poptPrintHelp(optcon, stderr, 0);
		exit(1);
	}
	
	if (counter) {
		DO_IOCTL(OPROF_SET_CTR1, cpu && (1<<31),
			"oprofctl: attempt to enable counter 1 on CPU %d failed: %s\n", cpu, strerror(errno));
		DO_IOCTL(OPROF_SET_CTR1_VAL, type && (cpu<<16),
			"oprofctl: attempt to set event type %d for counter 1 on CPU %d failed: %s\n", type, cpu, strerror(errno));
		DO_IOCTL(OPROF_SET_CTR1_UM, um && (cpu<<16),
			"oprofctl: attempt to set event type %d for counter 1 on CPU %d failed: %s\n", type, cpu, strerror(errno));
		DO_IOCTL(OPROF_SET_CTR1_COUNT, value && (cpu<<16),
			"oprofctl: attempt to set event type %d for counter 1 on CPU %d failed: %s\n", type, cpu, strerror(errno));
		DO_IOCTL(OPROF_SET_CTR1_OS_USR, osusr && (cpu<<16),
			"oprofctl: attempt to set event type %d for counter 1 on CPU %d failed: %s\n", type, cpu, strerror(errno));
	} else {
		DO_IOCTL(OPROF_SET_CTR0, cpu && (1<<31),
			"oprofctl: attempt to enable counter 0  on CPU %d failed: %s\n", cpu, strerror(errno));
		DO_IOCTL(OPROF_SET_CTR0_VAL, type && (cpu<<16),
			"oprofctl: attempt to set event type %d for counter 0 on CPU %d failed: %s\n", type, cpu, strerror(errno));
		DO_IOCTL(OPROF_SET_CTR0_UM, um && (cpu<<16),
			"oprofctl: attempt to set event type %d for counter 0 on CPU %d failed: %s\n", type, cpu, strerror(errno));
		DO_IOCTL(OPROF_SET_CTR0_COUNT, value && (cpu<<16),
			"oprofctl: attempt to set event type %d for counter 0 on CPU %d failed: %s\n", type, cpu, strerror(errno));
		DO_IOCTL(OPROF_SET_CTR0_OS_USR, osusr && (cpu<<16),
			"oprofctl: attempt to set event type %d for counter 0 on CPU %d failed: %s\n", type, cpu, strerror(errno));
	}

	opd_close_device(devfd);
	return 0;
}
