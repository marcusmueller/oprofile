/**
 * @file op_pe_utils.h
 * Definitions and prototypes for tools using Linux Performance Events Subsystem.
 *
 * @remark Copyright 2013 OProfile authors
 * @remark Read the file COPYING
 *
 * Created on: May 21, 2013
 * @author Maynard Johnson
 * (C) Copyright IBM Corp. 2013
 *
 */


#ifndef OP_PE_UTILS_H_
#define OP_PE_UTILS_H_

#include <dirent.h>

#include <vector>
#include <set>

#include "op_cpu_type.h"

#define OP_APPNAME_LEN 1024
#define OP_MAX_EVENTS 24
#define CALLGRAPH_MIN_COUNT_SCALE 15

/* A macro to be used for ppc64 architecture-specific code.  The '__powerpc__' macro
 * is defined for both ppc64 and ppc32 architectures, so we must further qualify by
 * including the 'HAVE_LIBPFM' macro, since that macro will be defined only for ppc64.
 */
#define PPC64_ARCH (HAVE_LIBPFM) && ((defined(__powerpc__) || defined(__powerpc64__)))

// Candidates for refactoring of operf
namespace op_pe_utils {

// prototypes
extern int op_check_perf_events_cap(bool use_cpu_minus_one);
extern int op_get_sys_value(const char * filename);
extern int op_get_cpu_for_perf_events_cap(void);
extern int op_validate_app_name(char ** app, char ** save_appname);
extern void op_get_default_event(bool do_callgraph);
extern void op_process_events_list(std::set<std::string> & passed_evts,
                                   bool do_profiling, bool do_callgraph);
extern int op_get_next_online_cpu(DIR * dir, struct dirent *entry);
extern std::set<int> op_get_available_cpus(int max_num_cpus);
}


#endif /* OP_PE_UTILS_H_ */
