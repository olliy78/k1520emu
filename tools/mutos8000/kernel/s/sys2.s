sys2 module
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
_unlink	L
G
_chdir	L
G
_chroot	L
G
_chmod	L
G
_chown	L
G
_ssig	L
G
_kill	L
G
_times	L
G
_profil	L
G
_alarm	L
G
_pause	L
G
_umask	L
G
_utime	L
G
_read	L
G
_write	L
G
_open	L
G
_creat	L
G
_close	L
G
_seek	L
G
_link	L
G
_mknod	L
G	_p_sys2	PR
E
_unlink:
	call	csv
	push	@r15,#%0002
	push	@r15,#_uchar
	call	_namei
	inc	r15,#4
	ld	r12,r2
	test	r12
	jr	eq,L4E80
	ld	r0,%0004(r12)
	cp	r0,%FC42
	jr	ne,L4DE6
	ld	r13,r12
	incb	%0001(r13),#1
	jr	L4DF6
L4DE6:
	push	@r15,%FC42
	push	@r15,%0002(r12)
	call	_iget
	inc	r15,#4
	ld	r13,r2
L4DF6:
	test	r13
	jr	eq,L4E78
	ld	r2,%0006(r13)
	and	r2,#%F000
	cp	r2,#%4000
	jr	ne,L4E10
	call	_suser
	test	r2
	jr	eq,L4E70
L4E10:
	ld	r0,%0002(r13)
	cp	r0,%0002(r12)
	jr	eq,L4E22
	ldb	%FC13,#%10
	jr	L4E70
L4E22:
	bitb	@r13,#5
	jr	eq,L4E2E
	push	@r15,r13
	call	_xrele
	inc	r15,#2
L4E2E:
	bitb	@r13,#5
	jr	eq,L4E42
	cp	%0008(r13),#%0001
	jr	ne,L4E42
	ldb	%FC13,#%1A
	jr	L4E70
L4E42:
	ldl	rr2,%FC2A
	ld	r5,#%0010
	xor	r4,r4
	subl	rr2,rr4
	ldl	%FC2A,rr2
	ld	r2,#%FC42
	ld	%FC26,r2
	ld	%FC28,r5
	clr	%FC42
	push	@r15,r12
	call	_writei
	inc	r15,#2
	dec	%0008(r13),#1
	setb	@r13,#6
L4E70:
	push	@r15,r13
	call	_iput
	inc	r15,#2
L4E78:
	push	@r15,r12
	call	_iput
	inc	r15,#2
L4E80:
	jp	cret
_chdir:
	call	csv
	push	@r15,#%FC2E
	call	_chdirec
	inc	r15,#2
	jp	cret
_chroot:
	call	csv
	call	_suser
	test	r2
	jr	eq,L4EAC
	push	@r15,#%FC30
	call	_chdirec
	inc	r15,#2
L4EAC:
	jp	cret
_chdirec:
	call	csv
	ld	r13,%0004(r14)
	push	@r15,#%0000
	push	@r15,#_uchar
	call	_namei
	inc	r15,#4
	ld	r12,r2
	test	r12
	jr	eq,L4EE8
	ld	r2,%0006(r12)
	and	r2,#%F000
	cp	r2,#%4000
	jr	eq,L4EEC
	ldb	%FC13,#%14
L4EE0:
	push	@r15,r12
	call	_iput
	inc	r15,#2
L4EE8:
	jp	cret
L4EEC:
	push	@r15,#%0040
	push	@r15,r12
	call	_access
	inc	r15,#4
	test	r2
	jr	ne,L4EE0
	push	@r15,r12
	call	_prele
	inc	r15,#2
	test	@r13
	jr	eq,L4F18
	push	@r15,@r13
	call	_plock
	inc	r15,#2
	push	@r15,@r13
	call	_iput
	inc	r15,#2
L4F18:
	ld	@r13,r12
	jr	L4EE8
