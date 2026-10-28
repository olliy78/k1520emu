dev0 module
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
_fdopen	L
G
_fdclose	L
G
L7F94	L
G
_fdintr	L
G
L8398	L
G
L83AA	L
G
L83BA	L
G
L83CE	L
G
L83FA	L
G
L83FE	L
G
_fdread	L
G
_fdwrite	L
G
_fdioctl	L
G
L85DE	L
G
L860C	L
G
L861A	L
G
L8628	L
G
L8636	L
G
L8642	L
G
L86EC	L
G	_p_dev0	PR
E
_fdopen:
	call	csv
	jp	L7E90
L7E1E:
	ld	r13,%0004(r14)
	and	r13,#%0007
	ld	r2,r13
	cp	r2,#%0001
	jp	ge,L7E46
	ld	r2,r13
	sla	r2,#4
	ld	r11,r2
	add	r11,#%D566
	ldb	rl2,@r11
	xorb	rh2,rh2
	testb	rl2
	jp	eq,L7E50
L7E46:
	ldb	%FC13,#%06
	jp	L7E8C
L7E50:
	test	%000E(r11)
	jp	ne,L7E72
	ld	r3,r13
	mult	rr2,#%000E
	ld	r2,r3
	ld	r12,r2
	add	r12,#%D558
	ldb	%0003(r12),#%01
	ld	%000E(r11),#%0001
L7E72:
	set	L8E6A,#10
	ldb	@r11,#%01
	xor	r0,r0
	ld	r1,r0
	ldl	%0002(r11),rr0
	ld	%0006(r11),#%7FFF
	clrb	%0001(r11)
L7E8C:
	jp	cret
L7E90:
	jp	L7E1E
_fdclose:
	call	csv
	ld	r13,%0004(r14)
	ld	r12,%0006(r14)
	jp	L7EFC
L7EA4:
	ld	r2,r13
	and	r2,#%0007
	sla	r2,#4
	ld	r11,r2
	add	r11,#%D566
	ld	r2,r12
	cp	r2,#%0002
	jp	eq,L7ECC
	bit	r12,#1
	jp	eq,L7EDC
	testb	%0001(r11)
	jp	eq,L7EDC
L7ECC:
	push	@r15,#%0001
	push	@r15,#%0061
	push	@r15,r13
	call	_fdsub
	inc	r15,#6
L7EDC:
	ld	r2,r13
	and	r2,#%0080
	jp	ne,L7EF6
	push	@r15,#%0000
	push	@r15,#%0051
	push	@r15,r13
	call	_fdsub
	inc	r15,#6
L7EF6:
	clrb	@r11
	jp	cret
L7EFC:
	jp	L7EA4
_fdsub:
	call	csv
	jp	L7F90
L7F08:
	ld	r13,#%D540
	call	_spl5
L7F10:
	bit	@r13,#3
	jp	eq,L7F36
	test	%000C(r13)
	jp	ne,L7F24
	bit	@r13,#1
	jp	ne,L7F36
L7F24:
	set	@r13,#6
	push	@r15,#%0014
	push	@r15,r13
	call	_sleep
	inc	r15,#4
	jp	L7F10
L7F36:
	ld	@r13,#%0009
	call	_spl0
	ld	r2,%0004(r14)
	ld	%000A(r13),r2
	ld	r2,%0008(r14)
	ld	%000C(r13),r2
	ld	r2,%0006(r14)
	ld	%0016(r13),r2
	xor	r0,r0
	ld	r1,r0
	ldl	%0010(r13),rr0
	push	@r15,r13
	call	L7F94
	inc	r15,#2
	test	%0008(r14)
	jp	eq,L7F8C
	push	@r15,r13
	call	_iowait
	inc	r15,#2
	bit	@r13,#6
	jp	eq,L7F84
	push	@r15,r13
	call	_wakeup
	inc	r15,#2
L7F84:
	ld	r2,@r13
	and	r2,#%0004
	ld	@r13,r2
L7F8C:
	jp	cret
L7F90:
	jp	L7F08
L7F94:
	call	csv
	ld	r13,%0004(r14)
	jp	L7FD8
L7FA0:
	clr	%0006(r13)
	call	_spl5
	test	L8E70
	jp	ne,L7FB8
	ld	L8E70,r13
	jp	L7FC0
