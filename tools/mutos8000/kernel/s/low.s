low module
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
L03F8	L
G
L03FE	L
G
L040C	L
G
L040E	L
G
_addupc	L
G
_splx	L
G
_spl7	L
G
_spl6	L
G
_spl5	L
G
_spl4	L
G
_spl1	L
G
_spl0	L
G
_fubyte	L
G
_fuword	L
G
_subyte	L
G
_suword	L
G
_copyin	L
G
_copyout	L
G
_copyseg	L
G
L05C6	L
G
_idle	L
G
csv	L
G
cret	L
G
L0614	L
G
L0620	L
G
L065E	L
G	_p_low	PR
E
! PSA-Eintrag Reset: reserviert, FCW, Segment, Handler !
L0000:
	WV %0000;WV %C000;WV %0000
L0006:
	WV start;WV %0000;WV %4004;WV %0000
	WV _trapent;WV %0000;WV %4008;WV %0000
	WV _trapent;WV %0000;WV %4018;WV %0000
	WV _trapent;WV %0000;WV %4024;WV %0000
	WV _trapent;WV %0000;WV %4028;WV %0000
	WV _trapent;WV %0000;WV %400C;WV %0000
	WV _trapent;WV %0000;WV %4000;WV %007B
	WV L023E;WV %0060;WV L0246;WV %000F
	WV L024E;WV %000F;WV L024E;WV %000F
	WV L024E;WV %0050;WV L025E;WV %000F
	WV L024E;WV %000F;WV L024E;WV %000F
	WV L024E;WV %0040;WV L0256;WV %0041
	WV L0256;WV %0042;WV L0256;WV %0043
	WV L0256;WV %0050;WV L0276;WV %000F
	WV L024E;WV %000F;WV L024E;WV %000F
	WV L024E;WV %0040;WV L0266;WV %0040
	WV L026E;WV %0041;WV L0266;WV %0041
	WV L026E;WV %0042;WV L0266;WV %0042
	WV L026E;WV %0043;WV L0266;WV %0043
	WV L026E;WV %000F;WV L024E;WV %000F
	WV L024E;WV %000F;WV L024E;WV %000F
	WV L024E;WV %000F;WV L024E;WV %000F
	WV L024E;WV %000F;WV L024E;WV %000F
	WV L024E;WV %000F;WV L024E;WV %000F
	WV L024E;WV %000F;WV L024E;WV %000F
	WV L024E;WV %000F;WV L024E;WV %000F
	WV L024E;WV %000F;WV L024E;WV %000F
	WV L024E;WV %000F;WV L024E;WV %000F
	WV L024E;WV %000F;WV L024E;WV %000F
	WV L024E;WV %000F;WV L024E;WV %000F
	WV L024E;WV %000F;WV L024E;WV %000F
	WV L024E;WV %000F;WV L024E;WV %000F
	WV L024E;WV %000F;WV L024E;WV %000F
	WV L024E;WV %000F;WV L024E;WV %000F
	WV L024E;WV %000F;WV L024E;WV %000F
	WV L024E;WV %000F;WV L024E;WV %000F
	WV L024E;WV %000F;WV L024E;WV %000F
	WV L024E;WV %000F;WV L024E;WV %000F
	WV L024E;WV %000F;WV L024E;WV %000F
	WV L024E;WV %000F;WV L024E;WV %000F
	WV L024E;WV %000F;WV L024E;WV %000F
	WV L024E;WV %000F;WV L024E;WV %000F
	WV L024E;WV %000F;WV L024E;WV %000F
	WV L024E;WV %000F;WV L024E;WV %000F
	WV L024E;WV %000F;WV L024E;WV %000F
	WV L024E;WV %000F;WV L024E;WV %000F
	WV L024E;WV %000F;WV L024E;WV %000F
	WV L024E;WV %000F;WV L024E;WV %000F
	WV L024E;WV %000F;WV L024E;WV %000F
	WV L024E;WV %000F;WV L024E;WV %000F
	WV L024E;WV %000F;WV L024E;WV %000F
	WV L024E;WV %000F;WV L024E;WV %000F
	WV L024E;WV %000F;WV L024E;WV %000F
	WV L024E;WV %000F;WV L024E;WV %000F
	WV L024E;WV %000F;WV L024E;WV %000F
	WV L024E;WV %000F;WV L024E;WV %000F
	WV L024E;WV %000F;WV L024E;WV %000F
	WV L024E;WV %000F;WV L024E;WV %000F
	WV L024E;WV %000F;WV L024E;WV %000F
	WV L024E;WV %000F;WV L024E;WV %000F
	WV L024E;WV %000F;WV L024E;WV %000F
	WV L024E;WV %000F;WV L024E;WV %000F
	WV L024E;WV %000F;WV L024E;WV %000F
	WV L024E;WV %000F;WV L024E;WV %000F
	WV L024E;WV %000F;WV L024E;WV %000F
	WV L024E;WV %000F;WV L024E;WV %000F
	WV L024E;WV %000F;WV L024E;WV %000F
	WV L024E;WV %000F;WV L024E;WV %000F
	WV L024E;WV %000F;WV L024E;WV %000F
	WV L024E;WV %000F;WV L024E;WV %000F
	WV L024E;WV %000F;WV L024E
	WV %888A
