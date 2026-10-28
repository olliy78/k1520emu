slp module
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
_sleep	L
G
_wakeup	L
G
_setrun	L
G
_setpri	L
G
_sched	L
G
_qswtch	L
G
_swtch	L
G
_newproc	L
G
L20F6	L
G	_p_slp	PR
E
_sleep:
	call	csv
	ld	r13,%FC1E
	call	_spl6
	ld	r12,r2
	test	%0004(r14)
	jr	ne,L18F2
	push	@r15,#L8A8E
	call	_panic
	inc	r15,#2
L18F2:
	ldb	@r13,#%01
	ld	r2,%0004(r14)
	ld	%0014(r13),r2
	ld	r2,%0006(r14)
	ldb	%0002(r13),rl2
	ld	r11,%0004(r14)
	sra	r11,#5
	and	r11,#%003F
	ld	r2,r11
	sla	r2,#1
	add	r2,#LCEC6
	ld	r3,@r2
	ld	%0018(r13),r3
	ld	r2,r11
	sla	r2,#1
	add	r2,#LCEC6
	ld	@r2,r13
	cp	%0006(r14),#%0019
	jr	le,L1996
	call	_issig
	test	r2
	jr	eq,L1970
	clr	%0014(r13)
	ldb	@r13,#%03
	ld	r2,r11
	sla	r2,#1
	add	r2,#LCEC6
	ld	r3,%0018(r13)
	ld	@r2,r3
	call	_spl0
L195A:
	push	@r15,#%FCAE
	ld	r2,%FC1E
	inc	r2,#16
	push	@r15,@r2
	call	L0620
	inc	r15,#4
L196C:
	jp	cret
L1970:
	call	_spl0
	testb	L8DD6
	jr	eq,L1988
	clrb	L8DD6
	push	@r15,#L8DD6
	call	_wakeup
	inc	r15,#2
L1988:
	call	_swtch
	call	_issig
	test	r2
	jr	ne,L195A
	jr	L199E
L1996:
	call	_spl0
	call	_swtch
L199E:
	push	@r15,r12
	call	_splx
	inc	r15,#2
	jr	L196C
_wakeup:
	call	csv
	ld	r13,%0004(r14)
	dec	r15,#2
	call	_spl6
	ld	r9,r2
	ld	r10,r13
	sra	r10,#5
	and	r10,#%003F
	ld	r2,r10
	sla	r2,#1
	add	r2,#LCEC6
	ld	r12,@r2
	xor	r11,r11
	jr	L1A20
L19D2:
	ldb	rl2,@r12
	xorb	rh2,rh2
	cp	r2,#%0005
	jr	eq,L1A1A
	test	r11
	jr	ne,L19F4
	ld	r2,r10
	sla	r2,#1
	add	r2,#LCEC6
	ld	r3,%0018(r12)
	ld	@r2,r3
	ld	r2,r3
	jr	L19FC
L19F4:
	ld	r2,%0018(r12)
	ld	%0018(r11),r2
L19FC:
	ld	%FFF2(r14),r2
	clr	%0014(r12)
	push	@r15,r12
	call	_setrun
	inc	r15,#2
	ld	r12,%FFF2(r14)
	jr	L1A20
L1A12:
	ld	r0,%0014(r12)
	cp	r0,r13
	jr	eq,L19D2
L1A1A:
	ld	r11,r12
	ld	r12,%0018(r12)
L1A20:
	test	r12
	jr	ne,L1A12
	push	@r15,r9
	call	_splx
	inc	r15,#2
	jp	cret
_setrq:
	call	csv
	call	_spl6
	ld	r12,r2
	ld	r13,L8DC8
	jr	L1A56
L1A40:
	cp	r13,%0004(r14)
	jr	ne,L1A52
	push	@r15,#L8A99
	call	_printf
	inc	r15,#2
	jr	L1A70
L1A52:
	ld	r13,%0018(r13)
L1A56:
	test	r13
	jr	ne,L1A40
	ld	r2,%0004(r14)
	add	r2,#%0018
	ld	r3,L8DC8
	ld	@r2,r3
	ld	r2,%0004(r14)
	ld	L8DC8,r2
L1A70:
	push	@r15,r12
	call	_splx
	inc	r15,#2
	jp	cret
