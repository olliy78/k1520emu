subr module
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
_bmap	L
G
_passc	L
G
_cpass	L
G
_nodev	L
G
_nulldev	L
G
_bcopy	L
G	_p_subr	PR
E
_bmap:
	call	csv
	ld	r13,%0004(r14)
	dec	r15,#16
	testl	%0006(r14)
	jr	pl,L21AA
L210E:
	ldb	%FC13,#%1B
L2114:
	xor	r3,r3
	exts	rr2
	jr	L21A6
L211A:
	ld	r12,%0008(r14)
	ld	r2,r12
	sla	r2,#2
	add	r2,r13
	add	r2,#%0012
	ldl	rr4,@r2
	ldl	%FFE8(r14),rr4
	testl	%FFE8(r14)
	jr	ne,L218A
	cp	%000A(r14),#%0001
	jr	eq,L2150
	push	@r15,%FFE4(r14)
	call	_alloc
	inc	r15,#2
	ld	%FFF2(r14),r2
	test	r2
	jr	ne,L2158
L2150:
	ldl	rr2,#%FFFFFFFF
	jr	L21A6
L2158:
	ld	r2,%FFF2(r14)
	inc	r2,#16
	ldl	rr4,@r2
	ldl	%FFE8(r14),rr4
	push	@r15,%FFF2(r14)
	call	_bdwrite
	inc	r15,#2
	ld	r2,r12
	sla	r2,#2
	add	r2,r13
	add	r2,#%0012
	ldl	rr4,%FFE8(r14)
	ldl	@r2,rr4
	ldb	rl2,@r13
	xorb	rh2,rh2
	or	r2,#%0042
	ldb	@r13,rl2
L218A:
	cp	r12,#%0009
	jr	ge,L21A2
	ld	r2,r12
	sla	r2,#2
	add	r2,r13
	add	r2,#%0016
L219C:
	ldl	rr4,@r2
	ldl	L8DE4,rr4
L21A2:
	ldl	rr2,%FFE8(r14)
L21A6:
	jp	cret
L21AA:
	ld	r2,%0002(r13)
	ld	%FFE4(r14),r2
	xor	r0,r0
	ld	r1,r0
	ldl	L8DE4,rr0
	ldl	rr2,%0006(r14)
	ldk	r1,#10
	cpl	rr2,rr0
	jr	lt,L211A
	clr	%FFEC(r14)
	ldk	r3,#1
	exts	rr2
	ldl	%FFE8(r14),rr2
	ldl	rr2,%0006(r14)
	subl	rr2,#%0000000A
	ldl	%0006(r14),rr2
	ld	%FFEE(r14),#%0003
	jr	L220E
L21E6:
	jr	eq,L2214
	inc	%FFEC(r14),#7
	ldl	rr2,%FFE8(r14)
	slal	rr2,#7
	ldl	%FFE8(r14),rr2
	ldl	rr0,%0006(r14)
	cpl	rr0,rr2
	jr	lt,L2214
	ldl	rr2,rr0
	subl	rr2,%FFE8(r14)
	ldl	%0006(r14),rr2
	dec	%FFEE(r14),#1
L220E:
	test	%FFEE(r14)
	jr	pl,L21E6
L2214:
	test	%FFEE(r14)
	jp	eq,L210E
	ldk	r2,#13
	sub	r2,%FFEE(r14)
	sla	r2,#2
	add	r2,r13
	add	r2,#%0012
	ldl	rr4,@r2
	ldl	%FFE8(r14),rr4
	testl	%FFE8(r14)
	jp	ne,L234C
	cp	%000A(r14),#%0001
	jr	eq,L2150
	push	@r15,%FFE4(r14)
	call	_alloc
	inc	r15,#2
	ld	%FFF2(r14),r2
	test	r2
	jp	eq,L2150
	inc	r2,#16
	ldl	rr4,@r2
	ldl	%FFE8(r14),rr4
	push	@r15,%FFF2(r14)
	call	_bdwrite
	inc	r15,#2
	ldk	r2,#13
	sub	r2,%FFEE(r14)
	sla	r2,#2
	add	r2,r13
	add	r2,#%0012
	ldl	rr4,%FFE8(r14)
	ldl	@r2,rr4
	ldb	rl2,@r13
	xorb	rh2,rh2
	or	r2,#%0042
	ldb	@r13,rl2
	jr	L234C
