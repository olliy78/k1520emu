iget module
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
_iget	L
G
_iput	L
G
_iupdat	L
G
_itrunc	L
G
_maknode	L
G
_wdir	L
G	_p_iget	PR
E
_iget:
	call	csv
	jr	L1136
L1128:
	setb	@r13,#4
	push	@r15,#%000A
	push	@r15,r13
	call	_sleep
	inc	r15,#4
L1136:
	xor	r11,r11
	ld	r13,#L907A
	jr	L11CC
L113E:
	ld	r0,%0006(r14)
	cp	r0,%0004(r13)
	jr	ne,L11B2
	ld	r0,%0004(r14)
	cp	r0,%0002(r13)
	jr	ne,L11B2
	ldb	rl2,@r13
	xorb	rh2,rh2
	and	r2,#%0001
	jr	ne,L1128
	ldb	rl2,@r13
	xorb	rh2,rh2
	and	r2,#%0008
	jr	eq,L1194
	ld	r12,#LA07E
	jr	L1184
L116C:
	ld	r0,%0004(r12)
	cp	r0,r13
	jr	ne,L1182
	ld	r2,@r12
	ld	%0004(r14),r2
	ld	%0006(r14),#%0002
	jr	L1136
L1182:
	inc	r12,#6
L1184:
	cp	r12,#LA09C
	jr	ult,L116C
	push	@r15,#L8A1C
	call	_panic
	inc	r15,#2
L1194:
	ldb	rl2,%0001(r13)
	xorb	rh2,rh2
	incb	%0001(r13),#1
	testb	rl2
	jr	ne,L11AA
	xor	r0,r0
	ld	r1,r0
	ldl	%0046(r13),rr0
L11AA:
	setb	@r13,#0
L11AC:
	ld	r2,r13
L11AE:
	jp	cret
L11B2:
	testb	%0001(r13)
	jr	ne,L11C8
	test	r11
	jr	eq,L11C6
	ldl	rr0,%0046(r11)
	cpl	rr0,%0046(r13)
	jr	le,L11C8
L11C6:
	ld	r11,r13
L11C8:
	add	r13,#%004A
L11CC:
	cp	r13,#L9EEE
	jr	ult,L113E
	ld	r13,r11
	test	r13
	jr	ne,L11EC
	push	@r15,#L8A23
	call	_printf
	inc	r15,#2
	ldb	%FC13,#%17
L11E8:
	xor	r2,r2
	jr	L11AE
L11EC:
	ld	r2,%0004(r14)
	ld	%0002(r13),r2
	ld	r2,%0006(r14)
	ld	%0004(r13),r2
	ldb	@r13,#%01
	incb	%0001(r13),#1
	xor	r0,r0
	ld	r1,r0
	ldl	%0046(r13),rr0
	ld	r2,%0006(r14)
	inc	r2,#15
	srl	r2,#3
	push	@r15,r2
	push	@r15,r0
	push	@r15,%0004(r14)
	call	_bread
	inc	r15,#6
	ld	r10,r2
	ld	r2,@r10
	and	r2,#%0004
	jr	eq,L123E
	push	@r15,r10
	call	_brelse
	ld	@r15,r13
	call	_iput
	inc	r15,#2
	jr	L11E8
L123E:
	ld	r9,%000E(r10)
	ld	r2,r9
	ld	r3,%0006(r14)
	inc	r3,#15
	and	r3,#%0007
	sla	r3,#6
	add	r2,r3
	ld	r9,r2
	push	@r15,r9
	push	@r15,r13
	call	_iexpand
	inc	r15,#2
	ld	@r15,r10
	call	_brelse
	inc	r15,#2
	jr	L11AC
_iexpand:
	call	csv
	ld	r13,%0004(r14)
	ld	r12,%0006(r14)
	dec	r15,#4
	ld	r2,@r12
	ld	%0006(r13),r2
	ld	r2,%0002(r12)
	ld	%0008(r13),r2
	ld	r2,%0004(r12)
	ld	%000A(r13),r2
	ld	r2,%0006(r12)
	ld	%000C(r13),r2
	ldl	rr2,%0008(r12)
	ldl	%000E(r13),rr2
	ld	r11,r13
	add	r11,#%0012
	ld	r2,r12
	inc	r2,#12
	ld	%FFF2(r14),r2
	clr	%FFF0(r14)
	jr	L12F2