_chmod:
	call	csv
	ld	r12,%FC20
	call	_owner
	ld	r13,r2
	test	r13
	jr	eq,L4F74
	ld	r2,%0006(r13)
	and	r2,#%F000
	ld	%0006(r13),r2
	test	%FC16
	jr	eq,L4F44
	res	%0002(r12),#9
L4F44:
	ld	r2,%0002(r12)
	and	r2,#%0FFF
	or	r2,%0006(r13)
	ld	%0006(r13),r2
	setb	@r13,#6
	bitb	@r13,#5
	jr	eq,L4F6C
	ld	r2,%0006(r13)
	and	r2,#%0200
	jr	ne,L4F6C
	push	@r15,r13
	call	_xrele
	inc	r15,#2
L4F6C:
	push	@r15,r13
	call	_iput
	inc	r15,#2
L4F74:
	jp	cret
_chown:
	call	csv
	ld	r12,%FC20
	call	_suser
	test	r2
	jr	eq,L4FAC
	call	_owner
	ld	r13,r2
	test	r13
	jr	eq,L4FAC
	ld	r2,%0002(r12)
	ld	%000A(r13),r2
	ld	r2,%0004(r12)
	ld	%000C(r13),r2
	setb	@r13,#6
	push	@r15,r13
	call	_iput
	inc	r15,#2
L4FAC:
	jp	cret
_ssig:
	call	csv
	dec	r15,#2
	ld	r2,%FC20
	ld	%FFF2(r14),r2
	ld	r2,%FFF2(r14)
	ld	r13,@r2
	test	r13
	jr	mi,L4FD6
	jr	eq,L4FD6
	cp	r13,#%0011
	jr	ge,L4FD6
	cp	r13,#%0009
	jr	ne,L4FDE
L4FD6:
	ldb	%FC13,#%16
	jr	L5018
L4FDE:
	ld	r2,r13
	sla	r2,#1
	add	r2,#%FCD2
	ld	r3,@r2
	ld	%FC22,r3
	ld	r2,r13
	sla	r2,#1
	add	r2,#%FCD2
	ld	r3,%FFF2(r14)
	inc	r3,#2
	ld	r4,@r3
	ld	@r2,r4
	ld	r2,%FC1E
	inc	r2,#6
	ldk	r3,#1
	ld	r4,r13
	dec	r4,#1
	sda	r3,r4
	com	r3
	and	r3,@r2
	ld	@r2,r3
L5018:
	jp	cret
_kill:
	call	csv
	dec	r15,#4
	ld	r10,%FC20
	clr	%FFF2(r14)
	ld	r11,@r10
	clr	%FFF0(r14)
	cp	r11,#%FFFF
	jr	ne,L5042
	test	%FC16
	jr	ne,L5042
	inc	%FFF0(r14),#1
	xor	r11,r11
L5042:
	ld	r12,%FC1E
	ld	r13,#LA11C
	jr	L509A
L504C:
	testb	@r13
	jr	eq,L5096
	test	r11
	jr	eq,L505C
	ld	r0,%000C(r13)
	cp	r0,r11
	jr	ne,L5096
L505C:
	test	r11
	jr	ne,L5076
	ld	r0,%000A(r13)
	cp	r0,%000A(r12)
	jr	eq,L5070
	test	%FFF0(r14)
	jr	eq,L5096
L5070:
	cp	r13,#LA138
	jr	ule,L5096
L5076:
	test	%FC16
	jr	eq,L5086
	ld	r0,%FC16
	cp	r0,%0008(r13)
	jr	ne,L5096
L5086:
	inc	%FFF2(r14),#1
	push	@r15,%0002(r10)
	push	@r15,r13
	call	_psignal
	inc	r15,#4
L5096:
	add	r13,#%001C
L509A:
	cp	r13,L8A8C
	jr	ule,L504C
	test	%FFF2(r14)
	jr	ne,L50AC
	ldb	%FC13,#%03
