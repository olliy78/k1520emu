;******************************************************************************
;******************************************************************************
;
;	O S / M - Quelle	(C) ZFT/KEAW Abt. Baisisoftware - 1987
;
;	PROGRAMM	BS1715.S
;
;
;	Bearbeiter:	P. Hoge
;	Datum:		23.12.87
;	Version:	1.2
;
;******************************************************************************
;
;	Umsetzung der Steuerzeichenfolgen fuer den 1715-Bildschirm
;	in ESC-Folgen fuer das P8000-Terminal (ADM31)
;
;Generierung einer neuen Version von BS1715.COM erfolgt unter UDOS:
;
;	%EDIT BS1715.S
;	%ASM BS1715.S (A)
;	%PLINK BS1715 (NOM)
;	%COPY BS1715 $NULL
;	OS/M-Diskette in Laufwerk 0 einlegen
;	%D
;	>OS
;	A:DDT PIP.COM
;	-M4880,4A7F,100
;	-^C
;	A:SAVE 2 BS1715.COM
;
;	max. freier RAM im BIOS von FE00H - FEFFH
;
;******************************************************************************
;******************************************************************************
 

 
	ORG	100H
 
BDOS:	EQU	0005H
WRITE:	EQU	09H
CRLF:	EQU	0A0DH
 
LOAD:	LD	HL,(0001H)	;TEST OB FILTER BEREITS GELADEN
	LD	DE,10		;CO+1 IM BIOS
	ADD	HL,DE
	LD	E,(HL)
	INC	HL
	LD	D,(HL)
	LD	HL,ENTRY
	OR	A
	SBC	HL,DE
	JR	Z,ERROR		;BS1715 SCHON GELADEN
 
	PUSH	DE
	LD	DE,ENTRY	;FILTER INS BIOS KOPIEREN
	LD	HL,LOAD+100H	;100H ENTSPRECHEND UDOS RECORDLAENGE
	LD	BC,CESC-ENTRY+1
	LDIR
	POP	HL		;CO SPRUNG UMLADEN
	LD	(OUTPUT+1),HL
	LD	HL,(0001H)
	LD	DE,10
	ADD	HL,DE
	LD	DE,ENTRY
	LD	(HL),E
	INC	HL
	LD	(HL),D
	JP	0
 
ERROR:	LD	DE,ERRMSG
	LD	C,WRITE
	CALL	BDOS
	JP	0
 
ERRMSG:	DEFM	'BS1715 already aktiv'
	DEFW	CRLF
	DEFM	'$'
 
	ORG	0FE50H
 
ENTRY:	LD	A,(CESC)
	OR	A
	LD	A,C
	JR	NZ,OE		;2.+3. BYTE DER ESC-FOLGE BEHANDELN
 
	CP	0EH		;2. ZEICHENSATZ
	RET	Z
	CP	0FH		;1. ZEICHENSATZ
	RET	Z
 
	LD	C,20H		;LEERZEICHEN AUSGEBEN
	CP	87H		;HELL, INVERS
	JR	Z,OUTP
	AND	7FH
	CP	04H		;NORMAL, NICHT INVERS
	JR	Z,OUTP
	CP	05H		;NORMAL, INVERS
	JR	Z,OUTP
	CP	06H		;HELL, NICHT INVERS
	JR	Z,OUTP
 
	CP	02H		;CURSOR AN
	RET	Z
	CP	03H		;CURSOR AUS
	RET	Z
 
	LD	C,A
	CP	1BH		;ESCAPE ?
	JR	NZ,O1
	LD	A,1
	LD	(CESC),A
OUTP:	JR	OUTPUT
 
O1:	CP	0CH		;BILDSCHIRM LOESCHEN UND HOME
	JR	NZ,O2
	LD	C,'*'
	CALL	ESC
	LD	C,1EH
	CALL	OUTPUT
	LD	C,0		;60ms VERZOEGERUNG
	CALL	OUTPUT
	JR	OUTPUT
 
O2:	LD	C,1EH		;HOME
	CP	01H
	JR	Z,OUTPUT
 
	LD	C,'Y'		;LOESCHEN AB CURSOR BIS BILDSCHIRMENDE
	CP	14H		;CURSOR BLEIBT STEHEN
	JR	Z,ESC
 
	LD	C,0CH		;CURSOR 1 ZEICHEN NACH RECHTS
	CP	15H		;WENN NOETIG ROLLEN
	JR	Z,OUTPUT
 
	LD	C,'T'		;ZEILE AB CURSOR LOESCHEN
	CP	16H
	JR	Z,ESC
 
	CP	18H		;ZEILE LOESCHEN, CURSOR AUF ANFANG
	JR	NZ,O3
	LD	C,0DH
	CALL	OUTPUT
	LD	C,'T'
	JR	ESC
 
O3:	LD	C,0BH		;CURSOR 1 ZEILE NACH OBEN
	CP	1AH
	JR	Z,OUTPUT
 
	LD	C,A
	CP	7FH
	JR	NZ,OUTPUT	;ALLE ANDEREN ZEICHEN
	LD	C,'W'		;ZEICHEN AUF CURSOR LOESCHEN
	CALL	ESC		;CURSOR NACH LINKS
	LD	C,08H
	JR	OUTPUT
 
OE:	PUSH	HL
	LD	HL,CESC
	CP	80H		;1715 ESC-FOLGE ?
	JR	C,OE1		;ALLE ANDEREN ESC-FOLGEN UNVERAENDERT AUSGEBEN
	SUB	60H
	LD	C,A
	LD	A,(HL)
	DEC	A
	JR	NZ,OE1
	INC	(HL)
	PUSH	BC
	LD	C,'='
	CALL	OUTPUT
	POP	BC
	JR	OE2
OE1:	LD	(HL),0
OE2:	POP	HL
	JR	OUTPUT
 
ESC:	PUSH	BC
	LD	C,1BH
	CALL	OUTPUT
	POP	BC
 
OUTPUT:	JP	0
 
CESC	DEFB	0
