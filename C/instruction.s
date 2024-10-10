	.file	1 "instruction.c"
	.section .mdebug.abi32
	.previous
	.nan	legacy
	.module	fp=xx
	.module	nooddspreg
	.abicalls
	.text
	.rdata
	.align	2
$LC2:
	.ascii	"%f\012\000"
	.text
	.align	2
	.globl	main
	.set	nomips16
	.set	nomicromips
	.ent	main
	.type	main, @function
main:
	.frame	$fp,304,$31		# vars= 272, regs= 2/0, args= 16, gp= 8
	.mask	0xc0000000,-4
	.fmask	0x00000000,0
	.set	noreorder
	.set	nomacro
	addiu	$sp,$sp,-304
	sw	$31,300($sp)
	sw	$fp,296($sp)
	move	$fp,$sp
	lui	$28,%hi(__gnu_local_gp)
	addiu	$28,$28,%lo(__gnu_local_gp)
	.cprestore	16
	lw	$2,%got(__stack_chk_guard)($28)
	lw	$2,0($2)
	sw	$2,292($fp)
	lui	$2,%hi($LC0)
	lwc1	$f0,%lo($LC0)($2)
	swc1	$f0,28($fp)
	li	$2,64			# 0x40
	sw	$2,32($fp)
	sw	$0,24($fp)
	.option	pic0
	b	$L2
	nop

	.option	pic2
$L3:
	lw	$2,24($fp)
	sll	$2,$2,2
	addiu	$2,$2,296
	addu	$2,$2,$fp
	lwc1	$f2,-260($2)
	lwc1	$f0,28($fp)
	mul.s	$f2,$f2,$f0
	lui	$2,%hi($LC1)
	lwc1	$f0,%lo($LC1)($2)
	add.s	$f0,$f2,$f0
	lw	$2,24($fp)
	sll	$2,$2,2
	addiu	$2,$2,296
	addu	$2,$2,$fp
	swc1	$f0,-260($2)
	lw	$2,24($fp)
	sll	$2,$2,2
	addiu	$2,$2,296
	addu	$2,$2,$fp
	lwc1	$f0,-260($2)
	cvt.d.s	$f0,$f0
	mfc1	$7,$f0
	mfhc1	$6,$f0
	lui	$2,%hi($LC2)
	addiu	$4,$2,%lo($LC2)
	lw	$2,%call16(printf)($28)
	move	$25,$2
	.reloc	1f,R_MIPS_JALR,printf
1:	jalr	$25
	nop

	lw	$28,16($fp)
	lw	$2,24($fp)
	addiu	$2,$2,1
	sw	$2,24($fp)
$L2:
	lw	$2,24($fp)
	slt	$2,$2,64
	bne	$2,$0,$L3
	nop

	nop
	lw	$2,%got(__stack_chk_guard)($28)
	lw	$3,292($fp)
	lw	$2,0($2)
	beq	$3,$2,$L5
	nop

	lw	$2,%call16(__stack_chk_fail)($28)
	move	$25,$2
	.reloc	1f,R_MIPS_JALR,__stack_chk_fail
1:	jalr	$25
	nop

$L5:
	move	$sp,$fp
	lw	$31,300($sp)
	lw	$fp,296($sp)
	addiu	$sp,$sp,304
	jr	$31
	nop

	.set	macro
	.set	reorder
	.end	main
	.size	main, .-main
	.rdata
	.align	2
$LC0:
	.word	1063675494
	.align	2
$LC1:
	.word	1056964608
	.ident	"GCC: (Ubuntu 10.5.0-1ubuntu1~20.04) 10.5.0"
	.section	.note.GNU-stack,"",@progbits
