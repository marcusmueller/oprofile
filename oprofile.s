	.file	"sprofile.c"
	.version	"01.01"
gcc2_compiled.:
#APP
	.section .modinfo
	.previous
#NO_APP
.section	.modinfo,"a",@progbits
	.type	 __module_kernel_version,@object
	.size	 __module_kernel_version,27
__module_kernel_version:
	.string	"kernel_version=2.4.0-test3"
.section	.rodata
.LC0:
	.string	"wq bug, forcing oops.\n"
	.align 32
.LC1:
	.string	"/usr/src/linux/include/linux/wait.h"
.LC2:
	.string	"kernel BUG at %s:%d!\n"
	.align 32
.LC3:
	.string	"bad magic %lx (should be %lx, creator %lx), "
	.align 32
.LC4:
	.string	"bad magic %lx (should be %lx), "
	.align 32
.LC5:
	.string	"/usr/src/linux/include/linux/mount.h"
	.align 32
.LC6:
	.string	"/usr/src/linux/include/asm/semaphore.h"
	.align 32
.LC7:
	.string	"/usr/src/linux/include/linux/sched.h"
	.align 32
.LC8:
	.string	"/usr/src/linux/include/linux/mm.h"
.LC9:
	.string	"sprofile 0.0.1"
.data
	.align 4
	.type	 sp_version,@object
	.size	 sp_version,4
sp_version:
	.long .LC9
.globl __module_author
.section	.modinfo
	.align 32
	.type	 __module_author,@object
	.size	 __module_author,42
__module_author:
	.string	"author=John Levon (moz@compsoc.man.ac.uk)"
.globl __module_description
	.align 32
	.type	 __module_description,@object
	.size	 __module_description,39
__module_description:
	.string	"description=Continous Profiling Module"
.globl __module_parm_sp_hash_size
	.type	 __module_parm_sp_hash_size,@object
	.size	 __module_parm_sp_hash_size,20
__module_parm_sp_hash_size:
	.string	"parm_sp_hash_size=i"
.globl __module_parm_desc_sp_hash_size
	.align 32
	.type	 __module_parm_desc_sp_hash_size,@object
	.size	 __module_parm_desc_sp_hash_size,55
__module_parm_desc_sp_hash_size:
	.string	"parm_desc_sp_hash_size=Number of entries in hash table"
.globl __module_parm_sp_buf_size
	.type	 __module_parm_sp_buf_size,@object
	.size	 __module_parm_sp_buf_size,19
__module_parm_sp_buf_size:
	.string	"parm_sp_buf_size=i"
.globl __module_parm_desc_sp_buf_size
	.align 32
	.type	 __module_parm_desc_sp_buf_size,@object
	.size	 __module_parm_desc_sp_buf_size,59
__module_parm_desc_sp_buf_size:
	.string	"parm_desc_sp_buf_size=Number of entries in eviction buffer"
.globl __module_parm_sp_ctr0_on
	.type	 __module_parm_sp_ctr0_on,@object
	.size	 __module_parm_sp_ctr0_on,22
__module_parm_sp_ctr0_on:
	.string	"parm_sp_ctr0_on=1-32b"
.globl __module_parm_desc_sp_ctr0_on
	.align 32
	.type	 __module_parm_desc_sp_ctr0_on,@object
	.size	 __module_parm_desc_sp_ctr0_on,38
__module_parm_desc_sp_ctr0_on:
	.string	"parm_desc_sp_ctr0_on=Enable counter 0"
.globl __module_parm_sp_ctr1_on
	.type	 __module_parm_sp_ctr1_on,@object
	.size	 __module_parm_sp_ctr1_on,22
__module_parm_sp_ctr1_on:
	.string	"parm_sp_ctr1_on=1-32b"
.globl __module_parm_desc_sp_ctr1_on
	.align 32
	.type	 __module_parm_desc_sp_ctr1_on,@object
	.size	 __module_parm_desc_sp_ctr1_on,38
__module_parm_desc_sp_ctr1_on:
	.string	"parm_desc_sp_ctr1_on=Enable counter 1"
.globl __module_parm_sp_ctr0_type
	.type	 __module_parm_sp_ctr0_type,@object
	.size	 __module_parm_sp_ctr0_type,24
__module_parm_sp_ctr0_type:
	.string	"parm_sp_ctr0_type=1-32s"
.globl __module_parm_desc_sp_ctr0_type
	.align 32
	.type	 __module_parm_desc_sp_ctr0_type,@object
	.size	 __module_parm_desc_sp_ctr0_type,57
__module_parm_desc_sp_ctr0_type:
	.string	"parm_desc_sp_ctr0_type=Symbolic event name for counter 0"
.globl __module_parm_sp_ctr1_type
	.type	 __module_parm_sp_ctr1_type,@object
	.size	 __module_parm_sp_ctr1_type,24
__module_parm_sp_ctr1_type:
	.string	"parm_sp_ctr1_type=1-32s"
.globl __module_parm_desc_sp_ctr1_type
	.align 32
	.type	 __module_parm_desc_sp_ctr1_type,@object
	.size	 __module_parm_desc_sp_ctr1_type,57
__module_parm_desc_sp_ctr1_type:
	.string	"parm_desc_sp_ctr1_type=Symbolic event name for counter 1"
.globl __module_parm_sp_ctr0_um
	.type	 __module_parm_sp_ctr0_um,@object
	.size	 __module_parm_sp_ctr0_um,22
__module_parm_sp_ctr0_um:
	.string	"parm_sp_ctr0_um=1-32s"
.globl __module_parm_desc_sp_ctr0_um
	.align 32
	.type	 __module_parm_desc_sp_ctr0_um,@object
	.size	 __module_parm_desc_sp_ctr0_um,45