L023E:
	push	@r15,#_trap
	jp	L033A
L0246:
	push	@r15,#_clock
	jp	L033A
L024E:
	push	@r15,#_trap
	jp	L033A
L0256:
	push	@r15,#_lpintr
	jp	L033A
L025E:
	push	@r15,#_rkintr
	jp	L033A
L0266:
	push	@r15,#_tcrint
	jp	L033A
L026E:
	push	@r15,#_tcxint
	jp	L033A
L0276:
	push	@r15,#_fdintr
	jp	L033A
start:
	ld	r0,#%0000
	ldctl	psapseg,r0
	ld	r0,#%0000
	ldctl	psapoff,r0
	ld	r12,#%0000
	ld	r13,#%0000
	ldl	rr14,rr12
	ld	r0,#%4000
	ldctl	fcw,r0
	ld	r0,#%8800
	ldctl	refresh,r0
	ld	r1,#%0036
	ld	r0,#%03F2
	ex	r0,@r1
	ei	nvi
	pushl	@r15,rr0
	popl	rr0,@r15
	pushl	@r15,rr0
	popl	rr0,@r15
	nop
	di	nvi
	ex	r0,@r1
	ld	r2,#%EC00
	clr	r1
	clr	L0006
L02C4:
	ld	@r2,r1
	inc	r2,#2
	cp	r2,#%0000
	jr	ne,L02C4
	ld	r2,#L8DC0
L02D2:
	ld	@r2,r1
	inc	r2,#2
	cp	r2,#%D586
	jr	ne,L02D2
	ld	r0,#%00C7
	ld	L8DC2,r0
	out	%0080,r0
	mset
	ld	r0,#%0000
	ld	%D576,r0
	call	_main
	ld	r0,L8DC0
	ld	r4,#%0000
	ld	r5,#L875E
	ld	%0004(r5),r0
	ld	r0,#%C000
	ldctl	fcw,r0
	ldps	@r4
_trapent:
	push	@r15,#_trap
	pushl	@r15,rr6
	ld	r7,%0006(r15)
	ldctl	r6,fcw
	test	%D578
	jr	ne,L0328
	rr	r6,#2
	and	r6,#%000F
	jr	L034C
L0328:
	ld	r6,%D576
	ld	r7,%D578
	ldl	%000A(r15),rr6
	popl	rr6,@r15
	inc	r15,#2
	jr	L03F2
L033A:
	pushl	@r15,rr6
	ld	r7,%0006(r15)
	ld	r6,r7
	rl	r6,#1
	and	r6,#%01FC
	ld	r6,%003C(r6)
L034C:
	dec	r15,#12
	ldm	@r15,r0,#6
	ld	r0,%0014(r15)
	and	r0,#%4000
	jr	eq,L0362
	ld	r0,#%0000
	jr	L0366
L0362:
	ld	r0,L8DC0
L0366:
	ld	r1,%0016(r15)
	cp	r0,r1
	jr	eq,L0374
	nop
	ld	%0016(r15),r0
