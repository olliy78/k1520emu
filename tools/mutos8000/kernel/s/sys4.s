sys4 module
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
_gtime	L
G
_ftime	L
G
_stime	L
G
_setuid	L
G
_getuid	L
G
_setgid	L
G
_getgid	L
G
_getpid	L
G
_sync	L
G
_nice	L
G	_p_sys4	PR
E
_gtime:
	call	csv
	ldl	rr2,L8DCC
	ldl	%FC22,rr2
	jp	cret
_ftime:
	call	csv
	dec	r15,#10
	ld	r13,%FC20
	call	_spl7
	ldl	rr2,L8DCC
	ldl	%FFEA(r14),rr2
	ld	r12,L8DCA
	call	_spl0
	cp	r12,#%0032
	jr	ule,L4C5A
	sub	r12,#%0032
	ldl	rr2,%FFEA(r14)
	addl	rr2,#%00000001
	ldl	%FFEA(r14),rr2
L4C5A:
	ld	r3,r12
	mult	rr2,#%03E8
	exts	rr2
	div	rr2,#%0032
	ld	%FFEE(r14),r3
	clr	%FFF0(r14)
	clr	%FFF2(r14)
	push	@r15,#%000A
	push	@r15,@r13
	ld	r2,r14
	add	r2,#%FFEA
	push	@r15,r2
	call	_copyout
	inc	r15,#6
	test	r2
	jr	pl,L4C90
	ldb	%FC13,#%0E
L4C90:
	jp	cret
_stime:
	call	csv
	ld	r13,%FC20
	call	_suser
	test	r2
	jr	eq,L4CC2
	ld	r12,#L907A
	jr	L4CB6
L4CAA:
	xor	r0,r0
	ld	r1,r0
	ldl	%0046(r12),rr0
	add	r12,#%004A
L4CB6:
	cp	r12,#L9EEE
	jr	ult,L4CAA
	ldl	rr2,@r13
	ldl	L8DCC,rr2
L4CC2:
	jp	cret
_setuid:
	call	csv
	ld	r12,%FC20
	ld	r13,@r12
	ld	r0,%FC1A
	cp	r0,r13
	jr	eq,L4CE0
	call	_suser
	test	r2
	jr	eq,L4CF0
L4CE0:
	ld	%FC16,r13
	ld	r2,%FC1E
	inc	r2,#8
	ld	@r2,r13
	ld	%FC1A,r13
L4CF0:
	jp	cret
_getuid:
	call	csv
	ld	r2,%FC1A
	ld	%FC22,r2
	ld	r2,%FC16
	ld	%FC24,r2
	jp	cret
_setgid:
	call	csv
	ld	r12,%FC20
	ld	r13,@r12
	ld	r0,%FC1C
	cp	r0,r13
	jr	eq,L4D26
	call	_suser
	test	r2
	jr	eq,L4D2E
L4D26:
	ld	%FC18,r13
	ld	%FC1C,r13
L4D2E:
	jp	cret
_getgid:
	call	csv
	ld	r2,%FC1C
	ld	%FC22,r2
	ld	r2,%FC18
	ld	%FC24,r2
	jp	cret
_getpid:
	call	csv
	ld	r2,%FC1E
	inc	r2,#12
	ld	r3,@r2
	ld	%FC22,r3
	ld	r2,%FC1E
	inc	r2,#14
	ld	r3,@r2
	ld	%FC24,r3
	jp	cret
_sync:
	call	csv
	call	_update
	jp	cret
_nice:
	call	csv
	ld	r12,%FC20
	ld	r13,@r12
	test	r13
	jr	pl,L4D8E
	call	_suser
	test	r2
	jr	ne,L4D8E
	xor	r13,r13
L4D8E:
	ld	r2,%FC1E
	inc	r2,#5
	ld	r3,r13
	ldb	rl4,@r2
	xorb	rh4,rh4
	add	r3,r4
	ld	r13,r3
	cp	r13,#%0028
	jr	lt,L4DA8
	ld	r13,#%0027
L4DA8:
	test	r13
	jr	pl,L4DAE
	xor	r13,r13
L4DAE:
	ld	r2,%FC1E
	inc	r2,#5
	ld	r3,r13
	ldb	@r2,rl3
	jp	cret
end _p_sys4
end sys4