__module_parm_desc_sp_ctr0_um:
	.string	"parm_desc_sp_ctr0_um=Unit Mask for counter 0"
.globl __module_parm_sp_ctr1_um
	.type	 __module_parm_sp_ctr1_um,@object
	.size	 __module_parm_sp_ctr1_um,22
__module_parm_sp_ctr1_um:
	.string	"parm_sp_ctr1_um=1-32s"
.globl __module_parm_desc_sp_ctr1_um
	.align 32
	.type	 __module_parm_desc_sp_ctr1_um,@object
	.size	 __module_parm_desc_sp_ctr1_um,45
__module_parm_desc_sp_ctr1_um:
	.string	"parm_desc_sp_ctr1_um=Unit Mask for counter 1"
.globl __module_parm_sp_ctr0_count
	.type	 __module_parm_sp_ctr0_count,@object
	.size	 __module_parm_sp_ctr0_count,25
__module_parm_sp_ctr0_count:
	.string	"parm_sp_ctr0_count=1-32i"
.globl __module_parm_desc_sp_ctr0_count
	.align 32
	.type	 __module_parm_desc_sp_ctr0_count,@object
	.size	 __module_parm_desc_sp_ctr0_count,81
__module_parm_desc_sp_ctr0_count:
	.string	"parm_desc_sp_ctr0_count=Number of events between samples for counter 0 (decimal)"
.globl __module_parm_sp_ctr1_count
	.type	 __module_parm_sp_ctr1_count,@object
	.size	 __module_parm_sp_ctr1_count,25
__module_parm_sp_ctr1_count:
	.string	"parm_sp_ctr1_count=1-32i"
.globl __module_parm_desc_sp_ctr1_count
	.align 32
	.type	 __module_parm_desc_sp_ctr1_count,@object
	.size	 __module_parm_desc_sp_ctr1_count,81
__module_parm_desc_sp_ctr1_count:
	.string	"parm_desc_sp_ctr1_count=Number of events between samples for counter 1 (decimal)"
.data
	.align 4
	.type	 sp_hash_size,@object
	.size	 sp_hash_size,4
sp_hash_size:
	.long 2048
	.align 4
	.type	 sp_buf_size,@object
	.size	 sp_buf_size,4
sp_buf_size:
	.long 2048
.globl sprof_wait
	.align 4
	.type	 sprof_wait,@object
	.size	 sprof_wait,20
sprof_wait:
	.long 0
	.long sprof_wait+4
	.long sprof_wait+4
	.long sprof_wait+12
	.long sprof_wait+12
.text
	.align 4
	.type	 evict_sp_entry,@function
evict_sp_entry:
	pushl %esi
	pushl %ebx
	movl 12(%esp),%ebx
	movl 16(%esp),%ecx
	movl 24(%ebx),%esi
	leal 0(,%esi,8),%eax
	addl 4(%ebx),%eax
	movl (%ecx),%edx
	movl %edx,(%eax)
	movl 4(%ecx),%edx
	movl %edx,4(%eax)
	movl 24(%ebx),%eax
	leal 1(%eax),%esi
	movl %esi,24(%ebx)
	movl %esi,%eax
	cmpl 12(%ebx),%eax
	jne .L1869
	movl $0,24(%ebx)
	movl $1,sprof_ready
	movl $35,%edx
	movl $sprof_wait,%eax
	call __wake_up
.L1869:
	popl %ebx
	popl %esi
	ret
.Lfe1:
	.size	 evict_sp_entry,.Lfe1-evict_sp_entry
	.align 4
	.type	 fill_sp_entry,@function
fill_sp_entry:
	movl 4(%esp),%edx
	movl 8(%esp),%eax
	movb 12(%esp),%cl
	movl 40(%eax),%eax
	movl %eax,(%edx)
	movl $-8192,%eax
#APP
	andl %esp,%eax; 
#NO_APP
	movw 108(%eax),%ax
	movw %ax,4(%edx)
	movzbl %cl,%eax
	sall $15,%eax
	orb $1,%al
	movw %ax,6(%edx)
	ret
.Lfe2:
	.size	 fill_sp_entry,.Lfe2-fill_sp_entry
	.align 4
	.type	 sp_check_ctr,@function
sp_check_ctr:
	subl $52,%esp
	pushl %ebp
	pushl %edi
	pushl %esi
	pushl %ebx
	movzbl 80(%esp),%edx
	movl %edx,64(%esp)
	movl %edx,%esi
	addl $193,%esi
	movl %esi,%ecx
#APP
	rdmsr
#NO_APP
	testl %eax,%eax
	jl .L1927
	movl $-8192,%edx
#APP
	andl %esp,%edx; 
#NO_APP
	movl %edx,20(%esp)
	movl 76(%esp),%ecx
	movl 40(%ecx),%ecx
	movl %ecx,16(%esp)
	movl %ecx,%ebx
	andl $1044480,%ebx
	sarl $3,%ebx
	xorl %ecx,%ebx
	movzbl 108(%edx),%eax
	xorl %eax,%ebx
	movl %ecx,%eax
	sall $9,%eax
	xorl %eax,%ebx
	movl 64(%esp),%eax
	sall $8,%eax
	xorl %eax,%ebx
	movl 72(%esp),%edx
	movl 8(%edx),%eax
	decl %eax
	andl %eax,%ebx
	xorl %ebp,%ebp
	movl (%edx),%ecx
	movl %ecx,32(%esp)
	sall $5,%ebx
	movl %ecx,60(%esp)
	movl %ebx,56(%esp)
	leal (%ecx,%ebx),%edi
	movl 20(%esp),%edx
	movl %edx,44(%esp)
	movl %esi,40(%esp)
	movl 64(%esp),%ecx
	sall $2,%ecx
	movl %ecx,36(%esp)
	movl 16(%esp),%edx
	movl %edx,48(%esp)
	.p2align 2
