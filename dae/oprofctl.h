/* $Id: oprofctl.h,v 1.1 2000/11/14 00:46:30 moz Exp $ */

#ifndef OPROFCTL_H
#define OPROFCTL_H

#include <popt.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h> 
#include <sys/ioctl.h>

#include "opd_util.h"
#include "../version.h"
#include "../op_ioctl.h" 

/* various defines */

//#define OPD_DEBUG

#ifdef OPD_DEBUG
#define dprintf(args...) printf(args)
#else
#define dprintf(args...)
#endif

#define streq(a,b) (!strcmp((a),(b)))
#define streqn(a,b,n) (!strncmp((a),(b),(n)))

#define NR_CPUS 32

/* event check returns */
#define OP_EVENTS_OK            0x0
#define OP_CTR0_NOT_FOUND       0x1
#define OP_CTR1_NOT_FOUND       0x2
#define OP_CTR0_NO_UM           0x4
#define OP_CTR1_NO_UM           0x8
#define OP_CTR0_NOT_ALLOWED     0x10
#define OP_CTR1_NOT_ALLOWED     0x20
#define OP_CTR0_PII_EVENT       0x40
#define OP_CTR1_PII_EVENT       0x80
#define OP_CTR0_PIII_EVENT     0x100
#define OP_CTR1_PIII_EVENT     0x200

int op_check_events_str(char *ctr0_type, char *ctr1_type, u8 ctr0_um, u8 ctr1_um, int p2, u8 *ctr0_t, u8 *ctr1_t);

#endif /* OPROFCTL_H */