L50AC:
	jp	cret
_times:
	call	csv
	ld	r13,%FC20
	push	@r15,#%0010
	push	@r15,@r13
	push	@r15,#%FCF4
	call	_copyout
	inc	r15,#6
	test	r2
	jr	pl,L50D2
	ldb	%FC13,#%0E
L50D2:
	jp	cret
_profil:
	call	csv
	ld	r13,%FC20
	ld	r2,@r13
	ld	%FD0A,r2
	ld	r2,%0002(r13)
	ld	%FD0C,r2
	ld	r2,%0004(r13)
	ld	%FD0E,r2
	ld	r2,%0006(r13)
	ld	%FD10,r2
	jp	cret
_alarm:
	call	csv
	ld	r11,%FC20
	ld	r13,%FC1E
	ld	r12,%001A(r13)
	ld	r2,@r11
	ld	%001A(r13),r2
	ld	%FC22,r12
	jp	cret
_pause:
	call	csv
L5122:
	push	@r15,#%0028
	push	@r15,#%FC00
	call	_sleep
	inc	r15,#4
	jr	L5122
_umask:
	call	csv
	ld	r13,%FC20
	ld	r12,%FD56
	ld	r2,@r13
	and	r2,#%01FF
	ld	%FD56,r2
	ld	%FC22,r12
	jp	cret
_utime:
	call	csv
	dec	r15,#8
	ld	r13,%FC20
	call	_owner
	ld	r12,r2
	test	r12
	jr	eq,L51AE
	push	@r15,#%0008
	ld	r2,r14
	add	r2,#%FFEC
	push	@r15,r2
	push	@r15,%0002(r13)
	call	_copyin
	inc	r15,#6
	test	r2
	jr	eq,L5186
	ldb	%FC13,#%0E
	jr	L51AE
L5186:
	ldb	rl2,@r12
	xorb	rh2,rh2
	or	r2,#%0046
	ldb	@r12,rl2
	ld	r2,r14
	dec	r2,#16
	push	@r15,r2
	ld	r2,r14
	add	r2,#%FFEC
	push	@r15,r2
	push	@r15,r12
	call	_iupdat
	inc	r15,#4
	ld	@r15,r12
	call	_iput
	inc	r15,#2
L51AE:
	jp	cret
_read:
	call	csv
	push	@r15,#%0001
	call	_rdwr
	inc	r15,#2
	jp	cret
_write:
	call	csv
	push	@r15,#%0002
	call	_rdwr
	inc	r15,#2
	jp	cret
_rdwr:
	call	csv
	ld	r13,%0004(r14)
	ld	r10,%FC20
	push	@r15,@r10
	call	_getf
	inc	r15,#2
	ld	r12,r2
	test	r12
	jr	eq,L52BA
	ldb	rl2,@r12
	xorb	rh2,rh2
	and	r2,r13
	jr	ne,L5200
	ldb	%FC13,#%09
	jr	L52BA
L5200:
	ld	r2,%0002(r10)
	ld	%FC26,r2
	ld	r2,%0004(r10)
	ld	%FC28,r2
	clrb	%FC12
	ldb	rl2,@r12
	xorb	rh2,rh2
	and	r2,#%0004
	jr	eq,L5236
	cp	r13,#%0001
	jr	ne,L522E
	push	@r15,r12
	call	_openp
L522A:
	inc	r15,#2
	jr	L52AE
L522E:
	push	@r15,r12
	call	_closep
	jr	L522A
L5236:
	ld	r11,%0002(r12)
	ldb	rl2,@r12
	xorb	rh2,rh2
	and	r2,#%0018
	jr	eq,L524E
	xor	r0,r0
	ld	r1,r0
	ldl	%FC2A,rr0
	jr	L5256
L524E:
	ldl	rr2,%0004(r12)
	ldl	%FC2A,rr2
L5256:
	ld	r2,%0006(r11)
	and	r2,#%2000
	jr	ne,L5268
	push	@r15,r11
	call	_plock
	inc	r15,#2