_setrun:
	call	csv
	ld	r13,%0004(r14)
	testb	@r13
	jr	eq,L1A92
	ldb	rl2,@r13
	xorb	rh2,rh2
	cp	r2,#%0005
	jr	ne,L1A9C
L1A92:
	push	@r15,#L8AA4
	call	_panic
	inc	r15,#2
L1A9C:
	ld	r2,%0014(r13)
	ld	r12,r2
	test	r2
	jr	eq,L1AAA
	push	@r15,r12
	jr	L1ADE
L1AAA:
	ldb	@r13,#%03
	push	@r15,r13
	call	_setrq
	inc	r15,#2
	ldb	rl0,%0002(r13)
	cpb	rl0,L8DDA
	jr	uge,L1AC4
	incb	L8DC4,#1
L1AC4:
	testb	L8DD8
	jr	eq,L1AE4
	ldb	rl2,%0001(r13)
	xorb	rh2,rh2
	and	r2,#%0001
	jr	ne,L1AE4
	clrb	L8DD8
	push	@r15,#L8DD8
L1ADE:
	call	_wakeup
	inc	r15,#2
L1AE4:
	jp	cret
_setpri:
	call	csv
	ld	r13,%0004(r14)
	ldb	rl3,%0004(r13)
	xorb	rh3,rh3
	and	r3,#%00FF
	exts	rr2
	div	rr2,#%0010
	ld	r12,r3
	ld	r2,r12
	ldb	rl3,%0005(r13)
	xorb	rh3,rh3
	add	r3,#%001E
	add	r2,r3
	ld	r12,r2
	cp	r12,#%007F
	jr	le,L1B1C
	ld	r12,#%007F
L1B1C:
	ldb	rl2,L8DDA
	xorb	rh2,rh2
	cp	r2,r12
	jr	le,L1B2A
	incb	L8DC4,#1
L1B2A:
	ld	r2,r12
	ldb	%0002(r13),rl2
	jp	cret
_sched:
	call	csv
L1B38:
	test	L8DE2
	jr	eq,L1B52
	clr	L8DE2
	call	_spl0
	call	_update
	clr	L8DE0
	clr	L8DE2
L1B52:
	call	_spl6
	ld	r11,#LB1E0
	ld	r13,#LA11C
	jr	L1BAC
L1B60:
	ldb	rl2,@r13
	xorb	rh2,rh2
	cp	r2,#%0003
	jr	ne,L1BA8
	ldb	rl2,%0001(r13)
	and	r2,#%0001
	jr	ne,L1BA8
	ldb	rl2,%0003(r13)
	xorb	rh2,rh2
	ldb	rl3,%0005(r13)
	xorb	rh3,rh3
	add	r3,#%FFEC
	sla	r3,#3
	sub	r2,r3
	cp	r2,r11
	jr	le,L1BA8
	ld	r12,r13
	ldb	rl2,%0003(r13)
	xorb	rh2,rh2
	ldb	rl3,%0005(r13)
	xorb	rh3,rh3
	add	r3,#%FFEC
	sla	r3,#3
	sub	r2,r3
	ld	r11,r2
L1BA8:
	add	r13,#%001C
L1BAC:
	cp	r13,L8A8C
	jr	ule,L1B60
	cp	r11,#LB1E0
	jr	ne,L1BCC
	incb	L8DD8,#1
	push	@r15,#%0000
	push	@r15,#L8DD8
L1BC4:
	call	_sleep
	inc	r15,#4
	jr	L1B38
L1BCC:
	call	_spl0
	push	@r15,r12
	call	_swapin
	inc	r15,#2
	test	r2
	jr	ne,L1B38
	call	_spl6
	xor	r12,r12
	ld	r9,#%FFFF
	ld	r10,r9
	ld	r13,#LA11C
	jr	L1C84
L1BEE:
	ldb	rl2,@r13
	xorb	rh2,rh2
	cp	r2,#%0005
	jr	eq,L1C80
	ldb	rl2,%0001(r13)
	and	r2,#%0047
	cp	r2,#%0001
	jr	ne,L1C80
	test	%0016(r13)
	jr	eq,L1C16
	ld	r2,%0016(r13)
	inc	r2,#10
	bitb	@r2,#3
	jr	ne,L1C80
