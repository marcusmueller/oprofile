#include <linux/sys.h>
#include <linux/linkage.h>
#include <asm/segment.h>
#define ASSEMBLY
#include <asm/smp.h>

ORIG_EAX	= 0x24
CS		= 0x2C
EFLAGS		= 0x30
ES		= 0x20
 
#define GET_CURRENT(reg) \
	movl %esp, reg;	 \
	andl $-8192, reg;

#define RESTORE_ALL     \
	popl %ebx;      \
	popl %ecx;      \
	popl %edx;      \
	popl %esi;      \
	popl %edi;      \
	popl %ebp;      \
	popl %eax;      \
1:      popl %ds;       \
2:      popl %es;       \
	addl $4,%esp;   \
3:      iret;	   	\
.section .fixup,"ax";   \
4:      movl $0,(%esp); \
	jmp 1b;	 	\
5:      movl $0,(%esp); \
	jmp 2b;	 	\
6:      pushl %ss;      \
	popl %ds;       \
	pushl %ss;      \
	popl %es;       \
	pushl $11;      \
	call my_sys_exit;\
.previous;	      	\
.section __ex_table,"a";\
	.align 4;       \
	.long 1b,4b;    \
	.long 2b,5b;    \
	.long 3b,6b;    \
.previous
 
/* hugely untested. Phil, I need someone who knows what they're doing
 * here. The call goes into the ES "slot" of the stack, but I don't
 * know why */
 
ENTRY(op_nmi22)
	pushl $0
	pushl $ SYMBOL_NAME(op_do_nmi)
	pushl %ds
	pushl %eax
	xorl %eax,%eax
	pushl %ebp
	pushl %edi
	pushl %esi
	pushl %edx
	decl %eax			# eax = -1
	pushl %ecx
	pushl %ebx
	cld
	movl %es,%ecx
	xchgl %eax, ORIG_EAX(%esp)	# orig_eax (get the error code. )
	movl %esp,%edx
	xchgl %ecx, ES(%esp)		# get the address and save es.
	pushl %eax			# push the error code
	pushl %edx
	movl $(__KERNEL_DS),%edx
	movl %edx,%ds
	movl %edx,%es
	GET_CURRENT(%ebx)
	call *%ecx
	addl $8,%esp
	GET_CURRENT(%ebx)
	movl EFLAGS(%esp),%eax		# mix EFLAGS and CS
	movb CS(%esp),%al
	RESTORE_ALL
