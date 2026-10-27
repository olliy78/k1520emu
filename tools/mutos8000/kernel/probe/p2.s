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
G	_main	PR
~~main:
E
call	csv
~argc:=%4
~argv:=%6
jp	L1
L2:
~i:=%FFF2
~buf:=%FFCA
~fp:=%FFC8
$DATA
$SECTION c__d
I
L4:
WV %7
$CODE
~cnt:=L4
clr	-%E(FP)
L5:
ld	r0,-%E(FP)
cp	r0,%4(FP)
jp	ge	L6
push	@SP,L4
inc	L4,#%1
ld	r2,-%E(FP)
sla	r2,#%1
add	r2,%6(FP)
push	@SP,@r2
push	@SP,#L8
call	_printf
inc	SP,#%6
L7:
inc	-%E(FP),#%1
jp	L5
L6:
push	@SP,#L10
push	@SP,#L9
call	_fopen
inc	SP,#%4
ld	-%38(FP),r2
test	-%38(FP)
jp	ne	L11
push	@SP,#%1
call	_exit
inc	SP,#%2
L11:
push	@SP,-%E(FP)
push	@SP,#L12
ld	r2,FP
add	r2,#-%36
push	@SP,r2
call	_sprintf
inc	SP,#%6
push	@SP,#-%3
call	_abs
inc	SP,#%2
push	@SP,r2
ld	r2,FP
add	r2,#-%36
push	@SP,r2
call	_strlen
inc	SP,#%2
pop	r0,@SP
add	r2,r0
add	r2,#%28
jp	L3
L3:
jp	cret
L1:
sub	SP,#%2C
jp	L2
end _main
I
L8:
BV %25;BV %73;BV %20;BV %25;BV %64;BV %A;BV %0
L9:
BV %78;BV %0
L10:
BV %72;BV %0
L12:
BV %25;BV %78;BV %0
end c_
