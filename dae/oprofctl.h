/* $Id: oprofctl.h,v 1.2 2000/12/06 20:39:50 moz Exp $ */
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