L228A:
	pushl	@r15,%FFE8(r14)
	push	@r15,%FFE4(r14)
	call	_bread
	inc	r15,#6
	ld	%FFF2(r14),r2
	bit	@r2,#2
	jr	eq,L22AC
	push	@r15,r2
	call	_brelse
	inc	r15,#2
	jp	L2114
L22AC:
	ld	r2,%FFF2(r14)
	inc	r2,#14
	ld	r3,@r2
	ld	%FFE6(r14),r3
	dec	%FFEC(r14),#7
	ldl	rr2,%0006(r14)
	ld	r4,%FFEC(r14)
	neg	r4
	sdal	rr2,r4
	ld	r12,r3
	and	r12,#%007F
	ld	r2,r12
	sla	r2,#2
	add	r2,%FFE6(r14)
	ldl	rr4,@r2
	ldl	%FFE8(r14),rr4
	testl	%FFE8(r14)
	jr	ne,L233E
	cp	%000A(r14),#%0001
	jr	eq,L2300
	push	@r15,%FFE4(r14)
	call	_alloc
	inc	r15,#2
	ld	%FFF0(r14),r2
	test	r2
	jr	ne,L230E
L2300:
	push	@r15,%FFF2(r14)
	call	_brelse
	inc	r15,#2
	jp	L2150
L230E:
	ld	r2,%FFF0(r14)
	inc	r2,#16
	ldl	rr4,@r2
	ldl	%FFE8(r14),rr4
	push	@r15,%FFF0(r14)
	call	_bdwrite
	inc	r15,#2
	ld	r2,r12
	sla	r2,#2
	add	r2,%FFE6(r14)
	ldl	rr4,%FFE8(r14)
	ldl	@r2,rr4
	push	@r15,%FFF2(r14)
	call	_bdwrite
	jr	L2346
L233E:
	push	@r15,%FFF2(r14)
	call	_brelse
L2346:
	inc	r15,#2
	inc	%FFEE(r14),#1
L234C:
	cp	%FFEE(r14),#%0003
	jr	le,L228A
	cp	r12,#%007F
	jp	ge,L21A2
	ld	r2,r12
	sla	r2,#2
	add	r2,%FFE6(r14)
	inc	r2,#4
	jp	L219C
_passc:
	call	csv
	ld	r13,%0004(r14)
	ldb	rl2,%FC12
	xorb	rh2,rh2
	ld	r12,r2
	cp	r12,#%0001
	jr	ne,L23AE
	ld	r2,%FC26
	ld	r3,r13
	ldb	@r2,rl3
L238A:
	dec	%FC28,#1
	ldl	rr2,%FC2A
	addl	rr2,#%00000001
	ldl	%FC2A,rr2
	inc	%FC26,#1
	test	%FC28
	jr	ne,L23C6
L23A6:
	ld	r2,#%FFFF
L23AA:
	jp	cret
L23AE:
	push	@r15,r13
	push	@r15,%FC26
	call	_subyte
	inc	r15,#4
	test	r2
	jr	pl,L238A
	ldb	%FC13,#%0E
	jr	L23A6
L23C6:
	xor	r2,r2
	jr	L23AA
_cpass:
	call	csv
	test	%FC28
	jr	ne,L2404
L23D4:
	ld	r2,#%FFFF
	jr	L2400
L23DA:
	ld	r2,%FC26
	ldb	rl3,@r2
	xorb	rh3,rh3
	ld	r13,r3
L23E4:
	dec	%FC28,#1
	ldl	rr2,%FC2A
	addl	rr2,#%00000001
	ldl	%FC2A,rr2
	inc	%FC26,#1
	ld	r2,r13
	and	r2,#%00FF
L2400:
	jp	cret
L2404:
	ldb	rl2,%FC12
	xorb	rh2,rh2
	ld	r12,r2
	cp	r12,#%0001
	jr	eq,L23DA
	push	@r15,%FC26
	call	_fubyte
	inc	r15,#2
	ld	r13,r2
	test	r13
	jr	pl,L23E4
	ldb	%FC13,#%0E
	jr	L23D4
_nodev:
	call	csv
	ldb	%FC13,#%13
	jp	cret
_nulldev:
	ret
_bcopy:
	call	csv
	ld	r13,%0008(r14)
	ld	r12,%0004(r14)
	ld	r11,%0006(r14)
L244A:
	ld	r2,r11
	inc	r11,#1
	ld	r3,r12
	inc	r12,#1
	ldb	rl4,@r3
	xorb	rh4,rh4
	ldb	@r2,rl4
	dec	r13,#1
	jr	ne,L244A
	jp	cret
end _p_subr
end subr