L1C16:
	ldb	rl2,@r13
	xorb	rh2,rh2
	cp	r2,#%0001
	jr	ne,L1C2A
	ldb	rl2,%0002(r13)
	cp	r2,#%0019
	jr	uge,L1C34
L1C2A:
	ldb	rl2,@r13
	xorb	rh2,rh2
	cp	r2,#%0006
	jr	ne,L1C42
L1C34:
	cp	r9,%0012(r13)
	jr	ge,L1C80
	ld	r12,r13
	ld	r9,%0012(r13)
	jr	L1C80
L1C42:
	test	r9
	jr	pl,L1C80
	ldb	rl2,@r13
	xorb	rh2,rh2
	cp	r2,#%0003
	jr	eq,L1C56
	cp	r2,#%0001
	jr	ne,L1C80
L1C56:
	ldb	rl2,%0003(r13)
	xorb	rh2,rh2
	ldb	rl3,%0005(r13)
	xorb	rh3,rh3
	add	r2,r3
	add	r2,#%FFEC
	cp	r2,r10
	jr	le,L1C80
	ld	r12,r13
	ldb	rl2,%0003(r13)
	xorb	rh2,rh2
	ldb	rl3,%0005(r13)
	add	r2,r3
	ld	r10,r2
	add	r10,#%FFEC
L1C80:
	add	r13,#%001C
L1C84:
	cp	r13,L8A8C
	jr	ule,L1BEE
	call	_spl0
	test	r9
	jr	pl,L1C9E
	cp	r11,#%0003
	jr	lt,L1CB6
	cp	r10,#%0002
	jr	lt,L1CB6
L1C9E:
	resb	%0001(r12),#0
	push	@r15,#%0000
	push	@r15,#%0001
	push	@r15,r12
	call	_xswap
	inc	r15,#6
	jp	L1B38
L1CB6:
	call	_spl6
	incb	L8DD6,#1
	push	@r15,#%0000
	push	@r15,#L8DD6
	jp	L1BC4
_swapin:
	call	csv
	ld	r13,%0004(r14)
	push	@r15,%0012(r13)
	push	@r15,#LA09C
	call	_malloc
	inc	r15,#4
	ld	r11,r2
	test	r11
	jr	ne,L1CEC
L1CE6:
	xor	r2,r2
L1CE8:
	jp	cret
L1CEC:
	ld	r2,%0016(r13)
	ld	r12,r2
	test	r2
	jr	eq,L1D5E
	push	@r15,r12
	call	L57A4
	inc	r15,#2
	testb	%0009(r12)
	jr	ne,L1D52
	push	@r15,%0004(r12)
	push	@r15,#LA09C
	call	_malloc
	inc	r15,#4
	ld	r9,r2
	test	r9
	jr	ne,L1D30
	push	@r15,r12
	call	L57AC
	ld	@r15,r11
	push	@r15,%0012(r13)
	push	@r15,#LA09C
	call	_mfree
	inc	r15,#6
	jr	L1CE6
L1D30:
	ld	%0002(r12),r9
	ldb	rl2,%000A(r12)
	xorb	rh2,rh2
	and	r2,#%0004
	jr	ne,L1D52
	push	@r15,#%0001
	push	@r15,%0004(r12)
	push	@r15,r9
	push	@r15,@r12
	call	_swap
	inc	r15,#8
L1D52:
	incb	%0009(r12),#1
	push	@r15,r12
	call	L57AC
	inc	r15,#2
L1D5E:
	ld	r10,r11
	sla	r10,#2
	add	r10,#%EC00
	push	@r15,r10
	push	@r15,#%0001
	push	@r15,#%0104
	push	@r15,r11
	push	@r15,%0010(r13)
	call	_swap
	inc	r15,#10
	ld	%0010(r13),r11
	setb	%0001(r13),#0
	clrb	%0003(r13)
	ldk	r2,#1
	jr	L1CE8
_qswtch:
	call	csv
	push	@r15,%FC1E
	call	_setrq
	inc	r15,#2
	call	_swtch
	jp	cret
_swtch:
	call	csv
	ld	r0,%FC1E
	cp	r0,#LA11C
	jr	eq,L1DEA
	push	@r15,#%FC00
	call	L0614
	inc	r15,#2
	test	r2
	jr	eq,L1DC6
	call	L3D40
	jr	L1E9E
