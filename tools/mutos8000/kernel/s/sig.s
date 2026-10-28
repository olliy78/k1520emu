sig module
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
_signal	L
G
_psignal	L
G
_issig	L
G
_psig	L
G
_fsig	L
G
L0E38	L
G
_ptrace	L
G	_p_sig	PR
E
_signal:
	call	csv
	ld	r13,%0004(r14)
	test	r13
	jr	eq,L0B6A
	ld	r12,#LA11C
	jr	L0B64
L0B4C:
	ld	r0,%000A(r12)
	cp	r0,r13
	jr	ne,L0B60
	push	@r15,%0006(r14)
	push	@r15,r12
	call	_psignal
	inc	r15,#4
L0B60:
	add	r12,#%001C
L0B64:
	cp	r12,L8A8C
	jr	ule,L0B4C
L0B6A:
	jp	cret
_psignal:
	call	csv
	ld	r13,%0004(r14)
	ld	r12,%0006(r14)
	cp	r12,#%0011
	jr	uge,L0BC4
	test	r12
	jr	eq,L0B96
	ldk	r2,#1
	ld	r3,r12
	dec	r3,#1
	sda	r2,r3
	or	r2,%0006(r13)
	ld	%0006(r13),r2
L0B96:
	ldb	rl2,%0002(r13)
	xorb	rh2,rh2
	cp	r2,#%0032
	jr	ule,L0BA8
	ldb	%0002(r13),#%32
L0BA8:
	ldb	rl2,@r13
	xorb	rh2,rh2
	cp	r2,#%0001
	jr	ne,L0BC4
	ldb	rl2,%0002(r13)
	cp	r2,#%0019
	jr	ule,L0BC4
	push	@r15,r13
	call	_setrun
	inc	r15,#2
L0BC4:
	jp	cret
_issig:
	call	csv
	ld	r12,%FC1E
	jr	L0C0E
L0BD2:
	push	@r15,r12
	call	_fsig
	inc	r15,#2
	ld	r13,r2
	ld	r2,r13
	sla	r2,#1
	add	r2,#%FCD2
	ld	r3,@r2
	and	r3,#%0001
	jr	eq,L0BF4
	bitb	%0001(r12),#4
	jr	eq,L0BFA
L0BF4:
	ld	r2,r13
L0BF6:
	jp	cret
L0BFA:
	ldk	r2,#1
	ld	r3,r13
	dec	r3,#1
	sda	r2,r3
	com	r2
	and	r2,%0006(r12)
	ld	%0006(r12),r2
L0C0E:
	test	%0006(r12)
	jr	ne,L0BD2
	xor	r2,r2
	jr	L0BF6
_stop:
	call	csv
	jr	L0C42
L0C1E:
	push	@r15,r13
	call	_wakeup
	inc	r15,#2
	ldb	@r12,#%06
	call	_swtch
	ldb	rl2,%0001(r12)
	xorb	rh2,rh2
	and	r2,#%0010
	jr	eq,L0C78
	call	_procxmt
	test	r2
	jr	ne,L0C78
L0C42:
	ld	r12,%FC1E
	cp	%000E(r12),#%0001
	jr	eq,L0C68
	ld	r13,#LA11C
	jr	L0C62
L0C54:
	ld	r0,%000C(r13)
	cp	r0,%000E(r12)
	jr	eq,L0C1E
	add	r13,#%001C
L0C62:
	cp	r13,L8A8C
	jr	ule,L0C54
L0C68:
	push	@r15,%FC1E
	call	_fsig
	ld	@r15,r2
	call	_exit
	inc	r15,#2
L0C78:
	jp	cret
_psig:
	call	csv
	ld	r11,%FC1E
	test	%FD5A
	jr	ne,L0C9A
	push	@r15,#%FD5C
	call	L03F8
	inc	r15,#2
	ld	%FD5A,#%0001
L0C9A:
	bitb	%0001(r11),#4
	jr	eq,L0CA4
	call	_stop
L0CA4:
	push	@r15,r11
	call	_fsig
	inc	r15,#2
	ld	r13,r2
	test	r13
	jr	eq,L0D28
	ldk	r2,#1
	ld	r3,r13
	dec	r3,#1
	sda	r2,r3
	com	r2
	and	r2,%0006(r11)
	ld	%0006(r11),r2
	ld	r2,r13
	sla	r2,#1
	add	r2,#%FCD2
	ld	r12,@r2
	test	r12
	jr	eq,L0CFE
	clrb	%FC13
	cp	r13,#%0004
	jr	eq,L0CF2
	cp	r13,#%0005
	jr	eq,L0CF2
	ld	r2,r13
	sla	r2,#1
	add	r2,#%FCD2
	clr	@r2
L0CF2:
	push	@r15,r13
	push	@r15,r12
	call	_sendsig
	inc	r15,#4
	jr	L0D28
