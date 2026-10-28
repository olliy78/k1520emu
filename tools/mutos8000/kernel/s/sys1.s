sys1 module
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
_exece	L
G
_rexit	L
G
_exit	L
G
_wait	L
G
_fork	L
G
_sbreak	L
G	_p_sys1	PR
E
_exece:
	call	csv
	dec	r15,#16
	push	@r15,#%0000
	push	@r15,#_uchar
	call	_namei
	inc	r15,#4
	ld	%FFE4(r14),r2
	test	r2
	jp	eq,L44B2
	push	@r15,#%0040
	push	@r15,%FFE4(r14)
	call	_access
	inc	r15,#4
	test	r2
	jr	ne,L41FE
	ld	r2,%FFE4(r14)
	inc	r2,#6
	ld	r3,@r2
	and	r3,#%F000
	ld	r2,r3
	cp	r3,#%8000
	jr	ne,L41FE
	ld	r2,%FFE4(r14)
	inc	r2,#6
	ld	r3,@r2
	and	r3,#%0049
	jr	ne,L4208
L41FE:
	ldb	%FC13,#%0D
	jp	L44A8
L4208:
	clr	%FFEC(r14)
	xor	r11,r11
	clr	%FFF2(r14)
	clr	%FFF0(r14)
	xor	r13,r13
	ld	r10,%FC20
	push	@r15,#%000A
	push	@r15,#LA0DC
	call	_malloc
	inc	r15,#4
	ld	%FFEC(r14),r2
	test	r2
	jr	ne,L423C
	push	@r15,#L8C58
	call	_panic
	inc	r15,#2
L423C:
	test	%0002(r10)
	jr	eq,L4324
L4242:
	clr	%FFE8(r14)
	test	%0002(r10)
	jr	eq,L425E
	push	@r15,%0002(r10)
	call	_fuword
	inc	r15,#2
	ld	%FFE8(r14),r2
	inc	%0002(r10),#2
L425E:
	test	%FFE8(r14)
	jr	ne,L4288
	test	%0004(r10)
	jr	eq,L4288
	clr	%0002(r10)
	push	@r15,%0004(r10)
	call	_fuword
	inc	r15,#2
	ld	%FFE8(r14),r2
	test	r2
	jr	eq,L4324
	inc	%0004(r10),#2
	inc	%FFF0(r14),#1
L4288:
	test	%FFE8(r14)
	jr	eq,L4324
	inc	%FFF2(r14),#1
	ld	r0,%FFE8(r14)
	cp	r0,#%FFFF
	jr	ne,L42A2
	ldb	%FC13,#%0E
L42A2:
	cp	r13,#%13FF
	jr	lt,L42AE
	ldb	%FC13,#%07
L42AE:
	ld	r2,%FFE8(r14)
	inc	%FFE8(r14),#1
	push	@r15,r2
	call	_fubyte
	inc	r15,#2
	ld	%FFE6(r14),r2
	test	r2
	jr	pl,L42CC
	ldb	%FC13,#%0E
L42CC:
	testb	%FC13
	jp	ne,L444E
	ld	r2,r13
	and	r2,#%01FF
	jr	ne,L430E
	test	r11
	jr	eq,L42E8
	push	@r15,r11
	call	_bdwrite
	inc	r15,#2
L42E8:
	ld	r3,%FFEC(r14)
	exts	rr2
	ld	r5,r13
	sra	r5,#9
	exts	rr4
	addl	rr2,rr4
	addl	rr2,_swplo
	pushl	@r15,rr2
	push	@r15,_swapdev
	call	_getblk
	inc	r15,#6
	ld	r11,r2
	ld	r12,%000E(r11)
L430E:
	inc	r13,#1
	ld	r2,r12
	inc	r12,#1
	ld	r3,%FFE6(r14)
	ldb	@r2,rl3
	test	%FFE6(r14)
	jr	eq,L4242
	jr	pl,L42A2
	jr	L4242
L4324:
	test	r11
	jr	eq,L4330
	push	@r15,r11
	call	_bdwrite
	inc	r15,#2
