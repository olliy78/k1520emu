pipe module
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
_uchar	L
G
_pipe	L
G
_openp	L
G
_closep	L
G
_plock	L
G
_prele	L
G
_getc	L
G
_putc	L
G	_p_pipe	PR
E
_uchar:
	call	csv
	ld	r2,%FC40
	inc	%FC40,#1
	push	@r15,r2
	call	_fubyte
	inc	r15,#2
	ld	r13,r2
	cp	r13,#%FFFF
	jr	ne,L39DC
	ldb	%FC13,#%0E
L39DC:
	ld	r2,r13
	jp	cret
_pipe:
	call	csv
	dec	r15,#2
	push	@r15,_pipedev
	call	_ialloc
	inc	r15,#2
	ld	r13,r2
	test	r13
	jr	eq,L3A64
	call	_falloc
	ld	r12,r2
	test	r12
	jr	ne,L3A20
	jr	L3A16
L3A04:
	clrb	%0001(r12)
	ld	r2,%FFF2(r14)
	sla	r2,#1
	add	r2,#%FC62
	clr	@r2
L3A16:
	push	@r15,r13
	call	_iput
	inc	r15,#2
	jr	L3A64
L3A20:
	ld	r2,%FC22
	ld	%FFF2(r14),r2
	call	_falloc
	ld	r11,r2
	test	r11
	jr	eq,L3A04
	ld	r2,%FC22
	ld	%FC24,r2
	ld	r2,%FFF2(r14)
	ld	%FC22,r2
	ldb	@r11,#%06
	ld	%0002(r11),r13
	ldb	@r12,#%05
	ld	%0002(r12),r13
	ldb	%0001(r13),#%02
	ld	r2,#%8000
	ld	%0006(r13),r2
	ldb	@r13,#%46
L3A64:
	jp	cret
_openp:
	call	csv
	ld	r13,%0004(r14)
	ld	r12,%0002(r13)
	jr	L3A9E
L3A76:
	push	@r15,r12
	call	_prele
	inc	r15,#2
	ldb	rl2,%0001(r12)
	xorb	rh2,rh2
	cp	r2,#%0002
	jr	ult,L3AFA
	set	%0006(r12),#8
	push	@r15,#%001A
	ld	r2,r12
	inc	r2,#2
	push	@r15,r2
	call	_sleep
	inc	r15,#4
L3A9E:
	push	@r15,r12
	call	_plock
	inc	r15,#2
	testl	%000E(r12)
	jr	eq,L3A76
	ldl	rr2,%0004(r13)
	ldl	%FC2A,rr2
	push	@r15,r12
	call	_readi
	inc	r15,#2
	ldl	rr2,%FC2A
	ldl	%0004(r13),rr2
	ldl	rr0,%0004(r13)
	cpl	rr0,%000E(r12)
	jr	ne,L3AF2
	xor	r0,r0
	ld	r1,r0
	ldl	%0004(r13),rr0
	ld	r1,r0
	ldl	%000E(r12),rr0
	bit	%0006(r12),#7
	jr	eq,L3AF2
	res	%0006(r12),#7
	ld	r2,r12
	inc	r2,#1
	push	@r15,r2
	call	_wakeup
	inc	r15,#2
L3AF2:
	push	@r15,r12
	call	_prele
	inc	r15,#2
L3AFA:
	jp	cret
_closep:
	call	csv
	ld	r13,%0004(r14)
	ld	r11,%0002(r13)
	ld	r12,%FC28
L3B0E:
	push	@r15,r11
	call	_plock
	inc	r15,#2
	test	r12
	jr	ne,L3B48
	push	@r15,r11
	call	_prele
	inc	r15,#2
	clr	%FC28
	jr	L3B44
L3B28:
	push	@r15,r11
	call	_prele
	inc	r15,#2
	ldb	%FC13,#%20
	push	@r15,#%000D
	push	@r15,%FC1E
	call	_psignal
	inc	r15,#4
L3B44:
	jp	cret
L3B48:
	ldb	rl2,%0001(r11)
	xorb	rh2,rh2
	cp	r2,#%0002
	jr	ult,L3B28
	ldl	rr2,%000E(r11)
	xor	r0,r0
	ld	r1,#%1000
	cpl	rr2,rr0
	jr	lt,L3B7E
	set	%0006(r11),#7
	push	@r15,r11
	call	_prele
	ld	@r15,#%001A
	ld	r2,r11
	inc	r2,#1
	push	@r15,r2
	call	_sleep
	inc	r15,#4
	jr	L3B0E
