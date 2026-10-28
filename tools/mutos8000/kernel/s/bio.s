bio module
$NONSEGMENTED
constant
	FP	:= r14
	SP	:= r15
	X	:= external
	G	:= global
	I	:= internal
	A	:= array
	PR	:= procedure
	E	:= entry
	B	:= byte
	W	:= word
	BV	:= bval
	WV	:= wval
	LV	:= lval
	L	:= label
G
_bread	L
G
_breada	L
G
_bwrite	L
G
_bdwrite	L
G
_brelse	L
G
_getblk	L
G
_geteblk	L
G
_iowait	L
G
_iodone	L
G
_clrbuf	L
G
L5CAE	L
G
_swap	L
G
_bflush	L
G
_physio	L
G
L60A8	L
G	_p_bio	PR
E
_bread:
	call	csv
	pushl	@r15,%0006(r14)
	push	@r15,%0004(r14)
	call	_getblk
	inc	r15,#6
	ld	r13,r2
	bit	@r13,#1
	jr	eq,L57F2
L57EC:
	ld	r2,r13
	jp	cret
L57F2:
	set	@r13,#0
	ld	%000C(r13),#%0200
	push	@r15,r13
	ld	r2,%0004(r14)
	srl	r2,#8
	sla	r2,#3
	add	r2,#L876A
	ld	r3,@r2
	call	@r3
	ld	@r15,r13
	call	_iowait
	inc	r15,#2
	jr	L57EC
_breada:
	call	csv
	xor	r13,r13
	pushl	@r15,%0006(r14)
	push	@r15,%0004(r14)
	call	_incore
	inc	r15,#6
	test	r2
	jr	ne,L586A
	pushl	@r15,%0006(r14)
	push	@r15,%0004(r14)
	call	_getblk
	inc	r15,#6
	ld	r13,r2
	ld	r2,@r13
	and	r2,#%0002
	jr	ne,L586A
	set	@r13,#0
	ld	%000C(r13),#%0200
	push	@r15,r13
	ld	r2,%0004(r14)
	srl	r2,#8
	sla	r2,#3
	add	r2,#L876A
	ld	r3,@r2
	call	@r3
	inc	r15,#2
L586A:
	testl	%000A(r14)
	jr	eq,L58CE
	pushl	@r15,%000A(r14)
	push	@r15,%0004(r14)
	call	_incore
	inc	r15,#6
	test	r2
	jr	ne,L58CE
	ld	r0,LA51E
	cp	r0,#LA518
	jr	eq,L58CE
	pushl	@r15,%000A(r14)
	push	@r15,%0004(r14)
	call	_getblk
	inc	r15,#6
	ld	r12,r2
	bit	@r12,#1
	jr	eq,L58A8
	push	@r15,r12
	call	_brelse
	jr	L58CC
L58A8:
	ld	r2,@r12
	or	r2,#%0101
	ld	@r12,r2
	ld	%000C(r12),#%0200
	push	@r15,r12
	ld	r2,%0004(r14)
	srl	r2,#8
	sla	r2,#3
	add	r2,#L876A
	ld	r3,@r2
	call	@r3
L58CC:
	inc	r15,#2
L58CE:
	test	r13
	jr	ne,L58E4
	pushl	@r15,%0006(r14)
	push	@r15,%0004(r14)
	call	_bread
	inc	r15,#6
L58E0:
	jp	cret
L58E4:
	push	@r15,r13
	call	_iowait
	inc	r15,#2
	ld	r2,r13
	jr	L58E0
_bwrite:
	call	csv
	ld	r13,%0004(r14)
	ld	r12,@r13
	ld	r2,r12
	and	r2,#%FD78
	ld	@r13,r2
	ld	%000C(r13),#%0200
	push	@r15,r13
	ld	r2,%000A(r13)
	srl	r2,#8
	sla	r2,#3
	add	r2,#L876A
	ld	r3,@r2
	call	@r3
	inc	r15,#2
	ld	r2,r12
	and	r2,#%0100
	jr	ne,L5936
	push	@r15,r13
	call	_iowait
	ld	@r15,r13
	call	_brelse
	jr	L5944
L5936:
	bit	r12,#9
	jr	eq,L593E
	set	@r13,#7
	jr	L5946
L593E:
	push	@r15,r13
	call	L60A8
L5944:
	inc	r15,#2
L5946:
	jp	cret
_bdwrite:
	call	csv
	ld	r13,%0004(r14)
	ld	r2,%000A(r13)
	srl	r2,#8
	sla	r2,#3
	add	r2,#L876C
	ld	r12,@r2
	bit	@r12,#10
	jr	eq,L5970
	push	@r15,r13
	call	_bawrite
	jr	L597E
