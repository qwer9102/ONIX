	.file	"param.c"
	.text
	.globl	add
	.type	add, @function
add:
	endbr64
	movl	%edi, -12(%rsp)
	movl	%esi, -16(%rsp)
	movl	-12(%rsp), %edx
	movl	-16(%rsp), %eax
	addl	%edx, %eax
	movl	%eax, -4(%rsp)
	movl	-4(%rsp), %eax
	ret
	.size	add, .-add
	.globl	main
	.type	main, @function
main:
	endbr64
	subq	$16, %rsp
	movl	$5, 4(%rsp)
	movl	$3, 8(%rsp)
	movl	8(%rsp), %edx
	movl	4(%rsp), %eax
	movl	%edx, %esi
	movl	%eax, %edi
	call	add
	movl	%eax, 12(%rsp)
	movl	$0, %eax
	addq	$16, %rsp
	ret
	.size	main, .-main
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
