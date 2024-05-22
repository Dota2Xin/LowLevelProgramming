	.file	"kernels.c"
	.text
	.globl	team
	.section	.rodata
.LC0:
	.string	"Ws Only"
.LC1:
	.string	"IDRK WUSSUP"
.LC2:
	.string	"ddx@ucsb.edu"
.LC3:
	.string	""
	.section	.data.rel.local,"aw"
	.align 32
	.type	team, @object
	.size	team, 40
team:
	.quad	.LC0
	.quad	.LC1
	.quad	.LC2
	.quad	.LC3
	.quad	.LC3
	.globl	naive_rotate_descr
	.data
	.align 32
	.type	naive_rotate_descr, @object
	.size	naive_rotate_descr, 44
naive_rotate_descr:
	.string	"naive_rotate: Naive baseline implementation"
	.text
	.globl	naive_rotate
	.type	naive_rotate, @function
naive_rotate:
.LFB6:
	.cfi_startproc
	endbr64
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset 6, -16
	movq	%rsp, %rbp
	.cfi_def_cfa_register 6
	movl	%edi, -20(%rbp)
	movq	%rsi, -32(%rbp)
	movq	%rdx, -40(%rbp)
	movl	$0, -8(%rbp)
	jmp	.L2
.L5:
	movl	$0, -4(%rbp)
	jmp	.L3
.L4:
	movl	-8(%rbp), %eax
	imull	-20(%rbp), %eax
	movl	%eax, %edx
	movl	-4(%rbp), %eax
	addl	%edx, %eax
	movslq	%eax, %rdx
	movq	%rdx, %rax
	addq	%rax, %rax
	addq	%rdx, %rax
	addq	%rax, %rax
	movq	%rax, %rdx
	movq	-32(%rbp), %rax
	addq	%rax, %rdx
	movl	-20(%rbp), %eax
	subl	$1, %eax
	subl	-4(%rbp), %eax
	imull	-20(%rbp), %eax
	movl	%eax, %ecx
	movl	-8(%rbp), %eax
	addl	%ecx, %eax
	movslq	%eax, %rcx
	movq	%rcx, %rax
	addq	%rax, %rax
	addq	%rcx, %rax
	addq	%rax, %rax
	movq	%rax, %rcx
	movq	-40(%rbp), %rax
	addq	%rcx, %rax
	movl	(%rdx), %ecx
	movl	%ecx, (%rax)
	movzwl	4(%rdx), %edx
	movw	%dx, 4(%rax)
	addl	$1, -4(%rbp)
.L3:
	movl	-4(%rbp), %eax
	cmpl	-20(%rbp), %eax
	jl	.L4
	addl	$1, -8(%rbp)
.L2:
	movl	-8(%rbp), %eax
	cmpl	-20(%rbp), %eax
	jl	.L5
	nop
	nop
	popq	%rbp
	.cfi_def_cfa 7, 8
	ret
	.cfi_endproc
.LFE6:
	.size	naive_rotate, .-naive_rotate
	.globl	rotate_descr
	.data
	.align 32
	.type	rotate_descr, @object
	.size	rotate_descr, 32
rotate_descr:
	.string	"rotate: Current working version"
	.text
	.globl	rotate
	.type	rotate, @function
rotate:
.LFB7:
	.cfi_startproc
	endbr64
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset 6, -16
	movq	%rsp, %rbp
	.cfi_def_cfa_register 6
	movl	%edi, -36(%rbp)
	movq	%rsi, -48(%rbp)
	movq	%rdx, -56(%rbp)
	movl	$0, -20(%rbp)
	movl	$0, -16(%rbp)
	movl	$0, -12(%rbp)
	movl	$0, -8(%rbp)
	movl	-36(%rbp), %eax
	imull	%eax, %eax
	movl	%eax, -4(%rbp)
	movl	$0, -20(%rbp)
	jmp	.L7
.L10:
	movl	-20(%rbp), %eax
	imull	-36(%rbp), %eax
	movl	%eax, -12(%rbp)
	movl	$0, -16(%rbp)
	jmp	.L8