L5970:
	ld	r2,@r13
	or	r2,#%0202
	ld	@r13,r2
	push	@r15,r13
	call	_brelse
L597E:
	inc	r15,#2
	jp	cret
_bawrite:
	call	csv
	ld	r13,%0004(r14)
	set	@r13,#8
	push	@r15,r13
	call	_bwrite
	inc	r15,#2
	jp	cret
_brelse:
	call	csv
	ld	r13,%0004(r14)
	bit	@r13,#6
	jr	eq,L59AE
	push	@r15,r13
	call	_wakeup
	inc	r15,#2
L59AE:
	bit	LA518,#6
	jr	eq,L59C2
	res	LA518,#6
	push	@r15,#LA518
	call	_wakeup
	inc	r15,#2
L59C2:
	bit	@r13,#2
	jr	eq,L59CE
	ld	r2,#%FFFF
	ld	%000A(r13),r2
L59CE:
	call	_spl6
	ld	r11,r2
	bit	@r13,#7
	jr	eq,L59F4
	ld	r12,#LA51E
	ld	r2,@r12
	inc	r2,#8
	ld	@r2,r13
	ld	r2,@r12
	ld	%0006(r13),r2
	ld	@r12,r13
	ld	r2,#LA518
	ld	%0008(r13),r2
	jr	L5A0E
L59F4:
	ld	r12,#LA520
	ld	r2,@r12
	inc	r2,#6
	ld	@r2,r13
	ld	r2,@r12
	ld	%0008(r13),r2
	ld	@r12,r13
	ld	r2,#LA518
	ld	%0006(r13),r2
L5A0E:
	ld	r2,@r13
	and	r2,#%FE37
	ld	@r13,r2
	push	@r15,r11
	call	_splx
	inc	r15,#2
	jp	cret
_incore:
	call	csv
	ld	r2,%0004(r14)
	srl	r2,#8
	sla	r2,#3
	add	r2,#L876C
	ld	r12,@r2
	ld	r13,%0002(r12)
	jr	L5A5C
L5A3E:
	ldl	rr0,%0010(r13)
	cpl	rr0,%0006(r14)
	jr	ne,L5A58
	ld	r0,%000A(r13)
	cp	r0,%0004(r14)
	jr	ne,L5A58
	ldk	r2,#1
L5A54:
	jp	cret
L5A58:
	ld	r13,%0002(r13)
L5A5C:
	cp	r13,r12
	jr	ne,L5A3E
	xor	r2,r2
	jr	L5A54
_getblk:
	call	csv
	ld	r2,%0004(r14)
	srl	r2,#8
	cp	r2,L8DD0
	jr	lt,L5A80
	push	@r15,#L8C76
	call	_panic
L5A7E:
	inc	r15,#2
L5A80:
	call	_spl0
	ld	r2,%0004(r14)
	srl	r2,#8
	sla	r2,#3
	add	r2,#L876C
	ld	r12,@r2
	test	r12
	jr	ne,L5AA4
	push	@r15,#L8C7D
	call	_panic
	inc	r15,#2
L5AA4:
	ld	r13,%0002(r12)
	jr	L5AEC
L5AAA:
	ldl	rr0,%0010(r13)
	cpl	rr0,%0006(r14)
	jr	ne,L5AE8
	ld	r0,%000A(r13)
	cp	r0,%0004(r14)
	jr	ne,L5AE8
	call	_spl6
	bit	@r13,#3
	jr	eq,L5AD6
	set	@r13,#6
	push	@r15,#%0015
	push	@r15,r13
L5ACE:
	call	_sleep
	inc	r15,#4
	jr	L5A80
L5AD6:
	call	_spl0
	push	@r15,r13
	call	L5C2E
	inc	r15,#2
L5AE2:
	ld	r2,r13
	jp	cret
L5AE8:
	ld	r13,%0002(r13)
L5AEC:
	cp	r13,r12
	jr	ne,L5AAA
	call	_spl6
	ld	r0,LA51E
	cp	r0,#LA518
	jr	ne,L5B0C
	set	LA518,#6
	push	@r15,#%0015
	push	@r15,#LA518
	jr	L5ACE
L5B0C:
	call	_spl0
	ld	r13,LA51E
	push	@r15,r13
	call	L5C2E
	inc	r15,#2
	bit	@r13,#9
	jr	eq,L5B2A
	set	@r13,#8
	push	@r15,r13
	call	_bwrite
	jr	L5A7E
