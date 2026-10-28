tty module
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
_tcopen	L
G
_tcclose	L
G
_tcread	L
G
_tcwrite	L
G
_tcxint	L
G
_tcrint	L
G
_tcioctl	L
G
_putchar	L
G	_p_tty	PR
E
_tcopen:
	call	csv
	ld	r11,%0004(r14)
	and	r11,#%00FF
	cp	r11,#%0004
	jr	lt,L60E6
	ldb	%FC13,#%06
	jr	L6154
L60E6:
	ld	r3,r11
	mult	rr2,#%003C
	ld	r2,r3
	ld	r13,r3
	add	r13,#LA530
	ld	r3,r11
	mult	rr2,#%0050
	ld	r2,r3
	add	r2,#%D2A2
	ld	%0012(r13),r2
	ld	r3,r11
	mult	rr2,#%000E
	ld	r12,r3
	add	r12,#%D26A
	ld	%0038(r13),r12
	clrb	%003A(r13)
	ld	r2,#_ttstart
	ld	%000C(r13),r2
	ld	r2,%0028(r13)
	and	r2,#%0004
	jr	ne,L6158
	ld	%0028(r13),#%0014
	ld	%001A(r13),#%0518
	test	r11
	jr	eq,L6140
	ldb	%0003(r12),#%01
L6140:
	push	@r15,r13
	call	_ttinit
	inc	r15,#2
L6148:
	push	@r15,r13
	push	@r15,%0004(r14)
	call	_ttyopen
	inc	r15,#4
L6154:
	jp	cret
L6158:
	bit	%0028(r13),#7
	jr	eq,L6148
	test	%FC16
	jr	eq,L6148
	ldb	%FC13,#%10
	jr	L6154
_tcclose:
	call	csv
	ld	r3,%0004(r14)
	and	r3,#%00FF
	mult	rr2,#%003C
	ld	r13,r3
	add	r13,#LA530
	push	@r15,r13
	call	L6DD0
	inc	r15,#2
	jp	cret
_tcread:
	call	csv
	ld	r3,%0004(r14)
	and	r3,#%00FF
	mult	rr2,#%003C
	ld	r2,r3
	add	r2,#LA530
	push	@r15,r2
	call	_ttread
	inc	r15,#2
	jp	cret
_tcwrite:
	call	csv
	ld	r3,%0004(r14)
	and	r3,#%00FF
	mult	rr2,#%003C
	ld	r2,r3
	add	r2,#LA530
	push	@r15,r2
	call	_ttwrite
	inc	r15,#2
	jp	cret
_tcxint:
	call	csv
	ld	r3,%0004(r14)
	and	r3,#%00FF
	mult	rr2,#%003C
	ld	r13,r3
	add	r13,#LA530
	push	@r15,r13
	call	_ttkick
	inc	r15,#2
	test	%0006(r13)
	jr	eq,L61FE
	cp	%0006(r13),#%0032
	jr	gt,L620A
L61FE:
	ld	r2,r13
	inc	r2,#6
	push	@r15,r2
	call	_wakeup
	inc	r15,#2
L620A:
	jp	cret
_tcrint:
	call	csv
	ld	r13,%0006(r14)
	ld	r3,%0004(r14)
	and	r3,#%00FF
	mult	rr2,#%003C
	ld	r11,r3
	add	r11,#LA530
	ld	r12,r13
	srl	r12,#8
	push	@r15,r11
	push	@r15,r12
	call	L7222
	inc	r15,#4
	jp	cret
_tcioctl:
	call	csv
	push	@r15,%0004(r14)
	push	@r15,%0008(r14)
	ld	r3,%0004(r14)
	and	r3,#%00FF
	mult	rr2,#%003C
	ld	r2,r3
	add	r2,#LA530
	push	@r15,r2
	push	@r15,%0006(r14)
	call	_ttioctl
	inc	r15,#8
	test	r2
	jr	ne,L6270
	ldb	%FC13,#%19
L6270:
	jp	cret
