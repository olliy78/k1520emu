mbx module
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
_ttyopen	L
G
_ttinit	L
G
L6DD0	L
G
_ioctl	L
G
_ttioctl	L
G
L70D6	L
G
L70F0	L
G
L7106	L
G
L711E	L
G
L7130	L
G
L714A	L
G
L7222	L
G
L7BE8	L
G
L7BF0	L
G
L7C06	L
G
L7C18	L
G
L7C1E	L
G
_ttrstrt	L
G
_ttkick	L
G
_ttread	L
G
_ttwrite	L
G	_p_mbx	PR
E
_ttyopen:
	call	csv
	ld	r13,%0006(r14)
	ld	r12,%FC1E
	ld	r2,%0004(r14)
	ld	%0014(r13),r2
	test	%000A(r12)
	jr	ne,L6D80
	ld	%FD14,r13
	ld	r2,%0004(r14)
	ld	%FD16,r2
	test	%0010(r13)
	jr	ne,L6D78
	ld	r2,%000C(r12)
	ld	%0010(r13),r2
L6D78:
	ld	r2,%0010(r13)
	ld	%000A(r12),r2
L6D80:
	res	%0028(r13),#1
	set	%0028(r13),#2
	jp	cret
_ttinit:
	call	csv
	ld	r13,%0004(r14)
	ldb	%0022(r13),#%03
	ldb	%0023(r13),#%1C
	ldb	%0024(r13),#%11
	ldb	%0025(r13),#%13
	ldb	%0026(r13),#%1A
	ldb	%0027(r13),#%FF
	ldb	%0018(r13),#%7F
	ldb	%0019(r13),#%15
	clrb	%0020(r13)
	clrb	%0021(r13)
	jp	cret
L6DD0:
	call	csv
	ld	r13,%0004(r14)
	clr	%0010(r13)
	push	@r15,r13
	call	_ttywait
	inc	r15,#2
	clr	%0028(r13)
	clr	%002A(r13)
	jp	cret
_ioctl:
	call	csv
	ld	r11,%FC20
	push	@r15,@r11
	call	_getf
	inc	r15,#2
	ld	r13,r2
	test	r13
	jr	eq,L6E76
	cp	%0002(r11),#%6601
	jr	ne,L6E18
	ld	r2,@r11
	add	r2,#%FC8A
	setb	@r2,#0
	jr	L6E76
L6E18:
	cp	%0002(r11),#%6602
	jr	ne,L6E2A
	ld	r2,@r11
	add	r2,#%FC8A
	resb	@r2,#0
	jr	L6E76
L6E2A:
	ld	r12,%0002(r13)
	ld	r9,%0006(r12)
	and	r9,#%F000
	cp	r9,#%2000
	jr	eq,L6E4A
	cp	r9,#%3000
	jr	eq,L6E4A
	ldb	%FC13,#%19
	jr	L6E76
L6E4A:
	ld	r10,%0014(r12)
	ldb	rl0,@r13
	xorb	rh0,rh0
	push	@r15,r0
	push	@r15,%0004(r11)
	push	@r15,%0002(r11)
	push	@r15,r10
	ld	r2,r10
	srl	r2,#8
	ld	r3,r2
	mult	rr2,#%000E
	ld	r2,r3
	add	r2,#L879E
	ld	r3,@r2
	call	@r3
	inc	r15,#8
L6E76:
	jp	cret
_ttioctl:
	call	csv
	ld	r13,%0006(r14)
	dec	r15,#2
	ld	r2,%0004(r14)
	ld	r3,r2
	xor	r2,r2
	div	rr2,#%0006
	ex	r2,r3
	add	r3,r3
	ld	r1,L8D3A(r3)
	jp	@r1
L6E9A:
	ldb	rl2,%0034(r13)
	xorb	rh2,rh2
	ld	%FFF2(r14),r2
	push	@r15,#%0002
	push	@r15,%0008(r14)
	ld	r2,r14
	dec	r2,#14
	push	@r15,r2
	call	_copyout
	inc	r15,#6
	test	r2
	jr	eq,L6EEC
L6EBC:
	ldb	%FC13,#%0E
	jr	L6EEC
L6EC4:
	push	@r15,#%0002
	ld	r2,r14
	dec	r2,#14
	push	@r15,r2
	push	@r15,%0008(r14)
	call	_copyin
	inc	r15,#6
	test	r2
L6EDA:
	jr	ne,L6EBC
	ld	r0,%FFF2(r14)
	cp	r0,L8882
	jr	ult,L6EF2
	ldb	%FC13,#%06
L6EEC:
	ldk	r2,#1
L6EEE:
	jp	cret
L6EF2:
	testb	%0034(r13)
	jr	eq,L6F10
	push	@r15,r13
	ldb	rl3,%0034(r13)
	xorb	rh3,rh3
	WV %1902
L6F02:
	addb	rh4,@r1
	ld	r2,r3
	add	r2,#L886A
	ld	r3,@r2
	call	@r3
	inc	r15,#2
L6F10:
	test	%FFF2(r14)
	jr	eq,L6F34
	push	@r15,%0008(r14)
	push	@r15,r13
	push	@r15,%000A(r14)
	ld	r3,%FFF2(r14)
	mult	rr2,#%0014
	ld	r2,r3
	add	r2,#L8868
	ld	r3,@r2
	call	@r3
	inc	r15,#6
L6F34:
	testb	%FC13
	jr	ne,L6EEC
	ld	r2,%FFF2(r14)
	ldb	%0034(r13),rl2
	jr	L6EEC
L6F44:
	set	%0028(r13),#7
	jr	L6EEC