L5B2A:
	ld	@r13,#%0008
	ld	r2,%0004(r13)
	inc	r2,#2
	ld	r3,%0002(r13)
	ld	@r2,r3
	ld	r2,%0002(r13)
	inc	r2,#4
	ld	r3,%0004(r13)
	ld	@r2,r3
	ld	r2,%0002(r12)
	ld	%0002(r13),r2
	ld	%0004(r13),r12
	ld	r2,%0002(r12)
	inc	r2,#4
	ld	@r2,r13
	ld	%0002(r12),r13
	ld	r2,%0004(r14)
	ld	%000A(r13),r2
	ldl	rr2,%0006(r14)
	ldl	%0010(r13),rr2
	jr	L5AE2
_geteblk:
	call	csv
L5B74:
	call	_spl6
	jr	L5B8C
L5B7A:
	set	LA518,#6
	push	@r15,#%0015
	push	@r15,#LA518
	call	_sleep
	inc	r15,#4
L5B8C:
	ld	r0,LA51E
	cp	r0,#LA518
	jr	eq,L5B7A
	call	_spl0
	ld	r12,#LA518
	ld	r13,LA51E
	push	@r15,r13
	call	L5C2E
	inc	r15,#2
	bit	@r13,#9
	jr	eq,L5BBA
	set	@r13,#8
	push	@r15,r13
	call	_bwrite
	inc	r15,#2
	jr	L5B74
L5BBA:
	ld	@r13,#%0008
	ld	r2,%0004(r13)
	inc	r2,#2
	ld	r3,%0002(r13)
	ld	@r2,r3
	ld	r2,%0002(r13)
	inc	r2,#4
	ld	r3,%0004(r13)
	ld	@r2,r3
	ld	r2,%0002(r12)
	ld	%0002(r13),r2
	ld	%0004(r13),r12
	ld	r2,%0002(r12)
	inc	r2,#4
	ld	@r2,r13
	ld	%0002(r12),r13
	ld	r2,#%FFFF
	ld	%000A(r13),r2
	ld	r2,r13
	jp	cret
_iowait:
	call	csv
	ld	r13,%0004(r14)
	call	_spl6
	jr	L5C16
L5C0A:
	push	@r15,#%0014
	push	@r15,r13
	call	_sleep
	inc	r15,#4
L5C16:
	ld	r2,@r13
	and	r2,#%0002
	jr	eq,L5C0A
	call	_spl0
	push	@r15,r13
	call	L60A8
	inc	r15,#2
	jp	cret
L5C2E:
	call	csv
	ld	r13,%0004(r14)
	call	_spl6
	ld	r12,r2
	ld	r2,%0008(r13)
	inc	r2,#6
	ld	r3,%0006(r13)
	ld	@r2,r3
	ld	r2,%0006(r13)
	inc	r2,#8
	ld	r3,%0008(r13)
	ld	@r2,r3
	set	@r13,#3
	push	@r15,r12
	call	_splx
	inc	r15,#2
	jp	cret
_iodone:
	call	csv
	ld	r13,%0004(r14)
	set	@r13,#1
	bit	@r13,#8
	jr	eq,L5C78
	push	@r15,r13
	call	_brelse
	jr	L5C80
L5C78:
	res	@r13,#6
	push	@r15,r13
	call	_wakeup
L5C80:
	inc	r15,#2
	jp	cret
_clrbuf:
	call	csv
	ld	r2,%0004(r14)
	inc	r2,#14
	ld	r13,@r2
	ld	r12,#%0100
L5C96:
	ld	r2,r13
	inc	r13,#2
	clr	@r2
	dec	r12,#1
	jr	ne,L5C96
	ld	r2,%0004(r14)
	add	r2,#%0016
	clr	@r2
	jp	cret
L5CAE:
	call	csv
	ld	r13,%0004(r14)
	ld	r12,%0006(r14)
	jr	L5CC6
L5CBC:
	testb	%D584
	jr	ne,L5CC6
	call	L5CF6
L5CC6:
	ld	r2,r12
	sra	r2,#1
	sla	r2,#1
	add	r2,#%D57C
	test	@r2
	jr	ne,L5CBC
	ld	r2,r12
	sra	r2,#1
	sla	r2,#1
	add	r2,#%D57C
	ld	@r2,r13
	testb	%D584
	jr	ne,L5CF2
	call	L5CF6
L5CF2:
	jp	cret
L5CF6:
	call	csv
	incb	%D584,#1
L5CFE:
	xor	r13,r13
	xor	r12,r12
	jr	L5D38
L5D04:
	ld	r2,r12
	sla	r2,#1
	add	r2,#%D57C
	test	@r2
	jr	eq,L5D36
	ld	r2,r12
	sla	r2,#1
	add	r2,#%D57C
	ld	r13,@r2