L4330:
	xor	r11,r11
	inc	r13,#1
	res	r13,#0
	ld	r2,%FFF2(r14)
	inc	r2,#3
	sla	r2,#1
	ld	%FFEE(r14),r2
	push	@r15,r2
	push	@r15,r13
	push	@r15,%FFE4(r14)
	call	L44B6
	inc	r15,#6
	test	r2
	jr	ne,L444E
	testb	%FC13
	jr	ne,L444E
	ld	r2,r13
	neg	r2
	dec	r2,#2
	ld	%FFEA(r14),r2
	sub	r2,%FFEE(r14)
	ld	%FFE8(r14),r2
	ld	r2,%FD04
	inc	r2,#16
	ld	r3,%FFE8(r14)
	ld	@r2,r3
	ld	r2,%FFF2(r14)
	sub	r2,%FFF0(r14)
	push	@r15,r2
	push	@r15,%FFE8(r14)
	call	_suword
	inc	r15,#4
	xor	r13,r13
	jr	L4408
L4392:
	push	@r15,%FFEA(r14)
	push	@r15,%FFE8(r14)
	call	_suword
	inc	r15,#4
L43A0:
	ld	r2,r13
	and	r2,#%01FF
	jr	ne,L43DE
	test	r11
	jr	eq,L43B4
	push	@r15,r11
	call	_brelse
	inc	r15,#2
L43B4:
	ld	r3,%FFEC(r14)
	exts	rr2
	ld	r5,r13
	sra	r5,#9
	exts	rr4
	addl	rr2,rr4
	addl	rr2,_swplo
	pushl	@r15,rr2
	push	@r15,_swapdev
	call	_bread
	inc	r15,#6
	ld	r11,r2
	set	@r11,#7
	res	@r11,#9
	ld	r12,%000E(r11)
L43DE:
	ld	r2,r12
	inc	r12,#1
	ldb	rl3,@r2
	xorb	rh3,rh3
	ld	%FFE6(r14),r3
	push	@r15,r3
	ld	r2,%FFEA(r14)
	inc	%FFEA(r14),#1
	push	@r15,r2
	call	_subyte
	inc	r15,#4
	inc	r13,#1
	ld	r2,%FFE6(r14)
	and	r2,#%00FF
	jr	ne,L43A0
L4408:
	inc	%FFE8(r14),#2
	ld	r0,%FFF2(r14)
	cp	r0,%FFF0(r14)
	jr	ne,L4428
	push	@r15,#%0000
	push	@r15,%FFE8(r14)
	call	_suword
	inc	r15,#4
	inc	%FFE8(r14),#2
L4428:
	dec	%FFF2(r14),#1
	jr	pl,L4392
	push	@r15,#%0000
	push	@r15,%FFE8(r14)
	call	_suword
	inc	r15,#2
	ld	@r15,#%0000
	push	@r15,%FFEA(r14)
	call	_suword
	inc	r15,#4
	call	_setregs
L444E:
	test	r11
	jr	eq,L445A
	push	@r15,r11
	call	_brelse
	inc	r15,#2
L445A:
	testb	%FC13
	jr	eq,L4496
	xor	r13,r13
	jr	L4490
L4464:
	ld	r3,%FFEC(r14)
	exts	rr2
	ld	r1,r13
	exts	rr0
	addl	rr2,rr0
	addl	rr2,_swplo
	pushl	@r15,rr2
	push	@r15,_swapdev
	call	_getblk
	inc	r15,#6
	ld	r11,r2
	set	@r11,#7
	res	@r11,#9
	push	@r15,r11
	call	_brelse
	inc	r15,#2
	inc	r13,#1
L4490:
	cp	r13,#%000A
	jr	lt,L4464
L4496:
	push	@r15,%FFEC(r14)
	push	@r15,#%000A
	push	@r15,#LA0DC
	call	_mfree
	inc	r15,#6
L44A8:
	push	@r15,%FFE4(r14)
	call	_iput
	inc	r15,#2
L44B2:
	jp	cret
L44B6:
	call	csv
	ld	r13,%0004(r14)
	dec	r15,#6
	ld	r2,#%FD18
	ld	%FC26,r2
	ld	%FC28,#%0002
	xor	r0,r0
	ld	r1,r0
	ldl	%FC2A,rr0
	ldb	%FC12,#%01
	push	@r15,r13
	call	_readi
	inc	r15,#2
	testb	%FC13
	jp	ne,L47DA
	bit	%FD18,#8
	jr	eq,L44FA
	ld	%FC28,#%0028
	jr	L4500
L44FA:
	ld	%FC28,#%0010