.L1931:
	leal 0(,%ebp,8),%ecx
	movl %ecx,20(%esp)
	movl %ecx,52(%esp)
	movl 48(%esp),%edx
	cmpl %edx,(%ecx,%edi)
	jne .L1932
	movzwl 4(%ecx,%edi),%eax
	movl 44(%esp),%ecx
	cmpl 108(%ecx),%eax
	jne .L1932
	movl 20(%esp),%edx
	movw 6(%edx,%edi),%si
	movl %esi,%eax
	andb $127,%ah
	cmpw $32767,%ax
	je .L1942
	incl %esi
	movw %si,6(%edx,%edi)
	movl 72(%esp),%ecx
	movl 36(%esp),%edx
	movl 16(%edx,%ecx),%eax
	negl %eax
	movl 40(%esp),%ecx
	xorl %edx,%edx
#APP
	wrmsr
#NO_APP
	jmp .L1939
	.p2align 2
.L1932:
	movl 56(%esp),%esi
	addl 60(%esp),%esi
	movl 52(%esp),%edx
	movw 6(%edx,%esi),%dx
	movw %dx,30(%esp)
	movl %edx,%eax
	andb $127,%ah
	cmpw $32767,%ax
	je .L1942
	testw %dx,%dx
	je .L1943
	incl %ebp
	cmpl $3,%ebp
	jbe .L1931
	movl 32(%esp),%ecx
	addl %ebx,%ecx
	movl 72(%esp),%edx
	movzbl 28(%edx),%eax
	leal (%ecx,%eax,8),%eax
	pushl %eax
	pushl 76(%esp)
	call evict_sp_entry
	pushl 72(%esp)
	pushl 88(%esp)
	movl 88(%esp),%ecx
	addl (%ecx),%ebx
	movzbl 28(%ecx),%eax
	leal (%ebx,%eax,8),%eax
	pushl %eax
	call fill_sp_entry
	movl 92(%esp),%edx
	movb 28(%edx),%al
	incb %al
	andb $3,%al
	movb %al,28(%edx)
	addl $20,%esp
.L1948:
	movl 64(%esp),%ebx
	addl $193,%ebx
	movl 72(%esp),%eax
	addl $16,%eax
	movl 64(%esp),%ecx
	movl (%eax,%ecx,4),%eax
	negl %eax
	movl %ebx,%ecx
	xorl %edx,%edx
#APP
	wrmsr
#NO_APP
	jmp .L1939
	.p2align 2
.L1942:
	movl 32(%esp),%eax
	addl %ebx,%eax
	addl 52(%esp),%eax
	pushl %eax
	pushl 76(%esp)
	call evict_sp_entry
	movl 80(%esp),%edx
	addl (%edx),%ebx
	movl 72(%esp),%eax
	sall $15,%eax
	orb $1,%al
	movl 60(%esp),%ecx
	movw %ax,6(%ecx,%ebx)
	addl $8,%esp
	jmp .L1948
	.p2align 2
.L1943:
	movl 64(%esp),%edx
	pushl %edx
	pushl 80(%esp)
	movl %esi,%eax
	addl 28(%esp),%eax
	pushl %eax
	call fill_sp_entry
	addl $12,%esp
	jmp .L1948
	.p2align 2
.L1939:
	movl $1,%eax
	jmp .L1954
	.p2align 2
.L1927:
	xorl %eax,%eax
.L1954:
	popl %ebx
	popl %esi
	popl %edi
	popl %ebp
	addl $52,%esp
	ret
.Lfe3:
	.size	 sp_check_ctr,.Lfe3-sp_check_ctr
	.align 4
.globl sp_do_nmi
	.type	 sp_do_nmi,@function
sp_do_nmi:
	pushl %edi
	pushl %esi
	pushl %ebx
	movl 16(%esp),%edi
	movl $sprof_data,%esi
	xorl %ebx,%ebx
	movl $390,%ecx
#APP
	rdmsr
#NO_APP
	andl $-4194305,%eax
#APP
	wrmsr
#NO_APP
	movb 29(%esi),%al
	testb $1,%al
	je .L1956
	pushl $0
	pushl %edi
	pushl %esi
	call sp_check_ctr
	movb %al,%bl
	addl $12,%esp
	movb 29(%esi),%al
.L1956:
	testb $2,%al
	je .L1957
	pushl $1
	pushl %edi
	pushl %esi
	call sp_check_ctr
	orb %al,%bl
	addl $12,%esp
.L1957:
	movl $390,%ecx
#APP
	rdmsr
#NO_APP
	orl $4194304,%eax
#APP
	wrmsr
#NO_APP
	testb %bl,%bl
	jne .L1958
	pushl $0
	pushl %edi
	call my_do_nmi
	addl $8,%esp
	.p2align 2
.L1958:
	popl %ebx
	popl %esi
	popl %edi
	ret
.Lfe4:
	.size	 sp_do_nmi,.Lfe4-sp_do_nmi
	.align 4
.globl mask_LVT_NMIs
	.type	 mask_LVT_NMIs,@function
mask_LVT_NMIs:
	movl -7344,%eax
	orl $65536,%eax
	movl %eax,-7344
	movl -7328,%eax
	orl $65536,%eax
	movl %eax,-7328
	movl -7360,%eax
	orl $65536,%eax
	movl %eax,-7360
	ret
.Lfe5:
	.size	 mask_LVT_NMIs,.Lfe5-mask_LVT_NMIs
	.align 4
.globl unmask_LVT_NMIs
	.type	 unmask_LVT_NMIs,@function