L1DC6:
	test	%FD5A
	jr	ne,L1DDC
	push	@r15,#%FD5C
	call	L03F8
	inc	r15,#2
	ld	%FD5A,#%0001
L1DDC:
	push	@r15,#%FCAE
	push	@r15,LA12C
	call	L0620
	inc	r15,#4
L1DEA:
	push	@r15,#%FCAE
	call	L0614
	inc	r15,#2
	test	r2
	jr	ne,L1E06
	push	@r15,#%FC00
	call	L0614
	inc	r15,#2
	test	r2
	jr	ne,L1E9E
L1E06:
	call	_spl6
	clrb	L8DC4
	xor	r10,r10
	xor	r11,r11
	ld	r13,#%0080
	ld	r12,L8DC8
	jr	L1E44
L1E1C:
	ldb	rl2,@r12
	xorb	rh2,rh2
	cp	r2,#%0003
	jr	ne,L1E3E
	bitb	%0001(r12),#0
	jr	eq,L1E3E
	ldb	rl2,%0002(r12)
	cp	r2,r13
	jr	ge,L1E3E
	ld	r10,r12
	ld	r9,r11
	ldb	rl2,%0002(r12)
	ld	r13,r2
L1E3E:
	ld	r11,r12
	ld	r12,%0018(r12)
L1E44:
	test	r12
	jr	ne,L1E1C
	ld	r12,r10
	test	r12
	jr	ne,L1E54
	call	_idle
	jr	L1E06
L1E54:
	ld	r11,r9
	test	r11
	jr	ne,L1E64
	ld	r2,%0018(r12)
	ld	L8DC8,r2
	jr	L1E6C
L1E64:
	ld	r2,%0018(r12)
	ld	%0018(r11),r2
L1E6C:
	ld	r2,r13
	ldb	L8DDA,rl2
	call	_spl0
	ldb	rl2,%0001(r12)
	xorb	rh2,rh2
	ld	r13,r2
	and	r13,#%0008
	resb	%0001(r12),#3
	test	r13
	jr	eq,L1E90
	push	@r15,#%FCC0
	jr	L1E94
L1E90:
	push	@r15,#%FC00
L1E94:
	push	@r15,%0010(r12)
	call	L0620
	inc	r15,#4
L1E9E:
	jp	cret
_newproc:
	call	csv
	dec	r15,#6
	clr	%FFF2(r14)
	jr	L1EB2
L1EAE:
	clr	L8DD4
L1EB2:
	inc	L8DD4,#1
	cp	L8DD4,#%7530
	jr	ge,L1EAE
	ld	r13,#LA11C
	jr	L1EEA
L1EC4:
	testb	@r13
	jr	ne,L1ED2
	test	%FFF2(r14)
	jr	ne,L1ED2
	ld	%FFF2(r14),r13
L1ED2:
	ld	r0,%000C(r13)
	cp	r0,L8DD4
	jr	eq,L1EB2
	ld	r0,%000A(r13)
	cp	r0,L8DD4
	jr	eq,L1EB2
	add	r13,#%001C
L1EEA:
	cp	r13,L8A8C
	jr	ule,L1EC4
	ld	r13,%FFF2(r14)
	test	r13
	jr	ne,L1F02
	push	@r15,#L8AB8
	call	_panic
	inc	r15,#2
L1F02:
	ld	r12,%FC1E
	ld	%FFF0(r14),r12
	ldb	@r13,#%03
	clr	%001A(r13)
	ldb	%0001(r13),#%01
	ld	r2,%0008(r12)
	ld	%0008(r13),r2
	ld	r2,%000A(r12)
	ld	%000A(r13),r2
	ldb	rl2,%0005(r12)
	xorb	rh2,rh2
	ldb	%0005(r13),rl2
	ld	r2,%0016(r12)
	ld	%0016(r13),r2
	ld	r2,L8DD4
	ld	%000C(r13),r2
	ld	r2,%000C(r12)
	ld	%000E(r13),r2
	clrb	%0003(r13)
	clrb	%0004(r13)
	xor	r11,r11
	jr	L1F78
