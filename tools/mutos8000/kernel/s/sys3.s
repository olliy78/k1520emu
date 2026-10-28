sys3 module
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
_cinit	L
G
L3D40	L
G
_estabur	L
G
_fstat	L
G
_stat	L
G
_dup	L
G
_smount	L
G
_sumount	L
G
_exec	L
G	_p_sys3	PR
E
_cinit:
	call	csv
	ld	r13,#LCF46
	inc	r13,#15
	and	r13,#%FFF0
	ld	r12,r13
	jr	L3D22
L3D16:
	ld	r2,%D266
	ld	@r12,r2
	ld	%D266,r12
	inc	r12,#16
L3D22:
	cp	r12,#_end
	jr	ule,L3D16
	xor	r13,r13
	ld	r11,#_cdevsw
	jr	L3D34
L3D30:
	inc	r13,#1
	inc	r11,#14
L3D34:
	test	@r11
	jr	ne,L3D30
	ld	L8DD2,r13
	jp	cret
L3D40:
	ret
_estabur:
	call	csv
	ld	r2,%0004(r14)
	add	r2,%0006(r14)
	add	r2,%0008(r14)
	cp	r2,#%0100
	jr	ugt,L3D5E
	xor	r2,r2
L3D5A:
	jp	cret
L3D5E:
	ldk	r2,#12
	ldb	%FC13,rl2
	ld	r2,#%FFFF
	jr	L3D5A
_fstat:
	call	csv
	ld	r12,%FC20
	push	@r15,@r12
	call	_getf
	inc	r15,#2
	ld	r13,r2
	test	r13
	jr	eq,L3D9E
	bitb	@r13,#2
	jr	eq,L3D8A
	pushl	@r15,%0004(r13)
	jr	L3D90
L3D8A:
	xor	r3,r3
	exts	rr2
	pushl	@r15,rr2
L3D90:
	push	@r15,%0002(r12)
	push	@r15,%0002(r13)
	call	_stat1
	inc	r15,#8
L3D9E:
	jp	cret
_stat:
	call	csv
	ld	r12,%FC20
	push	@r15,#%0000
	push	@r15,#_uchar
	call	_namei
	inc	r15,#4
	ld	r13,r2
	test	r13
	jr	eq,L3DD8
	xor	r3,r3
	exts	rr2
	pushl	@r15,rr2
	push	@r15,%0002(r12)
	push	@r15,r13
	call	_stat1
	inc	r15,#6
	ld	@r15,r13
	call	_iput
	inc	r15,#2
L3DD8:
	jp	cret
_stat1:
	call	csv
	ld	r13,%0004(r14)
	sub	r15,#%001E
	push	@r15,#L8DCC
	push	@r15,#L8DCC
	push	@r15,r13
	call	_iupdat
	inc	r15,#6
	ld	r2,%0002(r13)
	ld	%FFD6(r14),r2
	ld	r2,%0004(r13)
	ld	%FFD8(r14),r2
	ld	r2,%0006(r13)
	ld	%FFDA(r14),r2
	ld	r2,%0008(r13)
	ld	%FFDC(r14),r2
	ld	r2,%000A(r13)
	ld	%FFDE(r14),r2
	ld	r2,%000C(r13)
	ld	%FFE0(r14),r2
	ld	r2,%0014(r13)
	ld	%FFE2(r14),r2
	ldl	rr2,%000E(r13)
	subl	rr2,%0008(r14)
	ldl	%FFE4(r14),rr2
	ld	r2,%0004(r13)
	inc	r2,#15
	srl	r2,#3
	push	@r15,r2
	push	@r15,#%0000
	push	@r15,%0002(r13)
	call	_bread
	inc	r15,#6
	ld	r11,r2
	ld	r12,%000E(r11)
	ld	r2,r12
	ld	r3,%0004(r13)
	inc	r3,#15
	and	r3,#%0007
	sla	r3,#6
	add	r2,r3
	ld	r12,r2
	ldl	rr2,%0034(r12)
	ldl	%FFE8(r14),rr2
	ldl	rr2,%0038(r12)
	ldl	%FFEC(r14),rr2
	ldl	rr2,%003C(r12)
	ldl	%FFF0(r14),rr2
	push	@r15,r11
	call	_brelse
	ld	@r15,#%001E
	push	@r15,%0006(r14)
	ld	r2,r14
	add	r2,#%FFD6
	push	@r15,r2
	call	_copyout
	inc	r15,#6
	test	r2
	jr	pl,L3EAE
	ldb	%FC13,#%0E
