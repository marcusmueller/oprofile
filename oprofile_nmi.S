/* oprofile_nmi.S */

#include <linux/sys.h>
#include <linux/linkage.h>
#include <asm/segment.h>
#define ASSEMBLY
#include <asm/smp.h>

#define SAVE_ALL \
	cld; \
	pushl %es; \
	pushl %ds; \
	pushl %eax; \
	pushl %ebp; \
	pushl %edi; \
	pushl %esi; \
	pushl %edx; \
	pushl %ecx; \
	pushl %ebx; \
	movl $(__KERNEL_DS),%edx; \
	movl %edx,%ds; \
	movl %edx,%es;

#define RESTORE_ALL	\
	popl %ebx;	\
	popl %ecx;	\
	popl %edx;	\
	popl %esi;	\
	popl %edi;	\
	popl %ebp;	\
	popl %eax;	\
	popl %ds;	\
	popl %es;	\
	addl $4,%esp;	\
	iret;

ENTRY(op_nmi)
        pushl %eax
        SAVE_ALL
        movl %esp,%edx
        pushl %edx
        call SYMBOL_NAME(op_do_nmi)
        addl $4,%esp
        RESTORE_ALL