L6F4A:
	res	%0028(r13),#7
	jr	L6EEC
L6F50:
	push	@r15,r13
	call	_ttywait
	inc	r15,#2
L6F58:
	push	@r15,#%0006
	ld	r2,r13
	add	r2,#%0016
	push	@r15,r2
	push	@r15,%0008(r14)
	call	_copyin
	inc	r15,#6
	test	r2
	jr	eq,L6EEC
	jr	L6EBC
L6F74:
	push	@r15,#%0006
	push	@r15,%0008(r14)
	ld	r2,r13
	add	r2,#%0016
	push	@r15,r2
	call	_copyout
	inc	r15,#6
	test	r2
	jr	eq,L6EEC
	jr	L6EBC
L6F90:
	push	@r15,r13
	call	_ttywait
	inc	r15,#2
	ldb	rl2,%0021(r13)
	xorb	rh2,rh2
	ld	r12,r2
	push	@r15,#%000C
	ld	r2,r13
	add	r2,#%0016
	push	@r15,r2
	push	@r15,%0008(r14)
	call	_copyin
	inc	r15,#6
	test	r2
	! jr ne,L6EBC !
	WV %EE81
	ldb	rl2,%0021(r13)
	xorb	rh2,rh2
	and	r2,#%007F
	ldb	%0021(r13),rl2
	ldb	rl2,%0021(r13)
	xorb	rh2,rh2
	cp	r2,r12
	jr	eq,L6EEC
	testb	%0021(r13)
	jr	eq,L6FDE
	set	%002A(r13),#0
	jr	L6FE2
L6FDE:
	res	%002A(r13),#0
L6FE2:
	clrb	%002C(r13)
	jr	L6EEC
L6FE8:
	push	@r15,#%000C
	push	@r15,%0008(r14)
	ld	r2,r13
	add	r2,#%0016
	push	@r15,r2
	call	_copyout
	inc	r15,#6
	test	r2
	jp	eq,L6EEC
	jp	L6EBC
L7008:
	set	%0028(r13),#9
	jp	L6EEC
L7010:
	clr	%FFF2(r14)
	ld	r2,%001A(r13)
	and	r2,#%0022
	jr	eq,L7042
	ld	r2,@r13
	ld	%FFF2(r14),r2
L7024:
	push	@r15,#%0002
	push	@r15,%0008(r14)
	ld	r2,r14
	dec	r2,#14
	push	@r15,r2
	call	_copyout
	inc	r15,#6
	test	r2
	jp	eq,L6EEC
	jp	L6EBC
L7042:
	testb	%002E(r13)
	jr	eq,L7024
	ld	r11,%0002(r13)
L704C:
	ld	r2,r11
	inc	r11,#1
	ldb	rl3,@r2
	xorb	rh3,rh3
	and	r3,#%00FF
	ld	r2,r3
	cp	r3,#%00FF
	jr	eq,L7024
	inc	%FFF2(r14),#1
	jr	L704C
L7066:
	push	@r15,r13
	call	L71A4
	inc	r15,#2
	jp	L6EEC
L7072:
	push	@r15,%0008(r14)
	push	@r15,r13
	push	@r15,%0004(r14)
	ldb	rl3,%0034(r13)
	xorb	rh3,rh3
	mult	rr2,#%0014
	ld	r2,r3
	add	r2,#L8870
	ld	r3,@r2
	call	@r3
	inc	r15,#6
	jp	L6EEC
L7096:
	push	@r15,#%0006
	ld	r2,r13
	add	r2,#%0022
	push	@r15,r2
	push	@r15,%0008(r14)
	call	_copyin
	inc	r15,#6
	test	r2
	jp	eq,L6EEC
	jp	L6EBC
L70B6:
	push	@r15,#%0006
	push	@r15,%0008(r14)
	ld	r2,r13
	add	r2,#%0022
	push	@r15,r2
	call	_copyout
	inc	r15,#6
	test	r2
	jp	eq,L6EEC
	jp	L6EBC
L70D6:
	cp	r2,#%10AC
	jr	eq,L7072
	cp	r2,#%1357
	jp	eq,L6F58
	cp	r2,#%1358
	jr	eq,L7066
L70EA:
	xor	r2,r2
	jp	L6EEE
L70F0:
	cp	r2,#%10AC
	jr	eq,L7072
	cp	r2,#%1357
	jp	eq,L6F90
	cp	r2,#%1358
	jr	eq,L7096
	jr	L70EA
L7106:
	cp	r2,#%1355
	jp	eq,L6E9A
	cp	r2,#%1357
	jp	eq,L6FE8
	cp	r2,#%1358
	jr	eq,L70B6
	jr	L70EA
L711E:
	cp	r2,#%1355
	jp	eq,L6EC4
	cp	r2,#%1357
	jp	eq,L6F44
	jr	L70EA
L7130:
	cp	r2,#%1355
	jp	eq,L7008
	cp	r2,#%1356
	jp	eq,L6F74
	cp	r2,#%1357
	jp	eq,L6F4A
	jr	L70EA
L714A:
	cp	r2,#%1356
	jp	eq,L6F50
	cp	r2,#%1357
	jp	eq,L7010
	jr	L70EA
_ttywait:
	call	csv
	ld	r13,%0004(r14)
	call	_spl5
	jr	L718E
L716A:
	bit	%0028(r13),#4
	jr	eq,L7194
	push	@r15,r13
	ld	r2,%000C(r13)
	call	@r2
	inc	r15,#2
	set	%0028(r13),#6
	push	@r15,#%001D
	ld	r2,r13
	inc	r2,#6
	push	@r15,r2
	call	_sleep
	inc	r15,#4