L7FB8:
	ld	r2,L8E72
	inc	r2,#6
	ld	@r2,r13
L7FC0:
	ld	L8E72,r13
	test	L8E76
	jp	ne,L7FD0
	call	_fdstart
L7FD0:
	call	_spl0
	jp	cret
L7FD8:
	jp	L7FA0
_fdstart:
	call	csv
	jp	L8214
L7FE4:
	ld	r11,L8E70
	test	r11
	jp	eq,L8210
	ld	r2,%000A(r11)
	and	r2,#%0007
	ld	%FFF2(r14),r2
	ld	r2,%FFF2(r14)
	sla	r2,#4
	ld	r13,r2
	add	r13,#%D566
	ld	r3,%FFF2(r14)
	mult	rr2,#%000E
	ld	r2,r3
	ld	r12,r2
	add	r12,#%D558
	ldb	rl2,%0003(r12)
	xorb	rh2,rh2
	ld	%0008(r13),r2
	ldb	rl2,%0002(r12)
	xorb	rh2,rh2
	ld	%000A(r13),r2
	ld	r2,%0008(r12)
	ld	%000C(r13),r2
	clrb	%0001(r13)
	ldb	rl2,%0003(r12)
	xorb	rh2,rh2
	cp	r2,#%00F5
	jp	ne,L8068
	ld	r2,L8E76
	cp	r2,#%0002
	jp	ne,L8068
	ld	%0016(r11),#%0025
	ld	%0008(r12),#%0001
	ld	L8E76,#%0001
	jp	L81D4
L8068:
	ldb	rl2,@r13
	xorb	rh2,rh2
	cp	r2,#%FFFF
	jp	eq,L807C
	test	%0008(r13)
	jp	ne,L8082
L807C:
	set	@r11,#2
	jp	L81FC
L8082:
	cp	r11,#%D540
	jp	ne,L80D6
	ld	r2,%0016(r11)
	cp	r2,#%0006
	jp	eq,L81FC
	ld	r2,%0016(r11)
	cp	r2,#%0051
	jp	ne,L80AC
	ld	L8E76,#%0004
	jp	L80B2
L80AC:
	ld	L8E76,#%0003
L80B2:
	ld	r2,%0016(r11)
	cp	r2,#%0025
	jp	eq,L80CA
	ld	r2,%0016(r11)
	cp	r2,#%0027
	jp	ne,L80D2
L80CA:
	ld	r2,%000C(r11)
	ld	%0008(r12),r2
L80D2:
	jp	L81D4
L80D6:
	ldl	rr2,%0010(r11)
	ld	r5,%0006(r13)
	xor	r4,r4
	cpl	rr2,rr4
	jp	le,L80F2
	set	@r11,#2
	ldb	%0015(r11),#%06
	jp	L81FC
L80F2:
	ldl	rr2,%0010(r11)
	ld	r5,%0006(r13)
	xor	r4,r4
	cpl	rr2,rr4
	jp	ne,L811C
	bit	@r11,#0
	jp	eq,L811C
	ld	r2,%000C(r11)
	ld	%0016(r11),r2
	push	@r15,r11
	call	_clrbuf
	inc	r15,#2
	jp	L81FC
L811C:
	ld	r2,@r11
	and	r2,#%0001
	jp	ne,L8130
	ld	r2,%0012(r11)
	inc	r2,#1
	ld	%0006(r13),r2
L8130:
	ldl	rr2,%0002(r13)
	ldl	%FFEC(r14),rr2
	cpl	rr2,%0010(r11)
	jp	ne,L819A
	ld	r2,@r11
	and	r2,#%0001
	jp	ne,L8166
	test	L8E80
	jp	eq,L815C
	ld	%FFF0(r14),#%0001
	jp	L8162
L815C:
	ld	%FFF0(r14),#%0001
L8162:
	jp	L816A
L8166:
	clr	%FFF0(r14)
L816A:
	ld	L8E76,#%0002
	ld	r2,%000C(r11)
	ld	%0008(r12),r2
	ld	r2,%000E(r11)
	ld	%0006(r12),r2
	ldb	rl2,%0014(r11)
	xorb	rh2,rh2
	and	r2,#%0003
	ld	%0004(r12),r2
	ld	r2,%FFF0(r14)
	ld	%0016(r11),r2
	jp	L81D4