L0374:
	ld	r0,L8DC2
	ld	%0012(r15),r0
	ldctl	r4,nsp
	ex	r4,%0010(r15)
	ld	r5,r6
	rrdb	rh5,rl5
	xorb	rh5,rh5
	push	@r15,r5
	calr	_splx
	inc	r15,#2
	and	r6,#%000F
	pushl	@r15,rr6
	call	@r4
	bit	%0018(r15),#14
	jr	ne,L03B8
	test	L8DC4
	jr	eq,L03AA
	ld	@r15,#%000C
	call	_trap
L03AA:
	ld	r0,%0014(r15)
	ldctl	nsp,r0
	ld	r0,L8DC0
	ld	%001A(r15),r0
L03B8:
	inc	r15,#4
	ld	r0,%0012(r15)
	ld	L8DC2,r0
	out	%0080,r0
	ldm	r0,@r15,#8
	add	r15,#%0012
	bit	%0002(r15),#11
	jr	eq,L03F2
	di	vi
	pushl	@r15,rr0
	ld	r0,L8DC2
	ld	r1,r0
	or	r0,#%0008
	out	%0080,r0
	nop
	out	%0080,r1
	popl	rr0,@r15
	push	@r15,r0
	pop	r0,@r15
L03F2:
	ldps	L875A
	iret
L03F8:
	test	%D57A
	jr	L040A
L03FE:
	test	%D57A
	jr	L040A
	WV %4D04;WV %D57A;WV %E800
L040A:
	ret
L040C:
	ret
L040E:
	WV %2102;WV %FFFF;WV %9E08
_addupc:
	ld	r7,%0002(r15)
	ld	r6,%0004(r15)
	sub	r7,%0004(r6)
	resflg	c
	rrc	r7,#1
	ld	r5,%0006(r6)
	rrc	r5,#1
	mult	rr4,r7
	sral	rr4,#14
	inc	r5,#1
	res	r5,#0
	cp	r5,%0002(r6)
	ret	ge
	add	r5,@r6
	ld	r6,%0006(r15)
	ld	r4,L8DC0
	ldctl	r0,fcw
	ld	r1,#%0462
	di	vi,nvi
	ex	r1,%D578
	ld	r3,#%C000
	ldctl	fcw,r3
	res	r3,#15
	add	r6,@r4
	ld	@r4,r6
	ldctl	fcw,r3
	xor	r2,r2
	jr	L046C
	ldctl	fcw,r3
	clr	%0006(r6)
	ld	r2,#%FFFF
L046C:
	ld	%D578,r1
	ldctl	fcw,r0
	ret
_splx:
	ld	r1,%0002(r15)
	ldk	r0,#7
	and	r1,r0
	jr	eq,L04B4
	jr	L049A
_spl7:
	ldk	r1,#7
	jr	L049A
_spl6:
	ldk	r1,#6
	jr	L049A
_spl5:
	ldk	r1,#5
	jr	L049A
_spl4:
	ldk	r1,#4
	jr	L049A
	WV %BD13;WV %E803;WV %BD12;WV %E801
_spl1:
	ldk	r1,#1
L049A:
	ld	r2,L8DC2
	ld	r0,r2
	and	r0,#%00C0
	or	r0,r1
	out	%0080,r0
	ld	L8DC2,r0
	di	vi
	ret
_spl0:
	xor	r1,r1
L04B4:
	ld	r2,L8DC2
	ld	r0,r2
	and	r0,#%00C0
	or	r0,r1
	out	%0080,r0
	ld	L8DC2,r0
	di	vi
	ret
_fubyte:
	ld	r5,%0002(r15)
	ld	r4,L8DC0
	xor	r2,r2
	ld	r6,r2
	jr	L04E6
_fuword:
	ld	r5,%0002(r15)
	ld	r4,L8DC0
	ldk	r6,#1
	res	r5,#0
L04E6:
	ld	r3,#%C000
	ld	r1,#%0548
	ldctl	r0,fcw
	di	vi,nvi
	ex	r1,%D578
	ldctl	fcw,r3
	res	r3,#15
	dec	r6,#1
	jr	eq,L0502
	ldb	rl2,@r4
	jr	L0504
L0502:
	ld	r2,@r4