L1F56:
	ld	r2,r11
	sla	r2,#1
	add	r2,#%FC62
	test	@r2
	jr	eq,L1F76
	ld	r2,r11
	sla	r2,#1
	add	r2,#%FC62
	ld	r3,@r2
	inc	r3,#1
	ld	r2,r3
	incb	@r2,#1
L1F76:
	inc	r11,#1
L1F78:
	cp	r11,#%0014
	jr	lt,L1F56
	ld	r2,%FFF0(r14)
	add	r2,#%0016
	test	@r2
	jr	eq,L1FAA
	ld	r2,%FFF0(r14)
	add	r2,#%0016
	ld	r3,@r2
	inc	r3,#8
	ld	r2,r3
	incb	@r2,#1
	ld	r2,%FFF0(r14)
	add	r2,#%0016
	ld	r3,@r2
	inc	r3,#9
	ld	r2,r3
	incb	@r2,#1
L1FAA:
	ld	r2,%FC2E
	inc	r2,#1
	incb	@r2,#1
	test	%FC30
	jr	eq,L1FC0
	ld	r2,%FC30
	inc	r2,#1
	incb	@r2,#1
L1FC0:
	ld	r13,%FFF2(r14)
	ld	%FC1E,r13
	ld	r12,%FFF0(r14)
	ld	r11,#%0100
	ld	r2,%0010(r12)
	ld	%FFEE(r14),r2
	ld	%0012(r13),r11
	push	@r15,#%FCC0
	call	L0614
	inc	r15,#2
	test	r2
	jr	eq,L1FF4
	call	L3D40
	ldk	r2,#1
L1FF0:
	jp	cret
L1FF4:
	test	%FD5A
	jr	ne,L200A
	push	@r15,#%FD5C
	call	L03F8
	inc	r15,#2
	ld	%FD5A,#%0001
L200A:
	push	@r15,r11
	push	@r15,#LA09C
	call	_malloc
	inc	r15,#4
	ld	r10,r2
	test	r10
	jr	ne,L2092
	ldb	@r12,#%04
	ld	r2,%FFEE(r14)
	ld	%0010(r13),r2
	ldk	r11,#4
	ld	r2,#%FCFF
	srl	r2,#8
	ld	%FFEE(r14),r2
	ld	r2,%0010(r13)
	sra	r2,#8
	sla	r2,#10
	ld	r10,r2
	add	r10,#%ECFF
	srl	r10,#8
	jr	L2062
L204E:
	push	@r15,r10
	inc	r10,#1
	ld	r2,%FFEE(r14)
	inc	%FFEE(r14),#1
	push	@r15,r2
	call	_copyseg
	inc	r15,#4
L2062:
	ld	r2,r11
	dec	r11,#1
	test	r2
	jr	ne,L204E
	push	@r15,#%0000
	push	@r15,#%0000
	push	@r15,r13
	call	_xswap
	inc	r15,#6
	ldb	@r12,#%03
L207E:
	ld	%FC1E,r12
	push	@r15,r13
	call	_setrq
	inc	r15,#2
	setb	%0001(r13),#3
	xor	r2,r2
	jr	L1FF0
L2092:
	ld	%0010(r13),r10
	jr	L20AC
L2098:
	push	@r15,r10
	inc	r10,#1
	ld	r2,%FFEE(r14)
	inc	%FFEE(r14),#1
	push	@r15,r2
	call	_copyseg
	inc	r15,#4
L20AC:
	ld	r2,r11
	dec	r11,#1
	test	r2
	jr	ne,L2098
	ldk	r11,#4
	ld	r2,#%FCFF
	srl	r2,#8
	ld	%FFEE(r14),r2
	ld	r2,%0010(r13)
	sra	r2,#8
	sla	r2,#10
	ld	r10,r2
	add	r10,#%ECFF
	srl	r10,#8
L20D8:
	ld	r2,r11
	dec	r11,#1
	test	r2
	jr	eq,L207E
	push	@r15,r10
	inc	r10,#1
	ld	r2,%FFEE(r14)
	inc	%FFEE(r14),#1
	push	@r15,r2
	call	_copyseg
	inc	r15,#4
	jr	L20D8
L20F6:
	call	csv
	jp	cret
end _p_slp
end slp