L819A:
	ld	L8E76,#%0001
	ldl	rr0,%FFEC(r14)
	cpl	rr0,%0010(r11)
	jp	ge,L81C2
	ld	%0016(r11),#%0025
	ld	r2,%0012(r11)
	sub	r2,%FFEE(r14)
	ld	%0008(r12),r2
	jp	L81D4
L81C2:
	ld	%0016(r11),#%0027
	ld	r2,%FFEE(r14)
	sub	r2,%0012(r11)
	ld	%0008(r12),r2
L81D4:
	ldb	@r12,#%06
	ld	r2,%FFF2(r14)
	ldb	%0001(r12),rl2
	ld	r2,%0016(r11)
	ldb	%0002(r12),rl2
	clrb	%0003(r12)
	push	@r15,#%0006
	push	@r15,r12
	call	L5CAE
	inc	r15,#4
	jp	L8210
L81FC:
	ld	r2,%0006(r11)
	ld	L8E70,r2
	push	@r15,r11
	call	_iodone
	inc	r15,#2
	jp	L7FE4
L8210:
	jp	cret
L8214:
	dec	r15,#8
	jp	L7FE4
_fdintr:
	call	csv
	jp	L8462
L8222:
	ld	r12,L8E70
	test	r12
	jp	eq,L845E
	ld	r2,%000A(r12)
	and	r2,#%0007
	ld	%FFF2(r14),r2
	ld	r2,%FFF2(r14)
	sla	r2,#4
	ld	r11,r2
	add	r11,#%D566
	ld	r3,%FFF2(r14)
	mult	rr2,#%000E
	ld	r2,r3
	ld	r13,r2
	add	r13,#%D558
	ld	r2,L8E76
	cp	r2,#%0004
	jp	ne,L8270
	ld	L8E76,#%0003
	testb	%0003(r13)
	jp	eq,L845E
L8270:
	ldb	rl2,%0002(r13)
	xorb	rh2,rh2
	ld	%000A(r11),r2
	ldb	rl2,%0003(r13)
	xorb	rh2,rh2
	ld	%0008(r11),r2
	ld	r2,%0008(r13)
	ld	%000C(r11),r2
	ld	r2,@r12
	and	r2,#%0001
	jp	ne,L829C
	ldb	%0001(r11),#%01
L829C:
	ld	r10,L8E76
	clr	L8E76
	ldb	rl2,%0003(r13)
	xorb	rh2,rh2
	cp	r2,#%0001
	jp	eq,L8392
	ldb	rl2,%0003(r13)
	xorb	rh2,rh2
	cp	r2,#%00F5
	jp	ne,L82EE
	push	@r15,r12
	call	_fdadj
	inc	r15,#2
	ld	r2,r10
	cp	r2,#%0002
	jp	ne,L82DE
	ld	L8E76,r10
	call	_fdstart
	jp	L845E
L82DE:
	ldk	r10,#3
	xor	r2,r2
	ld	%000C(r11),r2
	ld	%0008(r13),r2
	jp	L8432
L82EE:
	cp	r12,#LCEAC
	jp	ne,L830A
	bit	@r12,#0
	jp	eq,L830A
	ldb	rl2,%0003(r13)
	xorb	rh2,rh2
	cp	r2,#%00F6
	jp	eq,L8392
L830A:
	ldb	rl2,%0003(r13)
	xorb	rh2,rh2
	cp	r2,#%00C1
	jp	ne,L8326
	ldb	rl2,%0003(r13)
	xorb	rh2,rh2
	cp	r2,#%00F3
	jp	eq,L8356
L8326:
	ld	r2,r10
	cp	r2,#%0002
	jp	ne,L8356
	inc	L8E80,#1
	ld	r2,L8E80
	cp	r2,#%0007
	jp	uge,L8352
	ldl	rr2,%0002(r11)
	addl	rr2,#%00000001
	ldl	%0002(r11),rr2
	jp	L845A
L8352:
	jp	L836A
L8356:
	testb	@r11
	jp	eq,L836A
	cp	r12,#LCEAC
	jp	eq,L836A
	ld	r2,#%FFFF
	ldb	@r11,rl2
L836A:
	ldb	rl2,%0003(r13)
	xorb	rh2,rh2
	and	r2,#%00FF
	push	@r15,r2
	push	@r15,%0008(r11)
	pushl	@r15,%0010(r12)
	push	@r15,%FFF2(r14)
	push	@r15,#L8D8C
	call	_printf
	inc	r15,#12
	set	@r12,#2
	jp	L8432
