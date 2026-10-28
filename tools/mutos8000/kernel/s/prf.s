prf module
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
_printf	L
G
_panic	L
G
_prdev	L
G
L18A4	L
G	_p_prf	PR
E
_printf:
	call	csv
	ld	r13,%0004(r14)
	dec	r15,#2
	ld	r11,r14
	inc	r11,#6
	jr	L1740
L1734:
	test	r12
	jr	eq,L17DC
	push	@r15,r12
	call	_putchar
	inc	r15,#2
L1740:
	ld	r2,r13
	inc	r13,#1
	ldb	rl3,@r2
	xorb	rh3,rh3
	ld	r12,r3
	cp	r12,#%0025
	jr	ne,L1734
	ld	r2,r13
	inc	r13,#1
	ldb	rl3,@r2
	xorb	rh3,rh3
	ld	r12,r3
	cp	r12,#%0064
	jr	eq,L1772
	cp	r12,#%0075
	jr	eq,L1772
	cp	r12,#%006F
	jr	eq,L1772
	cp	r12,#%0078
	jr	ne,L179C
L1772:
	cp	r12,#%006F
	jr	ne,L177E
	push	@r15,#%0008
	jr	L178E
L177E:
	cp	r12,#%0078
	jr	ne,L178A
	push	@r15,#%0010
	jr	L178E
L178A:
	push	@r15,#%000A
L178E:
	push	@r15,@r11
	push	@r15,#%0000
	call	_printn
	inc	r15,#6
	jr	L17D8
L179C:
	cp	r12,#%0073
	jr	ne,L17C4
	ld	r2,@r11
	ld	%FFF2(r14),r2
L17A8:
	ld	r2,%FFF2(r14)
	inc	%FFF2(r14),#1
	ldb	rl3,@r2
	xorb	rh3,rh3
	ld	r12,r3
	test	r3
	jr	eq,L17D8
	push	@r15,r12
	call	_putchar
	inc	r15,#2
	jr	L17A8
L17C4:
	cp	r12,#%0044
	jr	ne,L17D8
	push	@r15,#%000A
	pushl	@r15,@r11
	call	_printn
	inc	r15,#6
	inc	r11,#2
L17D8:
	inc	r11,#2
	jr	L1740
L17DC:
	jp	cret
_printn:
	call	csv
	dec	r15,#4
	testl	%0004(r14)
	jr	pl,L1808
	push	@r15,#%002D
	call	_putchar
	inc	r15,#2
	ldl	rr2,%0004(r14)
	com	r3
	com	r2
	addl	rr2,#%00000001
	ldl	%0004(r14),rr2
L1808:
	ldl	rr2,%0004(r14)
	ld	r5,%0008(r14)
	exts	rr4
	extsl	rq0
	divl	rq0,rr4
	ldl	%FFF0(r14),rr2
	testl	rr2
	jr	eq,L182C
	push	@r15,%0008(r14)
	pushl	@r15,%FFF0(r14)
	call	_printn
	inc	r15,#6
L182C:
	ld	r3,%0008(r14)
	exts	rr2
	pushl	@r15,rr2
	ldl	rr6,%0004(r14)
	extsl	rq4
	divl	rq4,@r15
	inc	r15,#4
	ldl	rr2,rr4
	ld	r2,r3
	add	r2,#L8A4A
	ldb	rl0,@r2
	xorb	rh0,rh0
	push	@r15,r0
	call	_putchar
	inc	r15,#2
	jp	cret
_panic:
	call	csv
	ld	r2,%0004(r14)
	ld	LCEC4,r2
	call	_update
	push	@r15,%0004(r14)
	push	@r15,#L8A5B
	call	_printf
	inc	r15,#4
L1874:
	call	_idle
	jr	L1874
_prdev:
	call	csv
	ld	r2,%0006(r14)
	and	r2,#%00FF
	push	@r15,r2
	ld	r2,%0006(r14)
	srl	r2,#8
	push	@r15,r2
	push	@r15,%0004(r14)
	push	@r15,#L8A66
	call	_printf
	inc	r15,#8
	jp	cret
L18A4:
	call	csv
	ld	r13,%0004(r14)
	push	@r15,%000A(r13)
	push	@r15,#L8A77
	call	_prdev
	inc	r15,#4
	push	@r15,%0008(r14)
	push	@r15,%0006(r14)
	pushl	@r15,%0010(r13)
	push	@r15,#L8A7B
	call	_printf
	inc	r15,#10
	jp	cret
end _p_prf
end prf