L718E:
	test	%0006(r13)
	jr	ne,L716A
L7194:
	push	@r15,r13
	call	L71A4
	inc	r15,#2
	call	_spl0
	jp	cret
L71A4:
	call	csv
	ld	r13,%0004(r14)
	push	@r15,r13
	call	_wakeup
	inc	r15,#2
	ld	r2,r13
	inc	r2,#6
	push	@r15,r2
	call	_wakeup
	inc	r15,#2
	call	_spl6
	ld	r12,r2
	res	%0028(r13),#8
	ld	r2,%002A(r13)
	and	r2,#%FFE1
	ld	%002A(r13),r2
	push	@r15,r13
	ld	r2,%0014(r13)
	srl	r2,#8
	ld	r3,r2
	mult	rr2,#%000E
	ld	r2,r3
	add	r2,#L87A0
	ld	r3,@r2
	call	@r3
	inc	r15,#2
L71F2:
	ld	r2,r13
	inc	r2,#6
	push	@r15,r2
	call	_getc
	inc	r15,#2
	test	r2
	jr	pl,L71F2
L7202:
	push	@r15,r13
	call	_getc
	inc	r15,#2
	test	r2
	jr	pl,L7202
	clrb	%002E(r13)
	clrb	%002C(r13)
	push	@r15,r12
	call	_splx
	inc	r15,#2
	jp	cret
L7222:
	call	csv
	ld	r13,%0004(r14)
	ld	r12,%0006(r14)
	and	r13,#%00FF
	ld	r2,%001A(r12)
	and	r2,#%0020
	jr	ne,L7240
	and	r13,#%007F
L7240:
	ld	r11,%001A(r12)
	bit	r11,#0
	jr	eq,L7250
	push	@r15,r12
	call	_ttflow
	inc	r15,#2
L7250:
	ld	r2,r11
	and	r2,#%0020
	jr	eq,L7264
	ld	r2,r11
	and	r2,#%0022
	cp	r2,#%0022
	jr	ne,L72CE
L7264:
	bit	%0028(r12),#8
	jr	eq,L7296
	ldb	rl2,%0024(r12)
	xorb	rh2,rh2
	cp	r2,r13
	jr	ne,L727C
	res	%0028(r12),#8
	jp	L75A6
L727C:
	ldb	rl2,%0025(r12)
	xorb	rh2,rh2
	cp	r2,r13
	jp	eq,L75AE
	res	%0028(r12),#8
	push	@r15,r12
	call	_ttkick
	inc	r15,#2
	jr	L72CE
L7296:
	ldb	rl2,%0025(r12)
	xorb	rh2,rh2
	cp	r2,r13
	jr	ne,L72C2
	set	%0028(r12),#8
	push	@r15,r12
	ld	r2,%0014(r12)
	srl	r2,#8
	ld	r3,r2
	mult	rr2,#%000E
	ld	r2,r3
	add	r2,#L87A0
	ld	r3,@r2
	call	@r3
	jp	L75AC
L72C2:
	ldb	rl2,%0024(r12)
	xorb	rh2,rh2
	cp	r2,r13
	jp	eq,L75AE
L72CE:
	ld	r2,r11
	and	r2,#%0020
	jp	ne,L751C
	bit	%002A(r12),#3
	jr	eq,L7326
	res	%002A(r12),#3
	bit	r11,#2
	jr	eq,L730C
	ldb	rl2,L8C9A(r13)
	xorb	rh2,rh2
	and	r2,#%007F
	jr	eq,L730C
	ldb	rl2,L8C9A(r13)
	xorb	rh2,rh2
	ld	r13,r2
	and	r13,#%007F
L72FE:
	push	@r15,r12
	push	@r15,r13
	call	_putc
	inc	r15,#4
	jp	L7598
L730C:
	push	@r15,r12
	push	@r15,r13
	call	_ttyspec
	inc	r15,#4
	test	r2
	jr	ne,L72FE
	push	@r15,r12
	push	@r15,#%005C
	call	_putc
	inc	r15,#4
L7326:
	cp	r13,#%0010
	jr	ne,L7346
	testb	%0021(r12)
	jr	eq,L7346
	xor	r2,#%0001
	ld	%002A(r12),r2
	ld	r2,%002A(r12)
	and	r2,#%0002
	jp	eq,L7598
L7346:
	bit	%002A(r12),#1
	jr	eq,L738A
	ld	r2,%002A(r12)
	and	r2,#%FFF9
	ld	%002A(r12),r2
	cp	r13,#%000D
	jr	eq,L7364
	cp	r13,#%000A
	jr	ne,L7372
L7364:
	ldb	rl2,%0021(r12)
	xorb	rh2,rh2
	dec	r2,#1
	ldb	%002C(r12),rl2
	jr	L7376
L7372:
	clrb	%002C(r12)
L7376:
	ldb	rl2,%0023(r12)
	xorb	rh2,rh2
	cp	r2,r13
	jr	eq,L738A
	ldb	rl2,%0022(r12)
	cp	r2,r13
	jp	ne,L75A6
L738A:
	ldb	rl2,%0023(r12)
	xorb	rh2,rh2
	cp	r2,r13
	jr	eq,L739E
	ldb	rl2,%0022(r12)
	cp	r2,r13
	jp	ne,L7516
L739E:
	push	@r15,r12
	call	L71A4
	inc	r15,#2
	ldb	rl2,%0022(r12)
	xorb	rh2,rh2
	cp	r2,r13
	jr	ne,L73B4
	ldk	r2,#2
	jr	L73B6
L73B4:
	ldk	r2,#3