.L9:
	movl	-16(%rbp), %eax
	addl	$1, %eax
	imull	-36(%rbp), %eax
	movl	%eax, %edx
	movl	-4(%rbp), %eax
	subl	%edx, %eax
	movl	%eax, %ecx
	movl	-20(%rbp), %eax
	addl	%ecx, %eax
	movl	%eax, -8(%rbp)
	movl	-12(%rbp), %edx
	movl	-16(%rbp), %eax
	addl	%edx, %eax
	movslq	%eax, %rdx
	movq	%rdx, %rax
	addq	%rax, %rax
	addq	%rdx, %rax
	addq	%rax, %rax
	movq	%rax, %rdx
	movq	-48(%rbp), %rax
	addq	%rax, %rdx
	movl	-8(%rbp), %eax
	movslq	%eax, %rcx
	movq	%rcx, %rax
	addq	%rax, %rax
	addq	%rcx, %rax
	addq	%rax, %rax
	movq	%rax, %rcx
	movq	-56(%rbp), %rax
	addq	%rcx, %rax
	movl	(%rdx), %ecx
	movl	%ecx, (%rax)
	movzwl	4(%rdx), %edx
	movw	%dx, 4(%rax)
	movl	-12(%rbp), %edx
	movl	-16(%rbp), %eax
	addl	%edx, %eax
	cltq
	leaq	1(%rax), %rdx
	movq	%rdx, %rax
	addq	%rax, %rax
	addq	%rdx, %rax
	addq	%rax, %rax
	movq	%rax, %rdx
	movq	-48(%rbp), %rax
	addq	%rax, %rdx
	movl	-8(%rbp), %eax
	subl	-36(%rbp), %eax
	movslq	%eax, %rcx
	movq	%rcx, %rax
	addq	%rax, %rax
	addq	%rcx, %rax
	addq	%rax, %rax
	movq	%rax, %rcx
	movq	-56(%rbp), %rax
	addq	%rcx, %rax
	movl	(%rdx), %ecx
	movl	%ecx, (%rax)
	movzwl	4(%rdx), %edx
	movw	%dx, 4(%rax)
	addl	$2, -16(%rbp)
.L8:
	movl	-16(%rbp), %eax
	cmpl	-36(%rbp), %eax
	jl	.L9
	addl	$1, -20(%rbp)
.L7:
	movl	-20(%rbp), %eax
	cmpl	-36(%rbp), %eax
	jl	.L10
	nop
	nop
	popq	%rbp
	.cfi_def_cfa 7, 8
	ret
	.cfi_endproc
.LFE7:
	.size	rotate, .-rotate
	.globl	register_rotate_functions
	.type	register_rotate_functions, @function
register_rotate_functions:
.LFB8:
	.cfi_startproc
	endbr64
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset 6, -16
	movq	%rsp, %rbp
	.cfi_def_cfa_register 6
	leaq	naive_rotate_descr(%rip), %rax
	movq	%rax, %rsi
	leaq	naive_rotate(%rip), %rax
	movq	%rax, %rdi
	call	add_rotate_function@PLT
	leaq	rotate_descr(%rip), %rax
	movq	%rax, %rsi
	leaq	rotate(%rip), %rax
	movq	%rax, %rdi
	call	add_rotate_function@PLT
	nop
	popq	%rbp
	.cfi_def_cfa 7, 8
	ret
	.cfi_endproc
.LFE8:
	.size	register_rotate_functions, .-register_rotate_functions
	.type	min, @function
min:
.LFB9:
	.cfi_startproc
	endbr64
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset 6, -16
	movq	%rsp, %rbp
	.cfi_def_cfa_register 6
	movl	%edi, -4(%rbp)
	movl	%esi, -8(%rbp)
	movl	-8(%rbp), %edx
	movl	-4(%rbp), %eax
	cmpl	%eax, %edx
	cmovle	%edx, %eax
	popq	%rbp
	.cfi_def_cfa 7, 8
	ret
	.cfi_endproc
.LFE9:
	.size	min, .-min
	.type	max, @function