L5268:
	cp	r13,#%0001
	jr	ne,L5276
	push	@r15,r11
	call	_readi
	jr	L527C
L5276:
	push	@r15,r11
	call	_writei
L527C:
	inc	r15,#2
	ld	r2,%0006(r11)
	and	r2,#%2000
	jr	ne,L5290
	push	@r15,r11
	call	_prele
	inc	r15,#2
L5290:
	ldb	rl2,@r12
	xorb	rh2,rh2
	and	r2,#%0018
	jr	ne,L52AE
	ldl	rr2,%0004(r12)
	ld	r5,%0004(r10)
	sub	r5,%FC28
	xor	r4,r4
	addl	rr2,rr4
	ldl	%0004(r12),rr2
L52AE:
	ld	r2,%0004(r10)
	sub	r2,%FC28
	ld	%FC22,r2
L52BA:
	jp	cret
_open:
	call	csv
	ld	r12,%FC20
	push	@r15,#%0000
	push	@r15,#_uchar
	call	_namei
	inc	r15,#4
	ld	r13,r2
	test	r13
	jr	eq,L52EE
	push	@r15,#%0000
	inc	%0002(r12),#1
	push	@r15,%0002(r12)
	push	@r15,r13
	call	_copen
	inc	r15,#6
L52EE:
	jp	cret
_creat:
	call	csv
	ld	r12,%FC20
	push	@r15,#%0001
	push	@r15,#_uchar
	call	_namei
	inc	r15,#4
	ld	r13,r2
	test	r13
	jr	ne,L5330
	testb	%FC13
	jr	ne,L5340
	ld	r2,%0002(r12)
	and	r2,#%0DFF
	push	@r15,r2
	call	_maknode
	inc	r15,#2
	ld	r13,r2
	test	r13
	jr	eq,L5340
	push	@r15,#%0002
	jr	L5334
L5330:
	push	@r15,#%0001
L5334:
	push	@r15,#%0002
	push	@r15,r13
	call	_copen
	inc	r15,#6
L5340:
	jp	cret
_copen:
	call	csv
	ld	r13,%0004(r14)
	ld	r12,%0006(r14)
	dec	r15,#2
	cp	%0008(r14),#%0002
	jr	eq,L538E
	bit	r12,#0
	jr	eq,L536A
	push	@r15,#%0100
	push	@r15,r13
	call	_access
	inc	r15,#4
L536A:
	bit	r12,#1
	jr	eq,L538E
	push	@r15,#%0080
	push	@r15,r13
	call	_access
	inc	r15,#4
	ld	r2,%0006(r13)
	and	r2,#%F000
	cp	r2,#%4000
	jr	ne,L538E
	ldb	%FC13,#%15
L538E:
	testb	%FC13
	jr	ne,L53F2
	cp	%0008(r14),#%0001
	jr	ne,L53A4
	push	@r15,r13
	call	_itrunc
	inc	r15,#2
L53A4:
	push	@r15,r13
	call	_prele
	inc	r15,#2
	call	_falloc
	ld	r11,r2
	test	r11
	jr	eq,L53F2
	ld	r2,r12
	and	r2,#%0003
	ldb	@r11,rl2
	ld	%0002(r11),r13
	ld	r2,%FC22
	ld	%FFF2(r14),r2
	ld	r2,r12
	and	r2,#%0002
	push	@r15,r2
	push	@r15,r13
	call	_openi
	inc	r15,#4
	testb	%FC13
	jr	eq,L53FA
	ld	r2,%FFF2(r14)
	sla	r2,#1
	add	r2,#%FC62
	clr	@r2
	decb	%0001(r11),#1
L53F2:
	push	@r15,r13
	call	_iput
	inc	r15,#2
L53FA:
	jp	cret