unmask_LVT_NMIs:
	movl -7344,%eax
	andl $-65537,%eax
	movl %eax,-7344
	movl -7328,%eax
	andl $-65537,%eax
	movl %eax,-7328
	movl -7360,%eax
	andl $-65537,%eax
	movl %eax,-7360
	ret
.Lfe6:
	.size	 unmask_LVT_NMIs,.Lfe6-unmask_LVT_NMIs
	.align 4
	.type	 install_nmi,@function
install_nmi:
	subl $8,%esp
	pushl %ebx
	movl $0,4(%esp)
	movw $0,8(%esp)
	call mask_LVT_NMIs
#APP
	sidt 4(%esp)
#NO_APP
	movl 6(%esp),%ecx
	movl %ecx,idt_addr
	leal 16(%ecx),%ebx
	movl 16(%ecx),%edx
	movzwl %dx,%edx
	movl 4(%ebx),%eax
	andl $-65536,%eax
	orl %eax,%edx
	movl %edx,kernel_nmi
	movl $1048576,%eax
	movl $sp_nmi,%edx
#APP
	movw %dx,%ax
	movw $-29184,%dx
	movl %eax,16(%ecx)
	movl %edx,4(%ebx)
#NO_APP
	call unmask_LVT_NMIs
	popl %ebx
	addl $8,%esp
	ret
.Lfe7:
	.size	 install_nmi,.Lfe7-install_nmi
	.align 4
	.type	 restore_nmi,@function
restore_nmi:
	call mask_LVT_NMIs
	movl $1048576,%eax
	movl kernel_nmi,%edx
	movl idt_addr,%ecx
#APP
	movw %dx,%ax
	movw $-29184,%dx
	movl %eax,16(%ecx)
	movl %edx,20(%ecx)
#NO_APP
	call unmask_LVT_NMIs
	ret
.Lfe8:
	.size	 restore_nmi,.Lfe8-restore_nmi
.section	.rodata
	.align 32
.LC10:
	.string	"<6>sprofile: disabled local APIC.\n"
.text
	.align 4
	.type	 disable_local_P6_APIC,@function
disable_local_P6_APIC:
	movl $27,%ecx
#APP
	rdmsr
#NO_APP
	andb $247,%ah
#APP
	wrmsr
#NO_APP
	movl -7392,%eax
	orl $65536,%eax
	movl %eax,-7392
	movl -7344,%eax
	orl $65536,%eax
	movl %eax,-7344
	movl -7328,%eax
	orl $65536,%eax
	movl %eax,-7328
	movl -7312,%eax
	orl $65536,%eax
	movl %eax,-7312
	movl -7360,%eax
	orl $65536,%eax
	movl %eax,-7360
	movl $65536,%eax
	movl %eax,-7392
	movl %eax,-7344
	movl %eax,-7328
	movl %eax,-7312
	movl %eax,-7360
	movl -7952,%eax
	andb $254,%ah
	movl %eax,-7952
	pushl $.LC10
	call printk
	addl $4,%esp
	ret
.Lfe9:
	.size	 disable_local_P6_APIC,.Lfe9-disable_local_P6_APIC
	.align 4
	.type	 smp_apic_setup,@function
smp_apic_setup:
	movl -7360,%eax
	orb $4,%ah
	andl $-66305,%eax
	movl %eax,-7360
	ret
.Lfe10:
	.size	 smp_apic_setup,.Lfe10-smp_apic_setup
.section	.rodata
	.align 32
.LC11:
	.string	"<6>sprofile: enabled local APIC\n"
	.align 32
.LC12:
	.string	"<3>sprofile: no local P6 APIC\n"
.text
	.align 4
	.type	 apic_setup,@function
apic_setup:
	call my_set_fixmap
	movl $27,%ecx
#APP
	rdmsr
#NO_APP
	orb $8,%ah
#APP
	wrmsr
#NO_APP
	movl -8144,%eax
	testb $240,%al
	je .L2084
	movl -8144,%eax
	shrl $16,%eax
	andl $15,%eax
	cmpl $4,%eax
	jne .L2084
	movl -7952,%eax
	orb $1,%ah
	movl %eax,-7952
	movl $34560,%eax
	movl %eax,-7344
	movl $1024,%eax
	movl %eax,-7328
	movl $0,-7552
	movl -7312,%eax
	orl $65536,%eax
	movl %eax,-7312
	movl $4145,%eax
	movl %eax,-7392
	movl $11,-7200
	pushl $0
	call smp_apic_setup
	pushl $.LC11
	call printk
	xorl %eax,%eax
	addl $8,%esp
	ret
	.p2align 2
.L2084:
	pushl $.LC12
	call printk
	movl $27,%ecx
#APP
	rdmsr
#NO_APP
	andb $247,%ah
#APP
	wrmsr
#NO_APP
	movl $-19,%eax
	addl $4,%esp
	ret
.Lfe11:
	.size	 apic_setup,.Lfe11-apic_setup
	.align 4
	.type	 pmc_setup,@function
pmc_setup:
	subl $8,%esp
	pushl %ebp
	pushl %edi
	pushl %esi
	pushl %ebx
	xorl %edi,%edi
	cmpb $0,sp_ctr0_val
	je .L2117
	movl sp_ctr0_count,%ebx
	negl %ebx
	movl $193,%ecx
	movl %ebx,%eax
	xorl %edx,%edx
#APP
	wrmsr
#NO_APP
	movl $390,%ecx
#APP
	rdmsr
#NO_APP
	movl %eax,%ebx
	movl %edx,%esi
	andl $2097152,%ebx
	orl $1245184,%ebx
	movzbl sp_ctr0_val,%eax
	orl %eax,%ebx
	movl sp_ctr0_um,%ebp
	testl %ebp,%ebp
	je .L2123
	pushl $0
	pushl $0
	pushl %ebp
	call simple_strtoul
	movl %eax,%ebp
	addl $12,%esp
	jmp .L2124
	.p2align 2