max:
.LFB10:
	.cfi_startproc
	endbr64
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset 6, -16
	movq	%rsp, %rbp
	.cfi_def_cfa_register 6
	movl	%edi, -4(%rbp)
	movl	%esi, -8(%rbp)
	movl	-8(%rbp), %edx
	movl	-4(%rbp), %eax
	cmpl	%eax, %edx
	cmovge	%edx, %eax
	popq	%rbp
	.cfi_def_cfa 7, 8
	ret
	.cfi_endproc
.LFE10:
	.size	max, .-max
	.type	initialize_pixel_sum, @function
initialize_pixel_sum:
.LFB11:
	.cfi_startproc
	endbr64
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset 6, -16
	movq	%rsp, %rbp
	.cfi_def_cfa_register 6
	movq	%rdi, -8(%rbp)
	movq	-8(%rbp), %rax
	movl	$0, 8(%rax)
	movq	-8(%rbp), %rax
	movl	8(%rax), %edx
	movq	-8(%rbp), %rax
	movl	%edx, 4(%rax)
	movq	-8(%rbp), %rax
	movl	4(%rax), %edx
	movq	-8(%rbp), %rax
	movl	%edx, (%rax)
	movq	-8(%rbp), %rax
	movl	$0, 12(%rax)
	nop
	popq	%rbp
	.cfi_def_cfa 7, 8
	ret
	.cfi_endproc
.LFE11:
	.size	initialize_pixel_sum, .-initialize_pixel_sum
	.type	accumulate_sum, @function
accumulate_sum:
.LFB12:
	.cfi_startproc
	endbr64
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset 6, -16
	movq	%rsp, %rbp
	.cfi_def_cfa_register 6
	movq	%rdi, -8(%rbp)
	movq	%rsi, -16(%rbp)
	movq	-8(%rbp), %rax
	movl	(%rax), %edx
	movzwl	-16(%rbp), %eax
	movzwl	%ax, %eax
	addl	%eax, %edx
	movq	-8(%rbp), %rax
	movl	%edx, (%rax)
	movq	-8(%rbp), %rax
	movl	4(%rax), %edx
	movzwl	-14(%rbp), %eax
	movzwl	%ax, %eax
	addl	%eax, %edx
	movq	-8(%rbp), %rax
	movl	%edx, 4(%rax)
	movq	-8(%rbp), %rax
	movl	8(%rax), %edx
	movzwl	-12(%rbp), %eax
	movzwl	%ax, %eax
	addl	%eax, %edx
	movq	-8(%rbp), %rax
	movl	%edx, 8(%rax)
	movq	-8(%rbp), %rax
	movl	12(%rax), %eax
	leal	1(%rax), %edx
	movq	-8(%rbp), %rax
	movl	%edx, 12(%rax)
	nop
	popq	%rbp
	.cfi_def_cfa 7, 8
	ret
	.cfi_endproc
.LFE12:
	.size	accumulate_sum, .-accumulate_sum
	.type	assign_sum_to_pixel, @function
assign_sum_to_pixel:
.LFB13:
	.cfi_startproc
	endbr64
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset 6, -16
	movq	%rsp, %rbp
	.cfi_def_cfa_register 6
	movq	%rdi, -8(%rbp)
	movq	%rdx, %rcx
	movq	%rsi, %rax
	movq	%rdi, %rdx
	movq	%rcx, %rdx
	movq	%rax, -32(%rbp)
	movq	%rdx, -24(%rbp)
	movl	-32(%rbp), %eax
	movl	-20(%rbp), %esi
	cltd
	idivl	%esi
	movl	%eax, %edx
	movq	-8(%rbp), %rax
	movw	%dx, (%rax)
	movl	-28(%rbp), %eax
	movl	-20(%rbp), %edi
	cltd
	idivl	%edi
	movl	%eax, %edx
	movq	-8(%rbp), %rax
	movw	%dx, 2(%rax)
	movl	-24(%rbp), %eax
	movl	-20(%rbp), %ecx
	cltd
	idivl	%ecx
	movl	%eax, %edx
	movq	-8(%rbp), %rax
	movw	%dx, 4(%rax)
	nop
	popq	%rbp
	.cfi_def_cfa 7, 8
	ret
	.cfi_endproc
.LFE13:
	.size	assign_sum_to_pixel, .-assign_sum_to_pixel
	.type	avg, @function