L4500:
	xor	r0,r0
	ld	r1,r0
	ldl	%FC2A,rr0
	ldb	%FC12,#%01
	ld	r2,#%FD18
	ld	%FC26,r2
	push	@r15,r13
	call	_readi
	inc	r15,#2
	clrb	%FC12
	testb	%FC13
	jp	ne,L47DA
	test	%FC28
	jr	eq,L453A
L4530:
	ldb	%FC13,#%08
	jp	L47DA
L453A:
	xor	r11,r11
	clr	%FFF2(r14)
	ld	r0,%FD18
	cp	r0,#%E807
	jr	ne,L45A0
	ld	r3,%FD1C
	xor	r2,r2
	ld	r5,%FD1A
	xor	r4,r4
	addl	rr2,rr4
	ldl	%FFEE(r14),rr2
	ld	r2,%FFF0(r14)
	ld	%FD1C,r2
	ldl	rr2,%FFEE(r14)
	ld	r5,%FD1C
	cpl	rr2,rr4
	jr	eq,L457A
L4570:
	ldb	%FC13,#%0C
	jp	L47DA
L457A:
	clr	%FD1A
	jr	L45D4
L4580:
	ldb	rl2,@r13
	xorb	rh2,rh2
	and	r2,#%0020
	jr	ne,L45E0
	ldb	rl2,%0001(r13)
	xorb	rh2,rh2
	cp	r2,#%0001
	jr	eq,L45E0
L4596:
	ldb	%FC13,#%1A
	jp	L47DA
L45A0:
	ld	r0,%FD18
	cp	r0,#%E707
	jr	ne,L4530
	ld	r3,%FD36
	xor	r2,r2
	ld	r5,%FD34
	xor	r4,r4
	addl	rr2,rr4
	ldl	%FFEE(r14),rr2
	ld	r2,%FFF0(r14)
	ld	%FD36,r2
	ldl	rr2,%FFEE(r14)
	ld	r5,%FD36
	cpl	rr2,rr4
	jr	ne,L4570
	clr	%FD34
L45D4:
	bit	%FD18,#8
	jr	eq,L45FC
	test	%FD34
	jr	ne,L4580
L45E0:
	test	%FD1A
	jr	eq,L45FC
	ldb	rl2,@r13
	xorb	rh2,rh2
	and	r2,#%0020
	jr	ne,L45FC
	ldb	rl2,%0001(r13)
	xorb	rh2,rh2
	cp	r2,#%0001
	jr	ne,L4596
L45FC:
	bit	%FD18,#8
	jr	eq,L461A
	ld	r10,%FD34
	add	r10,#%00FF
	srl	r10,#8
	ld	r3,%FD36
	xor	r2,r2
	ld	r5,%FD38
	jr	L4630
L461A:
	ld	r10,%FD1A
	add	r10,#%00FF
	srl	r10,#8
	ld	r3,%FD1C
	xor	r2,r2
	ld	r5,%FD1E
L4630:
	xor	r4,r4
	addl	rr2,rr4
	ldl	%FFEE(r14),rr2
	ld	r5,%FFF0(r14)
	cpl	rr2,rr4
	jr	ne,L4570
	ld	r12,r5
	add	r12,#%00FF
	srl	r12,#8
	ld	r9,%0006(r14)
	add	r9,%0008(r14)
	add	r9,#%00FF
	srl	r9,#8
	inc	r9,#5
	test	%FFF2(r14)
	jr	eq,L46D6
	testb	%FD13
	jr	ne,L468C
	ld	r2,r10
	add	r2,#%00FF
	ld	r3,#%FFF8
	sdl	r2,r3
	ld	r3,%FCA8
	add	r3,#%00FF
	ld	r4,#%FFF8
	sdl	r3,r4
	cp	r2,r3
	jp	ne,L4570
L468C:
	test	%0006(r14)
	jp	ne,L4570
	ld	r12,%FCAA
	ld	r9,%FCAC
	ldb	rl2,%FD13
	xorb	rh2,rh2
	ld	r11,r2
	call	_xfree
	push	@r15,r13
	call	L5790
	inc	r15,#2
	bit	%FD18,#8
	jr	eq,L46C8
	ld	r2,%FD04
	add	r2,#%0018
	ld	r3,%FD28
L46C2:
	res	r3,#0
	ld	@r2,r3
	jr	L47B6