.L2123:
	xorl %ebp,%ebp
.L2124:
	movl %ebp,%edx
	movzbl %dl,%edx
	sall $8,%edx
	orl %edx,%ebx
	movl $390,%ecx
	movl %ebx,%eax
	movl %esi,%edx
#APP
	wrmsr
#NO_APP
	jmp .L2125
	.p2align 2
.L2117:
	movl $390,%ecx
#APP
	rdmsr
#NO_APP
	movl %eax,%ebx
	andl $2097152,%ebx
	movl %ebx,%eax
	xorl %edx,%edx
#APP
	wrmsr
#NO_APP
.L2125:
	cmpb $0,sp_ctr1_val
	je .L2126
	sall $2,%edi
	movl %edi,16(%esp)
	movl $sp_ctr1_count,%eax
	movl (%edi,%eax),%ebx
	negl %ebx
	movl $194,%ecx
	movl %ebx,%eax
	xorl %edx,%edx
#APP
	wrmsr
#NO_APP
	movl $391,%ecx
#APP
	rdmsr
#NO_APP
	movl %eax,%ebx
	movl %edx,%esi
	andl $6291456,%ebx
	orl $1245184,%ebx
	movzbl sp_ctr1_val,%eax
	orl %eax,%ebx
	movl $sp_ctr1_um,%edx
	movl %edi,%ecx
	movl (%ecx,%edx),%edi
	testl %edi,%edi
	je .L2132
	pushl $0
	pushl $0
	pushl %edi
	call simple_strtoul
	movl %eax,%edi
	addl $12,%esp
	jmp .L2133
	.p2align 2
.L2132:
	xorl %edi,%edi
.L2133:
	movl %edi,%eax
	andl $255,%eax
	sall $8,%eax
	orl %eax,%ebx
	movl $391,%ecx
	movl %ebx,%eax
	movl %esi,%edx
#APP
	wrmsr
#NO_APP
	jmp .L2134
	.p2align 2
.L2126:
	movl $391,%ecx
#APP
	rdmsr
#NO_APP
	movl %eax,%ebx
	andl $6291456,%ebx
	movl %ebx,%eax
	xorl %edx,%edx
#APP
	wrmsr
#NO_APP
.L2134:
	popl %ebx
	popl %esi
	popl %edi
	popl %ebp
	addl $8,%esp
	ret
.Lfe12:
	.size	 pmc_setup,.Lfe12-pmc_setup
	.align 4
	.type	 pmc_start,@function
pmc_start:
	movl 4(%esp),%eax
	testl %eax,%eax
	je .L2136
	cmpl $0,(%eax)
	jne .L2135
.L2136:
	movl $390,%ecx
#APP
	rdmsr
#NO_APP
	orl $4194304,%eax
#APP
	wrmsr
#NO_APP
.L2135:
	ret
.Lfe13:
	.size	 pmc_start,.Lfe13-pmc_start
	.align 4
	.type	 pmc_stop,@function
pmc_stop:
	movl 4(%esp),%eax
	testl %eax,%eax
	je .L2138
	cmpl $0,(%eax)
	jne .L2137
.L2138:
	movl $390,%ecx
#APP
	rdmsr
#NO_APP
	andl $-4194305,%eax
#APP
	wrmsr
#NO_APP
.L2137:
	ret
.Lfe14:
	.size	 pmc_stop,.Lfe14-pmc_stop
	.align 4
	.type	 sprof_open,@function
sprof_open:
	cmpl $0,sprof_opened
	jne .L2146
	movl $1,sprof_opened
	pushl $0
	call pmc_start
	xorl %eax,%eax
	addl $4,%esp
	ret
	.p2align 2
.L2146:
	movl $-16,%eax
	ret
.Lfe15:
	.size	 sprof_open,.Lfe15-sprof_open
	.align 4
	.type	 sprof_release,@function
sprof_release:
	cmpl $0,sprof_opened
	je .L2149
	pushl $0
	call pmc_stop
	movl $0,sprof_opened
	xorl %eax,%eax
	addl $4,%esp
	ret
	.p2align 2
.L2149:
	movl $-14,%eax
	ret
.Lfe16:
	.size	 sprof_release,.Lfe16-sprof_release
	.align 4
	.type	 sprof_read,@function
sprof_read:
	subl $4,%esp
	pushl %ebp
	pushl %edi
	pushl %esi
	pushl %ebx
	movl 36(%esp),%eax
	movl sp_buf_size,%edx
	leal 0(,%edx,8),%ebp
	movl (%eax),%edi
	orl 4(%eax),%edi
	movl %edi,%eax
	testl %eax,%eax
	jne .L2153
	cmpl %ebp,32(%esp)
	je .L2152
.L2153:
	movl $-22,%eax
	jmp .L2151
	.p2align 2
.L2152:
	pushl $7
	pushl 36(%esp)
	call kmalloc
	movl %eax,24(%esp)
	addl $8,%esp
	cmpl $0,16(%esp)
	jne .L2155
	movl $-14,%eax
	jmp .L2151
	.p2align 2
.L2155:
	xorl %ebx,%ebx
	movl $sprof_ready,%ecx
	.p2align 2
.L2159:
	leal 0(,%ebx,4),%eax
	cmpl $0,(%eax,%ecx)
	jne .L2158
	incl %ebx
	jz .L2159
	movl $sprof_wait,%eax
	call interruptible_sleep_on
	movl $-8192,%eax
#APP
	andl %esp,%eax; 
#NO_APP
	cmpl $0,8(%eax)
	je .L2155
	movl 16(%esp),%edx
	pushl %edx
	call kfree
	movl $-512,%eax
	jmp .L2213
	.p2align 2