L3EAE:
	jp	cret
_dup:
	call	csv
	ld	r12,%FC20
	ld	r10,@r12
	and	r10,#%FFC0
	ld	r2,@r12
	and	r2,#%003F
	ld	@r12,r2
	push	@r15,r2
	call	_getf
	inc	r15,#2
	ld	r13,r2
	test	r13
	jr	eq,L3F38
	ld	r2,r10
	and	r2,#%0040
	jr	ne,L3EEA
	call	_ufalloc
	ld	r11,r2
	test	r11
	jr	mi,L3F38
	jr	L3F04
L3EEA:
	ld	r11,%0002(r12)
	test	r11
	jr	mi,L3EF8
	cp	r11,#%0014
	jr	lt,L3F00
L3EF8:
	ldb	%FC13,#%09
	jr	L3F38
L3F00:
	ld	%FC22,r11
L3F04:
	cp	r11,@r12
	jr	eq,L3F38
	ld	r2,r11
	sla	r2,#1
	add	r2,#%FC62
	test	@r2
	jr	eq,L3F28
	ld	r2,r11
	sla	r2,#1
	add	r2,#%FC62
	push	@r15,@r2
	call	_closef
	inc	r15,#2
L3F28:
	ld	r2,r11
	sla	r2,#1
	add	r2,#%FC62
	ld	@r2,r13
	incb	%0001(r13),#1
L3F38:
	jp	cret
_smount:
	call	csv
	dec	r15,#6
	ld	r10,%FC20
	call	_getmdev
	ld	%FFF2(r14),r2
	testb	%FC13
	jp	ne,L407C
	ld	r2,%0002(r10)
	ld	%FC40,r2
	push	@r15,#%0000
	push	@r15,#_uchar
	call	_namei
	inc	r15,#4
	ld	r13,r2
	test	r13
	jp	eq,L407C
	ldb	rl2,%0001(r13)
	xorb	rh2,rh2
	cp	r2,#%0001
	jr	ne,L406E
	ld	r2,%0006(r13)
	and	r2,#%2000
	jr	ne,L406E
	clr	%FFF0(r14)
	ld	r12,#LA07E
	jr	L3FB0
L3F94:
	test	%0002(r12)
	jr	eq,L3FA4
	ld	r0,%FFF2(r14)
	cp	r0,@r12
	jr	eq,L406E
	jr	L3FAE
L3FA4:
	test	%FFF0(r14)
	jr	ne,L3FAE
	ld	%FFF0(r14),r12
L3FAE:
	inc	r12,#6
L3FB0:
	cp	r12,#LA09C
	jr	ult,L3F94
	ld	r12,%FFF0(r14)
	test	r12
	jr	eq,L406E
	test	%0004(r10)
	jr	eq,L3FCA
	push	@r15,#%0000
	jr	L3FCE
L3FCA:
	push	@r15,#%0001
L3FCE:
	push	@r15,%FFF2(r14)
	ld	r2,%FFF2(r14)
	srl	r2,#8
	sla	r2,#3
	add	r2,#_bdevsw
	ld	r3,@r2
	call	@r3
	inc	r15,#4
	testb	%FC13
	jr	ne,L4074
	ldk	r3,#1
	exts	rr2
	pushl	@r15,rr2
	push	@r15,%FFF2(r14)
	call	_bread
	inc	r15,#6
	ld	%FFEE(r14),r2
	testb	%FC13
	jr	eq,L4012
	push	@r15,r2
	call	_brelse
	inc	r15,#2
	jr	L4074