L73B6:
	ld	r13,r2
	push	@r15,r13
	push	@r15,%0010(r12)
	call	_signal
L73C2:
	inc	r15,#2
	ld	@r15,r12
	push	@r15,#%000A
L73CA:
	call	_ttyecho
	jp	L7512
L73D2:
	push	@r15,r12
	push	@r15,r12
	call	_unputc
	ld	@r15,r2
	call	_ttyrub
	jp	L7512
L73E4:
	ldb	rl2,%0018(r12)
	xorb	rh2,rh2
	cp	r2,r13
	jr	ne,L7408
	push	@r15,r13
	push	@r15,#L8D1A
	call	_bitset
	inc	r15,#4
	test	r2
	jr	ne,L73D2
	push	@r15,r12
	call	_unputc
	jp	L7596
L7408:
	ldb	rl2,%0019(r12)
	xorb	rh2,rh2
	cp	r2,r13
	jr	ne,L746A
	ld	r2,r11
	and	r2,#%0100
	jr	eq,L742A
	push	@r15,r13
	push	@r15,#L8D1A
	call	_bitset
	inc	r15,#4
	test	r2
	jr	ne,L7440
L742A:
	push	@r15,r12
	call	_unputc
	inc	r15,#2
	test	r2
	jr	pl,L742A
	push	@r15,r12
	push	@r15,r13
	call	_ttyecho
	jr	L73C2
L7440:
	push	@r15,r12
	call	_unputc
	inc	r15,#2
	ld	r13,r2
	test	r13
	jp	mi,L75A6
	ldb	rl2,%002D(r12)
	xorb	rh2,rh2
	and	r2,#%00FF
	jr	mi,L7440
	jr	eq,L7440
	push	@r15,r12
	push	@r15,r13
	call	_ttyrub
	inc	r15,#4
	jr	L7440
L746A:
	ldb	rl2,%0026(r12)
	xorb	rh2,rh2
	cp	r2,r13
	jr	ne,L7480
	push	@r15,r12
	push	@r15,#%000A
	call	_ttyecho
	jr	L7552
L7480:
	cp	r13,#%005C
	jr	ne,L7490
	set	%002A(r12),#3
	push	@r15,r12
	push	@r15,r13
	jr	L73CA
L7490:
	cp	r13,#%0012
	jr	ne,L74B6
	test	%0006(r12)
	jp	ne,L75A6
	push	@r15,r12
	push	@r15,#%000A
	call	_ttyecho
	inc	r15,#2
	ld	@r15,#%0001
	push	@r15,r12
	call	_ttycol
	jr	L7512
L74B6:
	cp	r13,#%0017
	jr	ne,L751C
	push	@r15,r12
	push	@r15,r12
	call	_unputc
	inc	r15,#2
	ld	r13,r2
	push	@r15,r13
	call	_ttyrub
	inc	r15,#2
	ld	@r15,r13
	push	@r15,#L8D2A
	call	_bitset
	inc	r15,#4
	test	r2
	jr	eq,L75A6
	jr	L74EC
L74E2:
	push	@r15,r12
	push	@r15,r13
	call	_ttyrub
	inc	r15,#4
L74EC:
	push	@r15,r12
	call	_unputc
	inc	r15,#2
	ld	r13,r2
	push	@r15,r13
	push	@r15,#L8D2A
	call	_bitset
	inc	r15,#4
	test	r2
	jr	ne,L74E2
	test	r13
	jr	mi,L75A6
	push	@r15,r12
	push	@r15,r13
	call	_putc
L7512:
	inc	r15,#4
	jr	L75A6
L7516:
	bit	r11,#1
	jp	eq,L73E4
L751C:
	cp	r13,#%000D
	jr	ne,L7528
	bit	r11,#4
	jr	eq,L7528
	ldk	r13,#10
L7528:
	bit	r11,#2
	jr	eq,L753C
	cp	r13,#%0041
	jr	lt,L753C
	cp	r13,#%005A
	jr	gt,L753C
	add	r13,#%0020
L753C:
	cp	@r12,#%0100
	jr	le,L754A
	push	@r15,r12
	call	L71A4
	jr	L75AC
L754A:
	push	@r15,r12
	push	@r15,r13
	call	_putc
L7552:
	inc	r15,#4
	ld	r2,r11
	and	r2,#%0022
	jr	ne,L7574
	cp	r13,#%000A
	jr	eq,L7574
	ldb	rl2,%0026(r12)
	xorb	rh2,rh2
	cp	r2,r13
	jr	eq,L7574
	ldb	rl2,%0027(r12)
	cp	r2,r13
	jr	ne,L7598
L7574:
	ld	r2,r11
	and	r2,#%0022
	jr	ne,L7590
	push	@r15,r12
	push	@r15,#%00FF
	call	_putc
	inc	r15,#4
	test	r2
	jr	ne,L7590
	incb	%002E(r12),#1
L7590:
	push	@r15,r12
	call	_wakeup
L7596:
	inc	r15,#2
L7598:
	push	@r15,r12
	push	@r15,r13
	call	_ttyecho
	inc	r15,#4
	test	r2
	jr	eq,L75AE
L75A6:
	push	@r15,r12
	call	_ttkick
L75AC:
	inc	r15,#2
L75AE:
	jp	cret
_ttycol:
	call	csv
	ld	r13,%0004(r14)
	ld	r12,%0002(r13)
	ldb	rl2,%002E(r13)
	xorb	rh2,rh2
	ld	r10,r2
	testb	%0020(r13)
	jr	eq,L75D6
	ldb	rl2,%0020(r13)
	and	r2,#%00FF
	jr	L75DA
L75D6:
	ld	r2,#%00FF
