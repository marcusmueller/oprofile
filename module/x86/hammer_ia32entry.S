/*
 * Compatibility mode system call entry point for x86-64. 
 * 		
 * Copyright 2000,2001 Andi Kleen, SuSE Labs.
 * 
 */		 

#include <asm/calling.h>
#include <asm/offset.h>
#include <asm/current.h>
#include <linux/linkage.h>
#include <asm/errno.h>
#include <asm/ia32_unistd.h>	

	.macro IA32_ARG_FIXUP
	movl	%edi,%r8d
	movl	%ebp,%r9d
	xchg	%ecx,%esi
	movl	%ebx,%edi
	movl	%edx,%edx	/* zero extension */
	.endm 

	.macro PTREGSCALL label, func
	.globl \label
\label:
	leaq \func(%rip),%rax
	jmp  ia32_ptregs_common	
	.endm

	PTREGSCALL my_stub32_execve, my_sys32_execve
	PTREGSCALL my_stub32_fork, my_sys32_fork
	PTREGSCALL my_stub32_clone, my_sys32_clone
	PTREGSCALL my_stub32_vfork, my_sys32_vfork

ENTRY(ia32_ptregs_common)
	popq %r11
	SAVE_REST
	movq %r11, %r15
	call *%rax
	movq %r15, %r11
	RESTORE_REST
	pushq %r11
	ret