L46C8:
	ld	r2,%FD04
	add	r2,#%0018
	ld	r3,%FD22
	jr	L46C2
L46D6:
	push	@r15,#%0002
	push	@r15,r11
	push	@r15,r9
	push	@r15,r12
	push	@r15,r10
	call	_estabur
	inc	r15,#10
	test	r2
	jr	ne,L47DA
	clr	%FD10
	call	_xfree
	ld	r8,#%0100
	jr	L470C
L46FA:
	ld	r2,%FC1E
	inc	r2,#16
	ld	r3,@r2
	add	r3,r8
	push	@r15,r3
	call	L05C6
	inc	r15,#2
L470C:
	dec	r8,#1
	jr	pl,L46FA
	push	@r15,r13
	call	L5790
	ld	@r15,#%0002
	push	@r15,#%0000
	push	@r15,#%0000
	push	@r15,r12
	push	@r15,#%0000
	call	_estabur
	inc	r15,#10
	clr	%FC26
	bit	%FD18,#8
	jr	eq,L474C
	ld	r3,%FD34
	add	r3,#%0028
	xor	r2,r2
	ldl	%FC2A,rr2
	ld	r2,%FD36
	jr	L475C
L474C:
	ld	r3,%FD1A
	inc	r3,#16
	xor	r2,r2
	ldl	%FC2A,rr2
	ld	r2,%FD1C
L475C:
	ld	%FC28,r2
	push	@r15,r13
	call	_readi
	inc	r15,#2
	ld	r2,%FC1E
	inc	r2,#1
	ldb	rl3,@r2
	xorb	rh3,rh3
	and	r3,#%0010
	jr	ne,L47A8
	bit	%0006(r13),#11
	jr	eq,L4798
	test	%FC16
	jr	eq,L4798
	ld	r2,%000A(r13)
	ld	%FC16,r2
	ld	r2,%FC1E
	inc	r2,#8
	ld	r3,%000A(r13)
	ld	@r2,r3
L4798:
	bit	%0006(r13),#10
	jr	eq,L47B6
	ld	r2,%000C(r13)
	ld	%FC18,r2
	jr	L47B6
L47A8:
	push	@r15,#%0005
	push	@r15,%FC1E
	call	_psignal
	inc	r15,#4
L47B6:
	ld	%FCA8,r10
	ld	%FCAA,r12
	ld	%FCAC,r9
	ld	r2,r11
	ldb	%FD13,rl2
	push	@r15,#%0002
	push	@r15,r11
	push	@r15,r9
	push	@r15,r12
	push	@r15,r10
	call	_estabur
	inc	r15,#10
L47DA:
	ld	r2,%FFF2(r14)
	jp	cret
_setregs:
	call	csv
	ld	r13,#%FCD2
	jr	L47F8
L47EC:
	ld	r2,@r13
	and	r2,#%0001
	jr	ne,L47F6
	clr	@r13
L47F6:
	inc	r13,#2
L47F8:
	cp	r13,#%FCF4
	jr	ult,L47EC
	ld	r12,#L88CC
	jr	L4818
L4804:
	ld	r2,r12
	inc	r12,#1
	ldb	rl3,@r2
	xorb	rh3,rh3
	sla	r3,#1
	ld	r2,r3
	add	r2,%FD04
	clr	@r2
L4818:
	cp	r12,#L88D4
	jr	ult,L4804
	clr	%FC22
	clr	%FC24
	bit	%FD18,#8
	jr	eq,L483A
	ld	r2,%FD04
	add	r2,#%0018
	ld	r3,%FD28
	jr	L4846
L483A:
	ld	r2,%FD04
	add	r2,#%0018
	ld	r3,%FD22
L4846:
	res	r3,#0
	ld	@r2,r3
	ld	r13,#%FD5C
	jr	L4856
L4850:
	ld	r2,r13
	inc	r13,#2
	clr	@r2
L4856:
	cp	r13,#%FDEC
	jr	ult,L4850
	xor	r11,r11
	jr	L488A
L4860:
	bitb	%FC8A(r11),#0
	jr	eq,L4888
	ld	r2,r11
	sla	r2,#1
	add	r2,#%FC62
	push	@r15,@r2
	call	_closef
	inc	r15,#2
	ld	r2,r11
	sla	r2,#1
	add	r2,#%FC62
	clr	@r2
	resb	%FC8A(r11),#0