L75DA:
	ld	r9,r2
	xor	r8,r8
	ld	r11,@r13
	jr	L768C
L75E2:
	test	r10
	jr	eq,L75FE
	ld	r2,r12
	inc	r12,#1
	ldb	rl3,@r2
	xorb	rh3,rh3
	and	r3,#%00FF
	ld	r2,r3
	cp	r3,#%00FF
	jr	ne,L767E
	dec	r10,#1
	jr	L767E
L75FE:
	bit	%001A(r13),#2
	jr	eq,L7614
	ldb	rl2,@r12
	xorb	rh2,rh2
	cp	r2,#%0041
	jr	ult,L7614
	cp	r2,#%005A
	jr	ule,L7626
L7614:
	push	@r15,r13
	ldb	rl0,@r12
	xorb	rh0,rh0
	push	@r15,r0
	call	_ttyspec
	inc	r15,#4
	test	r2
	jr	eq,L763C
L7626:
	test	%0006(r14)
	jr	eq,L763A
	push	@r15,r13
	push	@r15,#%005C
	call	_ttyecho
	inc	r15,#4
	jr	L763C
L763A:
	inc	r8,#1
L763C:
	test	%0006(r14)
	jr	eq,L7656
	push	@r15,r13
	ld	r2,r12
	inc	r12,#1
	ldb	rl0,@r2
	xorb	rh0,rh0
	push	@r15,r0
	call	_ttyecho
	inc	r15,#4
	jr	L767E
L7656:
	ld	r2,r12
	inc	r12,#1
	ldb	rl3,@r2
	xorb	rh3,rh3
	cp	r3,#%0009
	jr	ne,L7676
	ld	r2,r8
	ldk	r3,#8
	ld	r4,r8
	and	r4,#%0007
	sub	r3,r4
	add	r2,r3
	ld	r8,r2
	jr	L7678
L7676:
	inc	r8,#1
L7678:
	cp	r8,r9
	jr	lt,L767E
	sub	r8,r9
L767E:
	ld	r2,r12
	and	r2,#%000F
	jr	ne,L768C
	ld	r12,%FFF0(r12)
	inc	r12,#2
L768C:
	ld	r2,r11
	dec	r11,#1
	test	r2
	jr	ne,L75E2
	ld	r2,r8
	jp	cret
_ttyecho:
	call	csv
	ld	r13,%0004(r14)
	ld	r12,%0006(r14)
	and	r13,#%007F
	ld	r2,%001A(r12)
	and	r2,#%0008
	jr	eq,L76CE
	push	@r15,r13
	push	@r15,#L8D1A
	call	_bitset
	inc	r15,#4
	test	r2
	jr	eq,L76D4
	ld	r2,%001A(r12)
	and	r2,#%0200
	jr	ne,L76D4
L76CE:
	xor	r2,r2
L76D0:
	jp	cret
L76D4:
	bit	%002A(r12),#4
	jr	eq,L76EA
	res	%002A(r12),#4
	push	@r15,r12
	push	@r15,#%005D
	call	L7A34
	inc	r15,#4
L76EA:
	bit	%001A(r12),#2
	jr	eq,L7700
	cp	r13,#%0041
	jr	lt,L7700
	cp	r13,#%005A
	jr	gt,L7700
	add	r13,#%0020
L7700:
	cp	r13,#%000A
	jr	ne,L7720
	bit	%002A(r12),#0
	jr	eq,L7720
	ld	r2,%001A(r12)
	and	r2,#%0020
	jr	ne,L7720
	testb	%002C(r12)
	jr	eq,L7720
	decb	%002C(r12),#1
L7720:
	push	@r15,r12
	push	@r15,r13
	call	L7A34
	inc	r15,#4
	ldk	r2,#1
	jr	L76D0
_ttyrub:
	call	csv
	ld	r13,%0004(r14)
	ld	r12,%0006(r14)
	ld	r2,%001A(r12)
	and	r2,#%0008
	jp	eq,L78A4
	test	r13
	jr	pl,L7754
	push	@r15,r12
	push	@r15,#%0007
	jp	L789E
L7754:
	bit	%001A(r12),#8
	jp	eq,L7880
	ldk	r11,#1
	push	@r15,r13
	push	@r15,#L8D1A
	call	_bitset
	inc	r15,#4
	test	r2
	jr	eq,L7788
	xor	r11,r11
	bit	%001A(r12),#9
	jr	eq,L7778
	ldk	r11,#2
L7778:
	push	@r15,r12
	push	@r15,r13
	call	_ttyspec
	inc	r15,#4
	test	r2
	jr	eq,L7828
	jr	L7826
L7788:
	cp	r13,#%0009
	jr	ne,L7808
	push	@r15,#%0000
	push	@r15,r12
	call	_ttycol
	inc	r15,#4
	ld	r11,r2
	test	r2
	jr	eq,L77CC
	ldb	rl2,%002D(r12)
	xorb	rh2,rh2
	and	r2,#%00FF
	ldb	rl3,%001E(r12)
	xorb	rh3,rh3
	and	r3,#%00FF
	cp	r3,r11
	jr	pl,L77BC
	ld	r3,r11
	jr	L77C6
L77BC:
	ldb	rl3,%001E(r12)
	xorb	rh3,rh3
	and	r3,#%00FF
L77C6:
	sub	r2,r3
	ld	r11,r2
	jr	L77E8
L77CC:
	ldb	rl2,%002D(r12)
	xorb	rh2,rh2
	and	r2,#%00FF
	ldb	rl3,%001E(r12)
	xorb	rh3,rh3
	and	r3,#%00FF
	sub	r2,r3
	ld	r11,r2
	jr	ne,L77E8
	ldk	r11,#8