L4012:
	ld	%0004(r12),r13
	ld	r2,%FFF2(r14)
	ld	@r12,r2
	call	_geteblk
	ld	%0002(r12),r2
	push	@r15,#%0200
	ld	r2,%0002(r12)
	inc	r2,#14
	push	@r15,@r2
	ld	r2,%FFEE(r14)
	inc	r2,#14
	push	@r15,@r2
	call	_bcopy
	inc	r15,#6
	ld	r2,%0002(r12)
	inc	r2,#14
	ld	r11,@r2
	clrb	%019B(r11)
	clrb	%019A(r11)
	ld	r2,%0004(r10)
	and	r2,#%0001
	ldb	%019D(r11),rl2
	push	@r15,%FFEE(r14)
	call	_brelse
	inc	r15,#2
	setb	@r13,#3
	push	@r15,r13
	call	_prele
	jr	L407A
L406E:
	ldb	%FC13,#%10
L4074:
	push	@r15,r13
	call	_iput
L407A:
	inc	r15,#2
L407C:
	jp	cret
_sumount:
	call	csv
	dec	r15,#4
	call	_getmdev
	ld	%FFF2(r14),r2
	testb	%FC13
	jr	ne,L4138
	push	@r15,r2
	call	_xumount
	inc	r15,#2
	call	_update
	ld	r12,#LA07E
	jr	L40B6
L40A6:
	test	%0002(r12)
	jr	eq,L40B4
	ld	r0,%FFF2(r14)
	cp	r0,@r12
	jr	eq,L40C4
L40B4:
	inc	r12,#6
L40B6:
	cp	r12,#LA09C
	jr	ult,L40A6
	ldb	%FC13,#%16
	jr	L4138
L40C4:
	ld	r13,#L907A
	jr	L40EA
L40CA:
	ld	r0,%FFF2(r14)
	cp	r0,%0002(r13)
	jr	ne,L40E6
	testb	%0001(r13)
	jr	eq,L40E2
	ldb	%FC13,#%10
	jr	L4138
L40E2:
	clr	%0004(r13)
L40E6:
	add	r13,#%004A
L40EA:
	cp	r13,#L9EEE
	jr	ult,L40CA
	push	@r15,#%0000
	push	@r15,%FFF2(r14)
	ld	r2,%FFF2(r14)
	srl	r2,#8
	sla	r2,#3
	add	r2,#L8768
	ld	r3,@r2
	call	@r3
	inc	r15,#4
	ld	r13,%0004(r12)
	resb	@r13,#3
	push	@r15,r13
	call	_plock
	ld	@r15,r13
	call	_iput
	inc	r15,#2
	ld	r2,%0002(r12)
	ld	%FFF0(r14),r2
	clr	%0002(r12)
	push	@r15,%FFF0(r14)
	call	_brelse
	inc	r15,#2
L4138:
	jp	cret
_getmdev:
	call	csv
	dec	r15,#2
	push	@r15,#%0000
	push	@r15,#_uchar
	call	_namei
	inc	r15,#4
	ld	r13,r2
	test	r13
	jr	ne,L415E
	ld	r2,#%FFFF
L415A:
	jp	cret
L415E:
	ld	r2,%0006(r13)
	and	r2,#%F000
	cp	r2,#%6000
	jr	eq,L4172
	ldb	%FC13,#%0F
L4172:
	ld	r2,%0014(r13)
	ld	%FFF2(r14),r2
	srl	r2,#8
	cp	r2,L8DD0
	jr	lt,L418A
	ldb	%FC13,#%06
L418A:
	push	@r15,r13
	call	_iput
	inc	r15,#2
	ld	r2,%FFF2(r14)
	jr	L415A
_exec:
	call	csv
	ld	r2,%FC20
	inc	r2,#4
	clr	@r2
	call	_exece
	jp	cret
end _p_sys3
end sys3