.L2158:
	movl $0,(%eax,%ecx)
	testl %ebx,%ebx
	jne .L2169
	pushl $0
	call pmc_stop
	addl $4,%esp
.L2169:
	movl %ebx,%eax
	sall $5,%eax
	movl sprof_data+4(%eax),%esi
	movl %ebp,%ecx
	shrl $2,%ecx
	movl 16(%esp),%edi
	movl %ebp,%edx
#APP
	rep ; movsl
	testb $2,%dl
	je 1f
	movsw
1:	testb $1,%dl
	je 2f
	movsb
2:
#NO_APP
	testl %ebx,%ebx
	jne .L2195
	pushl $0
	call pmc_start
	addl $4,%esp
	.p2align 2
.L2195:
	movl 32(%esp),%edi
	pushl %edi
	pushl 20(%esp)
	pushl 36(%esp)
	call __generic_copy_to_user
	addl $12,%esp
	testl %eax,%eax
	je .L2194
	movl 16(%esp),%edx
	pushl %edx
	call kfree
	movl $-14,%eax
	jmp .L2213
	.p2align 2
.L2194:
	movl 16(%esp),%edi
	pushl %edi
	call kfree
	movl 36(%esp),%eax
.L2213:
	addl $4,%esp
.L2151:
	popl %ebx
	popl %esi
	popl %edi
	popl %ebp
	popl %ecx
	ret
.Lfe17:
	.size	 sprof_read,.Lfe17-sprof_read
.data
	.align 32
	.type	 sprof_fops,@object
	.size	 sprof_fops,64
sprof_fops:
	.long __this_module
	.zero	4
	.long sprof_read
	.zero	20
	.long sprof_open
	.zero	4
	.long sprof_release
	.zero	20
.section	.rodata
	.align 32
.LC13:
	.string	"sp_hash_size value %d not in range\n"
	.align 32
.LC14:
	.string	"sp_buf_size value %d not in range\n"
	.align 32
.LC15:
	.string	"sprofile: neither counter enabled for CPU%d\n"
	.align 32
.LC16:
	.string	"ctr0 count value %d not in range\n"
	.align 32
.LC17:
	.string	"ctr1 count value %d not in range\n"
	.align 32
.LC18:
	.string	"sprofile: ctr0: no such event\n"
	.align 32
.LC19:
	.string	"sprofile: ctr1: no such event\n"
	.align 32
.LC20:
	.string	"sprofile: ctr0: invalid unit mask\n"
	.align 32
.LC21:
	.string	"sprofile: ctr1: invalid unit mask\n"
	.align 32
.LC22:
	.string	"sprofile: ctr0: can't count event\n"
	.align 32
.LC23:
	.string	"sprofile: ctr1: can't count event\n"
	.align 32
.LC24:
	.string	"sprofile: ctr0: event only available on PII\n"
	.align 32
.LC25:
	.string	"sprofile: ctr1: event only available on PII\n"
	.align 32
.LC26:
	.string	"sprofile: ctr0: event only available on PIII\n"
	.align 32
.LC27:
	.string	"sprofile: ctr1: event only available on PIII\n"
.text
	.align 4
	.type	 parms_ok,@function
parms_ok:
	subl $4,%esp
	pushl %ebp
	pushl %edi
	pushl %esi
	pushl %ebx
	movl sp_hash_size,%edx
	leal -256(%edx),%eax
	cmpl $3840,%eax
	jbe .L2217
	pushl %edx
	pushl $.LC13
	jmp .L2266
	.p2align 2
.L2217:
	movl sp_buf_size,%edx
	leal -512(%edx),%eax
	cmpl $3584,%eax
	jbe .L2223
	pushl %edx
	pushl $.LC14
	jmp .L2266
	.p2align 2
.L2223:
	xorl %esi,%esi
	.p2align 2
.L2231:
	movl %esi,%ecx
	sall $5,%ecx
	movl %ecx,%edi
	addl $sprof_data,%edi
	movzbl sprof_data+29(%ecx),%eax
	cmpb $0,sp_ctr0_on(%esi)
	je .L2232
	orb $1,%al
.L2232:
	movb %al,sprof_data+29(%ecx)
	movzbl sprof_data+29(%ecx),%eax
	cmpb $0,sp_ctr1_on(%esi)
	je .L2233
	orb $2,%al
.L2233:
	movb %al,sprof_data+29(%ecx)
	movb %al,%dl
	testb %al,%al
	jne .L2234
	pushl %esi
	pushl $.LC15
	jmp .L2266
	.p2align 2
.L2234:
	testb $1,%dl
	je .L2235
	movl sp_ctr0_count(,%esi,4),%edx
	leal -500(%edx),%eax
	cmpl $2147483147,%eax
	jbe .L2237
	pushl %edx
	pushl $.LC16
	jmp .L2266
	.p2align 2
.L2237:
	movl %edx,sprof_data+16(%ecx)
.L2235:
	leal 0(,%esi,4),%ebx
	testb $2,29(%edi)
	je .L2242
	movl sp_ctr1_count(%ebx),%edx
	leal -500(%edx),%eax
	cmpl $2147483147,%eax
	jbe .L2244
	pushl %edx
	pushl $.LC17
	.p2align 2
.L2266:
	call printk
	xorl %eax,%eax
	addl $8,%esp
	jmp .L2215
	.p2align 2
.L2244:
	movl %edx,20(%edi)
.L2242:
	movl sp_ctr0_um(%ebx),%eax
	movl $sp_ctr1_um,%edi
	movl $sp_ctr1_type,%ebp
	testl %eax,%eax
	je .L2249
	pushl $0
	pushl $0
	pushl %eax
	call simple_strtoul
	movb %al,31(%esp)
	addl $12,%esp
	jmp .L2250
	.p2align 2
