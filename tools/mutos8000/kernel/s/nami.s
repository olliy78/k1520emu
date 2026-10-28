nami module
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
_namei	L
G
L39A0	L
G	_p_nami	PR
E
_namei:
	call	csv
	dec	r15,#10
	ld	r13,%FC2E
	ld	r2,%0004(r14)
	call	@r2
	ld	r12,r2
	cp	r12,#%002F
	jr	ne,L36C4
	ld	r13,%FC30
	test	r13
	jr	ne,L36C4
	ld	r13,L8DC6
L36C4:
	push	@r15,%0004(r13)
	push	@r15,%0002(r13)
	call	_iget
	inc	r15,#4
	jr	L36DC
L36D4:
	ld	r2,%0004(r14)
	call	@r2
	ld	r12,r2
L36DC:
	cp	r12,#%002F
	jr	eq,L36D4
	test	r12
	jr	ne,L36F2
	test	%0006(r14)
	jr	eq,L36F2
	ldb	%FC13,#%02
L36F2:
	testb	%FC13
	jp	ne,L383A
	test	r12
	jr	ne,L3704
L36FE:
	ld	r2,r13
L3700:
	jp	cret
L3704:
	ld	r11,#%FC32
	jr	L372A
L370A:
	test	r12
	jr	eq,L3738
	testb	%FC13
	jr	ne,L3738
	cp	r11,#%FC40
	jr	uge,L3722
	ld	r2,r11
	inc	r11,#1
	ld	r3,r12
	ldb	@r2,rl3
L3722:
	ld	r2,%0004(r14)
	call	@r2
	ld	r12,r2
L372A:
	cp	r12,#%002F
	jr	ne,L370A
	jr	L3738
L3732:
	ld	r2,r11
	inc	r11,#1
	clrb	@r2
L3738:
	cp	r11,#%FC40
	jr	ult,L3732
	jr	L3748
L3740:
	ld	r2,%0004(r14)
	call	@r2
	ld	r12,r2
L3748:
	cp	r12,#%002F
	jr	eq,L3740
L374E:
	ld	r2,%0006(r13)
	and	r2,#%F000
	cp	r2,#%4000
	jr	eq,L3762
	ldb	%FC13,#%14
L3762:
	push	@r15,#%0040
	push	@r15,r13
	call	_access
	inc	r15,#4
	testb	%FC13
	jr	ne,L383A
	xor	r0,r0
	ld	r1,r0
	ldl	%FC2A,rr0
	ldb	%FC12,#%01
	ld	r1,r0
	ldl	%FFEA(r14),rr0
	clr	%FFF2(r14)
L378C:
	ldl	rr0,%FC2A
	cpl	rr0,%000E(r13)
	jr	lt,L3844
	test	%FFF2(r14)
	jr	eq,L37A6
	push	@r15,%FFF2(r14)
	call	_brelse
	inc	r15,#2
L37A6:
	cp	%0006(r14),#%0001
	jr	ne,L37EC
	test	r12
	jr	ne,L37EC
	push	@r15,#%0080
	push	@r15,r13
	call	_access
	inc	r15,#4
	test	r2
	jr	ne,L383A
	ld	%FC52,r13
	testl	%FFEA(r14)
	jr	eq,L37DE
	ldl	rr2,%FFEA(r14)
	ld	r5,#%0010
	xor	r4,r4
	subl	rr2,rr4
	ldl	%FC2A,rr2
	jr	L37E8
L37DE:
	ldb	rl2,@r13
	xorb	rh2,rh2
	or	r2,#%0042
	ldb	@r13,rl2
L37E8:
	xor	r2,r2
	jr	L3700
L37EC:
	ldb	%FC13,#%02
	jr	L383A
L37F4:
	test	%FFF2(r14)
	jr	eq,L3804
	push	@r15,%FFF2(r14)
	call	_brelse
	inc	r15,#2
