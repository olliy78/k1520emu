fio module
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
_getf	L
G
_closef	L
G
_openi	L
G
_access	L
G
_owner	L
G
_suser	L
G
_ufalloc	L
G
_falloc	L
G	_p_fio	PR
E
_getf:
	call	csv
	ld	r13,%0004(r14)
	test	r13
	jr	mi,L2AFC
	cp	r13,#%0014
	jr	ge,L2AFC
	ld	r2,r13
	sla	r2,#1
	add	r2,#%FC62
	ld	r12,@r2
	test	r12
	jr	eq,L2AFC
	ld	r2,r12
L2AF8:
	jp	cret
L2AFC:
	ldb	%FC13,#%09
	xor	r2,r2
	jr	L2AF8
_closef:
	call	csv
	ld	r13,%0004(r14)
	dec	r15,#8
	test	r13
	jr	eq,L2BDA
	ldb	rl2,%0001(r13)
	xorb	rh2,rh2
	cp	r2,#%0001
	jr	ule,L2B26
	decb	%0001(r13),#1
	jr	L2BDA
L2B26:
	ld	r12,%0002(r13)
	ldb	rl2,@r13
	xorb	rh2,rh2
	ld	%FFF2(r14),r2
	ld	r2,%0004(r13)
	ld	%FFEC(r14),r2
	ld	r2,%0014(r12)
	ld	%FFEE(r14),r2
	ld	r2,%0006(r12)
	ld	%FFF0(r14),r2
	push	@r15,r12
	call	_plock
	inc	r15,#2
	clrb	%0001(r13)
	bit	%FFF2(r14),#2
	jr	eq,L2B80
	ld	r2,%0006(r12)
	and	r2,#%FE7F
	ld	%0006(r12),r2
	ld	r2,r12
	inc	r2,#1
	push	@r15,r2
	call	_wakeup
	inc	r15,#2
	ld	r2,r12
	inc	r2,#2
	push	@r15,r2
	call	_wakeup
	inc	r15,#2
L2B80:
	push	@r15,r12
	call	_iput
	inc	r15,#2
	ld	r2,%FFF0(r14)
	and	r2,#%F000
	cp	r2,#%2000
	jr	eq,L2BAA
	cp	r2,#%3000
	jr	eq,L2BAA
	cp	r2,#%6000
	jr	eq,L2BFA
	cp	r2,#%7000
	jr	eq,L2BFA
	jr	L2BDA
L2BAA:
	ld	r2,%FFEE(r14)
	srl	r2,#8
	ld	r3,r2
	mult	rr2,#%000E
	ld	r2,r3
	add	r2,#L8798
L2BBE:
	ld	r11,@r2
	ld	r2,%FFF2(r14)
	and	r2,#%0018
	jr	eq,L2BDE
L2BCA:
	push	@r15,%FFEC(r14)
	push	@r15,%FFF2(r14)
	push	@r15,%FFEE(r14)
	call	@r11
	inc	r15,#6
L2BDA:
	jp	cret
L2BDE:
	ld	r13,#L9EEE
L2BE2:
	cp	r13,#LA07E
	jr	uge,L2BCA
	testb	%0001(r13)
	jr	eq,L2BF6
	ld	r0,%0002(r13)
	cp	r0,r12
	jr	eq,L2BDA
L2BF6:
	inc	r13,#8
	jr	L2BE2
L2BFA:
	ld	r2,%FFEE(r14)
	srl	r2,#8
	sla	r2,#3
	add	r2,#L8768
	jr	L2BBE
_openi:
	call	csv
	ld	r13,%0004(r14)
	dec	r15,#2
	ld	r2,%0014(r13)
	ld	%FFF2(r14),r2
	ld	r12,r2
	srl	r12,#8
	ld	r2,%0006(r13)
	and	r2,#%F000
	cp	r2,#%2000
	jr	eq,L2C46
	cp	r2,#%3000
	jr	eq,L2C46
	cp	r2,#%6000
	jr	eq,L2C7C
	cp	r2,#%7000
	jr	eq,L2C7C
	jr	L2C88
L2C46:
	cp	r12,L8DD2
	jr	uge,L2C82
	push	@r15,%0006(r14)
	push	@r15,%FFF2(r14)
	ld	r3,r12
	mult	rr2,#%000E
	ld	r2,r3
	add	r2,#_cdevsw
	jr	L2C74
