rk module
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
_mmread	L
G
_mmwrite	L
G
_rkopen	L
G
_rkclose	L
G
L6B06	L
G
_rkintr	L
G
_rkread	L
G
_rkwrite	L
G
_syopen	L
G
_syread	L
G
_sywrite	L
G
_syioctl	L
G	_p_rk	PR
E
_mmread:
	call	csv
	dec	r15,#4
	ld	r2,%0004(r14)
	and	r2,#%00FF
	cp	r2,#%0002
	jr	eq,L6A4E
L69E2:
	ldl	rr2,%FC2A
	ld	r4,#%FFF0
	sdal	rr2,r4
	ld	r12,r3
	ld	r11,%FC2C
	ld	r2,L8DC0
	ld	%FFF2(r14),r2
	call	_spl7
	ld	r2,r12
	sla	r2,#8
	ld	L8DC0,r2
	ld	r2,%0004(r14)
	and	r2,#%00FF
	cp	r2,#%0001
	jr	ne,L6A1C
	clr	L8DC0
L6A1C:
	push	@r15,r11
	call	_fubyte
	inc	r15,#2
	ld	r13,r2
	test	r13
	jr	pl,L6A30
	ldb	%FC13,#%06
L6A30:
	ld	r2,%FFF2(r14)
	ld	L8DC0,r2
	call	_spl0
	testb	%FC13
	jr	ne,L6A4E
	push	@r15,r13
	call	_passc
	inc	r15,#2
	test	r2
	jr	pl,L69E2
L6A4E:
	jp	cret
_mmwrite:
	call	csv
	dec	r15,#4
	ld	r2,%0004(r14)
	and	r2,#%00FF
	cp	r2,#%0002
	jr	ne,L6A6E
	clr	%FC28
L6A6A:
	jp	cret
L6A6E:
	ldl	rr2,%FC2A
	ld	r4,#%FFF0
	sdal	rr2,r4
	ld	r12,r3
	ld	r11,%FC2C
	call	_cpass
	ld	r13,r2
	test	r13
	jr	mi,L6A6A
	testb	%FC13
	jr	ne,L6A6A
	ld	r2,L8DC0
	ld	%FFF2(r14),r2
	call	_spl7
	ld	r2,r12
	sla	r2,#8
	ld	L8DC0,r2
	ld	r2,%0004(r14)
	and	r2,#%00FF
	cp	r2,#%0001
	jr	ne,L6AB8
	clr	L8DC0
L6AB8:
	push	@r15,r13
	push	@r15,r11
	call	_subyte
	inc	r15,#4
	test	r2
	jr	pl,L6ACC
	ldb	%FC13,#%06
L6ACC:
	ld	r2,%FFF2(r14)
	ld	L8DC0,r2
	call	_spl0
	jr	L6A6E
_rkopen:
	call	csv
	dec	r15,#4
	ld	r2,%0004(r14)
	and	r2,#%00FF
	ld	%FFF2(r14),r2
	cp	%FFF2(r14),#%0004
	jr	lt,L6AFA
	ldb	%FC13,#%06
L6AFA:
	jp	cret
_rkclose:
	call	csv
	jp	cret
L6B06:
	call	csv
	ld	r13,%0004(r14)
	ldl	rr2,%0010(r13)
	xor	r0,r0
	ld	r1,#%0260
	cpl	rr2,rr0
	jr	lt,L6B2E
	set	@r13,#2
	ldb	%0015(r13),#%06
	push	@r15,r13
	call	_iodone
	inc	r15,#2
	jr	L6B5C
L6B2E:
	clr	%0006(r13)
	call	_spl5
	test	L8E88
	jr	ne,L6B42
	ld	L8E88,r13
	jr	L6B4A
L6B42:
	ld	r2,L8E8A
	inc	r2,#6
	ld	@r2,r13
L6B4A:
	ld	L8E8A,r13
	test	L8E8E
	jr	ne,L6B58
	call	_rkstart
L6B58:
	call	_spl0
L6B5C:
	jp	cret
