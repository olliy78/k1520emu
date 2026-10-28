main module
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
_main	L
G	_p_main	PR
E
_main:
	call	csv
	call	_startup
	clr	LA12C
	clr	LA12E
	ldb	LA11C,#%03
	ldb	rl2,LA11C+%1
	xorb	rh2,rh2
	or	r2,#%0003
	ldb	LA11C+%1,rl2
	ldb	LA120+%1,#%14
	ld	r2,#LA11C
	ld	%FC1E,r2
	clr	%FD56
	call	L35AC
	call	_spl0
	call	_cinit
	call	_binit
	call	_iinit
	push	@r15,#%0002
	push	@r15,_rootdev
	call	_iget
	inc	r15,#4
	ld	L8DC6,r2
	resb	@r2,#0
	push	@r15,#%0002
	push	@r15,_rootdev
	call	_iget
	inc	r15,#4
	ld	%FC2E,r2
	resb	@r2,#0
	clr	%FC30
	call	_newproc
	test	r2
	jr	eq,L073E
	ld	r2,L8B4C
	add	r2,#%00FF
	srl	r2,#8
	inc	r2,#4
	push	@r15,r2
	call	L20F6
	ld	@r15,#%0002
	push	@r15,#%0000
	push	@r15,#%0000
	ld	r2,L8B4C
	add	r2,#%00FF
	srl	r2,#8
	push	@r15,r2
	push	@r15,#%0000
	call	_estabur
	inc	r15,#10
	push	@r15,L8B4C
	push	@r15,#%0000
	push	@r15,#L8B32
	call	_copyout
	inc	r15,#6
	jr	L0742
L073E:
	call	_sched
L0742:
	jp	cret
_iinit:
	call	csv
	push	@r15,#%0001
	push	@r15,_rootdev
	ld	r2,_rootdev
	srl	r2,#8
	sla	r2,#3
	add	r2,#_bdevsw
	ld	r3,@r2
	call	@r3
	inc	r15,#4
	ldk	r3,#1
	exts	rr2
	pushl	@r15,rr2
	push	@r15,_rootdev
	call	_bread
	inc	r15,#6
	ld	r12,r2
	call	_geteblk
	ld	r13,r2
	testb	%FC13
	jr	eq,L0790
	push	@r15,#L88C6
	call	_panic
	inc	r15,#2
L0790:
	push	@r15,#%01B8
	push	@r15,%000E(r13)
	push	@r15,%000E(r12)
	call	_bcopy
	inc	r15,#4
	ld	@r15,r12
	call	_brelse
	inc	r15,#2
	ld	LA080,r13
	ld	r2,_rootdev
	ld	LA07E,r2
	ld	r11,%000E(r13)
	clrb	%019A(r11)
	clrb	%019B(r11)
	clrb	%019D(r11)
	ldl	rr2,%019E(r11)
	ldl	L8DCC,rr2
	jp	cret
_binit:
	call	csv
	dec	r15,#2
	ld	r2,#LA518
	ld	LA520,r2
	ld	LA51E,r2
	ld	LA51C,r2
	ld	LA51A,r2
	xor	r11,r11
	jr	L0840
L07F0:
	ld	r3,r11
	mult	rr2,#%0018
	ld	r13,r3
	add	r13,#L8E9A
	ld	r2,#%FFFF
	ld	%000A(r13),r2
	ld	r2,r11
	sla	r2,#9
	add	r2,#LA628
	ld	%000E(r13),r2
	clrb	%0014(r13)
	ld	r2,#LA518
	ld	%0004(r13),r2
	ld	r2,LA51A
	ld	%0002(r13),r2
	ld	r2,LA51A
	inc	r2,#4
	ld	@r2,r13
	ld	LA51A,r13
	ld	@r13,#%0008
	push	@r15,r13
	call	_brelse
	inc	r15,#2
	inc	r11,#1
L0840:
	cp	r11,#%0014
	jr	lt,L07F0
	ld	r2,#_bdevsw
	ld	%FFF2(r14),r2
	jr	L086C
L0850:
	ld	r2,%FFF2(r14)
	inc	r2,#6
	ld	r12,@r2
	test	r12
	jr	eq,L0864
	ld	%0002(r12),r12
	ld	%0004(r12),r12
L0864:
	inc	L8DD0,#1
	inc	%FFF2(r14),#8
L086C:
	ld	r2,%FFF2(r14)
	ld	r3,@r2
	test	r3
	jr	ne,L0850
	jp	cret
end _p_main
end main