L5D1E:
	ldk	r2,#6
	test	@r2
	jr	ne,L5D1E
	ld	@r2,r13
	call	L065E
	ld	r2,r12
	sla	r2,#1
	add	r2,#%D57C
	clr	@r2
L5D36:
	inc	r12,#1
L5D38:
	cp	r12,#%0004
	jr	lt,L5D04
	test	r13
	jr	ne,L5CFE
	clrb	%D584
	jp	cret
_swap:
	call	csv
	ld	r13,%0004(r14)
	ld	r12,%0008(r14)
	ld	r11,%000C(r14)
	cp	r12,#%0104
	jr	lt,L5E16
	push	@r15,%000A(r14)
	push	@r15,#%0004
	ld	r2,r11
	add	r2,#%00FF
	srl	r2,#8
	push	@r15,r2
	push	@r15,r13
	call	_swap
	inc	r15,#8
	inc	r13,#2
	ldk	r8,#2
	ld	r9,%00A8(r11)
	add	r9,%00AA(r11)
	push	@r15,%000A(r14)
	push	@r15,r9
	push	@r15,%0006(r14)
	push	@r15,r13
	call	_swap
	inc	r15,#8
	inc	r9,#1
	sra	r9,#1
	add	r8,r9
	add	r13,r9
	ld	r9,%0104(r11)
	add	r9,#%0400
	add	r9,r11
	ld	r9,%0010(r9)
	srl	r9,#8
	res	r9,#0
	ld	r2,%0006(r14)
	add	r2,r9
	ld	%0006(r14),r2
	ld	r2,#%0100
	sub	r2,r9
	ld	r9,r2
	push	@r15,%000A(r14)
	push	@r15,r9
	push	@r15,%0006(r14)
	push	@r15,r13
	call	_swap
	inc	r15,#8
	ld	r2,r8
	ld	r3,r9
	inc	r3,#1
	sra	r3,#1
	add	r2,r3
	ld	r8,r2
	ld	r2,r13
	ld	r3,r9
	inc	r3,#1
	sra	r3,#1
	add	r2,r3
	ld	r13,r2
	cp	%000A(r14),#%0001
	jp	ne,L5F0C
	ld	r2,r13
	sub	r2,r8
	push	@r15,r2
	push	@r15,r8
	push	@r15,#LA0DC
	call	_mfree
	inc	r15,#6
	jr	L5F0C
L5E16:
	ld	r10,#LCE28
	bit	@r10,#3
	jr	eq,L5E2C
	ld	r2,LCE40
	and	r2,#%0040
	jr	ne,L5E2C
	ld	r10,#LCE40
L5E2C:
	call	_spl6
	jr	L5E40
L5E32:
	set	@r10,#6
	push	@r15,#%0001
	push	@r15,r10
	call	_sleep
	inc	r15,#4
L5E40:
	bit	@r10,#3
	jr	ne,L5E32
	jr	L5EE2
L5E46:
	ld	r2,%000A(r14)
	or	r2,#%0018
	ld	@r10,r2
	ld	r2,_swapdev
	ld	%000A(r10),r2
	ld	r9,r12
	cp	r9,#%0010
	jr	le,L5E64
	ld	r9,#%0010
L5E64:
	ld	r2,r9
	sla	r2,#8
	ld	%000C(r10),r2
	ld	r3,r13
	exts	rr2
	addl	rr2,_swplo
	ldl	%0010(r10),rr2
	ld	r2,%0006(r14)
	sla	r2,#8
	ld	%000E(r10),r2
	ld	r2,%0006(r14)
	sra	r2,#8
	and	r2,#%007F
	ldb	%0014(r10),rl2
	push	@r15,r10
	ld	r2,_swapdev
	srl	r2,#8
	sla	r2,#3
	add	r2,#L876A
	ld	r3,@r2
	call	@r3
	inc	r15,#2
	call	_spl6
	jr	L5EC0
L5EB4:
	push	@r15,#%0000
	push	@r15,r10
	call	_sleep
	inc	r15,#4
L5EC0:
	ld	r2,@r10
	and	r2,#%0002
	jr	eq,L5EB4
	sub	r12,r9
	ld	r2,%0006(r14)
	add	r2,r9
	ld	%0006(r14),r2
	ld	r2,r13
	ld	r3,r9
	inc	r3,#1
	sra	r3,#1
	add	r2,r3
	ld	r13,r2
L5EE2:
	test	r12
	jr	ne,L5E46
	bit	@r10,#6
	jr	eq,L5EF2
	push	@r15,r10
	call	_wakeup
	inc	r15,#2
