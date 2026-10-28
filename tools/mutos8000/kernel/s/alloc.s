alloc module
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
_alloc	L
G
_free	L
G
_ialloc	L
G
_ifree	L
G
_getfs	L
G
_update	L
G	_p_alloc	PR
E
_alloc:
	call	csv
	dec	r15,#4
	push	@r15,%0004(r14)
	call	_getfs
	inc	r15,#2
	ld	r13,r2
	jr	L2F6E
L2F5C:
	push	@r15,#%000A
	ld	r2,r13
	add	r2,#%019A
	push	@r15,r2
	call	_sleep
	inc	r15,#4
L2F6E:
	testb	%019A(r13)
	jr	ne,L2F5C
L2F74:
	test	%0006(r13)
	jr	mi,L2F92
	jr	eq,L2F92
	cp	%0006(r13),#%0032
	jr	le,L2FB8
	push	@r15,%0004(r14)
	push	@r15,#L8AF2
	call	_prdev
	inc	r15,#4
L2F92:
	clr	%0006(r13)
	xor	r0,r0
	ld	r1,r0
	ldl	%01A2(r13),rr0
	push	@r15,%0004(r14)
	push	@r15,#L8B01
	call	_prdev
	inc	r15,#4
	ldb	%FC13,#%1C
	xor	r2,r2
L2FB4:
	jp	cret
L2FB8:
	dec	%0006(r13),#1
	ld	r2,%0006(r13)
	sla	r2,#2
	add	r2,r13
	inc	r2,#8
	ldl	rr4,@r2
	ldl	%FFF0(r14),rr4
	testl	%FFF0(r14)
	jr	eq,L2F92
	push	@r15,%0004(r14)
	pushl	@r15,%FFF0(r14)
	push	@r15,r13
	call	L316A
	inc	r15,#8
	test	r2
	jr	ne,L2F74
	test	%0006(r13)
	jr	eq,L2FF0
	jr	pl,L3050
L2FF0:
	incb	%019A(r13),#1
	pushl	@r15,%FFF0(r14)
	push	@r15,%0004(r14)
	call	_bread
	inc	r15,#6
	ld	r12,r2
	ld	r2,@r12
	and	r2,#%0004
	jr	ne,L302E
	ld	r2,%000E(r12)
	ld	r3,@r2
	ld	%0006(r13),r3
	push	@r15,#%00C8
	ld	r2,r13
	inc	r2,#8
	push	@r15,r2
	ld	r2,%000E(r12)
	inc	r2,#2
	push	@r15,r2
	call	_bcopy
	inc	r15,#6
L302E:
	push	@r15,r12
	call	_brelse
	inc	r15,#2
	clrb	%019A(r13)
	ld	r2,r13
	add	r2,#%019A
	push	@r15,r2
	call	_wakeup
	inc	r15,#2
	test	%0006(r13)
	jr	mi,L2F92
	jr	eq,L2F92
L3050:
	pushl	@r15,%FFF0(r14)
	push	@r15,%0004(r14)
	call	_getblk
	inc	r15,#6
	ld	r12,r2
	push	@r15,r12
	call	_clrbuf
	inc	r15,#2
	testl	%01A2(r13)
	jr	eq,L307C
	ldl	rr2,%01A2(r13)
	subl	rr2,#%00000001
	ldl	%01A2(r13),rr2
L307C:
	ldb	%019C(r13),#%01
	ld	r2,r12
	jr	L2FB4
_free:
	call	csv
	push	@r15,%0004(r14)
	call	_getfs
	inc	r15,#2
	ld	r13,r2
	ldb	%019C(r13),#%01
	jr	L30B0
L309E:
	push	@r15,#%000A
	ld	r2,r13
	add	r2,#%019A
	push	@r15,r2
	call	_sleep
	inc	r15,#4
L30B0:
	testb	%019A(r13)
	jr	ne,L309E
	push	@r15,%0004(r14)
	pushl	@r15,%0006(r14)
	push	@r15,r13
	call	L316A
	inc	r15,#8
	test	r2
	jr	ne,L3166
	test	%0006(r13)
	jr	eq,L30D2
	jr	pl,L30E0
L30D2:
	ld	%0006(r13),#%0001
	xor	r0,r0
	ld	r1,r0
	ldl	%0008(r13),rr0
L30E0:
	cp	%0006(r13),#%0032
	jr	lt,L313C
	incb	%019A(r13),#1
	pushl	@r15,%0006(r14)
	push	@r15,%0004(r14)
	call	_getblk
	inc	r15,#6
	ld	r12,r2
	ld	r2,%000E(r12)
	ld	r3,%0006(r13)
	ld	@r2,r3
	push	@r15,#%00C8
	ld	r2,%000E(r12)
	inc	r2,#2
	push	@r15,r2
	ld	r2,r13
	inc	r2,#8
	push	@r15,r2
	call	_bcopy
	inc	r15,#6
	clr	%0006(r13)
	push	@r15,r12
	call	_bwrite
	inc	r15,#2
	clrb	%019A(r13)
	ld	r2,r13
	add	r2,#%019A
	push	@r15,r2
	call	_wakeup
	inc	r15,#2