L0504:
	ldctl	fcw,r3
	jr	L054E
_subyte:
	ld	r5,%0002(r15)
	ld	r4,L8DC0
	xor	r6,r6
	jr	L0520
_suword:
	ld	r5,%0002(r15)
	ld	r4,L8DC0
	ldk	r6,#1
	res	r5,#0
L0520:
	ld	r2,%0004(r15)
	ld	r3,#%C000
	ld	r1,#%0548
	ldctl	r0,fcw
	di	vi,nvi
	ex	r1,%D578
	ldctl	fcw,r3
	res	r3,#15
	dec	r6,#1
	jr	ne,L0540
	ld	@r4,r2
	jr	L0542
L0540:
	ldb	@r4,rl2
L0542:
	ldctl	fcw,r3
	xor	r2,r2
	jr	L054E
	WV %7D3A;WV %2102;WV %FFFF
L054E:
	ld	%D578,r1
	ldctl	fcw,r0
	ret
_copyin:
	ld	r4,L8DC0
	ld	r6,#%0000
	jr	L0568
_copyout:
	ld	r4,#%0000
	ld	r6,L8DC0
L0568:
	ld	r5,%0002(r15)
	ld	r7,%0004(r15)
	ld	r2,%0006(r15)
	resflg	c
	rrc	r2,#1
	ld	r3,#%C000
	ld	r1,#%0594
	ldctl	r0,fcw
	di	vi,nvi
	ex	r1,%D578
	ldctl	fcw,r3
	res	r3,#15
	ldir	@r6,@r4,r2
	ldctl	fcw,r3
	jr	L059A
	WV %7D3A;WV %2102;WV %FFFF
L059A:
	ld	%D578,r1
	ldctl	fcw,r0
	ret
_copyseg:
	ld	r4,%0002(r15)
	ld	r6,%0004(r15)
	xor	r5,r5
	ld	r7,r5
	exb	rh5,rl4
	exb	rh7,rl6
	ld	r2,#%0080
	ld	r3,#%C000
	ldctl	r0,fcw
	ldctl	fcw,r3
	ldir	@r6,@r4,r2
	ldctl	fcw,r0
	ret
L05C6:
	ld	r6,%0002(r15)
	xor	r7,r7
	exb	rh7,rl6
	ldl	rr4,rr6
	inc	r7,#2
	ld	r2,#%007F
	ld	r3,#%C000
	ldctl	r0,fcw
	ldctl	fcw,r3
	ld	@r4,#%0000
	ldir	@r6,@r4,r2
	ldctl	fcw,r0
	ret
_idle:
	calr	_spl0
	ei	vi
	halt
	push	@r15,r2
	calr	_splx
	inc	r15,#2
	ret
csv:
	ld	r1,@r15
	dec	r15,#12
	ldm	@r15,r8,#7
	ld	r14,r15
	inc	r14,#12
	jp	@r1
cret:
	ld	r5,r14
	ldm	r8,%FFF4(r5),#6
	ld	r15,r5
	pop	r14,@r15
	ret
L0614:
	pop	r7,@r15
	ld	r1,@r15
	ldm	@r1,r7,#9
	xor	r2,r2
	jp	@r7
L0620:
	calr	_spl7
	inc	r15,#2
	popl	rr8,@r15
	ld	r6,L8DC0
	sll	r6,#2
	add	r6,#%EC00
	ld	r7,#%FC00
	ld	r0,#%0200
	ld	r10,r0
	ld	r11,r7
	ldir	@r6,@r7,r0
	ld	r6,r8
	sll	r8,#2
	add	r8,#%EC00
	ldir	@r11,@r8,r10
	ldm	r7,@r9,#9
	ld	L8DC0,r6
	calr	_spl0
	ldk	r2,#1
	jp	@r7
L065E:
	ld	r6,L8DC2
	ld	r7,r6
	or	r7,#%0010
	out	%0080,r7
	nop
	out	%0080,r6
	ret
end _p_low
I
! Datenteil des Abbilds: ab hier Tabellen und Texte (letzter Code: Sprung 8756) !
L875A:
	WV %C000;WV %03F6
L875E:
	WV %0000;WV %1000;WV %0000;WV %0000
end low
