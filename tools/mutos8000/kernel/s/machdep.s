machdep module
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
_startup	L
G
L35AC	L
G
_sendsig	L
G
L3630	L
G	_p_machdep	PR
E
_startup:
	call	csv
	call	_ttyinit
	ld	r13,#%FCFF
	srl	r13,#8
	inc	r13,#4
	ld	r12,#%0055
	jr	L355A
L350E:
	ld	r2,r13
	and	r2,#%7F00
	ld	L8DC0,r2
	push	@r15,r12
	ld	r2,r13
	sla	r2,#8
	push	@r15,r2
	call	_subyte
	inc	r15,#4
	ld	r2,r13
	sla	r2,#8
	push	@r15,r2
	call	_fubyte
	inc	r15,#2
	ld	r3,r12
	cp	r2,r12
	jr	ne,L3560
	push	@r15,r13
	call	L05C6
	inc	r15,#2
	inc	L8DDC,#1
	push	@r15,r13
	push	@r15,#%0001
	push	@r15,#LA09C
	call	_mfree
	inc	r15,#6
	inc	r13,#1
L355A:
	cp	r13,#%0400
	jr	ult,L350E
L3560:
	ld	r3,L8DDC
	exts	rr2
	slal	rr2,#8
	pushl	@r15,rr2
	push	@r15,#L8B4E
	call	_printf
	inc	r15,#6
	clr	L8DC0
	cp	L8DDC,#%0100
	jr	le,L3588
	ld	L8DDC,#%0100
L3588:
	push	@r15,#%0001
	push	@r15,_nswap
	push	@r15,#LA0DC
	call	_mfree
	inc	r15,#6
	ldl	rr2,_swplo
	subl	rr2,#%00000001
	ldl	_swplo,rr2
	jp	cret
L35AC:
	ret
_sendsig:
	call	csv
	ld	r2,%FD04
	inc	r2,#16
	ld	r13,@r2
	dec	r13,#8
	push	@r15,r13
	call	L0E38
	inc	r15,#2
	ld	r2,%FD04
	add	r2,#%0018
	push	@r15,@r2
	ld	r2,r13
	inc	r2,#6
	push	@r15,r2
	call	_suword
	inc	r15,#4
	ld	r2,%FD04
	add	r2,#%0016
	push	@r15,@r2
	ld	r2,r13
	inc	r2,#4
	push	@r15,r2
	call	_suword
	inc	r15,#4
	ld	r2,%FD04
	add	r2,#%0014
	push	@r15,@r2
	ld	r2,r13
	inc	r2,#2
	push	@r15,r2
	call	_suword
	inc	r15,#4
	ld	r2,%FD04
	dec	r2,#2
	push	@r15,@r2
	push	@r15,r13
	call	_suword
	inc	r15,#4
	ld	r2,%FD04
	inc	r2,#16
	ld	@r2,r13
	ld	r2,%FD04
	add	r2,#%0018
	ld	r3,%0004(r14)
	ld	@r2,r3
	jp	cret
L3630:
	call	csv
	ld	r2,%FD04
	inc	r2,#16
	ld	r13,@r2
	ld	r2,%FD04
	add	r2,#%0018
	push	@r15,r2
	ld	r2,r13
	inc	r2,#6
	push	@r15,r2
	call	_fuword
	inc	r15,#2
	ld	r3,r2
	pop	r2,@r15
	ld	@r2,r3
	ld	r2,%FD04
	add	r2,#%0016
	push	@r15,r2
	ld	r2,r13
	inc	r2,#4
	push	@r15,r2
	call	_fuword
	inc	r15,#2
	ld	r3,r2
	pop	r2,@r15
	ld	@r2,r3
	ld	r2,%FD04
	add	r2,#%0014
	push	@r15,r2
	ld	r2,r13
	inc	r2,#2
	push	@r15,r2
	call	_fuword
	inc	r15,#2
	ld	r3,r2
	pop	r2,@r15
	ld	@r2,r3
	ld	r2,%FD04
	inc	r2,#16
	ld	r3,r13
	inc	r3,#8
	ld	@r2,r3
	jp	cret
end _p_machdep
end machdep