L0CFE:
	ld	r2,r13
	sub	r2,#%0003
	cp	r2,#%0009
	jr	ugt,L0D20
	add	r2,r2
	ld	r1,L89EA(r2)
	jp	@r1
	call	_core
	test	r2
	jr	eq,L0D20
	add	r13,#%0080
	jr	L0D20
L0D20:
	push	@r15,r13
	call	_exit
	inc	r15,#2
L0D28:
	jp	cret
_fsig:
	call	csv
	ld	r2,%0004(r14)
	inc	r2,#6
	ld	r13,@r2
	ldk	r12,#1
	jr	L0D4C
L0D3C:
	bit	r13,#0
	jr	eq,L0D46
	ld	r2,r12
L0D42:
	jp	cret
L0D46:
	sra	r13,#1
	inc	r12,#1
L0D4C:
	cp	r12,#%0011
	jr	lt,L0D3C
	xor	r2,r2
	jr	L0D42
_core:
	call	csv
	clrb	%FC13
	ld	r2,#L8A10
	ld	%FC40,r2
	push	@r15,#%0001
	push	@r15,#L39A0
	call	_namei
	inc	r15,#4
	ld	r13,r2
	test	r13
	jr	ne,L0D90
	testb	%FC13
	jr	ne,L0E2E
	push	@r15,#%01B6
	call	_maknode
	inc	r15,#2
	ld	r13,r2
	test	r13
	jr	eq,L0E2E
L0D90:
	push	@r15,#%0080
	push	@r15,r13
	call	_access
	inc	r15,#4
	test	r2
	jr	ne,L0E20
	ld	r2,%0006(r13)
	and	r2,#%F000
	cp	r2,#%8000
	jr	ne,L0E20
	ld	r0,%FC16
	cp	r0,%FC1A
	jr	ne,L0E20
	push	@r15,r13
	call	_itrunc
	inc	r15,#2
	xor	r0,r0
	ld	r1,r0
	ldl	%FC2A,rr0
	ld	r2,#%FC00
	ld	%FC26,r2
	ld	%FC28,#%0400
	ldb	%FC12,#%01
	push	@r15,r13
	call	_writei
	inc	r15,#2
	clr	%FC26
	ld	r2,%FCAA
	sll	r2,#8
	ld	%FC28,r2
	clrb	%FC12
	push	@r15,r13
	call	_writei
	inc	r15,#2
	ld	r2,%FD04
	inc	r2,#16
	ld	r12,@r2
	and	r12,#%FF00
	ld	%FC26,r12
	xor	r2,r2
	sub	r2,r12
	ld	%FC28,r2
	push	@r15,r13
	call	_writei
	inc	r15,#2
L0E20:
	push	@r15,r13
	call	_iput
	inc	r15,#2
	testb	%FC13
	jr	eq,L0E34
L0E2E:
	xor	r2,r2
L0E30:
	jp	cret
L0E34:
	ldk	r2,#1
	jr	L0E30
L0E38:
	call	csv
	xor	r2,r2
	jp	cret
_ptrace:
	call	csv
	ld	r12,%FC20
	test	@r12
	jr	eq,L0E50
	jr	pl,L0E5A
L0E50:
	ld	r2,%FC1E
	inc	r2,#1
	setb	@r2,#4
	jr	L0F0E
L0E5A:
	ld	r13,#LA11C
	jr	L0E86
L0E60:
	ldb	rl2,@r13
	xorb	rh2,rh2
	cp	r2,#%0006
	jr	ne,L0E82
	ld	r0,%000C(r13)
	cp	r0,%0002(r12)
	jr	ne,L0E82
	ld	r2,%FC1E
	inc	r2,#12
	ld	r0,@r2
	cp	r0,%000E(r13)
	jr	eq,L0E94
L0E82:
	add	r13,#%001C
L0E86:
	cp	r13,L8A8C
	jr	ule,L0E60
	ldb	%FC13,#%03
	jr	L0F0E
L0E94:
	test	LA620
	jr	eq,L0EAA
	push	@r15,#%0019
	push	@r15,#LA620
	call	_sleep
	inc	r15,#4
	jr	L0E94
L0EAA:
	ld	r2,%000C(r13)
	ld	LA620,r2
	ld	r2,%0006(r12)
	ld	LA626,r2
	ld	r2,%0004(r12)
	ld	LA624,r2
	ld	r2,@r12
	ld	LA622,r2
	resb	%0001(r13),#5
	push	@r15,r13
	call	_setrun
	inc	r15,#2
	jr	L0EE6
L0ED6:
	jr	eq,L0EEC
	push	@r15,#%0019
	push	@r15,#LA620
	call	_sleep
	inc	r15,#4
L0EE6:
	test	LA622
	jr	pl,L0ED6
L0EEC:
	ld	r2,LA626
	ld	%FC22,r2
	test	LA622
	jr	pl,L0F00
	ldb	%FC13,#%05
