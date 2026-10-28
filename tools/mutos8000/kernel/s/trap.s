trap module
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
_trap	L
G
_nosys	L
G
_nullsys	L
G	_p_trap	PR
E
_trap:
	call	csv
	ld	r13,%0006(r14)
	dec	r15,#8
	ldl	rr2,%FCF8
	ldl	%FFEC(r14),rr2
	clr	%FD5A
	ld	r2,%001C(r14)
	and	r2,#%4000
	jr	ne,L089E
	set	%0004(r14),#4
L089E:
	ld	r2,r14
	inc	r2,#8
	ld	%FD04,r2
	ld	r2,%0004(r14)
	and	r2,#%00FF
	sub	r2,#%000A
	cp	r2,#%0015
	jr	ugt,L08C0
	add	r2,r2
	ld	r1,L88F4(r2)
	jp	@r1
L08C0:
	test	L88E0
	jr	eq,L08C8
L08C6:
	jr	L08C6
L08C8:
	inc	L88E0,#1
	push	@r15,#%0000
	push	@r15,#L8920
	call	_printf
	inc	r15,#4
	ld	r2,r14
	add	r2,#%001C
	push	@r15,r2
	push	@r15,#L892A
	call	_printf
	inc	r15,#4
	push	@r15,%001C(r14)
	push	@r15,%0020(r14)
	push	@r15,%001E(r14)
	push	@r15,#L8934
	call	_printf
	inc	r15,#8
	push	@r15,%0004(r14)
	push	@r15,#L894A
	call	_printf
	inc	r15,#2
	ld	@r15,r13
	push	@r15,#L8958
	call	_printf
	inc	r15,#2
	ld	@r15,#L8964
	call	_panic
	inc	r15,#2
L0926:
	ldk	r12,#10
	jp	L0AC8
	inc	%0020(r14),#2
	ld	r2,%0020(r14)
	dec	r2,#4
	push	@r15,r2
	call	_fuword
	inc	r15,#2
	cp	r2,#%F009
	jr	ne,L094C
	test	%FCDA
	jp	eq,L0AD4
L094C:
	ldk	r12,#4
	jp	L0AC8
	cp	r13,#%7C06
	jr	ne,L095E
	ldk	r12,#5
	jp	L0AC8
L095E:
	cp	r13,#%7C04
	jr	ne,L096A
	ldk	r12,#7
	jp	L0AC8
L096A:
	cp	r13,#%7C05
	jr	ne,L094C
	ldk	r12,#6
	jp	L0AC8
	ldk	r12,#5
	res	%001C(r14),#11
	jp	L0AC8
L0980:
	clrb	%FC13
	res	%001C(r14),#7
	ld	r11,%0020(r14)
	ld	r2,r13
	and	r2,#%003F
	sll	r2,#2
	ld	r10,r2
	add	r10,#_sysent
	cp	r10,#_sysent
	jr	ne,L09AA
	call	L3630
	jp	L0AD4
L09AA:
	ld	r2,%FD04
	ld	%FFF2(r14),r2
	ld	r2,L88E2
	ld	r3,%FFF2(r14)
	ld	r0,#%0004
	ldir	@r2,@r3,r0
	ld	r2,%FC9E
	ld	%FC40,r2
	ld	r2,%FD04
	inc	r2,#8
	ld	r3,@r2
	ld	%FC22,r3
	ld	r2,%FD04
	inc	r2,#10
	ld	r3,@r2
	ld	%FC24,r3
	ld	r2,#%FC9E
	ld	%FC20,r2
	push	@r15,#%FCAE
	call	L0614
	inc	r15,#2
	test	r2
	jr	eq,L0A06
	testb	%FC13
	jr	ne,L0A0C
	ldb	%FC13,#%04
	jr	L0A0C
L0A06:
	ld	r2,%0002(r10)
	call	@r2
L0A0C:
	testb	%FC13
	jr	eq,L0A34
	set	%001C(r14),#7
	ld	r2,%FD04
	inc	r2,#8
	ldb	rl3,%FC13
	xorb	rh3,rh3
	ld	@r2,r3
	ld	r2,%FD04
	inc	r2,#10
	ldb	rl3,%FC14
	xorb	rh3,rh3
L0A30:
	ld	@r2,r3
	jr	L0AD4
L0A34:
	ld	r2,%FD04
	inc	r2,#8
	ld	r3,%FC22
	ld	@r2,r3
	ld	r2,%FD04
	inc	r2,#10
	ld	r3,%FC24
	jr	L0A30
	ld	r11,%0018(r14)
	push	@r15,%FD04
	call	L040E
	inc	r15,#2
	test	r2
	jr	ne,L0A6A
	push	@r15,r11
	call	L0E38
	inc	r15,#2
	test	r2
	jr	ne,L0AD4
L0A6A:
	ldk	r12,#11
	jr	L0AC8
	push	@r15,%0020(r14)
	push	@r15,%001E(r14)
	push	@r15,#L88E4
	push	@r15,#L8969
	call	_printf
	inc	r15,#8
	bit	%0004(r14),#4
	jp	ne,L0926
	push	@r15,#L8990
	call	_panic
	inc	r15,#2
	ld	r12,r13
	srl	r12,#14
	ld	r9,r13
	sll	r9,#2
	and	r9,#%F600
	push	@r15,r9
	push	@r15,r12
	push	@r15,#L88E4
	push	@r15,#L8997
	call	_printf
	inc	r15,#8
	jr	L0AD4
	push	@r15,r13
	push	@r15,#L89C3
	call	_printf
	inc	r15,#4
	jr	L0B26
L0AC8:
	push	@r15,r12
	push	@r15,%FC1E
	call	_psignal
	inc	r15,#4
L0AD4:
	call	_issig
	test	r2
	jr	eq,L0AE0
	call	_psig
L0AE0:
	push	@r15,%FC1E
	call	_setpri
	inc	r15,#2
	ldb	L8DDA,rl2
	testb	L8DC4
	jr	eq,L0AF8
	call	_qswtch
L0AF8:
	test	%FD10
	jr	eq,L0B16
	ld	r2,%FCFA
	sub	r2,%FFEE(r14)
	push	@r15,r2
	push	@r15,#%FD0A
	push	@r15,%0020(r14)
	call	_addupc
	inc	r15,#6
L0B16:
	test	%FD5A
	jr	eq,L0B26
	push	@r15,#%FD5C
	call	L03FE
	inc	r15,#2
L0B26:
	jp	cret
_nosys:
	call	csv
	ldb	%FC13,#%16
	jp	cret
_nullsys:
	WV %9E08
end _p_trap
end trap
