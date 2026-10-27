c_ module
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
X	csv	PR
X	cret	PR;
G
_tab	L
_tab:
WV %1
WV %2
WV %3
WV %4
G
_ptab	L
_ptab:
WV _tab
WV %4+_tab
G
_str	L
_str:
BV %68;BV %61;BV %6C;BV %6C;BV %6F;BV %0
G
_msg	L
_msg:
WV L2
G
_cb	L
_cb:
BV %5
$EVEN
G
_lv	L
_lv:
LV %186A0
X	_zero	W
X	_zb	long
X	_sv	A[%A B]
X	_sp	W
X	_fp	W
G	_f1	PR
~~f1:
E
call	csv
~a:=%4
~b:=%6
jp	L3
L4:
~i:=r13
~j:=r12
~k:=%FFF2
ld	r3,%4(FP)
exts	rr2
div	rr2,%6(FP)
ld	r2,r3
ld	r5,%4(FP)
mult	rr4,%6(FP)
ld	r3,r5
add	r2,r3
ld	r5,%4(FP)
exts	rr4
div	rr4,%6(FP)
ld	r3,r4
sub	r2,r3
ld	-%E(FP),r2
ld	r2,%6(FP)
ld	r3,%4(FP)
xor	r2,r3
ld	r3,%6(FP)
sra	r3,#%2
add	r2,r3
ld	r3,%4(FP)
and	r3,%6(FP)
add	r2,r3
ld	r3,%4(FP)
or	r3,%6(FP)
add	r2,r3
ld	r3,%4(FP)
sla	r3,#%3
add	r2,r3
ld	r3,%4(FP)
com	r3
add	r2,r3
ld	r3,%6(FP)
neg	r3
add	r2,r3
ld	-%E(FP),r2
clr	r13
L6:
cp	r13,#%A
jp	ge	L7
ld	r0,%4(FP)
cp	r0,%6(FP)
jp	le	L9
inc	-%E(FP),#%1
jp	L10
L9:
dec	-%E(FP),#%1
L10:
ld	r0,%4(FP)
cp	r0,%6(FP)
jp	eq	L6000
cp	r13,#%3
jp	eq	L11
L6000:
inc	-%E(FP),#%2
L11:
L12:
test	r12
jp	eq	L13
dec	r12,#%1
jp	L12
L13:
L16:
dec	-%E(FP),#%1
L14:
cp	-%E(FP),#%64
jp	gt	L16
L15:
L8:
inc	r13,#%1
jp	L6
L7:
ld	r2,%4(FP)
jp	L18
L19:
ld	-%E(FP),#%A
jp	L17
L20:
ld	-%E(FP),#%14
jp	L17
L21:
ld	-%E(FP),#%1E
jp	L17
L22:
ld	-%E(FP),#%28
jp	L17
L23:
clr	-%E(FP)
jp	L17
L18:
cp	r2,#%1
jp	eq	L19
cp	r2,#%2
jp	eq	L20
cp	r2,#%3
jp	eq	L21
cp	r2,#%C8
jp	eq	L22
jp	L23
L17:
test	-%E(FP)
jp	eq	L6002
ld	r2,r13
jp	L6003
L6002:
ld	r2,r12
L6003:
jp	L5
L5:
jp	cret
L3:
dec	SP,#%2
jp	L4
end _f1
I
G	_f2	PR
~~f2:
E
call	csv
~p:=%4
~q:=%6
jp	L24
L25:
L27:
ld	r2,%4(FP)
inc	%4(FP),#%1
ld	r3,%6(FP)
inc	%6(FP),#%1
ldb	rl4,@r3
xorb	rh4,rh4
ldb	@r2,rl4
testb	rl4
jp	eq	L28
jp	L27
L28:
ld	r2,_sp
ld	@r2,#%3
ld	r2,_sp
inc	r2,#%2
ldb	@r2,#%78
ld	r2,_sp
inc	r2,#%8
ld	r3,@r2
inc	r3,#%4
ld	r2,r3
ldk	r5,#%7
exts	rr4
ldl	@r2,rr4
inc	_sv,#%1
call	_a
sla	r2,#%1
add	r2,#_tab
ld	@r2,#%5
push	@SP,%4(FP)
push	@SP,#%1
ld	r2,_fp
call	@r2
inc	SP,#%4
add	r2,_zero
jp	L26
L26:
jp	cret
L24:
jp	L25
end _f2
I
G	_f3	PR
~~f3:
E
call	csv
~u:=%4
~v:=%6
jp	L29
L30:
~c:=%FFF3
ld	r2,%4(FP)
ldb	-%D(FP),rl2
ldb	rl2,-%D(FP)
xorb	rh2,rh2
ldb	%1+_zb,rl2
ld	r0,%4(FP)
cp	r0,%6(FP)
jp	uge	L32
ld	r3,%4(FP)
xor	r2,r2
div	rr2,%6(FP)
ld	r2,r3
jp	L31
L32:
ld	r2,%4(FP)
srl	r2,#%1
jp	L31
L31:
jp	cret
L29:
dec	SP,#%2
jp	L30
end _f3
I
G	_f4	PR
~~f4:
E
call	csv
~x:=%4
~y:=%8
jp	L33
L34:
~z:=%FFF0
ldl	rr2,%4(FP)
multl	rq0,%8(FP)
ldl	rr6,%4(FP)
extsl	rq4
divl	rq4,%8(FP)
ldl	rr4,rr6
addl	rr2,rr4
ldl	-%10(FP),rr2
ldl	rr2,%4(FP)
slal	rr2,#%2
ldl	-%10(FP),rr2
ldl	rr2,%4(FP)
ld	r4,#-%3
sdal	rr2,r4
ldl	-%10(FP),rr2
ldl	rr2,-%10(FP)
addl	rr2,_lv
ldl	-%10(FP),rr2
ldl	rr0,%4(FP)
cpl	rr0,%8(FP)
jp	le	L36
ldl	rr2,-%10(FP)
com	r3
com	r2
addl	rr2,#%1
ldl	-%10(FP),rr2
L36:
push	@SP,r2
push	@SP,#%2
push	@SP,#%1
call	_f3
inc	SP,#%4
ld	r3,r2
pop	r2,@SP
xor	r2,r2
ld	r5,#%FF
exts	rr4
and	r4,%4(FP)
and	r5,%6(FP)
addl	rr2,rr4
addl	rr2,-%10(FP)
jp	L35
L35:
jp	cret
L33:
dec	SP,#%4
jp	L34
end _f4
I
L2:
BV %77;BV %65;BV %6C;BV %74;BV %0
end c_