L8392:
	ld	r2,r10
	jp	L841E
L8398:
	ldl	rr2,%0002(r11)
	addl	rr2,#%00000001
	ldl	%0002(r11),rr2
	jp	L8432
L83AA:
	cp	r12,#%D540
	jp	ne,L83FA
	ld	r2,%0016(r12)
	jp	L83E6
L83BA:
	ldl	rr2,%0002(r11)
	ld	r5,%000C(r12)
	xor	r4,r4
	subl	rr2,rr4
	ldl	%0002(r11),rr2
	jp	L83FA
L83CE:
	ldl	rr2,%0002(r11)
	ld	r5,%000C(r12)
	xor	r4,r4
	addl	rr2,rr4
	ldl	%0002(r11),rr2
	jp	L83FA
	jp	L83FA
L83E6:
	sub	r2,#%0025
	cp	r2,#%0002
	jp	ugt,L83FA
	add	r2,r2
	ld	r1,L8D5E(r2)
	jp	@r1
L83FA:
	jp	L8432
L83FE:
	ldl	rr2,%0010(r12)
	ldl	%0002(r11),rr2
	jp	L845A
L840A:
	push	@r15,#L8DB8
	call	_printf
	inc	r15,#2
	set	@r12,#2
	jp	L8432
	jp	L8432
L841E:
	sub	r2,#%0001
	cp	r2,#%0002
	jp	ugt,L840A
	add	r2,r2
	ld	r1,L8D64(r2)
	jp	@r1
L8432:
	clr	L8E80
	ld	r2,%0006(r12)
	ld	L8E70,r2
	ld	r2,%000C(r12)
	sub	r2,%0008(r13)
	ld	%0016(r12),r2
	push	@r15,r12
	call	_iodone
	inc	r15,#2
	test	L8E70
	jp	eq,L845E
L845A:
	call	_fdstart
L845E:
	jp	cret
L8462:
	dec	r15,#2
	jp	L8222
_fdadj:
	call	csv
	ld	r13,%0004(r14)
	jp	L84E0
L8474:
	ld	r2,%000A(r13)
	and	r2,#%0007
	sla	r2,#4
	ld	r11,r2
	add	r11,#%D566
	cp	r13,#%D540
	jp	ne,L84D4
	ldl	rr0,%0002(r11)
	cpl	rr0,%0010(r13)
	jp	le,L84B6
	ldl	rr2,%0002(r11)
	ld	r5,%0008(r12)
	xor	r4,r4
	subl	rr2,rr4
	ldl	%0002(r11),rr2
	ld	r2,%0004(r11)
	ld	%0006(r11),r2
	jp	L84D0
L84B6:
	ldl	rr2,%0002(r11)
	ld	r5,%0008(r12)
	xor	r4,r4
	addl	rr2,rr4
	ldl	%0002(r11),rr2
	ld	r2,%0004(r11)
	dec	r2,#1
	ld	%0006(r11),r2
L84D0:
	jp	L84DC
L84D4:
	ld	r2,%0012(r13)
	ld	%0006(r11),r2
L84DC:
	jp	cret
L84E0:
	jp	L8474
_fdread:
	call	csv
	jp	L8518
L84EC:
	push	@r15,%0004(r14)
	call	_fdphys
	inc	r15,#2
	testb	%FC13
	jp	ne,L8514
	push	@r15,#%0001
	push	@r15,%0004(r14)
	push	@r15,#LCEAC
	push	@r15,#L7F94
	call	_physio
	inc	r15,#8
L8514:
	jp	cret
L8518:
	jp	L84EC
_fdwrite:
	call	csv
	jp	L8550
L8524:
	push	@r15,%0004(r14)
	call	_fdphys
	inc	r15,#2
	testb	%FC13
	jp	ne,L854C
	push	@r15,#%0000
	push	@r15,%0004(r14)
	push	@r15,#LCEAC
	push	@r15,#L7F94
	call	_physio
	inc	r15,#8
L854C:
	jp	cret
L8550:
	jp	L8524
_fdphys:
	call	csv
	jp	L85AA
L855C:
	ld	r13,%0004(r14)
	and	r13,#%0007
	ld	r2,r13
	cp	r2,#%0001
	jp	lt,L8578
	ldb	%FC13,#%06
	jp	L85A6