.L2249:
	movb $0,19(%esp)
.L2250:
	movl (%ebx,%edi),%eax
	testl %eax,%eax
	je .L2251
	pushl $0
	pushl $0
	pushl %eax
	call simple_strtoul
	movb %al,%dl
	addl $12,%esp
	jmp .L2252
	.p2align 2
.L2251:
	xorl %edx,%edx
.L2252:
	movl %esi,%eax
	addl $sp_ctr1_val,%eax
	pushl %eax
	movl %esi,%eax
	addl $sp_ctr0_val,%eax
	pushl %eax
	pushl cpu_type
	movzbl %dl,%eax
	pushl %eax
	movzbl 35(%esp),%eax
	pushl %eax
	pushl (%ebx,%ebp)
	pushl sp_ctr0_type(%ebx)
	call sp_check_events_str
	movl %eax,%ebx
	addl $28,%esp
	testb $1,%bl
	je .L2253
	pushl $.LC18
	call printk
	addl $4,%esp
.L2253:
	testb $2,%bl
	je .L2254
	pushl $.LC19
	call printk
	addl $4,%esp
.L2254:
	testb $4,%bl
	je .L2255
	pushl $.LC20
	call printk
	addl $4,%esp
.L2255:
	testb $8,%bl
	je .L2256
	pushl $.LC21
	call printk
	addl $4,%esp
.L2256:
	testb $16,%bl
	je .L2257
	pushl $.LC22
	call printk
	addl $4,%esp
.L2257:
	testb $32,%bl
	je .L2258
	pushl $.LC23
	call printk
	addl $4,%esp
.L2258:
	testb $64,%bl
	je .L2259
	pushl $.LC24
	call printk
	addl $4,%esp
.L2259:
	testb %bl,%bl
	jge .L2260
	pushl $.LC25
	call printk
	addl $4,%esp
.L2260:
	testb $1,%bh
	je .L2261
	pushl $.LC26
	call printk
	addl $4,%esp
.L2261:
	testb $2,%bh
	je .L2262
	pushl $.LC27
	call printk
	addl $4,%esp
.L2262:
	testl %ebx,%ebx
	je .L2230
	xorl %eax,%eax
	jmp .L2215
	.p2align 2
.L2230:
	incl %esi
	jz .L2231
	movl $1,%eax
.L2215:
	popl %ebx
	popl %esi
	popl %edi
	popl %ebp
	popl %ecx
	ret
.Lfe18:
	.size	 parms_ok,.Lfe18-parms_ok
	.align 4
	.type	 sprof_free_mem,@function
sprof_free_mem:
	pushl %edi
	pushl %esi
	pushl %ebx
	movl 16(%esp),%edi
	xorl %esi,%esi
	cmpl %edi,%esi
	jae .L2269
	.p2align 2
.L2271:
	movl %esi,%ebx
	sall $5,%ebx
	pushl sprof_data(%ebx)
	call kfree
	pushl sprof_data+4(%ebx)
	call kfree
	addl $8,%esp
	incl %esi
	cmpl %edi,%esi
	jb .L2271
.L2269:
	popl %ebx
	popl %esi
	popl %edi
	ret
.Lfe19:
	.size	 sprof_free_mem,.Lfe19-sprof_free_mem
.section	.rodata
	.align 32
.LC28:
	.string	"sprofile: failed to allocate hash table of %lu bytes\n"
	.align 32
.LC29:
	.string	"sprofile: failed to allocate eviction buffer of %lu bytes\n"
.text
	.align 4
	.type	 sprof_init_data,@function
sprof_init_data:
	subl $4,%esp
	pushl %ebp
	pushl %edi
	pushl %esi
	pushl %ebx
	movl $sprof_wait,%eax
	testl %eax,%eax
	jne .L2274
	pushl $.LC0
	call printk
	addl $4,%esp
	pushl $126
	pushl $.LC1
	pushl $.LC2
	call printk
#APP
	.byte 0x0f,0x0b
#NO_APP
	addl $12,%esp
	.p2align 2
.L2274:
	movl $0,sprof_wait
	movl $sprof_wait+4,sprof_wait+4
	movl $sprof_wait+4,sprof_wait+8
	movl $sprof_wait+12,sprof_wait+12
#APP
	movl $1f,sprof_wait+16
1:
#NO_APP
	movl $0,16(%esp)
	.p2align 2
.L2294:
	movl 16(%esp),%ebx
	sall $5,%ebx
	movl sp_hash_size,%edi
	sall $5,%edi
	movl %edi,%ebp
	movl sp_buf_size,%eax
	leal 0(,%eax,8),%esi
	pushl $7
	pushl %edi
	call kmalloc
	movl %eax,%ecx
	movl %ecx,sprof_data(%ebx)
	addl $8,%esp
	testl %ecx,%ecx
	jne .L2295
	pushl %edi
	pushl $.LC28
	call printk
	pushl 24(%esp)
	call sprof_free_mem
	movl $-14,%eax
	addl $12,%esp
	jmp .L2273
	.p2align 2
.L2295:
	pushl $7
	pushl %esi
	call kmalloc
	movl %eax,sprof_data+4(%ebx)
	addl $8,%esp
	testl %eax,%eax
	jne .L2299
	pushl %esi
	pushl $.LC29
	call printk
	pushl sprof_data(%ebx)
	call kfree
	pushl 28(%esp)
	call sprof_free_mem
	movl $-14,%eax
	addl $16,%esp
	jmp .L2273
	.p2align 2
.L2299:
	movl sprof_data(%ebx),%edi
	movl %ebp,%ecx
	shrl $2,%ecx
	xorl %eax,%eax
	movl %ebp,%edx