L3B7E:
	ldl	rr2,%000E(r11)
	ldl	%FC2A,rr2
	push	@r15,#%1000
	push	@r15,r12
	call	_min
	inc	r15,#4
	ld	%FC28,r2
	sub	r12,r2
	push	@r15,r11
	call	_writei
	ld	@r15,r11
	call	_prele
	inc	r15,#2
	bit	%0006(r11),#8
	jr	eq,L3B0E
	res	%0006(r11),#8
	ld	r2,r11
	inc	r2,#2
	push	@r15,r2
	call	_wakeup
	inc	r15,#2
	jr	L3B0E
_plock:
	call	csv
	ld	r13,%0004(r14)
	jr	L3BD6
L3BC8:
	setb	@r13,#4
	push	@r15,#%000A
	push	@r15,r13
	call	_sleep
	inc	r15,#4
L3BD6:
	bitb	@r13,#0
	jr	ne,L3BC8
	setb	@r13,#0
	jp	cret
_prele:
	call	csv
	ld	r13,%0004(r14)
	resb	@r13,#0
	bitb	@r13,#4
	jr	eq,L3BF8
	resb	@r13,#4
	push	@r15,r13
	call	_wakeup
	inc	r15,#2
L3BF8:
	jp	cret
_getc:
	call	csv
	ld	r13,%0004(r14)
	call	_spl6
	ld	r10,r2
	test	@r13
	jr	eq,L3C10
	jr	pl,L3C22
L3C10:
	ld	r11,#%FFFF
	clr	@r13
	xor	r2,r2
	ld	%0004(r13),r2
	ld	%0002(r13),r2
	jr	L3C72
L3C22:
	ld	r2,%0002(r13)
	inc	%0002(r13),#1
	ldb	rl3,@r2
	xorb	rh3,rh3
	ld	r11,r3
	and	r11,#%00FF
	dec	@r13,#1
	test	@r13
	jr	eq,L3C3C
	jr	pl,L3C50
L3C3C:
	ld	r12,%0002(r13)
	dec	r12,#1
	and	r12,#%FFF0
	clr	%0002(r13)
	clr	%0004(r13)
	jr	L3C68
L3C50:
	ld	r2,%0002(r13)
	and	r2,#%000F
	jr	ne,L3C72
	ld	r12,%0002(r13)
	dec	r12,#16
	ld	r2,@r12
	inc	r2,#2
	ld	%0002(r13),r2
L3C68:
	ld	r2,%D266
	ld	@r12,r2
	ld	%D266,r12
L3C72:
	push	@r15,r10
	call	_splx
	inc	r15,#2
	ld	r2,r11
	jp	cret
_putc:
	call	csv
	ld	r13,%0006(r14)
	call	_spl6
	ld	r10,r2
	ld	r11,%0004(r13)
	test	r11
	jr	eq,L3C9A
	test	@r13
	jr	pl,L3CC4
L3C9A:
	ld	r12,%D266
	test	r12
	jr	ne,L3CB2
L3CA2:
	push	@r15,r10
	call	_splx
	inc	r15,#2
	ld	r2,#%FFFF
L3CAE:
	jp	cret
L3CB2:
	ld	r2,@r12
	ld	%D266,r2
	clr	@r12
	ld	r11,r12
	inc	r11,#2
	ld	%0002(r13),r11
	jr	L3CE8
L3CC4:
	ld	r2,r11
	and	r2,#%000F
	jr	ne,L3CE8
	ld	r12,r11
	dec	r12,#16
	ld	r2,%D266
	ld	@r12,r2
	test	r2
	jr	eq,L3CA2
	ld	r12,r2
	ld	r2,@r12
	ld	%D266,r2
	clr	@r12
	ld	r11,r12
	inc	r11,#2
L3CE8:
	ld	r2,r11
	inc	r11,#1
	ld	r3,%0004(r14)
	ldb	@r2,rl3
	inc	@r13,#1
	ld	%0004(r13),r11
	push	@r15,r10
	call	_splx
	inc	r15,#2
	xor	r2,r2
	jr	L3CAE
end _p_pipe
end pipe
