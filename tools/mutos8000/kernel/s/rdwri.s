rdwri module
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
_readi	L
G
_writei	L
G
_min	L
G	_p_rdwri	PR
E
_readi:
	call	csv
	ld	r13,%0004(r14)
	dec	r15,#16
	test	%FC28
	jp	eq,L25FA
	testl	%FC2A
	jr	pl,L2482
	ldb	%FC13,#%16
	jp	L25FA
L2482:
	setb	@r13,#2
	ld	r2,%0014(r13)
	ld	%FFF0(r14),r2
	ld	r10,%0006(r13)
	and	r10,#%F000
	cp	r10,#%2000
	jr	eq,L24A0
	cp	r10,#%3000
	jr	ne,L24C2
L24A0:
	push	@r15,%FFF0(r14)
	ld	r2,%FFF0(r14)
	srl	r2,#8
	ld	r3,r2
	mult	rr2,#%000E
	ld	r2,r3
	add	r2,#L879A
	ld	r3,@r2
	call	@r3
	inc	r15,#2
	jp	L25FA
L24C2:
	ldl	rr2,%FC2A
	ld	r4,#%FFF7
	sdal	rr2,r4
	ldl	%FFE8(r14),rr2
	ldl	%FFEC(r14),rr2
	ld	r12,%FC2C
	and	r12,#%01FF
	push	@r15,%FC28
	ld	r2,#%0200
	sub	r2,r12
	push	@r15,r2
	call	_min
	inc	r15,#4
	ld	r11,r2
	cp	r10,#%6000
	jr	eq,L2542
	cp	r10,#%7000
	jr	eq,L2542
	ldl	rr2,%000E(r13)
	subl	rr2,%FC2A
	ldl	%FFE4(r14),rr2
	testl	rr2
	jr	mi,L25FA
	jr	eq,L25FA
	ld	r3,r11
	exts	rr2
	cpl	rr2,%FFE4(r14)
	jr	le,L251E
	ld	r11,%FFE6(r14)
L251E:
	push	@r15,#%0001
	pushl	@r15,%FFE8(r14)
	push	@r15,r13
	call	_bmap
	inc	r15,#8
	ldl	%FFE8(r14),rr2
	testb	%FC13
	jr	ne,L25FA
	ld	r2,%0002(r13)
	ld	%FFF0(r14),r2
	jr	L2550
L2542:
	ldl	rr2,%FFE8(r14)
	addl	rr2,#%00000001
	ldl	L8DE4,rr2
L2550:
	testl	%FFE8(r14)
	jr	pl,L2568
	call	_geteblk
	ld	%FFF2(r14),r2
	push	@r15,r2
	call	_clrbuf
	inc	r15,#2
	jr	L259E
L2568:
	ldl	rr2,%0046(r13)
	addl	rr2,#%00000001
	cpl	rr2,%FFEC(r14)
	jr	ne,L258C
	pushl	@r15,L8DE4
	pushl	@r15,%FFE8(r14)
	push	@r15,%FFF0(r14)
	call	_breada
	inc	r15,#10
	jr	L259A
L258C:
	pushl	@r15,%FFE8(r14)
	push	@r15,%FFF0(r14)
	call	_bread
	inc	r15,#6
L259A:
	ld	%FFF2(r14),r2
L259E:
	ldl	rr2,%FFEC(r14)
	ldl	%0046(r13),rr2
	ld	r2,%FFF2(r14)
	add	r2,#%0016
	ld	r3,#%0200
	sub	r3,@r2
	push	@r15,r3
	push	@r15,r11
	call	_min
	inc	r15,#4
	ld	r11,r2
	test	r11
	jr	eq,L25DC
	push	@r15,#%0001
	push	@r15,r11
	ld	r2,%FFF2(r14)
	inc	r2,#14
	ld	r3,@r2
	add	r3,r12
	push	@r15,r3
	call	_iomove
	inc	r15,#6
L25DC:
	push	@r15,%FFF2(r14)
	call	_brelse
	inc	r15,#2
	testb	%FC13
	jr	ne,L25FA
	test	%FC28
	jr	eq,L25FA
	test	r11
	jr	eq,L25FA
	jp	pl,L24C2
L25FA:
	jp	cret
_writei:
	call	csv
	ld	r13,%0004(r14)
	dec	r15,#8
	testl	%FC2A
	jr	pl,L2618
	ldb	%FC13,#%16
	jp	L2748
L2618:
	ld	r2,%0014(r13)
	ld	%FFF0(r14),r2
	ld	r10,%0006(r13)
	and	r10,#%F000
	cp	r10,#%2000
	jr	eq,L2634
	cp	r10,#%3000
	jr	ne,L265E
L2634:
	ldb	rl2,@r13
	xorb	rh2,rh2
	or	r2,#%0042
	ldb	@r13,rl2
	push	@r15,%FFF0(r14)
	ld	r2,%FFF0(r14)
	srl	r2,#8
	ld	r3,r2
	mult	rr2,#%000E
	ld	r2,r3
	add	r2,#L879C
	ld	r3,@r2
	call	@r3
	inc	r15,#2
	jr	L2748