L2C62:
	push	@r15,%0006(r14)
	push	@r15,%FFF2(r14)
	ld	r2,r12
	sll	r2,#3
	add	r2,#_bdevsw
L2C74:
	ld	r3,@r2
	call	@r3
	inc	r15,#4
	jr	L2C88
L2C7C:
	cp	r12,L8DD0
	jr	ult,L2C62
L2C82:
	ldb	%FC13,#%06
L2C88:
	jp	cret
_access:
	call	csv
	ld	r13,%0004(r14)
	ld	r12,%0006(r14)
	cp	r12,#%0080
	jr	ne,L2CD0
	push	@r15,%0002(r13)
	call	_getfs
	inc	r15,#2
	add	r2,#%019D
	testb	@r2
	jr	eq,L2CC0
	ldb	%FC13,#%1E
	jr	L2D04
L2CB8:
	ldb	%FC13,#%1A
	jr	L2D04
L2CC0:
	bitb	@r13,#5
	jr	eq,L2CCC
	push	@r15,r13
	call	_xrele
	inc	r15,#2
L2CCC:
	bitb	@r13,#5
	jr	ne,L2CB8
L2CD0:
	test	%FC16
	jr	ne,L2CDA
L2CD6:
	xor	r2,r2
	jr	L2D06
L2CDA:
	ld	r0,%FC16
	cp	r0,%000A(r13)
	jr	eq,L2CF6
	sra	r12,#3
	ld	r0,%FC18
	cp	r0,%000C(r13)
	jr	eq,L2CF6
	sra	r12,#3
L2CF6:
	ld	r2,%0006(r13)
	and	r2,r12
	jr	ne,L2CD6
	ldb	%FC13,#%0D
L2D04:
	ldk	r2,#1
L2D06:
	jp	cret
_owner:
	call	csv
	push	@r15,#%0000
	push	@r15,#_uchar
	call	_namei
	inc	r15,#4
	ld	r13,r2
	test	r13
	jr	ne,L2D28
	jr	L2D42
L2D24:
	ld	r2,r13
	jr	L2D44
L2D28:
	ld	r0,%FC16
	cp	r0,%000A(r13)
	jr	eq,L2D24
	call	_suser
	test	r2
	jr	ne,L2D24
	push	@r15,r13
	call	_iput
	inc	r15,#2
L2D42:
	xor	r2,r2
L2D44:
	jp	cret
_suser:
	call	csv
	test	%FC16
	jr	ne,L2D58
	ldk	r2,#1
L2D54:
	jp	cret
L2D58:
	ldb	%FC13,#%01
	xor	r2,r2
	jr	L2D54
_ufalloc:
	call	csv
	xor	r13,r13
	jr	L2D88
L2D6A:
	ld	r2,r13
	sla	r2,#1
	add	r2,#%FC62
	test	@r2
	jr	ne,L2D86
	ld	%FC22,r13
	clrb	%FC8A(r13)
	ld	r2,r13
L2D82:
	jp	cret
L2D86:
	inc	r13,#1
L2D88:
	cp	r13,#%0014
	jr	lt,L2D6A
	ldb	%FC13,#%18
	ld	r2,#%FFFF
	jr	L2D82
_falloc:
	call	csv
	call	_ufalloc
	ld	r12,r2
	test	r12
	jr	pl,L2DCA
L2DA8:
	xor	r2,r2
	jr	L2DC6
L2DAC:
	ld	r2,r12
	sla	r2,#1
	add	r2,#%FC62
	ld	@r2,r13
	incb	%0001(r13),#1
	xor	r0,r0
	ld	r1,r0
	ldl	%0004(r13),rr0
	ld	r2,r13
L2DC6:
	jp	cret
L2DCA:
	ld	r13,#L9EEE
	jr	L2DD8
L2DD0:
	testb	%0001(r13)
	jr	eq,L2DAC
	inc	r13,#8
L2DD8:
	cp	r13,#LA07E
	jr	ult,L2DD0
	push	@r15,#L8ADA
	call	_printf
	inc	r15,#2
	ldb	%FC13,#%17
	jr	L2DA8
end _p_fio
end fio