L77E8:
	ldb	rl2,%001E(r12)
	xorb	rh2,rh2
	and	r2,#%00FF
	ldb	rl3,%002D(r12)
	xorb	rh3,rh3
	and	r3,#%00FF
	sub	r3,r11
	cp	r2,r3
	jr	mi,L7828
	clrb	%001E(r12)
	jr	L7828
L7808:
	bit	%001A(r12),#2
	jr	eq,L7828
	cp	r13,#%0041
	jr	lt,L781A
	cp	r13,#%005A
	jr	le,L7826
L781A:
	cp	r13,#%0060
	jr	eq,L7826
	cp	r13,#%007B
	jr	lt,L7828
L7826:
	inc	r11,#1
L7828:
	test	r11
	jr	mi,L783A
	ldb	rl2,%002D(r12)
	xorb	rh2,rh2
	and	r2,#%00FF
	cp	r2,r11
	jr	pl,L7852
L783A:
	push	@r15,r12
	push	@r15,#%000A
	call	L7A34
	inc	r15,#2
	ld	@r15,#%0001
	push	@r15,r12
	call	_ttycol
	jr	L78A2
L7852:
	ld	r2,r11
	dec	r11,#1
	test	r2
	jr	eq,L78A4
	push	@r15,r12
	push	@r15,#%0008
	call	L7A34
	inc	r15,#2
	ld	@r15,r12
	push	@r15,#%0020
	call	L7A34
	inc	r15,#2
	ld	@r15,r12
	push	@r15,#%0008
	call	L7A34
	inc	r15,#4
	jr	L7852
L7880:
	ld	r2,%002A(r12)
	and	r2,#%0010
	jr	ne,L789A
	set	%002A(r12),#4
	push	@r15,r12
	push	@r15,#%005B
	call	L7A34
	inc	r15,#4
L789A:
	push	@r15,r12
	push	@r15,r13
L789E:
	call	L7A34
L78A2:
	inc	r15,#4
L78A4:
	jp	cret
_unputc:
	call	csv
	ld	r13,%0004(r14)
	call	_spl6
	ld	r9,r2
	ld	r11,%0004(r13)
	test	r11
	jr	eq,L78D2
	dec	r11,#1
	ld	r2,r11
	ldb	rl3,@r2
	xorb	rh3,rh3
	ld	r10,r3
	and	r10,#%00FF
	cp	r10,#%00FF
	jr	ne,L78E2
L78D2:
	push	@r15,r9
	call	_splx
	inc	r15,#2
	ld	r2,#%FFFF
L78DE:
	jp	cret
L78E2:
	ld	%0004(r13),r11
	dec	r11,#2
	dec	@r13,#1
	test	@r13
	jr	eq,L78F0
	jr	pl,L790C
L78F0:
	ld	r12,r11
	and	r12,#%FFF0
	xor	r2,r2
	ld	%0004(r13),r2
	ld	%0002(r13),r2
	ld	r2,%D266
	ld	@r12,r2
	ld	%D266,r12
	jr	L793C
L790C:
	ld	r2,r11
	and	r2,#%000F
	jr	ne,L793C
	ld	r12,r11
	ld	r2,%D266
	ld	@r12,r2
	ld	%D266,r12
	ld	r12,%0002(r13)
	and	r12,#%FFF0
	jr	L792C
L792A:
	ld	r12,@r12
L792C:
	ld	r0,@r12
	cp	r0,r11
	jr	ne,L792A
	clr	@r12
	ld	r2,r12
	inc	r2,#16
	ld	%0004(r13),r2
L793C:
	push	@r15,r9
	call	_splx
	inc	r15,#2
	ld	r2,r10
	jr	L78DE
_bitset:
	call	csv
	ld	r13,%0006(r14)
	test	r13
	jr	pl,L795A
	xor	r2,r2
L7956:
	jp	cret
L795A:
	ld	r2,r13
	sra	r2,#3
	add	r2,%0004(r14)
	ldk	r3,#1
	ld	r4,r13
	and	r4,#%0007
	sda	r3,r4
	ldb	rl4,@r2
	xorb	rh4,rh4
	and	r3,r4
	ld	r2,r3
	jr	L7956
_ttyspec:
	call	csv
	ld	r13,%0004(r14)
	ld	r12,%0006(r14)
	test	r13
	jr	pl,L798E
L798A:
	xor	r2,r2
	jr	L79C8
L798E:
	ld	r11,r12
	add	r11,#%0022
	xor	r10,r10
	jr	L79A6
L7998:
	ld	r2,r11
	inc	r11,#1
	ldb	rl3,@r2
	xorb	rh3,rh3
	cp	r3,r13
	jr	eq,L79C6
	inc	r10,#1
L79A6:
	cp	r10,#%0006
	jr	ult,L7998
	ldb	rl2,%0018(r12)
	xorb	rh2,rh2
	cp	r2,r13
	jr	eq,L79C6
	ldb	rl2,%0019(r12)
	cp	r2,r13
	jr	ne,L79CC
	jr	L79C6
L79C0:
	testb	%0021(r12)
	jr	eq,L798A
L79C6:
	ldk	r2,#1
L79C8:
	jp	cret
L79CC:
	cp	r13,#%0012
	jr	eq,L79C6
	cp	r13,#%0017
	jr	eq,L79C6
	cp	r13,#%0010
	jr	eq,L79C0
	jr	L798A
_ttflow:
	call	csv
	ld	r13,%0004(r14)
	ld	r12,@r13
	cp	r12,#%0100
	jr	le,L79FC
	push	@r15,r13
	call	L71A4
	inc	r15,#2
	res	%0028(r13),#10