avg:
.LFB14:
	.cfi_startproc
	endbr64
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset 6, -16
	movq	%rsp, %rbp
	.cfi_def_cfa_register 6
	subq	$96, %rsp
	movl	%edi, -68(%rbp)
	movl	%esi, -72(%rbp)
	movl	%edx, -76(%rbp)
	movq	%rcx, -88(%rbp)
	movq	%fs:40, %rax
	movq	%rax, -8(%rbp)
	xorl	%eax, %eax
	leaq	-32(%rbp), %rax
	movq	%rax, %rdi
	call	initialize_pixel_sum
	movl	-72(%rbp), %eax
	subl	$1, %eax
	movl	$0, %esi
	movl	%eax, %edi
	call	max
	movl	%eax, -52(%rbp)
	jmp	.L23
.L26:
	movl	-76(%rbp), %eax
	subl	$1, %eax
	movl	$0, %esi
	movl	%eax, %edi
	call	max
	movl	%eax, -48(%rbp)
	jmp	.L24
.L25:
	movl	-52(%rbp), %eax
	imull	-68(%rbp), %eax
	movl	%eax, %edx
	movl	-48(%rbp), %eax
	addl	%edx, %eax
	movslq	%eax, %rdx
	movq	%rdx, %rax
	addq	%rax, %rax
	addq	%rdx, %rax
	addq	%rax, %rax
	movq	%rax, %rdx
	movq	-88(%rbp), %rax
	addq	%rdx, %rax
	leaq	-32(%rbp), %rdx
	movzwl	(%rax), %esi
	movzwl	2(%rax), %ecx
	salq	$16, %rcx
	orq	%rsi, %rcx
	movzwl	4(%rax), %eax
	salq	$32, %rax
	orq	%rcx, %rax
	movq	%rax, %rsi
	movq	%rdx, %rdi
	call	accumulate_sum
	addl	$1, -48(%rbp)
.L24:
	movl	-68(%rbp), %eax
	leal	-1(%rax), %edx
	movl	-76(%rbp), %eax
	addl	$1, %eax
	movl	%edx, %esi
	movl	%eax, %edi
	call	min
	cmpl	%eax, -48(%rbp)
	jle	.L25
	addl	$1, -52(%rbp)
.L23:
	movl	-68(%rbp), %eax
	leal	-1(%rax), %edx
	movl	-72(%rbp), %eax
	addl	$1, %eax
	movl	%edx, %esi
	movl	%eax, %edi
	call	min
	cmpl	%eax, -52(%rbp)
	jle	.L26
	movq	-32(%rbp), %rcx
	movq	-24(%rbp), %rdx
	leaq	-44(%rbp), %rax
	movq	%rcx, %rsi
	movq	%rax, %rdi
	call	assign_sum_to_pixel
	movl	-44(%rbp), %eax
	movl	%eax, -38(%rbp)
	movzwl	-40(%rbp), %eax
	movw	%ax, -34(%rbp)
	movl	$0, %eax
	movl	-38(%rbp), %edx
	movl	%edx, %ecx
	movabsq	$-4294967296, %rdx
	andq	%rdx, %rax
	orq	%rcx, %rax
	movzwl	-34(%rbp), %edx
	movzwl	%dx, %edx
	salq	$32, %rdx
	movabsq	$-281470681743361, %rcx
	andq	%rcx, %rax
	orq	%rdx, %rax
	movq	-8(%rbp), %rdx
	subq	%fs:40, %rdx
	je	.L28
	call	__stack_chk_fail@PLT
.L28:
	leave
	.cfi_def_cfa 7, 8
	ret
	.cfi_endproc
.LFE14:
	.size	avg, .-avg
	.globl	naive_smooth_descr
	.data
	.align 32
	.type	naive_smooth_descr, @object
	.size	naive_smooth_descr, 44
naive_smooth_descr:
	.string	"naive_smooth: Naive baseline implementation"
	.text
	.globl	naive_smooth
	.type	naive_smooth, @function