L4888:
	inc	r11,#1
L488A:
	cp	r11,#%0014
	jr	lt,L4860
	push	@r15,#%000E
	push	@r15,#%FD40
	push	@r15,#%FC32
	call	_bcopy
	inc	r15,#6
	jp	cret
_rexit:
	call	csv
	ld	r13,%FC20
	ld	r2,@r13
	and	r2,#%00FF
	sla	r2,#8
	push	@r15,r2
	call	_exit
	inc	r15,#2
	jp	cret
_exit:
	call	csv
	ld	r12,%FC1E
	ldb	rl2,%0001(r12)
	xorb	rh2,rh2
	and	r2,#%FFAF
	ldb	%0001(r12),rl2
	clr	%001A(r12)
	xor	r13,r13
	jr	L48F2
L48E2:
	ld	r2,r13
	sla	r2,#1
	add	r2,#%FCD2
	ld	@r2,#%0001
	inc	r13,#1
L48F2:
	cp	r13,#%0011
	jr	lt,L48E2
	xor	r13,r13
	jr	L491E
L48FC:
	ld	r2,r13
	sla	r2,#1
	add	r2,#%FC62
	ld	r10,@r2
	ld	r2,r13
	sla	r2,#1
	add	r2,#%FC62
	clr	@r2
	push	@r15,r10
	call	_closef
	inc	r15,#2
	inc	r13,#1
L491E:
	cp	r13,#%0014
	jr	lt,L48FC
	push	@r15,%FC2E
	call	_plock
	inc	r15,#2
	push	@r15,%FC2E
	call	_iput
	inc	r15,#2
	test	%FC30
	jr	eq,L4952
	push	@r15,%FC30
	call	_plock
	inc	r15,#2
	push	@r15,%FC30
	call	_iput
	inc	r15,#2
L4952:
	call	_xfree
	push	@r15,%0010(r12)
	push	@r15,%0012(r12)
	push	@r15,#LA09C
	call	_mfree
	inc	r15,#6
	ldb	@r12,#%05
	ld	r2,%0004(r14)
	ld	%0010(r12),r2
	ldl	rr2,%FCFC
	addl	rr2,%FCF4
	ldl	%0012(r12),rr2
	ldl	rr2,%FD00
	addl	rr2,%FCF8
	ldl	%0016(r12),rr2
	xor	r13,r13
	ld	r11,#LA11C
	jr	L49DA
L4994:
	ld	r0,%000E(r11)
	cp	r0,%000C(r12)
	jr	ne,L49C4
	ld	%000E(r11),#%0001
	ldb	rl2,@r11
	xorb	rh2,rh2
	cp	r2,#%0005
	jr	ne,L49B2
	inc	r13,#1
	jr	L49D6
L49B2:
	ldb	rl2,@r11
	xorb	rh2,rh2
	cp	r2,#%0006
	jr	ne,L49D6
	push	@r15,r11
	call	_setrun
	jr	L49D4
L49C4:
	ld	r0,%000C(r11)
	cp	r0,%000E(r12)
	jr	ne,L49D6
	push	@r15,r11
	call	_wakeup
L49D4:
	inc	r15,#2
L49D6:
	add	r11,#%001C
L49DA:
	cp	r11,L8A8C
	jr	ule,L4994
	test	r13
	jr	eq,L49EE
	push	@r15,#LA138
	call	_wakeup
	inc	r15,#2
L49EE:
	call	_swtch
	jp	cret
_wait:
	call	csv
	xor	r13,r13
L49FC:
	ld	r12,#LA11C
	jr	L4AB0
L4A02:
	ld	r2,%FC1E
	inc	r2,#12
	ld	r0,@r2
	cp	r0,%000E(r12)
	jr	ne,L4AAC
	inc	r13,#1
	ldb	rl2,@r12
	xorb	rh2,rh2
	cp	r2,#%0005
	jr	ne,L4A76
	ld	r2,%000C(r12)
	ld	%FC22,r2
	ld	r2,%0010(r12)
	ld	%FC24,r2
	ldl	rr2,%FCFC
	addl	rr2,%0012(r12)
	ldl	%FCFC,rr2
	ldl	rr2,%FD00
	addl	rr2,%0016(r12)
	ldl	%FD00,rr2
	clr	%000C(r12)
	clr	%000E(r12)
	clr	%000A(r12)
	clr	%0006(r12)
	clrb	%0001(r12)
	clr	%0014(r12)
	clrb	@r12
	cp	r12,L8A8C
	jr	ne,L4AD0