L79FC:
	cp	r12,#%0080
	jr	lt,L7A30
	ld	r2,%0028(r13)
	and	r2,#%0400
	jr	ne,L7A30
	ld	r2,r13
	inc	r2,#6
	push	@r15,r2
	ldb	rl0,%0025(r13)
	xorb	rh0,rh0
	push	@r15,r0
	call	_putc
	inc	r15,#4
	test	r2
	jr	ne,L7A30
	set	%0028(r13),#10
	push	@r15,r13
	call	_ttkick
	inc	r15,#2
L7A30:
	jp	cret
L7A34:
	call	csv
	ld	r13,%0004(r14)
	ld	r12,%0006(r14)
	cp	r13,#%000A
	jr	ne,L7A58
	bit	%001A(r12),#4
	jr	eq,L7A58
	push	@r15,r12
	push	@r15,#%000D
	call	L7A34
	inc	r15,#4
L7A58:
	ld	r2,%001A(r12)
	and	r2,#%0020
	jr	ne,L7AE4
	and	r13,#%007F
	bit	%001A(r12),#9
	jr	eq,L7A98
	push	@r15,r13
	push	@r15,#L8D1A
	call	_bitset
	inc	r15,#4
	test	r2
	jr	eq,L7A98
	push	@r15,r12
	push	@r15,#%005E
	call	L7A34
	inc	r15,#4
	cp	r13,#%007F
	jr	ne,L7A94
	ld	r13,#%0023
	jr	L7A98
L7A94:
	or	r13,#%0060
L7A98:
	testb	%0021(r12)
	jr	eq,L7AE4
	bit	%002A(r12),#0
	jr	eq,L7AE4
	cp	r13,#%000A
	jr	ne,L7AC6
	incb	%002C(r12),#1
	ldb	rl2,%002C(r12)
	xorb	rh2,rh2
	and	r2,#%00FF
	ldb	rl3,%0021(r12)
	xorb	rh3,rh3
	and	r3,#%00FF
	cp	r2,r3
	jr	pl,L7ACC
L7AC6:
	cp	r13,#%000C
	jr	ne,L7AE4
L7ACC:
	clrb	%002C(r12)
	set	%002A(r12),#2
	ld	r2,r12
	inc	r2,#6
	push	@r15,r2
	push	@r15,#%0080
	call	_putc
	inc	r15,#4
L7AE4:
	bit	%001A(r12),#2
	jr	eq,L7B76
	cp	r13,#%0061
	jr	lt,L7B2C
	cp	r13,#%007A
	jr	gt,L7B2C
	add	r13,#%FFE0
	jr	L7B76
L7AFC:
	bit	%001A(r12),#10
	jr	eq,L7B82
	testb	%001E(r12)
	jr	ne,L7B12
	ldb	rl2,%002D(r12)
	xorb	rh2,rh2
	ldb	%001E(r12),rl2
L7B12:
	push	@r15,r12
	push	@r15,#%0020
	call	L7A34
	inc	r15,#4
	ldb	rl2,@r11
	xorb	rh2,rh2
	and	r2,#%0007
	jr	ne,L7B12
	jp	L7C46
L7B2C:
	cp	r13,#%0041
	jr	lt,L7B46
	cp	r13,#%005A
	jr	gt,L7B46
	push	@r15,r12
	push	@r15,#%005C
	call	L7A34
	inc	r15,#4
	jr	L7B76
L7B46:
	ld	r11,#L8D52
L7B4A:
	ld	r2,r11
	inc	r11,#1
	ldb	rl3,@r2
	xorb	rh3,rh3
	testb	rl3
	jr	eq,L7B76
	ld	r2,r11
	inc	r11,#1
	ldb	rl3,@r2
	xorb	rh3,rh3
	cp	r3,r13
	jr	ne,L7B4A
	push	@r15,r12
	push	@r15,#%005C
	call	L7A34
	inc	r15,#4
	ldb	rl2,%FFFE(r11)
	xorb	rh2,rh2
	ld	r13,r2
L7B76:
	ld	r11,r12
	add	r11,#%002D
	cp	r13,#%0009
	jr	eq,L7AFC
L7B82:
	cp	r13,#%0020
	jr	lt,L7BC2
	cp	r13,#%007E
	jr	gt,L7BC2
	testb	%0020(r12)
	jr	eq,L7BB6
	ldb	rl2,@r11
	xorb	rh2,rh2
	and	r2,#%00FF
	ldb	rl3,%0020(r12)
	xorb	rh3,rh3
	and	r3,#%00FF
	cp	r2,r3
	jr	mi,L7BB6
	push	@r15,r12
	push	@r15,#%000A
	call	L7A34
	inc	r15,#4
L7BB6:
	incb	@r11,#1
	ld	r2,r12
	inc	r2,#6
	push	@r15,r2
	push	@r15,r13
	jr	L7C40
L7BC2:
	ld	r2,r12
	inc	r2,#6
	push	@r15,r2
	push	@r15,r13
	call	_putc
	inc	r15,#4
	ld	r2,r13
	and	r2,#%007F
	sub	r2,#%0008
	cp	r2,#%0005
	jr	ugt,L7C46
	add	r2,r2
	ld	r1,L8D46(r2)
	jp	@r1
L7BE8:
	testb	@r11
	jr	eq,L7C46
	decb	@r11,#1
	jr	L7C46
L7BF0:
	ldb	rl2,@r11
	xorb	rh2,rh2
	or	r2,#%0007
	ldb	@r11,rl2
	incb	@r11,#1
	ldb	rl2,%001E(r12)