naive_smooth:
.LFB15:
	.cfi_startproc
	endbr64
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset 6, -16
	movq	%rsp, %rbp
	.cfi_def_cfa_register 6
	pushq	%rbx
	subq	$56, %rsp
	.cfi_offset 3, -24
	movl	%edi, -36(%rbp)
	movq	%rsi, -48(%rbp)
	movq	%rdx, -56(%rbp)
	movl	$0, -24(%rbp)
	jmp	.L30
.L33:
	movl	$0, -20(%rbp)
	jmp	.L31
.L32:
	movl	-24(%rbp), %eax
	imull	-36(%rbp), %eax
	movl	%eax, %edx
	movl	-20(%rbp), %eax
	addl	%edx, %eax
	movslq	%eax, %rdx
	movq	%rdx, %rax
	addq	%rax, %rax
	addq	%rdx, %rax
	addq	%rax, %rax
	movq	%rax, %rdx
	movq	-56(%rbp), %rax
	leaq	(%rdx,%rax), %rbx
	movq	-48(%rbp), %rcx
	movl	-20(%rbp), %edx
	movl	-24(%rbp), %esi
	movl	-36(%rbp), %eax
	movl	%eax, %edi
	call	avg
	movw	%ax, (%rbx)
	movq	%rax, %rdx
	shrq	$16, %rdx
	andb	$-1, %dh
	movw	%dx, 2(%rbx)
	shrq	$32, %rax
	andb	$-1, %ah
	movw	%ax, 4(%rbx)
	addl	$1, -20(%rbp)
.L31:
	movl	-20(%rbp), %eax
	cmpl	-36(%rbp), %eax
	jl	.L32
	addl	$1, -24(%rbp)
.L30:
	movl	-24(%rbp), %eax
	cmpl	-36(%rbp), %eax
	jl	.L33
	nop
	nop
	movq	-8(%rbp), %rbx
	leave
	.cfi_def_cfa 7, 8
	ret
	.cfi_endproc
.LFE15:
	.size	naive_smooth, .-naive_smooth
	.globl	smooth_descr
	.data
	.align 32
	.type	smooth_descr, @object
	.size	smooth_descr, 32
smooth_descr:
	.string	"smooth: Current working version"
	.text
	.globl	smooth
	.type	smooth, @function
smooth:
.LFB16:
	.cfi_startproc
	endbr64
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset 6, -16
	movq	%rsp, %rbp
	.cfi_def_cfa_register 6
	subq	$32, %rsp
	movl	%edi, -4(%rbp)
	movq	%rsi, -16(%rbp)
	movq	%rdx, -24(%rbp)
	movq	-24(%rbp), %rdx
	movq	-16(%rbp), %rcx
	movl	-4(%rbp), %eax
	movq	%rcx, %rsi
	movl	%eax, %edi
	call	naive_smooth
	nop
	leave
	.cfi_def_cfa 7, 8
	ret
	.cfi_endproc
.LFE16:
	.size	smooth, .-smooth
	.globl	register_smooth_functions
	.type	register_smooth_functions, @function
register_smooth_functions:
.LFB17:
	.cfi_startproc
	endbr64
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset 6, -16
	movq	%rsp, %rbp
	.cfi_def_cfa_register 6
	leaq	smooth_descr(%rip), %rax
	movq	%rax, %rsi
	leaq	smooth(%rip), %rax
	movq	%rax, %rdi
	call	add_smooth_function@PLT
	leaq	naive_smooth_descr(%rip), %rax
	movq	%rax, %rsi
	leaq	naive_smooth(%rip), %rax
	movq	%rax, %rdi
	call	add_smooth_function@PLT
	nop
	popq	%rbp
	.cfi_def_cfa 7, 8
	ret
	.cfi_endproc
.LFE17:
	.size	register_smooth_functions, .-register_smooth_functions
	.ident	"GCC: (Ubuntu 11.4.0-1ubuntu1~22.04) 11.4.0"
	.section	.note.GNU-stack,"",@progbits
	.section	.note.gnu.property,"a"
	.align 8
	.long	1f - 0f
	.long	4f - 1f
	.long	5
0:
	.string	"GNU"
1:
	.align 8
	.long	0xc0000002
	.long	3f - 2f
2:
	.long	0x3
3:
	.align 8
4:
