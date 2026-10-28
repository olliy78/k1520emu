dev1 module
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
_ttyinit	L
G
_lpopen	L
G
_lpclose	L
G
_lpwrite	L
G
_lpintr	L
G	_p_dev1	PR
E
_ttyinit:
	call	csv
	ld	r13,#%D26A
	ldb	%0003(r13),#%01
	jp	cret
_lpopen:
	call	csv
	ld	r13,%0004(r14)
	and	r13,#%0003
	cp	r13,L8C98
	jr	ge,L6474
	cp	r13,#%0002
	jr	ge,L6474
	ld	r3,r13
	mult	rr2,#%001A
	ld	r2,r3
	ld	r12,r3
	add	r12,#%D3E2
	ldb	rl2,%0006(r12)
	xorb	rh2,rh2
	testb	rl2
	jr	eq,L647C
L6474:
	ldb	%FC13,#%05
	jr	L64D6
L647C:
	ld	r2,%0004(r14)
	and	r2,#%003F
	set	r2,#3
	ldb	%0006(r12),rl2
	ldb	%0007(r12),#%04
	ld	%0010(r12),#%0084
	ld	%000E(r12),#%0042
	ld	r2,r13
	sla	r2,#7
	add	r2,#%D416
	ld	%0014(r12),r2
	ld	r3,r13
	mult	rr2,#%000E
	ld	r2,r3
	add	r2,#%D516
	ld	%0016(r12),r2
	clrb	%0018(r12)
	ldb	rl2,%0006(r12)
	xorb	rh2,rh2
	and	r2,#%0004
	jr	ne,L64D6
	push	@r15,#%000C
	push	@r15,r13
	call	_lpcanon
	inc	r15,#4
L64D6:
	jp	cret
_lpclose:
	call	csv
	ld	r13,%0004(r14)
	and	r13,#%0003
	ld	r3,r13
	mult	rr2,#%001A
	ld	r2,r3
	ld	r12,r3
	add	r12,#%D3E2
	ldb	rl2,%0006(r12)
	xorb	rh2,rh2
	and	r2,#%0004
	jr	ne,L650E
	push	@r15,#%000C
	push	@r15,r13
	call	_lpcanon
	inc	r15,#4
	jr	L6512
L650E:
	clr	%000C(r12)
L6512:
	call	_spl4
	ldb	rl2,%0006(r12)
	xorb	rh2,rh2
	and	r2,#%0080
	cp	r2,#%0080
	jr	ne,L6538
	setb	%0006(r12),#6
	push	@r15,#%0021
	push	@r15,r12
	call	_sleep
	inc	r15,#4
	jr	L6544
L6538:
	push	@r15,r13
	call	L680E
	inc	r15,#2
	setb	%0006(r12),#7
L6544:
	ldb	rl2,%0006(r12)
	xorb	rh2,rh2
	and	r2,#%0080
	cp	r2,#%0080
	jr	ne,L6564
	setb	%0006(r12),#6
	push	@r15,#%0021
	push	@r15,r12
	call	_sleep
	inc	r15,#4
L6564:
	call	_spl0
	clrb	%0006(r12)
	jp	cret
_lpwrite:
	call	csv
	ld	r13,%0004(r14)
	and	r13,#%0003
	ld	r3,r13
	mult	rr2,#%001A
	ld	r2,r3
	ld	r11,r3
	add	r11,#%D3E2
L658A:
	test	%FC28
	jr	eq,L6608
	call	_spl4
	jr	L65A2
L6596:
	setb	%0006(r11),#7
	push	@r15,r13
	call	L680E
	inc	r15,#2
L65A2:
	cp	@r11,#%0082
	jr	le,L65E8
	ldb	rl2,%0006(r11)
	xorb	rh2,rh2
	and	r2,#%0080
	jr	eq,L6596
	setb	%0006(r11),#6
	push	@r15,#%0021
	push	@r15,r11
	call	_sleep
	inc	r15,#4
	testb	%FC13
	jr	eq,L65A2
	ld	r2,%FC28
	ld	r3,@r11
	add	r3,%0012(r11)
	add	r2,r3
	ld	%FC28,r2
L65DA:
	push	@r15,r11
	call	_getc
	inc	r15,#2
	test	r2
	jr	mi,L664C
	jr	L65DA
L65E8:
	call	_spl0
	ld	r2,%FC26
	inc	%FC26,#1
	push	@r15,r2
	call	_fubyte
	inc	r15,#2
	ld	r12,r2
	test	r12
	jr	pl,L6650
	ldb	%FC13,#%0E
L6608:
	call	_spl4
	ldb	rl2,%0006(r11)
	xorb	rh2,rh2
	and	r2,#%0080
	cp	r2,#%0080
	jr	ne,L662C
	setb	%0006(r11),#6
	push	@r15,#%0021
	push	@r15,r11
	call	_sleep
	inc	r15,#4
L662C:
	ldb	rl2,%0006(r11)
	xorb	rh2,rh2
	and	r2,#%0080
	jr	ne,L6648
	test	@r11
	jr	eq,L6648
	push	@r15,r13
	call	L680E
	inc	r15,#2
	setb	%0006(r11),#7
L6648:
	call	_spl0
L664C:
	jp	cret
L6650:
	dec	%FC28,#1
	call	_spl4
	push	@r15,r12
	push	@r15,r13
	call	_lpcanon
	inc	r15,#4
	call	_spl0
	jr	L658A
_lpcanon:
	call	csv
	ld	r13,%0004(r14)
	ld	r12,%0006(r14)
	ld	r3,r13
	mult	rr2,#%001A
	ld	r2,r3
	ld	r11,r3
	add	r11,#%D3E2
	bitb	%0006(r11),#4
	jr	eq,L6710
	cp	r12,#%0061
	jr	lt,L66C0
	cp	r12,#%007A
	jr	gt,L66C0
	add	r12,#%FFE0
	jr	L6710
L669A:
	cp	r12,#%000A
	jr	eq,L66A6
	cp	r12,#%000C
	jr	ne,L66B2
L66A6:
	push	@r15,r11
	push	@r15,r12
	call	_putc
	inc	r15,#4
	ldk	r12,#13
L66B2:
	push	@r15,r11
	push	@r15,r12
	call	_putc
	inc	r15,#4
L66BC:
	jp	cret
L66C0:
	ld	r2,r12
	cp	r2,#%0060
	jr	eq,L66EE
	cp	r2,#%007B
	jr	eq,L66E2
	cp	r2,#%007C
	jr	eq,L66F4
	cp	r2,#%007D
	jr	eq,L66E8
	cp	r2,#%007E
	jr	eq,L66FA
	jr	L6710
L66E2:
	ld	r12,#%0028
	jr	L66FE
L66E8:
	ld	r12,#%0029
	jr	L66FE
L66EE:
	ld	r12,#%0027
	jr	L66FE
L66F4:
	ld	r12,#%0021
	jr	L66FE
L66FA:
	ld	r12,#%005E
L66FE:
	push	@r15,r12
	push	@r15,r13
	call	_lpcanon
	inc	r15,#4
	dec	%0008(r11),#1
	ld	r12,#%002D
L6710:
	bitb	%0006(r11),#2
	jr	ne,L669A
	ld	r2,r12
	cp	r2,#%0008
	jr	eq,L67B2
	cp	r2,#%0009
	jr	eq,L6752
	cp	r2,#%000A
	jr	eq,L6770
	cp	r2,#%000C
	jr	eq,L6780
	cp	r2,#%000D
	jr	eq,L679E
	cp	r2,#%0020
	jr	eq,L674C
	ld	r0,%0008(r11)
	cp	r0,%000A(r11)
	jr	ge,L67D8
	bitb	%0006(r11),#5
	jr	eq,L67C8
L674C:
	inc	%0008(r11),#1
	jr	L66BC
L6752:
	ld	r2,%0008(r11)
	inc	r2,#8
	ldb	rl3,%0007(r11)
	xorb	rh3,rh3
	sub	r2,r3
	and	r2,#%FFF8
	ldb	rl3,%0007(r11)
	add	r2,r3
L676A:
	ld	%0008(r11),r2
	jr	L66BC
L6770:
	inc	%000C(r11),#1
	ld	r0,%000C(r11)
	cp	r0,%000E(r11)
	jr	lt,L6780
	ldk	r12,#12
L6780:
	clr	%000A(r11)
	test	%000C(r11)
	jr	eq,L679E
	push	@r15,r11
	push	@r15,r12
	call	_putc
	inc	r15,#4
	cp	r12,#%000C
	jr	ne,L679E
	clr	%000C(r11)
L679E:
	push	@r15,r11
	push	@r15,#%000D
	call	_putc
	inc	r15,#4
	ldb	rl2,%0007(r11)
	xorb	rh2,rh2
	jr	L676A
L67B2:
	ldb	rl2,%0007(r11)
	xorb	rh2,rh2
	cp	r2,%0008(r11)
	jp	ge,L66BC
	dec	%0008(r11),#1
	jp	L66BC
L67C8:
	push	@r15,r11
	push	@r15,#%000D
	call	_putc
	inc	r15,#4
	clr	%000A(r11)
L67D8:
	ld	r0,%0008(r11)
	cp	r0,%0010(r11)
	jr	ge,L674C
	jr	L67F4
L67E4:
	push	@r15,r11
	push	@r15,#%0020
	call	_putc
	inc	r15,#4
	inc	%000A(r11),#1
L67F4:
	ld	r0,%0008(r11)
	cp	r0,%000A(r11)
	jr	gt,L67E4
	push	@r15,r11
	push	@r15,r12
	call	_putc
	inc	r15,#4
	inc	%000A(r11),#1
	jr	L674C
L680E:
	call	csv
	ld	r13,%0004(r14)
	ld	r3,r13
	mult	rr2,#%001A
	ld	r2,r3
	ld	r12,r3
	add	r12,#%D3E2
	ld	r9,%0014(r12)
	xor	r10,r10
L682A:
	cp	r10,#%0080
	jr	ge,L685C
	push	@r15,r12
	call	_getc
	inc	r15,#2
	ld	r11,r2
	test	r11
	jr	mi,L685C
	ld	r2,r9
	inc	r9,#1
	ld	r3,r11
	ldb	@r2,rl3
	ld	r2,r10
	inc	r10,#1
	cp	r2,#%0028
	jr	gt,L6856
	bitb	%0006(r12),#2
	jr	eq,L682A
L6856:
	cp	r11,#%000D
	jr	ne,L682A
L685C:
	ld	r2,r10
	ld	%0012(r12),r10
	test	r10
	jr	mi,L6870
	jr	eq,L6870
	push	@r15,r13
	call	_lpxfer
	inc	r15,#2
L6870:
	jp	cret
_lpintr:
	call	csv
	ld	r13,%0004(r14)
	dec	r15,#4
	ld	r3,r13
	mult	rr2,#%001A
	ld	r2,r3
	ld	r11,r3
	add	r11,#%D3E2
	ldb	rl2,%0006(r11)
	xorb	rh2,rh2
	and	r2,#%0080
	jr	eq,L68F6
	ld	r12,%0016(r11)
	ld	r9,%0014(r11)
	ldb	rl2,%0003(r12)
	xorb	rh2,rh2
	cp	r2,#%0001
	jr	eq,L68FA
	ldb	rl2,%0002(r12)
	ld	%FFF0(r14),r2
	ldb	rl2,%0003(r12)
	xorb	rh2,rh2
	ld	%FFF2(r14),r2
	incb	%0018(r11),#1
	ldb	rl2,%0018(r11)
	xorb	rh2,rh2
	cp	r2,#%000A
	jr	ugt,L68D6
	push	@r15,r13
	call	_lpxfer
	jr	L68F4
L68D6:
	ldb	%FC13,#%05
L68DC:
	resb	%0006(r11),#7
L68E0:
	clrb	%0018(r11)
	bitb	%0006(r11),#6
	jr	eq,L68F6
	resb	%0006(r11),#6
	push	@r15,r11
	call	_wakeup
L68F4:
	inc	r15,#2
L68F6:
	jp	cret
L68FA:
	test	@r11
	jr	mi,L68DC
	jr	eq,L68DC
	push	@r15,r13
	call	L680E
	inc	r15,#2
	jr	L68E0
_lpxfer:
	call	csv
	ld	r3,%0004(r14)
	mult	rr2,#%001A
	ld	r12,r3
	add	r12,#%D3E2
	ld	r13,%0016(r12)
	ldb	@r13,#%02
	ld	r2,%0004(r14)
	ldb	%0001(r13),rl2
	clrb	%0003(r13)
	clr	%0004(r13)
	ld	r2,%0014(r12)
	ld	%0006(r13),r2
	ld	r2,%0012(r12)
	ld	%0008(r13),r2
	ldb	%0002(r13),#%01
	push	@r15,#%0002
	push	@r15,r13
	call	L5CAE
	inc	r15,#4
	jp	cret
L695A:
	call	csv
	ld	r13,%0006(r14)
	ld	r3,%0004(r14)
	and	r3,#%0007
	mult	rr2,#%001A
	ld	r2,r3
	ld	r12,r3
	add	r12,#%D3E2
	test	r13
	jr	eq,L699C
	ldb	rl2,%0006(r12)
	xorb	rh2,rh2
	ldb	@r13,rl2
	ldb	rl2,%0007(r12)
	ldb	%0001(r13),rl2
	ld	r2,%000E(r12)
	ld	%0002(r13),r2
	ld	r2,%0010(r12)
	ld	%0004(r13),r2
	jr	L69CA
L699C:
	ldb	rl2,%FC9E
	xorb	rh2,rh2
	and	r2,#%003F
	set	r2,#3
	ldb	%0006(r12),rl2
	ldb	rl2,%FC9F
	xorb	rh2,rh2
	and	r2,#%000F
	ldb	%0007(r12),rl2
	ld	r2,%FCA0
	ld	%000E(r12),r2
	ld	r2,%FCA2
	ld	%0010(r12),r2
L69CA:
	jp	cret
end _p_dev1
end dev1