L7C00:
	xorb	rh2,rh2
	ld	r13,r2
	jr	L7C26
L7C06:
	bit	%001A(r12),#10
	jr	eq,L7C10
	clrb	%001E(r12)
L7C10:
	clrb	@r11
	ldb	rl2,%001C(r12)
	jr	L7C00
L7C18:
	ldb	rl2,%001F(r12)
	jr	L7C00
L7C1E:
	clrb	@r11
	ldb	rl2,%001D(r12)
	jr	L7C00
L7C26:
	test	r13
	jr	eq,L7C46
	ld	r2,%001A(r12)
	and	r2,#%0020
	jr	ne,L7C46
	ld	r2,r12
	inc	r2,#6
	push	@r15,r2
	ld	r2,r13
	set	r2,#7
	push	@r15,r2
L7C40:
	call	_putc
	inc	r15,#4
L7C46:
	jp	cret
_ttrstrt:
	call	csv
	ld	r13,%0004(r14)
	res	%0028(r13),#0
	push	@r15,r13
	call	_ttkick
	inc	r15,#2
	jp	cret
_ttkick:
	call	csv
	ld	r13,%0004(r14)
	call	_spl5
	ld	r12,r2
	ld	r2,%0028(r13)
	and	r2,#%0121
	jr	ne,L7C84
	push	@r15,r13
	ld	r2,%000C(r13)
	call	@r2
	inc	r15,#2
L7C84:
	push	@r15,r12
	call	_splx
	inc	r15,#2
	jp	cret
_ttread:
	call	csv
	ld	r13,%0004(r14)
	ld	r2,%0028(r13)
	and	r2,#%0010
	jr	ne,L7CA6
	xor	r2,r2
	jr	L7DA2
L7CA6:
	call	_spl5
	jr	L7CC6
L7CAC:
	test	@r13
	jr	ne,L7CE0
L7CB0:
	ld	r2,%0028(r13)
	and	r2,#%0010
	jr	eq,L7DA0
	push	@r15,#%001C
	push	@r15,r13
	call	_sleep
	inc	r15,#4
L7CC6:
	ld	r2,%001A(r13)
	and	r2,#%0022
	jr	ne,L7CD6
	testb	%002E(r13)
	jr	eq,L7CB0
L7CD6:
	ld	r2,%001A(r13)
	and	r2,#%0022
	jr	ne,L7CAC
L7CE0:
	call	_spl0
	ld	r2,%001A(r13)
	and	r2,#%0022
	jr	eq,L7D06
L7CEE:
	test	@r13
	jr	eq,L7D70
	push	@r15,r13
	call	_getc
	ld	@r15,r2
	call	_passc
	inc	r15,#2
	test	r2
	jr	mi,L7D70
	jr	L7CEE
L7D06:
	push	@r15,r13
	call	_getc
	inc	r15,#2
	ld	r12,r2
	cp	r12,#%00FF
	jr	eq,L7D22
	push	@r15,r12
	call	_passc
	inc	r15,#2
	test	r2
	jr	pl,L7D06
L7D22:
	cp	r12,#%00FF
	jr	eq,L7D46
	ld	r2,%0002(r13)
	ldb	rl3,@r2
	xorb	rh3,rh3
	and	r3,#%00FF
	ld	r2,r3
	cp	r3,#%00FF
	jr	ne,L7D46
	push	@r15,r13
	call	_getc
	inc	r15,#2
	ld	r12,r2
L7D46:
	cp	r12,#%00FF
	jr	ne,L7D50
	decb	%002E(r13),#1
L7D50:
	testb	%0021(r13)
	jr	eq,L7D6C
	set	%002A(r13),#0
	ld	r2,%002A(r13)
	and	r2,#%FFF9
	ld	%002A(r13),r2
	clrb	%002C(r13)
	jr	L7D70
L7D6C:
	res	%002A(r13),#0
L7D70:
	bit	%0028(r13),#10
	jr	eq,L7DA0
	cp	@r13,#%0033
	jr	ge,L7DA0
	ld	r2,r13
	inc	r2,#6
	push	@r15,r2
	ldb	rl0,%0024(r13)
	xorb	rh0,rh0
	push	@r15,r0
	call	_putc
	inc	r15,#4
	test	r2
	jr	ne,L7DA0
	res	%0028(r13),#10
	push	@r15,r13
	call	_ttkick
	inc	r15,#2
L7DA0:
	ld	r2,@r13
L7DA2:
	jp	cret
_ttwrite:
	call	csv
	ld	r13,%0004(r14)
	ld	r2,%0028(r13)
	and	r2,#%0010
	jr	ne,L7DE6
L7DB8:
	xor	r2,r2
	jp	cret
L7DBE:
	push	@r15,r13
	call	_ttkick
	inc	r15,#2
	set	%0028(r13),#6
	push	@r15,#%001D
	ld	r2,r13
	inc	r2,#6
	push	@r15,r2
	call	_sleep
	inc	r15,#4
	jr	L7DF0
L7DDC:
	push	@r15,r13
	push	@r15,r12
	call	L7A34
	inc	r15,#4
L7DE6:
	test	%FC28
	jr	eq,L7E0C
	call	_spl5
L7DF0:
	cp	%0006(r13),#%0064
	jr	gt,L7DBE
	bit	%002A(r13),#2
	jr	ne,L7DBE
	call	_spl0
	call	_cpass
	ld	r12,r2
	test	r12
	jr	pl,L7DDC
L7E0C:
	push	@r15,r13
	call	_ttkick
	inc	r15,#2
	jr	L7DB8
end _p_mbx
end mbx
