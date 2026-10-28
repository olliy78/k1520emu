text module
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
_saccess	L
G
_xswap	L
G
_xfree	L
G
L5790	L
G
L57A4	L
G
L57AC	L
G
_xumount	L
G
_xrele	L
G	_p_text	PR
E
_saccess:
	call	csv
	ld	r10,%FC20
	ld	r13,%FC16
	ld	r12,%FC18
	ld	r2,%FC1A
	ld	%FC16,r2
	ld	r2,%FC1C
	ld	%FC18,r2
	push	@r15,#%0000
	push	@r15,#_uchar
	call	_namei
	inc	r15,#4
	ld	r11,r2
	test	r11
	jr	eq,L5620
	bit	%0002(r10),#2
	jr	eq,L55F4
	push	@r15,#%0100
	push	@r15,r11
	call	_access
	inc	r15,#4
L55F4:
	bit	%0002(r10),#1
	jr	eq,L5606
	push	@r15,#%0080
	push	@r15,r11
	call	_access
	inc	r15,#4
L5606:
	bit	%0002(r10),#0
	jr	eq,L5618
	push	@r15,#%0040
	push	@r15,r11
	call	_access
	inc	r15,#4
L5618:
	push	@r15,r11
	call	_iput
	inc	r15,#2
L5620:
	ld	%FC16,r13
	ld	%FC18,r12
	jp	cret
_xswap:
	call	csv
	ld	r13,%0004(r14)
	ld	r12,%0008(r14)
	test	r12
	jr	ne,L5640
	ld	r12,%0012(r13)
L5640:
	ld	r10,%0010(r13)
	sla	r10,#2
	ld	r9,r10
	add	r9,#%EC00
	ld	r10,%00AA(r9)
	inc	r10,#1
	srl	r10,#1
	inc	r10,#2
	ld	r11,%0104(r9)
	add	r11,#%0400
	add	r11,r9
	ld	r11,%0010(r11)
	ld	r2,r10
	ld	r3,#%0100
	ld	r4,r11
	srl	r4,#8
	sub	r3,r4
	inc	r3,#1
	srl	r3,#1
	add	r2,r3
	ld	r10,r2
	push	@r15,r10
	push	@r15,#LA0DC
	call	_malloc
	inc	r15,#4
	ld	r11,r2
	test	r11
	jr	ne,L569C
	push	@r15,#L8C64
	call	_panic
	inc	r15,#2
L569C:
	setb	%0001(r13),#2
	push	@r15,%0016(r13)
	call	L57B4
	ld	@r15,r9
	push	@r15,#%0000
	push	@r15,#%0104
	push	@r15,%0010(r13)
	push	@r15,r11
	call	_swap
	inc	r15,#10
	test	%0006(r14)
	jr	eq,L56D4
	push	@r15,%0010(r13)
	push	@r15,r12
	push	@r15,#LA09C
	call	_mfree
	inc	r15,#6
L56D4:
	ld	%0010(r13),r11
	ldb	rl2,%0001(r13)
	xorb	rh2,rh2
	and	r2,#%FFFA
	ldb	%0001(r13),rl2
	clrb	%0003(r13)
	testb	L8DD8
	jr	eq,L56FE
	clrb	L8DD8
	push	@r15,#L8DD8
	call	_wakeup
	inc	r15,#2
L56FE:
	jp	cret
_xfree:
	call	csv
	ld	r2,%FC1E
	add	r2,#%0016
	ld	r13,@r2
	test	r13
	jr	eq,L578C
	push	@r15,r13
	call	L57A4
	inc	r15,#2
	resb	%000A(r13),#3
	ld	r2,%FC1E
	add	r2,#%0016
	clr	@r2
	ld	r12,%0006(r13)
	decb	%0008(r13),#1
	testb	%0008(r13)
	jr	ne,L5784
	ld	r2,%0006(r12)
	and	r2,#%0200
	jr	ne,L5784
	clr	%0006(r13)
	push	@r15,@r13
	ld	r2,%0004(r13)
	inc	r2,#1
	sra	r2,#1
	push	@r15,r2
	push	@r15,#LA0DC
	call	_mfree
	inc	r15,#6
	push	@r15,%0002(r13)
	push	@r15,%0004(r13)
	push	@r15,#LA09C
	call	_mfree
	inc	r15,#6
	resb	@r12,#5
	bitb	@r12,#0
	jr	eq,L577C
	decb	%0001(r12),#1
	jr	L578C
L577C:
	push	@r15,r12
	call	_iput
	jr	L578A
L5784:
	push	@r15,r13
	call	L57B4
L578A:
	inc	r15,#2
L578C:
	jp	cret
L5790:
	call	csv
	ld	r13,%0004(r14)
	jp	cret
L579C:
	call	csv
	jp	cret
L57A4:
	call	csv
	jp	cret
L57AC:
	call	csv
	jp	cret
L57B4:
	call	csv
	jp	cret
_xumount:
	call	csv
	jp	cret
_xrele:
	call	csv
	jp	cret
L57CC:
	call	csv
	jp	cret
end _p_text
end text