L313C:
	ld	r2,%0006(r13)
	inc	%0006(r13),#1
	sla	r2,#2
	add	r2,r13
	inc	r2,#8
	ldl	rr4,%0006(r14)
	ldl	@r2,rr4
	ldl	rr2,%01A2(r13)
	addl	rr2,#%00000001
	ldl	%01A2(r13),rr2
	ldb	%019C(r13),#%01
L3166:
	jp	cret
L316A:
	call	csv
	ld	r13,%0004(r14)
	ldl	rr2,%0006(r14)
	ld	r5,@r13
	xor	r4,r4
	cpl	rr2,rr4
	jr	lt,L3186
	ldl	rr0,rr2
	cpl	rr2,%0002(r13)
	jr	lt,L319A
L3186:
	push	@r15,%000A(r14)
	push	@r15,#L8B0A
	call	_prdev
	inc	r15,#4
	ldk	r2,#1
L3196:
	jp	cret
L319A:
	xor	r2,r2
	jr	L3196
_ialloc:
	call	csv
	dec	r15,#10
	push	@r15,%0004(r14)
	call	_getfs
	inc	r15,#2
	ld	r13,r2
	jr	L31C4
L31B2:
	push	@r15,#%000A
	ld	r2,r13
	add	r2,#%019B
	push	@r15,r2
	call	_sleep
	inc	r15,#4
L31C4:
	testb	%019B(r13)
	jr	ne,L31B2
	jr	L3250
L31CC:
	jr	eq,L3256
	dec	%00D0(r13),#1
	ld	r2,%00D0(r13)
	sla	r2,#1
	add	r2,r13
	add	r2,#%00D2
	ld	r3,@r2
	ld	%FFEE(r14),r3
	cp	%FFEE(r14),#%0002
	jr	ult,L3250
	push	@r15,%FFEE(r14)
	push	@r15,%0004(r14)
	call	_iget
	inc	r15,#4
	ld	r11,r2
	test	r11
	jr	ne,L3242
L3202:
	xor	r2,r2
	jr	L323E
L3206:
	clr	%FFF2(r14)
	jr	L3224
L320C:
	ld	r2,%FFF2(r14)
	sla	r2,#2
	add	r2,r11
	add	r2,#%0012
	xor	r0,r0
	ld	r1,r0
	ldl	@r2,rr0
	inc	%FFF2(r14),#1
L3224:
	cp	%FFF2(r14),#%000D
	jr	lt,L320C
	test	%01A6(r13)
	jr	eq,L3236
	dec	%01A6(r13),#1
L3236:
	ldb	%019C(r13),#%01
	ld	r2,r11
L323E:
	jp	cret
L3242:
	test	%0006(r11)
	jr	eq,L3206
	push	@r15,r11
	call	_iput
	inc	r15,#2
L3250:
	test	%00D0(r13)
	jr	pl,L31CC
L3256:
	incb	%019B(r13),#1
	ld	%FFEE(r14),#%0001
	ldl	rr2,#%00000002
L3266:
	ldl	%FFEA(r14),rr2
	ld	r5,@r13
	xor	r4,r4
	cpl	rr2,rr4
	jr	lt,L32A6
L3272:
	clrb	%019B(r13)
	ld	r2,r13
	add	r2,#%019B
	push	@r15,r2
	call	_wakeup
	inc	r15,#2
	test	%00D0(r13)
	jr	eq,L328C
	jr	pl,L3250
L328C:
	push	@r15,%0004(r14)
	push	@r15,#L8B14
	call	_prdev
	inc	r15,#4
	ldb	%FC13,#%1C
	clr	%01A6(r13)
	jr	L3202
L32A6:
	pushl	@r15,%FFEA(r14)
	push	@r15,%0004(r14)
	call	_bread
	inc	r15,#6
	ld	r12,r2
	bit	@r12,#2
	jr	eq,L32C8
	push	@r15,r12
	call	_brelse
	inc	r15,#2
	inc	%FFEE(r14),#8
	jr	L334E
L32C8:
	ld	r2,%000E(r12)
	ld	%FFF0(r14),r2
	clr	%FFF2(r14)
	jr	L3336
L32D6:
	ld	r2,%FFF0(r14)
	test	@r2
	jr	ne,L3322
	ld	r11,#L907A
	jr	L32FC
L32E4:
	ld	r0,%0004(r14)
	cp	r0,%0002(r11)
	jr	ne,L32F8
	ld	r0,%FFEE(r14)
	cp	r0,%0004(r11)
	jr	eq,L3322
L32F8:
	add	r11,#%004A