L8578:
	ld	r2,r13
	sla	r2,#4
	ld	r12,r2
	add	r12,#%D566
	ldl	rr2,%FC2A
	ld	r4,#%FFF7
	sdal	rr2,r4
	ldl	%FFF0(r14),rr2
	ldl	rr2,%FFF0(r14)
	ldl	%0002(r12),rr2
	ld	r2,%FFF2(r14)
	inc	r2,#1
	ld	%0006(r12),r2
L85A6:
	jp	cret
L85AA:
	dec	r15,#4
	jp	L855C
_fdioctl:
	call	csv
	jp	L8752
L85B8:
	ld	r2,%0004(r14)
	and	r2,#%0007
	ld	%FFF2(r14),r2
	ld	r2,%FFF2(r14)
	sla	r2,#4
	ld	r13,r2
	add	r13,#%D566
	ld	r12,#%D540
	ld	r2,%0006(r14)
	jp	L873A
L85DE:
	push	@r15,#%0004
	ld	r2,r14
	add	r2,#%FFEC
	push	@r15,r2
	push	@r15,%0008(r14)
	call	_copyin
	inc	r15,#6
	test	r2
	jp	eq,L8604
	ldb	%FC13,#%0E
	jp	L874E
L8604:
	ld	r2,%FFEC(r14)
	jp	L8650
L860C:
	ld	r11,%FFEE(r14)
	ld	%FFF0(r14),#%0001
	jp	L8660
L861A:
	ld	r11,%FFEE(r14)
	ld	%FFF0(r14),#%7FFF
	jp	L8660
L8628:
	ldk	r11,#1
	ld	r2,%FFEE(r14)
	ld	%FFF0(r14),r2
	jp	L8660
L8636:
	ldk	r11,#1
	ld	%FFF0(r14),#%0001
	jp	L8660
L8642:
	ldb	%FC13,#%06
	jp	L874E
	jp	L8660
L8650:
	cp	r2,#%0007
	jp	ugt,L8642
	add	r2,r2
	ld	r1,L8D78(r2)
	jp	@r1
L8660:
	test	r11
	jp	mi,L8672
	jp	eq,L8672
	test	%FFF0(r14)
	jp	ne,L867C
L8672:
	ldb	%FC13,#%06
	jp	L874E
L867C:
	dec	r11,#1
	test	r11
	jp	mi,L86E0
	push	@r15,%FFF0(r14)
	ld	r2,%FFEC(r14)
	sla	r2,#1
	add	r2,#L8D6A
	push	@r15,@r2
	push	@r15,%0004(r14)
	call	_fdsub
	inc	r15,#6
	ld	r2,%FFEC(r14)
	cp	r2,#%0003
	jp	eq,L86B8
	ld	r2,%FFEC(r14)
	cp	r2,#%0004
	jp	ne,L86CA
L86B8:
	test	%0016(r12)
	jp	eq,L86CA
	ldb	%FC13,#%05
	jp	L86E0
L86CA:
	bit	@r12,#2
	jp	ne,L86E0
	ld	r2,%0008(r13)
	and	r2,#%00F4
	jp	ne,L86E0
	jp	L867C
L86E0:
	push	@r15,r12
	call	L60A8
	inc	r15,#2
	jp	L874E
L86EC:
	ld	r2,%000A(r13)
	ld	%FFDE(r14),r2
	ld	r2,%0008(r13)
	ld	%FFE0(r14),r2
	ld	r2,%000C(r13)
	ld	%FFE2(r14),r2
	ld	%FFDC(r14),#%0004
	push	@r15,#%0010
	push	@r15,%0008(r14)
	ld	r2,r14
	add	r2,#%FFDC
	push	@r15,r2
	call	_copyout
	inc	r15,#6
	test	r2
	jp	eq,L872C
	ldb	%FC13,#%0E
L872C:
	jp	L874E
L8730:
	ldb	%FC13,#%06
	jp	L874E
L873A:
	sub	r2,#%6D01
	cp	r2,#%0001
	jp	ugt,L8730
	add	r2,r2
	ld	r1,L8D88(r2)
	jp	@r1
L874E:
	jp	cret
L8752:
	sub	r15,#%0018
	jp	L85B8
end _p_dev0
end dev0
