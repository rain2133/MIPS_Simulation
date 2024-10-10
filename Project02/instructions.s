main:
	addi	$sp, $sp, -304
	sw		$31, 300($sp)
	sw		$fp, 296($sp)
	move	$fp, $sp
	lwc1	$f0, 0($gp) // 0.9
	swc1	$f0, 32($fp)
	move	$2, $0
	addi	$2, $2, 64
	sw		$2, 28($fp) //n
	sw		$0, 24($fp) //i
	j		$L2
$L3:
	lw		$2, 24($fp)
	sll		$2, $2, 2
	add		$2, $2, $fp
	lwc1	$f2, 36($2) //f[i]
	lwc1	$f0, 32($fp) //d
	muls	$f2, $f2, $f0
	lwc1	$f0, 4($gp)  //0.5
	adds	$f0, $f2, $f0 
	swc1	$f0, 36($2)
	lw		$2, 24($fp)
	addi	$2, $2, 1
	sw		$2, 24($fp)
$L2:
	lw		$2, 24($fp)
	slti	$2, $2, 64
	bne		$2, $0, $L3
	move	$sp, $fp
	lw		$31, 300($sp)
	lw		$fp, 296($sp)
	addi	$sp, $sp, 304
	jr		$31