L3804:
	push	@r15,#%0001
	ldl	rr2,%FC2A
	ld	r4,#%FFF7
	sdal	rr2,r4
	pushl	@r15,rr2
	push	@r15,r13
	call	_bmap
	inc	r15,#4
	ldl	@r15,rr2
	push	@r15,%0002(r13)
	call	_bread
	inc	r15,#6
	ld	%FFF2(r14),r2
	bit	@r2,#2
	jr	eq,L3856
	push	@r15,r2
	call	_brelse
	inc	r15,#2
L383A:
	push	@r15,r13
	call	_iput
	inc	r15,#2
	jr	L37E8
L3844:
	ld	r3,#%01FF
	exts	rr2
	and	r2,%FC2A
	and	r3,%FC2C
	testl	rr2
	jr	eq,L37F4
L3856:
	push	@r15,#%0010
	push	@r15,#%FC42
	ld	r2,%FFF2(r14)
	inc	r2,#14
	ld	r3,%FC2C
	and	r3,#%01FF
	add	r3,@r2
	push	@r15,r3
	call	_bcopy
	inc	r15,#6
	ldl	rr2,%FC2A
	ld	r5,#%0010
	xor	r4,r4
	addl	rr2,rr4
	ldl	%FC2A,rr2
	test	%FC42
	jr	ne,L389C
	testl	%FFEA(r14)
	jp	ne,L378C
	ldl	%FFEA(r14),rr2
	jp	L378C
L389C:
	clr	%FFF0(r14)
	jr	L38BE
L38A2:
	ld	r2,%FFF0(r14)
	add	r2,#%FC32
	ld	r3,%FFF0(r14)
	add	r3,#%FC44
	ldb	rl0,@r2
	cpb	rl0,@r3
	jp	ne,L378C
	inc	%FFF0(r14),#1
L38BE:
	cp	%FFF0(r14),#%000E
	jr	lt,L38A2
	test	%FFF2(r14)
	jr	eq,L38D6
	push	@r15,%FFF2(r14)
	call	_brelse
	inc	r15,#2
L38D6:
	cp	%0006(r14),#%0002
	jr	ne,L38F6
	test	r12
	jr	ne,L38F6
	push	@r15,#%0080
	push	@r15,r13
	call	_access
	inc	r15,#4
	test	r2
	jr	ne,L383A
	jp	L36FE
L38F6:
	ld	r2,%0002(r13)
	ld	%FFEE(r14),r2
	cp	%FC42,#%0002
	jr	ne,L397E
	cp	%0004(r13),#%0002
	jr	ne,L397E
	ldb	rl2,%FC45
	xorb	rh2,rh2
	cp	r2,#%002E
	jr	ne,L397E
	ld	%FFF0(r14),#%0001
	jr	L3976
L3922:
	ld	r3,%FFF0(r14)
	mult	rr2,#%0006
	ld	r2,r3
	add	r2,#LA080
	test	@r2
	jr	eq,L3972
	ld	r3,%FFF0(r14)
	mult	rr2,#%0006
	ld	r2,r3
	add	r2,#LA07E
	ld	r0,@r2
	cp	r0,%FFEE(r14)
	jr	ne,L3972
	push	@r15,r13
	call	_iput
	inc	r15,#2
	ld	r3,%FFF0(r14)
	mult	rr2,#%0006
	ld	r2,r3
	add	r2,#LA082
	ld	r13,@r2
	incb	%0001(r13),#1
	push	@r15,r13
	call	_plock
	inc	r15,#2
	jp	L374E
L3972:
	inc	%FFF0(r14),#1
L3976:
	cp	%FFF0(r14),#%0005
	jr	lt,L3922
L397E:
	push	@r15,r13
	call	_iput
	inc	r15,#2
	push	@r15,%FC42
	push	@r15,%FFEE(r14)
	call	_iget
	inc	r15,#4
	ld	r13,r2
	test	r13
	jp	ne,L36F2
	jp	L37E8
L39A0:
	call	csv
	ld	r2,%FC40
	inc	%FC40,#1
	ldb	rl3,@r2
	xorb	rh3,rh3
	and	r3,#%00FF
	ld	r2,r3
	jp	cret
end _p_nami
end nami
