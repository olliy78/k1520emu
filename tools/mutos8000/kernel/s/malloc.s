malloc module
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
_malloc	L
G
_mfree	L
G	_p_malloc	PR
E
_malloc:
	call	csv
	ld	r12,%0004(r14)
	jr	L2E52
L2DFA:
	ld	r3,r12
	sub	r3,%0004(r14)
	exts	rr2
	div	rr2,#%0004
	ld	r2,r3
	cp	r3,#%000F
	jr	ge,L2E56
	ld	r0,@r12
	cp	r0,%0006(r14)
	jr	lt,L2E50
	ld	r13,%0002(r12)
	ld	r2,%0002(r12)
	add	r2,%0006(r14)
	ld	%0002(r12),r2
	ld	r2,@r12
	sub	r2,%0006(r14)
	ld	@r12,r2
	test	r2
	jr	ne,L2E4A
L2E32:
	inc	r12,#4
	ld	r2,r12
	dec	r2,#2
	ld	r3,%0002(r12)
	ld	@r2,r3
	ld	r2,r12
	dec	r2,#4
	ld	r3,@r12
	ld	@r2,r3
	test	r3
	jr	ne,L2E32
L2E4A:
	ld	r2,r13
L2E4C:
	jp	cret
L2E50:
	inc	r12,#4
L2E52:
	test	@r12
	jr	ne,L2DFA
L2E56:
	xor	r2,r2
	jr	L2E4C
_mfree:
	call	csv
	ld	r13,%0008(r14)
	ld	r12,%0004(r14)
	cp	r12,#LA09C
	jr	ne,L2E88
	testb	L8DD6
	jr	eq,L2E88
	clrb	L8DD6
	push	@r15,#L8DD6
	call	_wakeup
	inc	r15,#2
	jr	L2E88
L2E82:
	test	@r12
	jr	eq,L2E90
	inc	r12,#4
L2E88:
	ld	r0,%0002(r12)
	cp	r0,r13
	jr	ule,L2E82
L2E90:
	cp	r12,%0004(r14)
	jr	ule,L2EDC
	ld	r2,%FFFE(r12)
	add	r2,%FFFC(r12)
	cp	r2,r13
	jr	ne,L2EDC
	ld	r2,r12
	dec	r2,#4
	ld	r3,@r2
	add	r3,%0006(r14)
	ld	@r2,r3
	ld	r2,r13
	add	r2,%0006(r14)
	cp	r2,%0002(r12)
	jr	ne,L2F44
	ld	r2,r12
	dec	r2,#4
	ld	r3,@r2
	add	r3,@r12
L2EC2:
	ld	@r2,r3
	test	@r12
	jr	eq,L2F44
	inc	r12,#4
	ld	r2,r12
	dec	r2,#2
	ld	r3,%0002(r12)
	ld	@r2,r3
	ld	r2,r12
	dec	r2,#4
	ld	r3,@r12
	jr	L2EC2
L2EDC:
	ld	r2,r13
	add	r2,%0006(r14)
	cp	r2,%0002(r12)
	jr	ne,L2F02
	test	@r12
	jr	eq,L2F02
	ld	r2,%0002(r12)
	sub	r2,%0006(r14)
	ld	%0002(r12),r2
	ld	r2,@r12
	add	r2,%0006(r14)
	ld	@r12,r2
	jr	L2F44
L2F02:
	test	%0006(r14)
	jr	eq,L2F44
L2F08:
	ld	r3,r12
	sub	r3,%0004(r14)
	exts	rr2
	div	rr2,#%0004
	ld	r2,r3
	cp	r3,#%000F
	jr	le,L2F26
	push	@r15,#L8AE4
	call	_panic
	inc	r15,#2
L2F26:
	ld	r11,%0002(r12)
	ld	%0002(r12),r13
	ld	r13,r11
	ld	r11,@r12
	ld	r2,%0006(r14)
	ld	@r12,r2
	inc	r12,#4
	ld	r2,r11
	ld	%0006(r14),r11
	test	r11
	jr	ne,L2F08
L2F44:
	jp	cret
end _p_malloc
end malloc