L12B2:
	ld	r2,r11
	inc	r11,#1
	clrb	@r2
	ld	r2,r11
	inc	r11,#1
	ld	r3,%FFF2(r14)
	inc	%FFF2(r14),#1
	ldb	rl4,@r3
	xorb	rh4,rh4
	ldb	@r2,rl4
	ld	r2,r11
	inc	r11,#1
	ld	r3,%FFF2(r14)
	inc	%FFF2(r14),#1
	ldb	rl4,@r3
	xorb	rh4,rh4
	ldb	@r2,rl4
	ld	r2,r11
	inc	r11,#1
	ld	r3,%FFF2(r14)
	inc	%FFF2(r14),#1
	ldb	rl4,@r3
	xorb	rh4,rh4
	ldb	@r2,rl4
	inc	%FFF0(r14),#1
L12F2:
	cp	%FFF0(r14),#%000D
	jr	lt,L12B2
	jp	cret
_iput:
	call	csv
	ld	r13,%0004(r14)
	ldb	rl2,%0001(r13)
	xorb	rh2,rh2
	cp	r2,#%0001
	jr	ne,L1362
	setb	@r13,#0
	test	%0008(r13)
	jr	eq,L131C
	jr	pl,L1340
L131C:
	push	@r15,r13
	call	_itrunc
	inc	r15,#2
	clr	%0006(r13)
	ldb	rl2,@r13
	xorb	rh2,rh2
	or	r2,#%0042
	ldb	@r13,rl2
	push	@r15,%0004(r13)
	push	@r15,%0002(r13)
	call	_ifree
	inc	r15,#4
L1340:
	push	@r15,#L8DCC
	push	@r15,#L8DCC
	push	@r15,r13
	call	_iupdat
	inc	r15,#4
	ld	@r15,r13
	call	_prele
	inc	r15,#2
	clrb	@r13
	ldl	rr2,L8DCC
	ldl	%0046(r13),rr2
L1362:
	decb	%0001(r13),#1
	push	@r15,r13
	call	_prele
	inc	r15,#2
	jp	cret
_iupdat:
	call	csv
	ld	r13,%0004(r14)
	dec	r15,#6
	ldb	rl2,@r13
	xorb	rh2,rh2
	and	r2,#%0046
	jp	eq,L14F2
	push	@r15,%0002(r13)
	call	_getfs
	inc	r15,#2
	add	r2,#%019D
	ldb	rl3,@r2
	xorb	rh3,rh3
	testb	rl3
	jp	ne,L14F2
	ld	r2,%0004(r13)
	inc	r2,#15
	srl	r2,#3
	push	@r15,r2
	push	@r15,#%0000
	push	@r15,%0002(r13)
	call	_bread
	inc	r15,#6
	ld	r12,r2
	bit	@r12,#2
	jr	eq,L13CA
	push	@r15,r12
	call	_brelse
	jp	L14F0
L13CA:
	ld	r2,%000E(r12)
	ld	%FFF2(r14),r2
	ld	r3,%0004(r13)
	inc	r3,#15
	and	r3,#%0007
	sla	r3,#6
	add	r2,r3
	ld	%FFF2(r14),r2
	ld	r3,%0006(r13)
	ld	@r2,r3
	ld	r2,%FFF2(r14)
	inc	r2,#2
	ld	r3,%0008(r13)
	ld	@r2,r3
	ld	r2,%FFF2(r14)
	inc	r2,#4
	ld	r3,%000A(r13)
	ld	@r2,r3
	ld	r2,%FFF2(r14)
	inc	r2,#6
	ld	r3,%000C(r13)
	ld	@r2,r3
	ld	r2,%FFF2(r14)
	inc	r2,#8
	ldl	rr4,%000E(r13)
	ldl	@r2,rr4
	ld	r11,%FFF2(r14)
	inc	r11,#12
	ld	r2,r13
	add	r2,#%0012
	ld	%FFF0(r14),r2
	clr	%FFEE(r14)
	jr	L149E
