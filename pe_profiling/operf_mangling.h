/*
 * @file pe_profiling/operf_mangling.h
 * This file is based on daemon/opd_mangling and is used for
 * mangling and opening of sample files for operf.
 *
 * @remark Copyright 2011 OProfile authors
 * @remark Read the file COPYING
 *
 * Created on: Dec 15, 2011
 * @author Maynard Johnson
 * (C) Copyright IBM Corp. 2011
 */

#ifndef OPERF_MANGLING_H_
#define OPERF_MANGLING_H_

#include "odb.h"

struct operf_sfile;

/*
 * operf_open_sample_file - open a sample file
 * @param sf  operf_sfile to open sample file for
 * @param counter  counter number
 * @param cg if this is a callgraph file
 *
 * Open image sample file for the sfile, counter
 * counter and set up memory mappings for it.
 *
 * Returns 0 on success.
 */
int operf_open_sample_file(odb_t *file, struct operf_sfile *last,
                         struct operf_sfile * sf, int counter, int cg);


#endif /* OPERF_MANGLING_H_ */