_rkstart:
	call	csv
	dec	r15,#10
	ld	r12,L8E88
	test	r12
	jr	eq,L6BC6
	ld	r13,#%D532
	inc	L8E8E,#1
	clrb	@r13
	ld	r2,%000A(r12)
	and	r2,#%00FF
	ldb	%0001(r13),rl2
	clrb	%0003(r13)
	ldb	rl2,%0014(r12)
	xorb	rh2,rh2
	ld	%0004(r13),r2
	ld	r2,%000E(r12)
	ld	%0006(r13),r2
	ld	r2,%000C(r12)
	ld	%0008(r13),r2
	ldl	rr2,%0010(r12)
	ldl	%000A(r13),rr2
	bit	@r12,#0
	jr	eq,L6BB4
	clrb	%0002(r13)
	jr	L6BBA
L6BB4:
	ldb	%0002(r13),#%01
L6BBA:
	push	@r15,#%0000
	push	@r15,r13
	call	L5CAE
	inc	r15,#4
L6BC6:
	jp	cret
_rkintr:
	call	csv
	test	L8E8E
	jr	eq,L6C1A
	ld	r10,#%D532
	ld	r11,L8E88
	clr	L8E8E
	ldb	rl2,%0003(r10)
	xorb	rh2,rh2
	cp	r2,#%0001
	jr	eq,L6BFE
	ldb	rl2,%0003(r10)
	cp	r2,#%00C8
	jr	ne,L6C1E
	ldb	%0015(r11),#%06
L6BFC:
	set	@r11,#2
L6BFE:
	clr	L8E98
	ld	r2,%0006(r11)
	ld	L8E88,r2
	clr	%0016(r11)
	push	@r15,r11
	call	_iodone
	inc	r15,#2
L6C16:
	call	_rkstart
L6C1A:
	jp	cret
L6C1E:
	ldb	rl2,%0002(r10)
	xorb	rh2,rh2
	ld	r13,r2
	ldb	rl2,%0003(r10)
	ld	r12,r2
	ld	r2,r12
	and	r2,#%00FF
	push	@r15,r2
	ld	r2,r13
	and	r2,#%00FF
	push	@r15,r2
	push	@r15,r11
	call	L18A4
	inc	r15,#6
	inc	L8E98,#1
	cp	L8E98,#%0003
	jr	ugt,L6BFC
	jr	L6C16
_rkread:
	call	csv
	push	@r15,#%0001
	push	@r15,%0004(r14)
	push	@r15,#LCE94
	push	@r15,#L6B06
	call	_physio
	inc	r15,#8
	jp	cret
_rkwrite:
	call	csv
	push	@r15,#%0000
	push	@r15,%0004(r14)
	push	@r15,#LCE94
	push	@r15,#L6B06
	call	_physio
	inc	r15,#8
	jp	cret
_syopen:
	call	csv
	test	%FD14
	jr	ne,L6CA0
	ldb	%FC13,#%06
	jr	L6CC2
L6CA0:
	push	@r15,%0006(r14)
	push	@r15,%FD16
	ld	r2,%FD16
	srl	r2,#8
	ld	r3,r2
	mult	rr2,#%000E
	ld	r2,r3
	add	r2,#_cdevsw
	ld	r3,@r2
	call	@r3
	inc	r15,#4
L6CC2:
	jp	cret
_syread:
	call	csv
	push	@r15,%FD16
	ld	r2,%FD16
	srl	r2,#8
	ld	r3,r2
	mult	rr2,#%000E
	ld	r2,r3
	add	r2,#L879A
	ld	r3,@r2
	call	@r3
	inc	r15,#2
	jp	cret
_sywrite:
	call	csv
	push	@r15,%FD16
	ld	r2,%FD16
	srl	r2,#8
	ld	r3,r2
	mult	rr2,#%000E
	ld	r2,r3
	add	r2,#L879C
	ld	r3,@r2
	call	@r3
	inc	r15,#2
	jp	cret
_syioctl:
	call	csv
	push	@r15,%000A(r14)
	push	@r15,%0008(r14)
	push	@r15,%0006(r14)
	push	@r15,%FD16
	ld	r2,%FD16
	srl	r2,#8
	ld	r3,r2
	mult	rr2,#%000E
	ld	r2,r3
	add	r2,#L879E
	ld	r3,@r2
	call	@r3
	inc	r15,#8
	jp	cret
end _p_rk
end rk