#APP
	rep ; stosl
	testb $2,%dl
	je 1f
	stosw
1:	testb $1,%dl
	je 2f
	stosb
2:
#NO_APP
	movl sprof_data+4(%ebx),%edi
	movl %esi,%ecx
	shrl $2,%ecx
	movl %esi,%edx
#APP
	rep ; stosl
	testb $2,%dl
	je 1f
	stosw
1:	testb $1,%dl
	je 2f
	stosb
2:
#NO_APP
	movl sp_hash_size,%eax
	movl %eax,sprof_data+8(%ebx)
	movl sp_buf_size,%edx
	movl %edx,sprof_data+12(%ebx)
	incl 16(%esp)
	jz .L2294
	xorl %eax,%eax
.L2273:
	popl %ebx
	popl %esi
	popl %edi
	popl %ebp
	popl %ecx
	ret
.Lfe20:
	.size	 sprof_init_data,.Lfe20-sprof_init_data
.section	.rodata
	.align 32
.LC30:
	.string	"sprofile: not an Intel P6 processor. Sorry.\n"
.LC31:
	.string	"CPU type %u\n"
.text
	.align 4
.globl hw_ok
	.type	 hw_ok,@function
hw_ok:
	cmpb $0,boot_cpu_data+1
	jne .L2346
	cmpb $5,boot_cpu_data
	ja .L2345
.L2346:
	pushl $.LC30
	call printk
	xorl %eax,%eax
	addl $4,%esp
	ret
	.p2align 2
.L2345:
	movb boot_cpu_data+2,%al
	cmpb $2,%al
	seta %dl
	movzbl %dl,%edx
	movl %edx,cpu_type
	cmpb $5,%al
	jbe .L2347
	incl %edx
.L2347:
	movl %edx,cpu_type
	pushl %edx
	pushl $.LC31
	call printk
	movl $1,%eax
	addl $8,%esp
	ret
.Lfe21:
	.size	 hw_ok,.Lfe21-hw_ok
.section	.rodata
.LC32:
	.string	"%s\n"
.LC33:
	.string	"sprof"
	.align 32
.LC34:
	.string	"sprofile: /dev/sprofile enabled, major %u\n"
.text
	.align 4
.globl sprof_init
	.type	 sprof_init,@function
sprof_init:
	pushl %ebx
	call parms_ok
	testl %eax,%eax
	je .L2350
	call hw_ok
	testl %eax,%eax
	jne .L2349
.L2350:
	movl $-22,%eax
	jmp .L2348
	.p2align 2
.L2349:
	pushl sp_version
	pushl $.LC32
	call printk
	call sprof_init_data
	movl %eax,%ebx
	addl $8,%esp
	testl %ebx,%ebx
	jne .L2348
	call install_nmi
	call apic_setup
	movl %eax,%ebx
	testl %ebx,%ebx
	je .L2352
	call restore_nmi
	pushl $1
	call sprof_free_mem
	movl %ebx,%eax
	addl $4,%esp
	jmp .L2348
	.p2align 2
.L2352:
	pushl $0
	call pmc_setup
	pushl $sprof_fops
	pushl $.LC33
	pushl $0
	call register_chrdev
	movl %eax,%ebx
	movl %ebx,sp_major
	addl $16,%esp
	testl %ebx,%ebx
	jl .L2354
	pushl %ebx
	pushl $.LC34
	call printk
	xorl %eax,%eax
	jmp .L2359
	.p2align 2
.L2354:
	pushl $0
	call disable_local_P6_APIC
	call restore_nmi
	pushl $1
	call sprof_free_mem
	movl %ebx,%eax
.L2359:
	addl $8,%esp
.L2348:
	popl %ebx
	ret
.Lfe22:
	.size	 sprof_init,.Lfe22-sprof_init
	.align 4
.globl sprof_exit
	.type	 sprof_exit,@function
sprof_exit:
	pushl $.LC33
	pushl sp_major
	call unregister_chrdev
	addl $8,%esp
	pushl $0
	call disable_local_P6_APIC
	call restore_nmi
	addl $4,%esp
	pushl $-967296
	call __const_udelay
	addl $4,%esp
	pushl $1
	call sprof_free_mem
	addl $4,%esp
	ret
.Lfe23:
	.size	 sprof_exit,.Lfe23-sprof_exit
.globl init_module
	.set	init_module,sprof_init
.globl cleanup_module
	.set	cleanup_module,sprof_exit
	.comm	idt_addr,4,4
	.comm	kernel_nmi,4,4
	.local	sp_ctr0_on
	.comm	sp_ctr0_on,1,1
	.local	sp_ctr1_on
	.comm	sp_ctr1_on,1,1
	.local	sp_ctr0_type
	.comm	sp_ctr0_type,4,4
	.local	sp_ctr1_type
	.comm	sp_ctr1_type,4,4
	.local	sp_ctr0_um
	.comm	sp_ctr0_um,4,4
	.local	sp_ctr1_um
	.comm	sp_ctr1_um,4,4
	.local	sp_ctr0_count
	.comm	sp_ctr0_count,4,4
	.local	sp_ctr1_count
	.comm	sp_ctr1_count,4,4
	.local	sp_ctr0_val
	.comm	sp_ctr0_val,1,1
	.local	sp_ctr1_val
	.comm	sp_ctr1_val,1,1
	.local	sp_major
	.comm	sp_major,4,4
	.local	cpu_type
	.comm	cpu_type,4,4
	.local	sprof_opened
	.comm	sprof_opened,4,4
	.local	sprof_ready
	.comm	sprof_ready,4,4
	.local	sprof_data
	.comm	sprof_data,32,32
	.ident	"GCC: (GNU) egcs-2.91.66 19990314/Linux (egcs-1.1.2 release)"