L1432:
	ld	r2,%FFF0(r14)
	inc	%FFF0(r14),#1
	testb	@r2
	jr	eq,L1464
	ld	r2,%0006(r13)
	and	r2,#%F000
	cp	r2,#%3000
	jr	eq,L1464
	ld	r2,%0006(r13)
	and	r2,#%F000
	cp	r2,#%7000
	jr	eq,L1464
	push	@r15,#L8A39
	call	_printf
	inc	r15,#2
L1464:
	ld	r2,r11
	inc	r11,#1
	ld	r3,%FFF0(r14)
	inc	%FFF0(r14),#1
	ldb	rl4,@r3
	xorb	rh4,rh4
	ldb	@r2,rl4
	ld	r2,r11
	inc	r11,#1
	ld	r3,%FFF0(r14)
	inc	%FFF0(r14),#1
	ldb	rl4,@r3
	xorb	rh4,rh4
	ldb	@r2,rl4
	ld	r2,r11
	inc	r11,#1
	ld	r3,%FFF0(r14)
	inc	%FFF0(r14),#1
	ldb	rl4,@r3
	xorb	rh4,rh4
	ldb	@r2,rl4
	inc	%FFEE(r14),#1
L149E:
	cp	%FFEE(r14),#%000D
	jr	lt,L1432
	bitb	@r13,#2
	jr	eq,L14BA
	ld	r2,%FFF2(r14)
	add	r2,#%0034
	ld	r3,%0006(r14)
	ldl	rr4,@r3
	ldl	@r2,rr4
L14BA:
	bitb	@r13,#1
	jr	eq,L14CE
	ld	r2,%FFF2(r14)
	add	r2,#%0038
	ld	r3,%0008(r14)
	ldl	rr4,@r3
	ldl	@r2,rr4
L14CE:
	bitb	@r13,#6
	jr	eq,L14E0
	ld	r2,%FFF2(r14)
	add	r2,#%003C
	ldl	rr4,L8DCC
	ldl	@r2,rr4
L14E0:
	ldb	rl2,@r13
	xorb	rh2,rh2
	and	r2,#%FFB9
	ldb	@r13,rl2
	push	@r15,r12
	call	_bdwrite
L14F0:
	inc	r15,#2
L14F2:
	jp	cret
_itrunc:
	call	csv
	ld	r13,%0004(r14)
	dec	r15,#6
	ld	r12,%0006(r13)
	and	r12,#%F000
	cp	r12,#%8000
	jr	eq,L1514
	cp	r12,#%4000
	jr	ne,L15AE
L1514:
	ld	r2,%0002(r13)
	ld	%FFF2(r14),r2
	ldk	r12,#12
	jr	L1598
L1520:
	ld	r2,r12
	sla	r2,#2
	add	r2,r13
	add	r2,#%0012
	ldl	rr4,@r2
	ldl	%FFEE(r14),rr4
	testl	%FFEE(r14)
	jr	eq,L1596
	ld	r2,r12
	sla	r2,#2
	add	r2,r13
	add	r2,#%0012
	xor	r0,r0
	ld	r1,r0
	ldl	@r2,rr0
	ld	r2,r12
	sub	r2,#%000A
	cp	r2,#%0002
	jr	ugt,L155E
	add	r2,r2
	ld	r1,L8A16(r2)
	jp	@r1
L155E:
	pushl	@r15,%FFEE(r14)
	push	@r15,%FFF2(r14)
	call	_free
	inc	r15,#6
	jr	L1596
L156E:
	push	@r15,#%0000
	push	@r15,#%0000
L1576:
	pushl	@r15,%FFEE(r14)
	push	@r15,%FFF2(r14)
	call	_tloop
	inc	r15,#10
	jr	L1596
	push	@r15,#%0000