L0F00:
	clr	LA620
	push	@r15,#LA620
	call	_wakeup
	inc	r15,#2
L0F0E:
	jp	cret
_procxmt:
	call	csv
	ld	r2,%FC1E
	inc	r2,#12
	ld	r0,@r2
	cp	r0,LA620
	jr	eq,L0F2A
L0F24:
	xor	r2,r2
L0F26:
	jp	cret
L0F2A:
	ld	r13,LA622
	clr	LA622
	push	@r15,#LA620
	call	_wakeup
	inc	r15,#2
	ld	r2,r13
	sub	r2,#%0001
	cp	r2,#%0008
	jp	ugt,L1112
	add	r2,r2
	ld	r1,L89FE(r2)
	jp	@r1
	push	@r15,LA624
	call	_fubyte
	inc	r15,#2
	cp	r2,#%FFFF
	jp	eq,L1112
	push	@r15,LA624
	call	_fuword
	inc	r15,#2
	ld	LA626,r2
	jr	L0F24
	ld	r13,LA624
	test	r13
	jp	mi,L1112
	cp	r13,#%0400
	jp	ge,L1112
	ld	r2,r13
	sra	r2,#1
	sla	r2,#1
	add	r2,#%FC00
	ld	r3,@r2
	ld	LA626,r3
	jr	L0F24
	ld	r2,%FC1E
	add	r2,#%0016
	ld	r3,@r2
	ld	r11,r3
	test	r3
	jr	eq,L0FCC
	ldb	rl2,%0008(r11)
	xorb	rh2,rh2
	cp	r2,#%0001
	jp	ne,L1112
	ld	r2,%0006(r11)
	inc	r2,#6
	bit	@r2,#9
	jp	ne,L1112
	ld	r2,%0006(r11)
	resb	@r2,#5
L0FCC:
	push	@r15,#%0006
	ldb	rl0,%FD13
	xorb	rh0,rh0
	push	@r15,r0
	push	@r15,%FCAC
	push	@r15,%FCAA
	push	@r15,%FCA8
	call	_estabur
	inc	r15,#10
	push	@r15,#%0000
	push	@r15,LA624
	call	_suword
	inc	r15,#4
	ld	r13,r2
	push	@r15,LA626
	push	@r15,LA624
	call	_suword
	inc	r15,#2
	ld	@r15,#%0002
	ldb	rl0,%FD13
	xorb	rh0,rh0
	push	@r15,r0
	push	@r15,%FCAC
	push	@r15,%FCAA
	push	@r15,%FCA8
	call	_estabur
	inc	r15,#10
	test	r13
	jr	mi,L1112
	test	r11
	jp	eq,L0F24
	setb	%000A(r11),#1
	jp	L0F24
	push	@r15,#%0000
	push	@r15,LA624
	call	_suword
	inc	r15,#4
	test	r2
	jr	mi,L1112
	push	@r15,LA626
	push	@r15,LA624
	call	_suword
	inc	r15,#4
	jp	L0F24
	ld	r13,LA624
	ld	r2,r13
	sra	r2,#1
	sla	r2,#1
	ld	r12,r2
	add	r12,#%FC00
	cp	r12,#%FD5C
	jr	ult,L107C
	cp	r12,#%FDEC
	jr	ult,L10B6
L107C:
	xor	r13,r13
	jr	L1094
L1080:
	ldb	rl2,L88CC(r13)
	xorb	rh2,rh2
	sla	r2,#1
	add	r2,%FD04
	cp	r2,r12
	jr	eq,L10B6
	inc	r13,#1
L1094:
	cp	r13,#%0010
	jr	lt,L1080
	ld	r2,%FD04
	add	r2,#%0014
	cp	r2,r12
	jr	ne,L1112
	ld	r2,LA626
	and	r2,#LB7FE+%1
	ld	LA626,r2
	set	LA626,#12
L10B6:
	ld	r2,LA626
	ld	@r12,r2
	jp	L0F24
	ld	r2,%FD04
	add	r2,#%0014
	set	@r2,#11
	cp	LA624,#%0001
	jr	eq,L10E0
	ld	r2,%FD04
	add	r2,#%0018
	ld	r3,LA624
	ld	@r2,r3
L10E0:
	ld	r2,%FC1E
	inc	r2,#6
	clr	@r2
	test	LA626
	jr	eq,L10FC
	push	@r15,LA626
	push	@r15,%FC1E
	call	_psignal
	inc	r15,#4
L10FC:
	ldk	r2,#1
	jp	L0F26
	push	@r15,%FC1E
	call	_fsig
	ld	@r15,r2
	call	_exit
	inc	r15,#2
L1112:
	ld	r2,#%FFFF
	ld	LA622,r2
	jp	L0F24
	WV %5E08;WV %0F24
end _p_sig
end sig
