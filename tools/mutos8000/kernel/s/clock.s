clock module
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
_clock	L
G
_timeout	L
G	_p_clock	PR
E
_clock:
	call	csv
	dec	r15,#2
	call	L040C
	ld	r9,%0006(r14)
	srl	r9,#8
	test	LCE5C
	jr	eq,L28D0
	ld	r10,r9
	ld	r12,#LCE58
	jr	L286A
L2862:
	test	%0004(r12)
	jr	eq,L2870
	inc	r12,#6
L286A:
	test	@r12
	jr	eq,L2862
	jr	mi,L2862
L2870:
	dec	@r12,#1
	dec	r10,#1
	jr	eq,L2878
	jr	pl,L286A
L2878:
	ld	r2,%001A(r14)
	and	r2,#%0007
	jr	ne,L28D0
	call	_spl5
	test	LCE58
	jr	eq,L288E
	jr	pl,L28D0
L288E:
	ld	r13,#LCE58
	jr	L28A8
L2894:
	test	@r13
	jr	eq,L289A
	jr	pl,L28AE
L289A:
	push	@r15,%0002(r13)
	ld	r2,%0004(r13)
	call	@r2
	inc	r15,#2
	inc	r13,#6
L28A8:
	test	%0004(r13)
	jr	ne,L2894
L28AE:
	ld	r12,#LCE58
	jr	L28C4
L28B4:
	ld	r2,@r13
	ld	@r12,r2
	ld	r2,%0002(r13)
	ld	%0002(r12),r2
	inc	r13,#6
	inc	r12,#6
L28C4:
	ld	r2,%0004(r13)
	ld	%0004(r12),r2
	test	r2
	jr	ne,L28B4
L28D0:
	ld	r2,%001C(r14)
	and	r2,#%4000
	jr	ne,L2900
	ldl	rr2,%FCF4
	ld	r5,r9
	exts	rr4
	addl	rr2,rr4
	ldl	%FCF4,rr2
	test	%FD10
	jr	eq,L290E
	push	@r15,r9
	push	@r15,#%FD0A
	push	@r15,%0020(r14)
	call	_addupc
	inc	r15,#6
	jr	L290E
L2900:
	ldl	rr2,%FCF8
	ld	r5,r9
	exts	rr4
	addl	rr2,rr4
	ldl	%FCF8,rr2
L290E:
	ld	r11,%FC1E
	incb	%0004(r11),#1
	testb	%0004(r11)
	jr	ne,L2920
	decb	%0004(r11),#1
L2920:
	ld	r2,L8DCA
	add	r2,r9
	ld	L8DCA,r2
	cp	r2,#%0032
	jp	lt,L2A48
	ld	r2,%001A(r14)
	and	r2,#%0007
	jp	ne,L2A48
	ld	r2,L8DCA
	sub	r2,#%0032
	ld	L8DCA,r2
	ldl	rr2,L8DCC
	addl	rr2,#%00000001
	ldl	L8DCC,rr2
	call	_spl1
	inc	L8DE0,#1
	cp	L8DE0,#%001E
	jr	lt,L2986
	test	L888A
	jr	eq,L2986
	inc	L8DE2,#1
	testb	L8DD8
	jr	eq,L2986
	clrb	L8DD8
	push	@r15,#L8DD8
	call	_wakeup
	inc	r15,#2
L2986:
	incb	L8DC4,#1
	push	@r15,#L8DCA
	call	_wakeup
	inc	r15,#2
	ld	r11,#LA11C
	jr	L2A2E
L299A:
	testb	@r11
	jr	eq,L2A2A
	ldb	rl2,@r11
	xorb	rh2,rh2
	cp	r2,#%0005
	jr	uge,L2A2A
	ldb	rl2,%0003(r11)
	cp	r2,#%007F
	jr	eq,L29B6
	incb	%0003(r11),#1
L29B6:
	test	%001A(r11)
	jr	eq,L29D2
	dec	%001A(r11),#1
	test	%001A(r11)
	jr	ne,L29D2
	push	@r15,#%000E
	push	@r15,r11
	call	_psignal
	inc	r15,#4
L29D2:
	ldb	rl2,%0004(r11)
	xorb	rh2,rh2
	and	r2,#%00FF
	sla	r2,#3
	ld	r3,r2
	exts	rr2
	div	rr2,#%000A
	ld	r2,r3
	ldb	rl3,%0005(r11)
	xorb	rh3,rh3
	add	r2,r3
	add	r2,#%FFEC
	ld	%FFF2(r14),r2
	jr	pl,L2A00
	clr	%FFF2(r14)
L2A00:
	cp	%FFF2(r14),#%00FF
	jr	le,L2A0E
	ld	%FFF2(r14),#%00FF
L2A0E:
	ld	r2,%FFF2(r14)
	ldb	%0004(r11),rl2
	ldb	rl2,%0002(r11)
	xorb	rh2,rh2
	cp	r2,#%0032
	jr	ult,L2A2A
	push	@r15,r11
	call	_setpri
	inc	r15,#2
L2A2A:
	add	r11,#%001C
L2A2E:
	cp	r11,L8A8C
	jr	ule,L299A
	testb	L8DD6
	jr	eq,L2A48
	clrb	L8DD6
	push	@r15,#L8DD6
	call	_wakeup
	inc	r15,#2
L2A48:
	jp	cret
_timeout:
	call	csv
	dec	r15,#2
	ld	r11,%0008(r14)
	ld	r13,#LCE58
	call	_spl7
	ld	%FFF2(r14),r2
	jr	L2A6E
L2A64:
	ld	r0,@r13
	cp	r0,r11
	jr	gt,L2A74
	sub	r11,r0
	inc	r13,#6
L2A6E:
	test	%0004(r13)
	jr	ne,L2A64
L2A74:
	cp	r13,#LCE8E
	jr	ult,L2A84
	push	@r15,#L8AC2
	call	_panic
	inc	r15,#2
L2A84:
	ld	r2,@r13
	sub	r2,r11
	ld	@r13,r2
	ld	r12,r13
	jr	L2A90
L2A8E:
	inc	r12,#6
L2A90:
	test	%0004(r12)
	jr	ne,L2A8E
	jr	L2AB0
L2A98:
	ld	r2,@r12
	ld	%0006(r12),r2
	ld	r2,%0004(r12)
	ld	%000A(r12),r2
	ld	r2,%0002(r12)
	ld	%0008(r12),r2
	dec	r12,#6
L2AB0:
	cp	r12,r13
	jr	uge,L2A98
	ld	@r13,r11
	ld	r2,%0004(r14)
	ld	%0004(r13),r2
	ld	r2,%0006(r14)
	ld	%0002(r13),r2
	push	@r15,%FFF2(r14)
	call	_splx
	inc	r15,#2
	jp	cret
end _p_clock
end clock