_close:
	call	csv
	ld	r12,%FC20
	push	@r15,@r12
	call	_getf
	inc	r15,#2
	ld	r13,r2
	test	r13
	jr	eq,L5428
	ld	r2,@r12
	sla	r2,#1
	add	r2,#%FC62
	clr	@r2
	push	@r15,r13
	call	_closef
	inc	r15,#2
L5428:
	jp	cret
_seek:
	call	csv
	ld	r12,%FC20
	push	@r15,@r12
	call	_getf
	inc	r15,#2
	ld	r13,r2
	test	r13
	jr	eq,L54A6
	ldb	rl2,@r13
	xorb	rh2,rh2
	and	r2,#%001C
	jr	eq,L5454
	ldb	%FC13,#%1D
	jr	L54A6
L5454:
	cp	%0006(r12),#%0001
	jr	ne,L546A
	ldl	rr2,%0002(r12)
	addl	rr2,%0004(r13)
	ldl	%0002(r12),rr2
	jr	L5482
L546A:
	cp	%0006(r12),#%0002
	jr	ne,L5482
	ld	r2,%0002(r13)
	inc	r2,#14
	ldl	rr4,%0002(r12)
	addl	rr4,@r2
	ldl	%0002(r12),rr4
L5482:
	testl	%0002(r12)
	jr	pl,L5496
	ldb	%FC13,#%16
	xor	r0,r0
	ld	r1,r0
	ldl	%0002(r12),rr0
L5496:
	ldl	rr2,%0002(r12)
	ldl	%0004(r13),rr2
	ldl	rr2,%0002(r12)
	ldl	%FC22,rr2
L54A6:
	jp	cret
_link:
	call	csv
	ld	r11,%FC20
	push	@r15,#%0000
	push	@r15,#_uchar
	call	_namei
	inc	r15,#4
	ld	r13,r2
	test	r13
	jr	eq,L5552
	ld	r2,%0006(r13)
	and	r2,#%F000
	cp	r2,#%4000
	jr	ne,L54DC
	call	_suser
	test	r2
	jr	eq,L554A
L54DC:
	push	@r15,r13
	call	_prele
	inc	r15,#2
	ld	r2,%0002(r11)
	ld	%FC40,r2
	push	@r15,#%0001
	push	@r15,#_uchar
	call	_namei
	inc	r15,#4
	ld	r12,r2
	test	r12
	jr	eq,L5510
	ldb	%FC13,#%11
	push	@r15,r12
	call	_iput
	inc	r15,#2
	jr	L554A
L5510:
	testb	%FC13
	jr	ne,L554A
	ld	r2,%FC52
	inc	r2,#2
	ld	r0,@r2
	cp	r0,%0002(r13)
	jr	eq,L5536
	push	@r15,%FC52
	call	_iput
	inc	r15,#2
	ldb	%FC13,#%12
	jr	L554A
L5536:
	push	@r15,r13
	call	_wdir
	inc	r15,#2
	testb	%FC13
	jr	ne,L554A
	inc	%0008(r13),#1
	setb	@r13,#6
L554A:
	push	@r15,r13
	call	_iput
	inc	r15,#2
L5552:
	jp	cret
_mknod:
	call	csv
	ld	r12,%FC20
	call	_suser
	test	r2
	jr	eq,L5582
	push	@r15,#%0001
	push	@r15,#_uchar
	call	_namei
	inc	r15,#4
	ld	r13,r2
	test	r13
	jr	eq,L5582
	ldb	%FC13,#%11
	jr	L55A2
L5582:
	testb	%FC13
	jr	ne,L55AA
	push	@r15,%0002(r12)
	call	_maknode
	inc	r15,#2
	ld	r13,r2
	test	r13
	jr	eq,L55AA
	ld	r3,%0004(r12)
	exts	rr2
	ldl	%0012(r13),rr2
L55A2:
	push	@r15,r13
	call	_iput
	inc	r15,#2
L55AA:
	jp	cret
end _p_sys2
end sys2