L265E:
	test	%FC28
	jr	eq,L2748
L2664:
	ldl	rr2,%FC2A
	ld	r4,#%FFF7
	sdal	rr2,r4
	ldl	%FFEC(r14),rr2
	ld	r11,%FC2C
	and	r11,#%01FF
	push	@r15,%FC28
	ld	r2,#%0200
	sub	r2,r11
	push	@r15,r2
	call	_min
	inc	r15,#4
	ld	r12,r2
	cp	r10,#%6000
	jr	eq,L26BC
	cp	r10,#%7000
	jr	eq,L26BC
	push	@r15,#%0000
	pushl	@r15,%FFEC(r14)
	push	@r15,r13
	call	_bmap
	inc	r15,#8
	ldl	%FFEC(r14),rr2
	testl	rr2
	jr	mi,L2748
	ld	r2,%0002(r13)
	ld	%FFF0(r14),r2
L26BC:
	cp	r12,#%0200
	jr	ne,L26D0
	pushl	@r15,%FFEC(r14)
	push	@r15,%FFF0(r14)
	call	_getblk
	jr	L26DC
L26D0:
	pushl	@r15,%FFEC(r14)
	push	@r15,%FFF0(r14)
	call	_bread
L26DC:
	inc	r15,#6
	ld	%FFF2(r14),r2
	push	@r15,#%0000
	push	@r15,r12
	ld	r2,%FFF2(r14)
	inc	r2,#14
	ld	r3,@r2
	add	r3,r11
	push	@r15,r3
	call	_iomove
	inc	r15,#6
	testb	%FC13
	jr	eq,L270A
	push	@r15,%FFF2(r14)
	call	_brelse
	jr	L2712
L270A:
	push	@r15,%FFF2(r14)
	call	_bdwrite
L2712:
	inc	r15,#2
	ldl	rr0,%FC2A
	cpl	rr0,%000E(r13)
	jr	le,L2732
	cp	r10,#%4000
	jr	eq,L272A
	cp	r10,#%8000
	jr	ne,L2732
L272A:
	ldl	rr2,%FC2A
	ldl	%000E(r13),rr2
L2732:
	ldb	rl2,@r13
	xorb	rh2,rh2
	or	r2,#%0042
	ldb	@r13,rl2
	testb	%FC13
	jr	ne,L2748
	test	%FC28
	jr	ne,L2664
L2748:
	jp	cret
L274C:
	call	csv
	ld	r0,%0004(r14)
	cp	r0,%0006(r14)
	jr	ule,L2760
	ld	r2,r0
L275C:
	jp	cret
L2760:
	ld	r2,%0006(r14)
	jr	L275C
_min:
	call	csv
	ld	r0,%0004(r14)
	cp	r0,%0006(r14)
	jr	uge,L277A
	ld	r2,r0
L2776:
	jp	cret
L277A:
	ld	r2,%0006(r14)
	jr	L2776
_iomove:
	call	csv
	ld	r13,%0004(r14)
	ld	r12,%0006(r14)
	test	r12
	jr	eq,L283E
	ldb	rl2,%FC12
	xorb	rh2,rh2
	cp	r2,#%0001
	jr	eq,L280A
	ld	r2,r12
	and	r2,#%0001
	jr	ne,L280A
	ld	r2,r13
	and	r2,#%0001
	jr	ne,L280A
	ld	r2,%FC26
	and	r2,#%0001
	jr	ne,L280A
	test	%0008(r14)
	jr	ne,L27CA
	push	@r15,r12
	push	@r15,r13
	push	@r15,%FC26
	call	_copyin
	jr	L27D6
L27CA:
	push	@r15,r12
	push	@r15,%FC26
	push	@r15,r13
	call	_copyout
L27D6:
	inc	r15,#6
	ld	r11,r2
	test	r11
	jr	eq,L27E6
	ldb	%FC13,#%0E
	jr	L283E
L27E6:
	ld	r2,%FC26
	add	r2,r12
	ld	%FC26,r2
	ldl	rr2,%FC2A
	ld	r5,r12
	exts	rr4
	addl	rr2,rr4
	ldl	%FC2A,rr2
	ld	r2,%FC28
	sub	r2,r12
	ld	%FC28,r2
	jr	L283E
L280A:
	test	%0008(r14)
	jr	ne,L2828
L2810:
	call	_cpass
	ld	r11,r2
	test	r11
	jr	mi,L283E
	ld	r2,r13
	inc	r13,#1
	ld	r3,r11
	ldb	@r2,rl3
	dec	r12,#1
	jr	ne,L2810
	jr	L283E
L2828:
	ld	r2,r13
	inc	r13,#1
	ldb	rl0,@r2
	xorb	rh0,rh0
	push	@r15,r0
	call	_passc
	inc	r15,#2
	test	r2
	jr	mi,L283E
	djnz	r12,L2828
L283E:
	jp	cret
end _p_rdwri
end rdwri