L158A:
	push	@r15,#%0001
	jr	L1576
	push	@r15,#%0001
	jr	L158A
L1596:
	dec	r12,#1
L1598:
	test	r12
	jr	pl,L1520
	xor	r0,r0
	ld	r1,r0
	ldl	%000E(r13),rr0
	ldb	rl2,@r13
	xorb	rh2,rh2
	or	r2,#%0042
	ldb	@r13,rl2
L15AE:
	jp	cret
_tloop:
	call	csv
	dec	r15,#4
	xor	r12,r12
	ld	r13,#%007F
	jr	L1630
L15C0:
	test	r12
	jr	ne,L15E6
	pushl	@r15,%0006(r14)
	push	@r15,%0004(r14)
	call	_bread
	inc	r15,#6
	ld	r12,r2
	bit	@r12,#2
	jr	eq,L15E2
	push	@r15,r12
	call	_brelse
	inc	r15,#2
	jr	L164E
L15E2:
	ld	r11,%000E(r12)
L15E6:
	ld	r2,r13
	sla	r2,#2
	add	r2,r11
	ldl	rr4,@r2
	ldl	%FFF0(r14),rr4
	testl	%FFF0(r14)
	jr	eq,L162E
	test	%000A(r14)
	jr	eq,L1620
	push	@r15,r12
	call	_brelse
	inc	r15,#2
	xor	r12,r12
	push	@r15,r12
	push	@r15,%000C(r14)
	pushl	@r15,%FFF0(r14)
	push	@r15,%0004(r14)
	call	_tloop
	inc	r15,#10
	jr	L162E
L1620:
	pushl	@r15,%FFF0(r14)
	push	@r15,%0004(r14)
	call	_free
	inc	r15,#6
L162E:
	dec	r13,#1
L1630:
	test	r13
	jr	pl,L15C0
	test	r12
	jr	eq,L1640
	push	@r15,r12
	call	_brelse
	inc	r15,#2
L1640:
	pushl	@r15,%0006(r14)
	push	@r15,%0004(r14)
	call	_free
	inc	r15,#6
L164E:
	jp	cret
_maknode:
	call	csv
	ld	r2,%FC52
	inc	r2,#2
	push	@r15,@r2
	call	_ialloc
	inc	r15,#2
	ld	r13,r2
	test	r13
	jr	ne,L167A
	push	@r15,%FC52
	call	_iput
	inc	r15,#2
	xor	r2,r2
L1676:
	jp	cret
L167A:
	ldb	rl2,@r13
	xorb	rh2,rh2
	or	r2,#%0046
	ldb	@r13,rl2
	ld	r2,%0004(r14)
	and	r2,#%F000
	jr	ne,L1692
	set	%0004(r14),#15
L1692:
	ld	r2,%FD56
	com	r2
	and	r2,%0004(r14)
	ld	%0006(r13),r2
	ld	%0008(r13),#%0001
	ld	r2,%FC16
	ld	%000A(r13),r2
	ld	r2,%FC18
	ld	%000C(r13),r2
	push	@r15,r13
	call	_wdir
	inc	r15,#2
	ld	r2,r13
	jr	L1676
_wdir:
	call	csv
	ld	r2,%FC52
	inc	r2,#8
	test	@r2
	jr	eq,L16D2
	jr	pl,L16DA
L16D2:
	ldb	%FC13,#%14
	jr	L1716
L16DA:
	ld	r2,%0004(r14)
	inc	r2,#4
	ld	r3,@r2
	ld	%FC42,r3
	push	@r15,#%000E
	push	@r15,#%FC44
	push	@r15,#%FC32
	call	_bcopy
	inc	r15,#6
	ld	%FC28,#%0010
	ldb	%FC12,#%01
	ld	r2,#%FC42
	ld	%FC26,r2
	push	@r15,%FC52
	call	_writei
	inc	r15,#2
L1716:
	push	@r15,%FC52
	call	_iput
	inc	r15,#2
	jp	cret
end _p_iget
end iget
