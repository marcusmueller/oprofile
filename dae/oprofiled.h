/* oprofiled.h */
/* John Levon (moz@compsoc.man.ac.uk) */
/* May 2000 */

#ifndef OPROFILED_H
#define OPROFILED_H

/* See objdump --section-headers /usr/src/linux/vmlinux */
/* used to catch out kernel samples (and also compute 
   text offset if no System.map or module info is available */ 
#define KERNEL_VMA_OFFSET           0xc0100000
 
#include <popt.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/poll.h>

#include "opd_util.h"
 
/* various defines */
 
#define OPD_DEBUG
 
/* maximum nr. of kernel modules */
#define OPD_MAX_MODULES 64
 
/* processes unused in this nr. of seconds are forgotten */
#define OPD_REAP_TIME 60*21
 
#define NR_CPUS 32
 
/* stats for sample collection */
#define OPD_MAX_STATS 8

/* nr. kernel samples not in process context */
#define OPD_KERNEL_NP 0
/* nr. samples for which process info couldn't be accessed */ 
#define OPD_LOST_PROCESS 1
/* nr. kernel samples in process context */
#define OPD_KERNEL_P 2
/* nr. userspace samples */
#define OPD_PROCESS 3
/* nr. samples for which map info couldn't be accessed */
#define OPD_LOST_MAP_PROCESS 4
/* nr. accesses of proc queue */
#define OPD_PROC_QUEUE_ACCESS 5
/* cumulative depth of proc queue accesses */
#define OPD_PROC_QUEUE_DEPTH 6
/* nr. of times buffer is read */
#define OPD_DUMP_COUNT 7
 
#define OPD_DEFAULT_BUF_SIZE 2048
 
#define streq(a,b) (!strcmp((a),(b))) 
#define streqn(a,b,n) (!strncmp((a),(b),(n))) 
 
#define OP_BITS 1

/* top OP_BITS bits of count are used as follows: */
/* which perf counter the sample is from */
#define OP_COUNTER (1<<15)

#define OP_COUNT_MASK ((1<<(16-OP_BITS))-1)

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
 
/* file format header, network-endian */
struct opd_header {
	u16 magic;
	u16 version;
};

/* note that pid_t is 32 bits, but only 16 are used
   currently, so to save cache, we use u16 */
struct op_sample {
        u32 eip;
        u16 pid;
        u16 count;
};

/* kernel module */
struct opd_module {
	u16 image_nr;
	char *name;
	u32 start;
	u32 end;
};
 
struct opd_map {
	u32 start;
	u32 offset;
	u32 end;
	u16 image_nr;
};

struct opd_proc {
	struct opd_map *maps;
	int nr_maps;
	int max_nr_maps;
	u16 pid;
	u16 exe_nr;
	time_t age;
	struct opd_proc *prev;
	struct opd_proc *next;
};
 
int op_check_events_str(char *ctr0_type, char *ctr1_type, u8 ctr0_um, u8 ctr1_um, int p2, u8 *ctr0_t, u8 *ctr1_t);
 
void opd_init_images(void);
int opd_get_offset(u16 pid, u32 eip, u16 *image_nr, u32 *offset, u16 *name);
void opd_read_system_map(const char *filename); 
void opd_alarm(int val); 
 
#endif /* OPROFILED_H */