L4A64:
	ld	r2,L8A8C
	sub	r2,#%001C
	ld	L8A8C,r2
	testb	@r2
	jr	ne,L4AD0
	jr	L4A64
L4A76:
	ldb	rl2,@r12
	xorb	rh2,rh2
	cp	r2,#%0006
	jr	ne,L4AAC
	ldb	rl2,%0001(r12)
	and	r2,#%0020
	jr	ne,L4AAC
	setb	%0001(r12),#5
	ld	r2,%000C(r12)
	ld	%FC22,r2
	push	@r15,r12
	call	_fsig
	inc	r15,#2
	sla	r2,#8
	or	r2,#%007F
	ld	%FC24,r2
	jr	L4AD0
L4AAC:
	add	r12,#%001C
L4AB0:
	cp	r12,L8A8C
	jr	ule,L4A02
	test	r13
	jr	eq,L4ACA
	push	@r15,#%001E
	push	@r15,%FC1E
	call	_sleep
	inc	r15,#4
	jr	L49FC
L4ACA:
	ldb	%FC13,#%0A
L4AD0:
	jp	cret
_fork:
	call	csv
	ld	r10,%FCAA
	inc	r10,#1
	srl	r10,#1
	inc	r10,#2
	ld	r2,r10
	ld	r3,#%0100
	ld	r4,%FD04
	inc	r4,#16
	ld	r5,@r4
	srl	r5,#8
	sub	r3,r5
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
	jr	ne,L4B1C
	ldb	%FC13,#%0C
	jr	L4BC2
L4B1C:
	push	@r15,r11
	push	@r15,r10
	push	@r15,#LA0DC
	call	_mfree
	inc	r15,#6
	xor	r11,r11
	xor	r12,r12
	ld	r13,#LA11C
	jr	L4B54
L4B34:
	testb	@r13
	jr	ne,L4B40
	test	r12
	jr	ne,L4B40
	ld	r12,r13
	jr	L4B50
L4B40:
	ld	r0,%0008(r13)
	cp	r0,%FC16
	jr	ne,L4B50
	testb	@r13
	jr	eq,L4B50
	inc	r11,#1
L4B50:
	add	r13,#%001C
L4B54:
	cp	r13,#LA464
	jr	ult,L4B34
	test	r12
	jr	eq,L4B70
	test	%FC16
	jr	eq,L4B78
	cp	r12,#LA448
	jr	eq,L4B70
	cp	r11,#%000F
	jr	le,L4B78
L4B70:
	ldb	%FC13,#%0B
	jr	L4BC2
L4B78:
	cp	r12,L8A8C
	jr	ule,L4B82
	ld	L8A8C,r12
L4B82:
	ld	r13,%FC1E
	call	_newproc
	test	r2
	jr	eq,L4BBA
	ld	r2,%000C(r13)
	ld	%FC22,r2
	ldl	rr2,L8DCC
	ldl	%FD4E,rr2
	xor	r0,r0
	ld	r1,r0
	ldl	%FD00,rr0
	ld	r1,r0
	ldl	%FCF8,rr0
	ld	r1,r0
	ldl	%FCFC,rr0
	ld	r1,r0
	ldl	%FCF4,rr0
	jr	L4BCC
L4BBA:
	ld	r2,%000C(r12)
	ld	%FC22,r2
L4BC2:
	ld	r2,%FD04
	add	r2,#%0018
	inc	@r2,#2
L4BCC:
	jp	cret
_sbreak:
	call	csv
	dec	r15,#2
	ld	r2,%FC20
	ld	r12,@r2
	add	r12,#%00FF
	srl	r12,#8
	ld	r11,r12
	sub	r11,%FCAA
	jr	mi,L4C0C
	jr	eq,L4C0C
	ld	r2,%FC1E
	inc	r2,#16
	ld	r13,@r2
	add	r13,r12
	jr	L4C04
L4BFA:
	dec	r13,#1
	push	@r15,r13
	call	L05C6
	inc	r15,#2
L4C04:
	ld	r2,r11
	dec	r11,#1
	test	r2
	jr	ne,L4BFA
L4C0C:
	ld	%FCAA,r12
	jp	cret
end _p_sys1
end sys1
