#include <linux/sys.h>
#include <linux/linkage.h>
#include <asm/ptrace.h>
#include <asm/segment.h>
#define ASSEMBLY
#include <asm/smp.h>
#include <asm/msr.h>
#include <asm/calling.h>

ENTRY(op_nmi)
	pushq $-1
	SAVE_ALL
	/* NMI could happen inside the critical section of a swapgs,
	   so it is needed to use this expensive way to check. */
	movl  $MSR_GS_BASE,%ecx
	rdmsr
	xorl  %ebx,%ebx
	testl %edx,%edx
	js    1f
	swapgs
	movl  $1,%ebx
1:	movq %rsp,%rdi
	call op_do_nmi
	cli
	testl %ebx,%ebx
	jz error_restore
	swapgs	
error_restore:	
	RESTORE_ALL 8
	iretq