L32FC:
	cp	r11,#L9EEE
	jr	ult,L32E4
	ld	r2,%00D0(r13)
	inc	%00D0(r13),#1
	sla	r2,#1
	add	r2,r13
	add	r2,#%00D2
	ld	r3,%FFEE(r14)
	ld	@r2,r3
	cp	%00D0(r13),#%0064
	jr	ge,L333E
L3322:
	inc	%FFEE(r14),#1
	ld	r2,%FFF0(r14)
	add	r2,#%0040
	ld	%FFF0(r14),r2
	inc	%FFF2(r14),#1
L3336:
	cp	%FFF2(r14),#%0008
	jr	lt,L32D6
L333E:
	push	@r15,r12
	call	_brelse
	inc	r15,#2
	cp	%00D0(r13),#%0064
	jr	ge,L3272
L334E:
	ldl	rr2,%FFEA(r14)
	addl	rr2,#%00000001
	jr	L3266
_ifree:
	call	csv
	push	@r15,%0004(r14)
	call	_getfs
	inc	r15,#2
	ld	r13,r2
	inc	%01A6(r13),#1
	testb	%019B(r13)
	jr	ne,L339A
	cp	%00D0(r13),#%0064
	jr	ge,L339A
	ld	r2,%00D0(r13)
	inc	%00D0(r13),#1
	sla	r2,#1
	add	r2,r13
	add	r2,#%00D2
	ld	r3,%0006(r14)
	ld	@r2,r3
	ldb	%019C(r13),#%01
L339A:
	jp	cret
_getfs:
	call	csv
	ld	r13,#LA07E
	jr	L33EC
L33A8:
	test	%0002(r13)
	jr	eq,L33EA
	ld	r0,@r13
	cp	r0,%0004(r14)
	jr	ne,L33EA
	ld	r2,%0002(r13)
	inc	r2,#14
	ld	r12,@r2
	cp	%0006(r12),#%0032
	jr	gt,L33CE
	cp	%00D0(r12),#%0064
	jr	le,L33E4
L33CE:
	push	@r15,%0004(r14)
	push	@r15,#L8B22
	call	_prdev
	inc	r15,#4
	clr	%0006(r12)
	clr	%00D0(r12)
L33E4:
	ld	r2,r12
L33E6:
	jp	cret
L33EA:
	inc	r13,#6
L33EC:
	cp	r13,#LA09C
	jr	ult,L33A8
	push	@r15,#L8B2C
	call	_panic
	inc	r15,#2
	xor	r2,r2
	jr	L33E6
_update:
	call	csv
	dec	r15,#2
	test	L8DDE
	jr	ne,L34F2
	inc	L8DDE,#1
	ld	r12,#LA07E
	jr	L34A0
L3416:
	test	%0002(r12)
	jr	eq,L349E
	ld	r2,%0002(r12)
	inc	r2,#14
	ld	r3,@r2
	ld	%FFF2(r14),r3
	ld	r2,%FFF2(r14)
	add	r2,#%019C
	testb	@r2
	jr	eq,L349E
	ld	r2,%FFF2(r14)
	add	r2,#%019B
	testb	@r2
	jr	ne,L349E
	ld	r2,%FFF2(r14)
	add	r2,#%019A
	testb	@r2
	jr	ne,L349E
	ld	r2,%FFF2(r14)
	add	r2,#%019D
	testb	@r2
	jr	ne,L349E
	ldk	r3,#1
	exts	rr2
	pushl	@r15,rr2
	push	@r15,@r12
	call	_getblk
	inc	r15,#6
	ld	r11,r2
	bit	@r11,#2
	jr	ne,L349E
	ld	r2,%FFF2(r14)
	add	r2,#%019C
	clrb	@r2
	ld	r2,%FFF2(r14)
	add	r2,#%019E
	ldl	rr4,L8DCC
	ldl	@r2,rr4
	push	@r15,#%0200
	push	@r15,%000E(r11)
	push	@r15,%FFF2(r14)
	call	_bcopy
	inc	r15,#4
	ld	@r15,r11
	call	_bwrite
	inc	r15,#2
L349E:
	inc	r12,#6
L34A0:
	cp	r12,#LA09C
	jr	ult,L3416
	ld	r13,#L907A
	jr	L34DE
L34AC:
	ldb	rl2,@r13
	xorb	rh2,rh2
	and	r2,#%0001
	jr	ne,L34DA
	testb	%0001(r13)
	jr	eq,L34DA
	setb	@r13,#0
	incb	%0001(r13),#1
	push	@r15,#L8DCC
	push	@r15,#L8DCC
	push	@r15,r13
	call	_iupdat
	inc	r15,#4
	ld	@r15,r13
	call	_iput
	inc	r15,#2
L34DA:
	add	r13,#%004A
L34DE:
	cp	r13,#L9EEE
	jr	ult,L34AC
	clr	L8DDE
	push	@r15,#%FFFF
	call	_bflush
	inc	r15,#2
L34F2:
	jp	cret
end _p_alloc
end alloc