L5EF2:
	call	_spl0
	ld	r2,@r10
	and	r2,#%FFB7
	ld	@r10,r2
	bit	@r10,#2
	jr	eq,L5F0C
	push	@r15,#L8C84
	call	_panic
	inc	r15,#2
L5F0C:
	jp	cret
_bflush:
	call	csv
	jr	L5F36
L5F16:
	ld	r0,%0004(r14)
	cp	r0,#%FFFF
	jr	eq,L5F26
	cp	r0,%000A(r13)
	jr	ne,L5F44
L5F26:
	set	@r13,#8
	push	@r15,r13
	call	L5C2E
	ld	@r15,r13
	call	_bwrite
	inc	r15,#2
L5F36:
	call	_spl6
	ld	r13,LA51E
	jr	L5F48
L5F40:
	bit	@r13,#9
	jr	ne,L5F16
L5F44:
	ld	r13,%0006(r13)
L5F48:
	cp	r13,#LA518
	jr	ne,L5F40
	call	_spl0
	jp	cret
_physio:
	call	csv
	ld	r13,%0006(r14)
	dec	r15,#2
	ld	r12,%FC26
	bit	r12,#0
	jp	ne,L609E
	ld	r2,%FC28
	and	r2,#%01FF
	jp	ne,L609E
	test	%FC28
	jp	eq,L609E
	ld	r2,r12
	add	r2,%FC28
	cp	r2,r12
	jp	ule,L609E
	ld	r2,%FCA8
	ld	%FFF2(r14),r2
	testb	%FD13
	jr	eq,L5F9C
	clr	%FFF2(r14)
L5F9C:
	ld	r11,r12
	srl	r11,#8
	and	r11,#%00FF
	cp	r11,%FFF2(r14)
	jr	lt,L609E
	ld	r2,r12
	add	r2,%FC28
	srl	r2,#8
	and	r2,#%00FF
	ld	r3,%FFF2(r14)
	add	r3,%FCAA
	cp	r2,r3
	jr	ult,L5FD4
	ld	r2,%FD04
	inc	r2,#16
	ld	r0,@r2
	cp	r0,%FC26
	jr	ugt,L609E
L5FD4:
	ld	r2,%FC28
	add	r2,#%00FF
	srl	r2,#8
	cp	r2,#%0010
	jr	ugt,L609E
	call	_spl6
	jr	L5FFA
L5FEC:
	set	@r13,#6
	push	@r15,#%0015
	push	@r15,r13
	call	_sleep
	inc	r15,#4
L5FFA:
	bit	@r13,#3
	jr	ne,L5FEC
	ld	r2,%000A(r14)
	or	r2,#%0018
	ld	@r13,r2
	ld	r2,%0008(r14)
	ld	%000A(r13),r2
	ld	%000E(r13),r12
	ld	r2,L8DC0
	sra	r2,#8
	and	r2,#%00FF
	ldb	%0014(r13),rl2
	ldl	rr2,%FC2A
	ld	r4,#%FFF7
	sdal	rr2,r4
	ldl	%0010(r13),rr2
	ld	r2,%FC28
	ld	%000C(r13),r2
	clrb	%0015(r13)
	ld	r2,%FC1E
	inc	r2,#1
	setb	@r2,#2
	push	@r15,r13
	ld	r2,%0004(r14)
	call	@r2
	inc	r15,#2
	call	_spl6
	jr	L6064
L6058:
	push	@r15,#%0014
	push	@r15,r13
	call	_sleep
	inc	r15,#4
L6064:
	ld	r2,@r13
	and	r2,#%0002
	jr	eq,L6058
	ld	r2,%FC1E
	inc	r2,#1
	resb	@r2,#2
	bit	@r13,#6
	jr	eq,L6080
	push	@r15,r13
	call	_wakeup
	inc	r15,#2
L6080:
	call	_spl0
	ld	r2,@r13
	and	r2,#%FFB7
	ld	@r13,r2
	ld	r2,%0016(r13)
	ld	%FC28,r2
	push	@r15,r13
	call	L60A8
	inc	r15,#2
	jr	L60A4
L609E:
	ldb	%FC13,#%0E
L60A4:
	jp	cret
L60A8:
	call	csv
	ld	r13,%0004(r14)
	bit	@r13,#2
	jr	eq,L60C8
	ldb	rl2,%0015(r13)
	xorb	rh2,rh2
	ldb	%FC13,rl2
	testb	rl2
	jr	ne,L60C8
	ldb	%FC13,#%05
L60C8:
	jp	cret
end _p_bio
end bio