_ttstart:
	call	csv
	ld	r13,%0004(r14)
	ld	r9,%0038(r13)
	ld	r10,%0012(r13)
	testb	%0003(r9)
	jr	eq,L631A
	bit	%0028(r13),#8
	jr	ne,L631A
	bit	%002A(r13),#1
	jr	ne,L631A
	xor	r11,r11
	jr	L62B6
L629A:
	ld	r2,r10
	inc	r10,#1
	ld	r3,r12
	and	r3,#%007F
L62A4:
	ldb	@r2,rl3
	ld	r2,r11
	inc	r11,#1
	cp	r2,#%0028
	jr	le,L62B6
	cp	r12,#%000A
	jr	eq,L62EC
L62B6:
	cp	r11,#%0050
	jr	ge,L62EC
	ld	r2,r13
	inc	r2,#6
	push	@r15,r2
	call	_getc
	inc	r15,#2
	ld	r12,r2
	test	r12
	jr	mi,L62EC
	bit	%001A(r13),#5
	jr	ne,L629A
	cp	r12,#%007F
	jr	gt,L62E2
	ld	r2,r10
	inc	r10,#1
	ld	r3,r12
	jr	L62A4
L62E2:
	cp	r12,#%0080
	jr	ne,L631E
	set	%002A(r13),#1
L62EC:
	ld	r2,r11
	ld	%0036(r13),r11
	test	r11
	jr	mi,L6300
	jr	eq,L6300
	push	@r15,r13
	call	_ttxfer
	inc	r15,#2
L6300:
	test	r12
	jr	pl,L631A
	bit	%0028(r13),#6
	jr	eq,L631A
	res	%0028(r13),#6
	ld	r2,r13
	inc	r2,#6
	push	@r15,r2
	call	_wakeup
	inc	r15,#2
L631A:
	jp	cret
L631E:
	ld	r2,r12
	and	r2,#%007F
	inc	r2,#4
	push	@r15,r2
	push	@r15,r13
	push	@r15,#_ttrstrt
	call	_timeout
	inc	r15,#6
	set	%0028(r13),#0
	jr	L62EC
_ttxfer:
	call	csv
	ld	r13,%0004(r14)
	ld	r12,%0038(r13)
	ldb	@r12,#%04
	ld	r2,%0014(r13)
	and	r2,#%00FF
	ldb	%0001(r12),rl2
	clrb	%0003(r12)
	clr	%0004(r12)
	ld	r2,%0012(r13)
	ld	%0006(r12),r2
	ld	r2,%0036(r13)
	ld	%0008(r12),r2
	ldb	%0002(r12),#%01
	push	@r15,#%0004
	push	@r15,r12
	call	L5CAE
	inc	r15,#4
	jp	cret
_putchar:
	call	csv
	ld	r13,%0004(r14)
	test	r13
	jr	eq,L63AE
	ld	r2,L8C96
	inc	L8C96,#1
	ld	r3,r13
	ldb	@r2,rl3
	ld	r0,L8C96
	cp	r0,#L8E68
	jr	ult,L63AE
	ld	r2,#L8DE8
	ld	L8C96,r2
L63AE:
	ld	r10,#%D26A
	ld	r9,#%D2A2
	ld	r11,#%2710
L63BA:
	testb	%0003(r10)
	jr	ne,L63C2
	djnz	r11,L63BA
L63C2:
	test	r13
	jr	eq,L642E
	push	@r15,#%0007
	call	_splx
	inc	r15,#2
	ld	r12,r2
	ldb	@r10,#%04
	clrb	%0001(r10)
	clrb	%0003(r10)
	clr	%0004(r10)
	ld	%0006(r10),r9
	ld	%0008(r10),#%0001
	ldb	%0002(r10),#%81
	ld	r2,r13
	ldb	@r9,rl2
	cp	r13,#%000A
	jr	ne,L640A
	inc	r9,#1
	ld	r2,r9
	ldb	@r2,#%0D
	ld	%0008(r10),#%0002
L640A:
	push	@r15,#%0004
	push	@r15,r10
	call	L5CAE
	inc	r15,#2
	ld	@r15,#%0000
	call	_putchar
	inc	r15,#2
	ldb	%0003(r10),#%01
	push	@r15,r12
	call	_splx
	inc	r15,#2
L642E:
	jp	cret
end _p_tty
end tty
