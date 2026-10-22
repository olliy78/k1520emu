			      OSM.URLADER			PAGE   1
 LOC   OBJ CODE	M STMT SOURCE STATEMENT				 ASM 5.9

		     1	;******************************************************************************
		     2	;******************************************************************************
		     3	;
		     4	;	O S / M	- Quelle	(C) ZFT/KEAW Abt. Baisisoftware	- 1986
		     5	;
		     6	;	Programm:	OSM.URLADER.S
		     7	;
		     8	;
		     9	;	Bearbeiter:	P. Hoge
		    10	;	Datum:		15.12.87
		    11	;	Version:	3.0
		    12	;
		    13	;******************************************************************************
		    14	;******************************************************************************
		    15
		    16
		    17
0C00		    18		ORG	0C00H
		    19
		    20	;CONSTANT
		    21
		    22	FDCBFOS	EQU	0EEBH
		    23	ERROR	EQU	0
		    24	FLOPPY	EQU	0BFDH
		    25	BIOS	EQU	0F000H
		    26	CCP	EQU	BIOS-1600H
		    27	OSMLEN	EQU	28H
		    28
0C00   FD21480C	    29	ENTRY	LD	IY,VEKTOR	;CCP, BDOS, BIOS LADEN
0C04   310040	    30		LD	SP,4000H
0C07   3A7E0C	    31		LD	A,(FDCONF)
0C0A   E60F	    32		AND	0FH
0C0C   FE07	    33		CP	7
0C0E   3802	    34		JR	C,TYP
0C10   3E07	    35		LD	A,7		;BEI TYP > 7 IMMER 7 EINSTELLEN
0C12   32EB0E	    36	TYP	LD	(FDCBFOS),A	;BFOS FLOPPY TREIBER EINSTELLEN
0C15   21540C	    37		LD	HL,LENTAB-1
0C18   85	    38		ADD	A,L
0C19   6F	    39		LD	L,A
0C1A   46	    40		LD	B,(HL)		;LAENGE	1 TRACK
0C1B   3E28	    41		LD	A,OSMLEN	;LAENGE	OS/M
0C1D   57	    42	LOOP	LD	D,A
0C1E   90	    43		SUB	B
0C1F   4F	    44		LD	C,A
0C20   78	    45		LD	A,B
0C21   3003	    46		JR	NC,L1
0C23   7A	    47		LD	A,D
0C24   0E00	    48		LD	C,0
0C26   FD7705	    49	L1	LD	(IY+5),A	;LAENGE
0C29   F5	    50		PUSH	AF
0C2A   C5	    51		PUSH	BC
0C2B   CDFD0B	    52		CALL	FLOPPY
0C2E   C1	    53		POP	BC
0C2F   F1	    54		POP	AF
0C30   FD340C	    55		INC	(IY+12)		;TRACK
0C33   FD8603	    56		ADD	A,(IY+3)	;NEUE LADE ADR
0C36   FD7703	    57		LD	(IY+3),A
0C39   FD7E0A	    58		LD	A,(IY+10)Š
			      OSM.URLADER			PAGE   2
 LOC   OBJ CODE	M STMT SOURCE STATEMENT				 ASM 5.9

0C3C   FE80	    59		CP	80H
0C3E   C20000	    60		JP	NZ,ERROR	;FLOPPY	LESE FEHLER
0C41   79	    61		LD	A,C
0C42   B7	    62		OR	A		;FERTIG	?
0C43   20D8	    63		JR	NZ,LOOP
0C45   C300F0	    64		JP	BIOS
		    65
0C48   00	    66	VEKTOR	DEFB	0		;UDOS REQUEST-VEKTOR
0C49   0A	    67		DEFB	0AH
0C4A   00D8	    68		DEFW	CCP-200H	;ADRESSE
0C4C   0000	    69		DEFW	0
0C4E   0000	    70		DEFW	0
0C50   0000	    71		DEFW	0
0C52   00	    72		DEFB	0
0C53   0000	    73		DEFW	0000H		;TRACK/SEKTOR
		    74
		    75
0C55   00	    76	LENTAB	DEFB	0
0C56   10	    77		DEFB	10H		;TYP 2
0C57   10	    78		DEFB	10H		;TYP 3
0C58   10	    79		DEFB	10H		;TYP 4
0C59   20	    80		DEFB	20H		;TYP 5
0C5A   24	    81		DEFB	24H		;TYP 6
0C5B   28	    82		DEFB	28H		;TYP 7
		    83
0C60		    84		ORG	0C60H
		    85
0C60   03	    86	SIOSI	DEFB	03H		;FLAG
0C61   2C	    87		DEFB	2CH		;CTCADR
0C62   04	    88		DEFB	4		;SPEED (9600)
0C63   24	    89		DEFB	24H		;SIOADR
0C64   7C	    90		DEFB	01111100B	;LPARM
0C65   82	    91		DEFB	10000010B	;LCONTR
0C66   13	    92		DEFB	13H		;XOFF
0C67   11	    93		DEFB	11H		;XON
0C68   FF	    94		DEFB	0FFH		;INMSK
		    95
0C69   00	    96	SIOCON	DEFB	00H		;Verzoegerung ein
0C6A   2D	    97		DEFB	2DH
0C6B   04	    98		DEFB	4
0C6C   26	    99		DEFB	26H
0C6D   7C	   100		DEFB	01111100B
0C6E   82	   101		DEFB	10000010B
0C6F   13	   102		DEFB	13H
0C70   11	   103		DEFB	11H
0C71   7F	   104		DEFB	7FH
		   105
0C72   21	   106	SIOLP	DEFB	21H
0C73   08	   107		DEFB	08H
0C74   04	   108		DEFB	4
0C75   2A	   109		DEFB	2AH
0C76   7C	   110		DEFB	01111100B
0C77   82	   111		DEFB	10000010B
0C78   13	   112		DEFB	13H
0C79   11	   113		DEFB	11H
0C7A   7F	   114		DEFB	7FH
		   115Š0C7B   42	   116	LINES	DEFB	66		;DRUCKER PARAMETER

			      OSM.URLADER			PAGE   3
 LOC   OBJ CODE	M STMT SOURCE STATEMENT				 ASM 5.9

0C7C   80	   117	COLS	DEFB	128
0C7D   00	   118	INDENT	DEFB	0
		   119
0C7E   5500	   120	FDCONF	DEFW	0055H
0C80   50383030	   121		DEFM	'P8000SYS'

			      BIOS				PAGE   1
 LOC   OBJ CODE	M STMT SOURCE STATEMENT				 ASM 5.9

		     1	;******************************************************************************
		     2	;******************************************************************************
		     3	;
		     4	;	O S / M	- Quelle	(C) ZFT/KEAW Abt. Baisisoftware	- 1985
		     5	;
		     6	;	Programm:	BIOS.S
		     7	;
		     8	;
		     9	;	Bearbeiter:	P. Hoge
		    10	;	Datum:		16.12.87
		    11	;	Version:	3.0
		    12	;
		    13	;******************************************************************************
		    14	;******************************************************************************
		    15
F000		    16		ORG	0F000H
		    17
		    18
		    19	;fuer bedingte Assemblierung in	Floppydisk Treiber
		    20
		    21	UDOS	EQU	0		;0 = OS/M BIOS
		    22	DMA	EQU	1		;1 = DMA Mode fuer FDC
		    23
		    24	;System	Adressen
		    25
		    26	BIOS				;Basis von BIOS
		    27	CCP	EQU	BIOS-1600H	;Basis von CCP
		    28	BDOS	EQU	CCP+806H	;Basis von BDOS
		    29	BDENTRY	EQU	CCP-20H		;Ueberschreibschutz fuer CCP
		    30	RESTART	EQU	0000H
		    31	IOBYTE	EQU	0003H
		    32	CDISK	EQU	0004H		;CURRENT DISK NUMMER
		    33	BUFF	EQU	0080H
		    34	SEKBUF	EQU	0E000H
		    35
		    36	;GLOBAL	CONSTANTS
		    37
		    38	CR	EQU	0DH
		    39	LF	EQU	0AH
		    40	CRLF	EQU	0A0DH
		    41	ETX	EQU	03H
		    42	EOF	EQU	1AH
		    43	RST_38	EQU	0FFH
		    44	JUMP	EQU	0C3H
		    45
		    46	;IO ADRESSEN
		    47
		    48	WEADP	EQU	0	;MEMORY	BANK SELECT PORT
		    49	CTC0	EQU	08H
		    50	CTC1	EQU	2CH
		    51	SIO0AD	EQU	24H
		    52	SIO0BD	EQU	26H
		    53	SIO1AD	EQU	28H
		    54	SIO1BD	EQU	2AH
		    55

			      BIOS				PAGE   2
 LOC   OBJ CODE	M STMT SOURCE STATEMENT				 ASM 5.9

		    56	*E
		    57	;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
		    58	;Systemtabelle fuer den	externen Zugriff auf das BIOS
		    59
F000   C380FA	    60	BOOTE	JP	BOOT
F003   C35FF1	    61	WBOOTE	JP	WBOOT
F006   C3A5F2	    62		JP	CSTS		;Console Input Status
F009   C3BDF2	    63		JP	CI		;Console Input
F00C   C36BF3	    64		JP	CO		;Console Output
F00F   C39DF3	    65		JP	LO		;List Output (Drucker)
F012   C391F3	    66		JP	SO		;Seriell Interface Output
F015   C312F3	    67		JP	SI		;Seriell Interface Input
F018   C3CBF4	    68		JP	HOME
F01B   C3A0F4	    69		JP	SELDSK
F01E   C3D0F4	    70		JP	SETTRK
F021   C3D5F4	    71		JP	SETSEC
F024   C3E6F4	    72		JP	SETDMA
F027   C3EBF4	    73		JP	READ
F02A   C34FF5	    74		JP	WRITE
F02D   C322F3	    75		JP	LSTS		;List Status
F030   C3DBF4	    76		JP	SECTRAN		;Sektor	Translate
		    77
		    78	;zusaetzliche Entries
		    79
F033   C3B5F2	    80		JP	SISTS		;serielles Interface Input Status
F036   C31AF3	    81		JP	SOSTS		;serielles Interface Output Status
F039   C38DF5	    82		JP	FLUSH		;Flush Diskpuffer
F03C   C397F5	    83		JP	DISCD		;Discard Diskpuffer
F03F   C3ABF6	    84		JP	MOV
F042   C9	    85		RET			;DSCRF
F043		    86		DEFS	2
F045   C9	    87		RET			;BYTBC
F046		    88		DEFS	2
F048   C9	    89		RET			;BYTBA
F049		    90		DEFS	2
		    91
		    92	;zusaetzliche OS/M Entries
		    93
F04B   C3B3F6	    94		JP	FLOPPY		;UDOS Parametervektor
F04E   5500	    95	FDCONF	DEFW	0055H
F050   0000	    96	CRCERR	DEFW	0		;FEHLER	COUNTER	CRC
F052   0000	    97	SECERR	DEFW	0		;SEKTORFEHLER
F054   0000	    98	TRKERR	DEFW	0		;SPURFEHLER
F056   E0F1	    99		DEFW	SIOBASE		;ADR DER SIO-STRUKTUREN
F058   EDF9	   100		DEFW	TYPTAB		;ADR DER FLOPPY-TYP-TABELLE
		   101
		   102	;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
		   103	;Disk Parameter	Basis
		   104
F05A   0000	   105	DPBASE	DEFW	0		;DISK A
F05C   0000	   106		DEFW	0
F05E   0000	   107		DEFW	0
F060   0000	   108		DEFW	0
F062   80FA	   109		DEFW	DIRBUF
F064   D3F0	   110		DEFW	DPBLK4
F066   00FB	   111		DEFW	CSV0
F068   E0FB	   112		DEFW	ALV0Š		   113

			      BIOS				PAGE   3
 LOC   OBJ CODE	M STMT SOURCE STATEMENT				 ASM 5.9

F06A   0000	   114		DEFW	0		;DISK B
F06C   0000	   115		DEFW	0
F06E   0000	   116		DEFW	0
F070   0000	   117		DEFW	0
F072   80FA	   118		DEFW	DIRBUF
F074   D3F0	   119		DEFW	DPBLK4
F076   40FB	   120		DEFW	CSV1
F078   30FC	   121		DEFW	ALV1
		   122
F07A   0000	   123		DEFW	0		;DISK C
F07C   0000	   124		DEFW	0
F07E   0000	   125		DEFW	0
F080   0000	   126		DEFW	0
F082   80FA	   127		DEFW	DIRBUF
F084   D3F0	   128		DEFW	DPBLK4
F086   80FB	   129		DEFW	CSV2
F088   80FC	   130		DEFW	ALV2
		   131
F08A   0000	   132		DEFW	0		;DISK D
F08C   0000	   133		DEFW	0
F08E   0000	   134		DEFW	0
F090   0000	   135		DEFW	0
F092   80FA	   136		DEFW	DIRBUF
F094   D3F0	   137		DEFW	DPBLK4
F096   B0FB	   138		DEFW	CSV3
F098   B2FC	   139		DEFW	ALV3
		   140
		   141	;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
		   142	;Disk Parameterbloecke
		   143
		   144	;Softsektor 8" single Density
		   145
F09A   1A00	   146	DPBLK1	DEFW	26
F09C   03	   147		DEFB	3
F09D   07	   148		DEFB	7
F09E   00	   149		DEFB	0
F09F   F200	   150		DEFW	242
F0A1   3F00	   151		DEFW	63
F0A3   C0	   152		DEFB	0C0H
F0A4   00	   153		DEFB	0
F0A5   1000	   154		DEFW	16
F0A7   0200	   155		DEFW	2
F0A9   00	   156		DEFB	0
F0AA   00	   157		DEFB	0
F0AB   4D00	   158		DEFW	77
		   159
		   160	;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
		   161	;Mini double density SCP Format	40 Spuren
		   162
F0AD   2000	   163	DPBLK2	DEFW	32	;32 log	Sekt a 128 Byte	= 16 phys. a 256 Byte
F0AF   04	   164		DEFB	4	;Blockgroessenfaktor
		   165				;4 = 2K	3=1K Bloecke
F0B0   0F	   166		DEFB	15	;BLOCKMASKE (Anzahl Sektoren/Block minus 1)
		   167				;15 bei	2k, 7 bei 1k Bloecken
F0B1   01	   168		DEFB	1	;Extentmaske (abhaengig	von der	Blockgroesse und
		   169				;der Anzahl Bloecke/Diskette
F0B2   4900	   170		DEFW	73	;Anzahl	der Bloecke minus 1ŠF0B4   3F00	   171		DEFW	63	;Anzahl	der directory Eintraege	minus 1

			      BIOS				PAGE   4
 LOC   OBJ CODE	M STMT SOURCE STATEMENT				 ASM 5.9

F0B6   80	   172		DEFB	80H	;Alloc 0
F0B7   00	   173		DEFB	0	;Alloc 1
F0B8   1000	   174		DEFW	16	;Laenge	des Checkvektors fuer Directory
		   175				; = (DRM+1)/4
F0BA   0300	   176		DEFW	3	;Anzahl	der Systemspuren
F0BC   01	   177		DEFB	1	;PSH (fuer Deblocking)
F0BD   01	   178		DEFB	1	;PHM (fuer Deblocking)
F0BE   2800	   179		DEFW	40	;nur OS/M Anzahl der Spuren insgesamt
		   180
		   181	;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
		   182	;Mini double density SCP Format	40 Spuren
		   183
F0C0   2000	   184	DPBLK3	DEFW	32
F0C2   04	   185		DEFB	4
F0C3   0F	   186		DEFB	15
F0C4   01	   187		DEFB	1
F0C5   4900	   188		DEFW	73
F0C7   3F00	   189		DEFW	63
F0C9   80	   190		DEFB	80H
F0CA   00	   191		DEFB	0
F0CB   1000	   192		DEFW	16
F0CD   0300	   193		DEFW	3
F0CF   01	   194		DEFB	1
F0D0   01	   195		DEFB	1
F0D1   2800	   196		DEFW	40
		   197
		   198	;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
		   199	;Mini double density SCP Format	80 Spuren
		   200
F0D3   2000	   201	DPBLK4	DEFW	32
F0D5   04	   202		DEFB	4
F0D6   0F	   203		DEFB	15
F0D7   01	   204		DEFB	1
F0D8   9900	   205		DEFW	153
F0DA   3F00	   206		DEFW	63
F0DC   80	   207		DEFB	80H
F0DD   00	   208		DEFB	0
F0DE   1000	   209		DEFW	16
F0E0   0300	   210		DEFW	3
F0E2   01	   211		DEFB	1
F0E3   01	   212		DEFB	1
F0E4   5000	   213		DEFW	80
		   214
		   215	;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
		   216	;Mini double density double side SCP Format 80 Spuren
		   217
F0E6   4000	   218	DPBLK5	DEFW	64
F0E8   04	   219		DEFB	4
F0E9   0F	   220		DEFB	15
F0EA   00	   221		DEFB	0
F0EB   3701	   222		DEFW	311
F0ED   7F00	   223		DEFW	127
F0EF   C0	   224		DEFB	0C0H
F0F0   00	   225		DEFB	0
F0F1   2000	   226		DEFW	32
F0F3   0200	   227		DEFW	2
F0F5   01	   228		DEFB	1ŠF0F6   01	   229		DEFB	1

			      BIOS				PAGE   5
 LOC   OBJ CODE	M STMT SOURCE STATEMENT				 ASM 5.9

F0F7   5000	   230		DEFW	80
		   231
		   232	;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
		   233	;Mini double density 2*9*512
		   234
F0F9   4800	   235	DPBLK6	DEFW	72
F0FB   04	   236		DEFB	4
F0FC   0F	   237		DEFB	15
F0FD   00	   238		DEFB	0
F0FE   5E01	   239		DEFW	350
F100   7F00	   240		DEFW	127
F102   C0	   241		DEFB	0C0H
F103   00	   242		DEFB	0
F104   2000	   243		DEFW	32
F106   0200	   244		DEFW	2
F108   02	   245		DEFB	2
F109   03	   246		DEFB	3
F10A   5000	   247		DEFW	80
		   248
		   249	;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
		   250	;Mini double density 2*5*1024
		   251
F10C   5000	   252	DPBLK7	DEFW	80
F10E   04	   253		DEFB	4
F10F   0F	   254		DEFB	15
F110   00	   255		DEFB	0
F111   8501	   256		DEFW	389
F113   7F00	   257		DEFW	127
F115   C0	   258		DEFB	0C0H
F116   00	   259		DEFB	0
F117   2000	   260		DEFW	32
F119   0200	   261		DEFW	2
F11B   03	   262		DEFB	3
F11C   07	   263		DEFB	7
F11D   5000	   264		DEFW	80
		   265
		   266	;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
		   267	;User-Format 1 (2*5*1024 DAC / HU-Krz)
		   268
F11F   5000	   269	DPBLK8	DEFW	80
F121   04	   270		DEFB	4
F122   0F	   271		DEFB	15
F123   00	   272		DEFB	0
F124   8A01	   273		DEFW	394
F126   BF00	   274		DEFW	191
F128   E0	   275		DEFB	0E0H
F129   00	   276		DEFB	0
F12A   3000	   277		DEFW	48
F12C   0100	   278		DEFW	1
F12E   03	   279		DEFB	3
F12F   07	   280		DEFB	7
F130   5000	   281		DEFW	80
		   282
		   283	;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
		   284	;User-Format 2 (2*5*1024 CP/A)
		   285
F132   5000	   286	DPBLK9	DEFW	80ŠF134   04	   287		DEFB	4

			      BIOS				PAGE   6
 LOC   OBJ CODE	M STMT SOURCE STATEMENT				 ASM 5.9

F135   0F	   288		DEFB	15
F136   00	   289		DEFB	0
F137   8F01	   290		DEFW	399
F139   BF00	   291		DEFW	191
F13B   E0	   292		DEFB	0E0H
F13C   00	   293		DEFB	0
F13D   3000	   294		DEFW	48
F13F   0000	   295		DEFW	0
F141   03	   296		DEFB	3
F142   07	   297		DEFB	7
F143   5000	   298		DEFW	80
		   299
		   300	;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
		   301	;Tabelle Sektorversatz fuer 8" Soft single density
		   302
F145   01	   303	XLT	DEFB	01H
F146   07	   304		DEFB	07H
F147   0D	   305		DEFB	0DH
F148   13	   306		DEFB	13H
F149   19	   307		DEFB	19H
F14A   05	   308		DEFB	05H
F14B   0B	   309		DEFB	0BH
F14C   11	   310		DEFB	11H
F14D   17	   311		DEFB	17H
F14E   03	   312		DEFB	03H
F14F   09	   313		DEFB	09H
F150   0F	   314		DEFB	0FH
F151   15	   315		DEFB	15H
F152   02	   316		DEFB	02H
F153   08	   317		DEFB	08H
F154   0E	   318		DEFB	0EH
F155   14	   319		DEFB	14H
F156   1A	   320		DEFB	1AH
F157   06	   321		DEFB	06H
F158   0C	   322		DEFB	0CH
F159   12	   323		DEFB	12H
F15A   18	   324		DEFB	18H
F15B   04	   325		DEFB	04H
F15C   0A	   326		DEFB	0AH
F15D   10	   327		DEFB	10H
F15E   16	   328		DEFB	16H
		   329

			      BIOS				PAGE   7
 LOC   OBJ CODE	M STMT SOURCE STATEMENT				 ASM 5.9

		   330	*E Ab hier koennen sich bei folgenden Versionen die Adressen aendern !!
		   331	;*-----------------------------------------------------------------------------
		   332	;*
		   333	;* Warmstart Prozedure
		   334	;*
		   335	;*-----------------------------------------------------------------------------
		   336
F15F   AF	   337	WBOOT	XOR	A
F160   3207DA	   338		LD	(CCP+7),A	;LOESCHEN EINGABEPUFFER
F163   318000	   339	KBOOT	LD	SP,BUFF
F166   CD8DF5	   340		CALL	FLUSH		;EVENT.	DISKPUFFER ZURUECKSCHREIBEN
F169   CD96F1	   341		CALL	FDKONF		;FLOPPY	KONFIGURATION EINSTELLEN
F16C   CD90F4	   342		CALL	LPINIT		;LP IN GRUNDSTELLUNG BRINGEN
		   343
F16F   018000	   344		LD	BC,BUFF		;DEFAULT BUFFER	ADRESSE
F172   CDE6F4	   345		CALL	SETDMA
F175   3EC3	   346		LD	A,JUMP		;INIT ENTRY POINTS
F177   320000	   347		LD	(RESTART),A
F17A   2103F0	   348		LD	HL,WBOOTE
F17D   220100	   349		LD	(RESTART+1),HL	;JUMP WBOOT AUF	ADRESSE	0
F180   323800	   350		LD	(0038H),A	;BREAK ENTRY
F183   223900	   351		LD	(0039H),HL
F186   320500	   352		LD	(0005H),A	;BDOS ENTRY
F189   21E0D9	   353		LD	HL,BDENTRY
F18C   220600	   354		LD	(0006H),HL
		   355
F18F   3A0400	   356		LD	A,(CDISK)	;AKTUELLE DISK
F192   4F	   357		LD	C,A		;LOGIN DISK FUER CCP
F193   C300DA	   358		JP	CCP		;SPRUNG	ZU CCP
		   359
		   360	;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
		   361	;FLOPPY	DISK KONFIGURATION
		   362
F196   214EF0	   363	FDKONF	LD	HL,FDCONF
F199   DD215AF0	   364		LD	IX,DPBASE
F19D   0602	   365		LD	B,2
F19F   7E	   366	FDK1	LD	A,(HL)
F1A0   F5	   367		PUSH	AF
F1A1   CDB0F1	   368		CALL	FDK2
F1A4   F1	   369		POP	AF
F1A5   0F	   370		RRCA
F1A6   0F	   371		RRCA
F1A7   0F	   372		RRCA
F1A8   0F	   373		RRCA
F1A9   CDB0F1	   374		CALL	FDK2
F1AC   23	   375		INC	HL
F1AD   10F0	   376		DJNZ	FDK1
F1AF   C9	   377		RET
		   378
F1B0   E60F	   379	FDK2	AND	0FH
F1B2   C8	   380		RET	Z
F1B3   FE0A	   381		CP	10
F1B5   D0	   382		RET	NC
F1B6   E5	   383		PUSH	HL
F1B7   210000	   384		LD	HL,0
F1BA   FE01	   385		CP	1
F1BC   2003	   386		JR	NZ,FDK3ŠF1BE   2145F1	   387		LD	HL,XLT

			      BIOS				PAGE   9
 LOC   OBJ CODE	M STMT SOURCE STATEMENT				 ASM 5.9

		   403	*E
		   404	;*-----------------------------------------------------------------------------
		   405	;*
		   406	;* Ein-	und Ausgabetreiber fuer	die V24	Kanaele
		   407	;*
		   408	;*-----------------------------------------------------------------------------
		   409
		   410	;SIO KOMMANDOS
		   411
		   412	COMM2	EQU	10H	;RUECKSETZEN EXTERN/STATUS INTERRUPT
		   413	COMM3	EQU	18H	;KANAL RUECKSETZEN
		   414	COMM4	EQU	20H	;RUECKSETZEN EMPFANGSINT BEI 1.ZEICHEN
		   415	COMM5	EQU	28H	;RUECKSETZEN TRANSMITTER INT
		   416	COMM6	EQU	30H	;RUECKSETZEN FEHLERBEDINGUNG
		   417
		   418	;OFFSETS
		   419
		   420	FLAG	EQU	0
		   421	CTCADR	EQU	1
		   422	SPEED	EQU	2
		   423	SIOADR	EQU	3
		   424	LPARM	EQU	4
		   425	LCONTR	EQU	5
		   426	XOFF	EQU	6
		   427	XON	EQU	7
		   428	INMSK	EQU	8
		   429	GET	EQU	9
		   430	STORE	EQU	10
		   431	COUNT	EQU	11
		   432	BUFLEN	EQU	12
		   433	BUFADR	EQU	13
		   434	INTADR	EQU	15
		   435
		   436	;NUR FUER LPSTRUCTUR
		   437
		   438	LINES	EQU	17
		   439	COLS	EQU	18
		   440	INDENT	EQU	19
		   441	CURLINE	EQU	20
		   442	CURCOL	EQU	21
		   443	LPFLAG	EQU	22
		   444
		   445	;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
		   446	;PARAMETER FUER	DIE SIO	KANAELE
		   447
		   448	SIOBASE
		   449	SIOSI				;(TTY0)
F1E0   03	   450		DEFB	03H		;AUTOM XOFF/XON
F1E1   2C	   451		DEFB	CTC1+0
F1E2   04	   452		DEFB	4		;1=38400, 2=19200, 4=9600 ...
F1E3   24	   453		DEFB	SIO0AD
F1E4   7C	   454		DEFB	01111100B	;8 BIT,	NO PARITY, 2 STOP, VORTEILER 16
F1E5   82	   455		DEFB	10000010B	;RTS UND CTS SETZEN
F1E6   13	   456		DEFB	13H
F1E7   11	   457		DEFB	11H
F1E8   FF	   458		DEFB	0FFH
F1E9   00	   459		DEFB	0ŠF1EA   00	   460		DEFB	0

			      BIOS				PAGE  10
 LOC   OBJ CODE	M STMT SOURCE STATEMENT				 ASM 5.9

F1EB   00	   461		DEFB	0
F1EC   A0	   462		DEFB	160
F1ED   1BFD	   463		DEFW	SIBUF
F1EF   19F2	   464		DEFW	SIINT
		   465
		   466	SIOCON				;(TTY1,CONSOLE)
F1F1   00	   467		DEFB	0
F1F2   2D	   468		DEFB	CTC1+1
F1F3   04	   469		DEFB	4		;1=38400, 2=19200, 4=9600 ...
F1F4   26	   470		DEFB	SIO0BD
F1F5   7C	   471		DEFB	01111100B
F1F6   82	   472		DEFB	10000010B
F1F7   13	   473		DEFB	13H
F1F8   11	   474		DEFB	11H
F1F9   7F	   475		DEFB	7FH
F1FA   00	   476		DEFB	0
F1FB   00	   477		DEFB	0
F1FC   00	   478		DEFB	0
F1FD   32	   479		DEFB	50
F1FE   E4FC	   480		DEFW	CIBUF
F200   1FF2	   481		DEFW	CONINT
		   482
		   483	LPSTRUCT
		   484	SIOLP				;(TTY3)
F202   01	   485		DEFB	01H
F203   08	   486		DEFB	CTC0+0
F204   04	   487		DEFB	4		;1=38400, 2=19200, 4=9600 ...
F205   2A	   488		DEFB	SIO1BD
F206   7C	   489		DEFB	01111100B
F207   82	   490		DEFB	10000010B
F208   13	   491		DEFB	13H
F209   11	   492		DEFB	11H
F20A   7F	   493		DEFB	7FH
F20B   00	   494		DEFB	0
F20C   00	   495		DEFB	0
F20D   00	   496		DEFB	0
F20E   05	   497		DEFB	5
F20F   16FD	   498		DEFW	LPBUF
F211   25F2	   499		DEFW	LPINT
		   500
		   501	;ZUSAETZLICH FUER LPSTRUCT
		   502
F213   42	   503		DEFB	66
F214   80	   504		DEFB	128
F215   00	   505		DEFB	0
F216   00	   506		DEFB	0
F217   00	   507		DEFB	0
F218   00	   508		DEFB	0
		   509
		   510	;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
		   511	;EMPFANGS ISR FUER SIO KANAELE
		   512	;INT SPEZIELLE EMPF. BEDINGUNG WIE NORMALER EMPFANG
		   513	;SENDE INT UND EXTERN/STATUS INT SIND NICHT ZUGELASSEN
		   514
F219   E5	   515	SIINT	PUSH	HL
F21A   21E0F1	   516		LD	HL,SIOSI
F21D   180A	   517		JR	RCVINTŠ		   518

			      BIOS				PAGE  11
 LOC   OBJ CODE	M STMT SOURCE STATEMENT				 ASM 5.9

F21F   E5	   519	CONINT	PUSH	HL
F220   21F1F1	   520		LD	HL,SIOCON
F223   1804	   521		JR	RCVINT
		   522
F225   E5	   523	LPINT	PUSH	HL
F226   2102F2	   524		LD	HL,SIOLP
		   525
F229   FB	   526	RCVINT	EI			;GEMEINSAME INT	ROUTINE
F22A   FDE5	   527		PUSH	IY
F22C   E5	   528		PUSH	HL
F22D   FDE1	   529		POP	IY
F22F   C5	   530		PUSH	BC
F230   F5	   531		PUSH	AF
F231   FD4E03	   532		LD	C,(IY+SIOADR)	;IO ADR	SIO
F234   ED40	   533		IN	B,(C)		;ZEICHEN EINLESEN
F236   0C	   534		INC	C		;CONTROLADRESSE
F237   3E30	   535		LD	A,COMM6		;RUECKSETZEN EVENT. EMPFANGSFEHLER
F239   ED79	   536		OUT	(C),A
F23B   78	   537		LD	A,B
F23C   FDCB0046	   538		BIT	0,(IY+FLAG)	;AUTOMAT. XOFF/XON BEI EMPFANG ZUGELASSEN ?
F240   281E	   539		JR	Z,RINT2		;SPRUNG	NEIN
F242   E67F	   540		AND	7FH
F244   FDBE06	   541		CP	(IY+XOFF)	;XOFF ZEICHEN ?
F247   2006	   542		JR	NZ,RINT1
F249   FDCB00D6	   543		SET	2,(IY+FLAG)	;AUSGABE HALT SETZEN
F24D   1809	   544		JR	RINTEND
		   545
F24F   FDBE07	   546	RINT1	CP	(IY+XON)	;ZEICHEN = XON ?
F252   200C	   547		JR	NZ,RINT2
F254   FDCB0096	   548		RES	2,(IY+FLAG)	;AUSGABE HALT AUFHEBEN
F258   F1	   549	RINTEND	POP	AF
F259   C1	   550		POP	BC
F25A   FDE1	   551		POP	IY
F25C   E1	   552		POP	HL
F25D   FB	   553	NOINT	EI
F25E   ED4D	   554	RRETI	RETI
		   555
		   556
F260   FD7E0A	   557	RINT2	LD	A,(IY+STORE)	;STOREPOINTER
F263   CDFEF2	   558		CALL	GENADR
F266   FDBE09	   559		CP	(IY+GET)	;GETPOINTER=STOREPOINTER = PUFFER VOLL
F269   2006	   560		JR	NZ,OKSTOR
F26B   FDCB00F6	   561		SET	6,(IY+FLAG)	;INPUT PUFFER OVERFLOW
F26F   18E7	   562		JR	RINTEND
		   563
F271   FD770A	   564	OKSTOR	LD	(IY+STORE),A	;INCREMENTIERTER STOREPOINTER
F274   70	   565		LD	(HL),B		;ZEICHEN IN INPUTPUFFER
		   566
F275   FD340B	   567		INC	(IY+COUNT)
F278   FDCB00FE	   568		SET	7,(IY+FLAG)	;ZEICHEN IM INPUT PUFFER
F27C   FDCB004E	   569		BIT	1,(IY+FLAG)	;AUTOMAT. XOFF/XON FUER	SENDEN ZUGELASSEN ?
F280   28D6	   570		JR	Z,RINTEND	;NEIN
F282   FD7E0C	   571		LD	A,(IY+BUFLEN)
F285   D614	   572		SUB	20		;BEI PUFFERLAENGE -20 WIRD
F287   FDBE0B	   573		CP	(IY+COUNT)	;XOFF GESENDET
F28A   20CC	   574		JR	NZ,RINTEND	;NEIN
F28C   FDCB005E	   575		BIT	3,(IY+FLAG)	;SCHON INPUT HALT GESENDET ?ŠF290   20C6	   576		JR	NZ,RINTEND	;JA

			      BIOS				PAGE  12
 LOC   OBJ CODE	M STMT SOURCE STATEMENT				 ASM 5.9

F292   FDCB00DE	   577		SET	3,(IY+FLAG)	;MERKEN	INPUT HALT GESENDET
F296   CD5EF2	   578		CALL	RRETI
F299   FD7E06	   579		LD	A,(IY+XOFF)	;XOFF ZEICHEN
F29C   CD5AF3	   580		CALL	SEND1
F29F   F1	   581		POP	AF
F2A0   C1	   582		POP	BC
F2A1   FDE1	   583		POP	IY
F2A3   E1	   584		POP	HL
F2A4   C9	   585		RET
		   586
		   587	;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
		   588	;CSTS -	CONSOLE	INPUT STATUS
		   589	;INPUT:
		   590	;OUTPUT: REG. A	ENTHAELT 00 WENN KEINE TASTE GEDRUECKT
		   591	;	 REG. A	ENTHAELT FF WENN TASTE GEDRUECKT (ZEICHEN IM RINGPUFFER)
		   592	;VERAENDERT A, FLAGS
		   593
F2A5   FDE5	   594	CSTS	PUSH	IY
F2A7   FD21F1F1	   595		LD	IY,SIOCON
F2AB   FD7E0B	   596	CSTS1	LD	A,(IY+COUNT)
F2AE   FDE1	   597		POP	IY
F2B0   B7	   598		OR	A
F2B1   C8	   599		RET	Z
F2B2   3EFF	   600		LD	A,-1
F2B4   C9	   601		RET
		   602
		   603	;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
		   604	;SISTS - SERIAL	INTERFACE INPUT	STATUS
		   605	;INPUT:
		   606	;OUTPUT: REG. A	ENTHAELT 00 WENN KEIN ZEICHEN EMPFANGEN
		   607	;	 REG. A	ENTHAELT FF WENN ZEICHEN EMPFANGEN (ZEICHEN IM RINGPUFFER)
		   608	;VERAENDERT A, FLAGS
		   609
F2B5   FDE5	   610	SISTS	PUSH	IY
F2B7   FD21E0F1	   611		LD	IY,SIOSI
F2BB   18EE	   612		JR	CSTS1
		   613
		   614	;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
		   615	;CI - CONSOLE INPUT
		   616	;INPUT:
		   617	;OUTPUT: CHARACTER IN REGISTER A
		   618	;VERAENDERT A, FLAGS
		   619
F2BD   FDE5	   620	CI	PUSH	IY
F2BF   FD21F1F1	   621		LD	IY,SIOCON
		   622
F2C3   FDCB007E	   623	LOAD	BIT	7,(IY+FLAG)
F2C7   28FA	   624		JR	Z,LOAD		;WARTEN	BIS ZEICHEN IM PUFFER
F2C9   E5	   625		PUSH	HL		;1 ZEICHEN AUS INPUTBUF
F2CA   FD7E09	   626		LD	A,(IY+GET)	;PUFFERPOINTER FUER ZEICHEN HOLEN
F2CD   CDFEF2	   627		CALL	GENADR
F2D0   F3	   628		DI
F2D1   FD7709	   629		LD	(IY+GET),A	;NEUER PUFFERPOINTER
F2D4   FD350B	   630		DEC	(IY+COUNT)
F2D7   2004	   631		JR	NZ,LOAD1
F2D9   FDCB00BE	   632		RES	7,(IY+FLAG)	;INPUT PUFFER LEER
F2DD   6E	   633	LOAD1	LD	L,(HL)		;ZEICHEN AUS INPUT PUFFERŠF2DE   FB	   634		EI

			      BIOS				PAGE  13
 LOC   OBJ CODE	M STMT SOURCE STATEMENT				 ASM 5.9

F2DF   FDCB005E	   635		BIT	3,(IY+FLAG)	;XOFF GESENDET ?
F2E3   2811	   636		JR	Z,LOAD2		;SPRUNG	NEIN
F2E5   FD7E0B	   637		LD	A,(IY+COUNT)
F2E8   FE0A	   638		CP	10		;PUFFER	SOWEIT LEER, DASS XON
		   639					;GESENDET WERDEN KANN ?
F2EA   200A	   640		JR	NZ,LOAD2
F2EC   FDCB009E	   641		RES	3,(IY+FLAG)
F2F0   FD7E07	   642		LD	A,(IY+XON)	;XON CHARACTER
F2F3   CD5AF3	   643		CALL	SEND1		;XON SENDEN
F2F6   7D	   644	LOAD2	LD	A,L
F2F7   E1	   645		POP	HL
F2F8   FDA608	   646		AND	(IY+INMSK)
F2FB   FDE1	   647		POP	IY
F2FD   C9	   648		RET
		   649
F2FE   FD6E0D	   650	GENADR	LD	L,(IY+BUFADR)
F301   FD660E	   651		LD	H,(IY+BUFADR+1)
F304   C5	   652		PUSH	BC
F305   4F	   653		LD	C,A
F306   0600	   654		LD	B,0
F308   09	   655		ADD	HL,BC
F309   3C	   656		INC	A		;PUFFERPOINTER
F30A   FDBE0C	   657		CP	(IY+BUFLEN)	;OBERE ADRESSE DES PUFFERS ERREICHT ?
F30D   2001	   658		JR	NZ,GENA1
F30F   AF	   659		XOR	A		;JA UNTERSTE ADRESSE SETZEN
F310   C1	   660	GENA1	POP	BC
F311   C9	   661		RET
		   662
		   663	;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
		   664	;SI - SERIAL INTERFACE INPUT
		   665	;INPUT:
		   666	;OUTPUT: CARRY FLAG = 0: GUELTIGES ZEICHEN IN A
		   667	;	 CARRY FLAG = 1	KEIN ZEICHEN, A=1AH WENN SI NICHT BEREIT
		   668	;VERAENDERT A, FLAGS
		   669
F312   FDE5	   670	SI	PUSH	IY
F314   FD21E0F1	   671		LD	IY,SIOSI
F318   18A9	   672		JR	LOAD
		   673
		   674	;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
		   675	;SOSTS - SERIAL	INTERFACE OUTPUT STATUS
		   676	;INPUT:
		   677	;OUTPUT: REG. A	ENTHALT	00 WENN	SO NICHT BEREIT
		   678	;	 REG. A	ENTHAELT FF WENN SO BEREIT
		   679	;VERAENDERT A, FLAGS
		   680
F31A   FDE5	   681	SOSTS	PUSH	IY
F31C   FD21E0F1	   682		LD	IY,SIOSI
F320   1806	   683		JR	LSTS1
		   684
		   685	;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
		   686	;LSTS -	LIST OUTPUT STATUS
		   687	;INPUT:
		   688	;OUTPUT: REG. A	ENTHAELT 00 WENN DRUCKER BESCHAEFTIGT
		   689	;	 REG. A	ENTHAELT FF WENN DRUCKER FUER NAECHSTES	ZEICHEN	BEREIT
		   690	;VERAENDERT A, FLAGS
		   691ŠF322   FDE5	   692	LSTS	PUSH	IY

			      BIOS				PAGE  14
 LOC   OBJ CODE	M STMT SOURCE STATEMENT				 ASM 5.9

F324   FD2102F2	   693		LD	IY,LPSTRUCT
F328   CD2DF3	   694	LSTS1	CALL	SIORDY
F32B   186D	   695		JR	SO2
		   696
		   697	;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
		   698	;TEST OB NAECHSTES ZEICHEN AUSGEGEBEN WERDEN KANN
		   699
F32D   C5	   700	SIORDY	PUSH	BC
F32E   AF	   701		XOR	A
F32F   FDCB0056	   702		BIT	2,(IY+FLAG)	;AUSGABE HALT ?	(^S) ?
F333   201D	   703		JR	NZ,SIOR2	;JA, NICHT BEREIT
F335   FD4E03	   704		LD	C,(IY+SIOADR)
F338   0C	   705		INC	C		;CONTROL ADRESSE READ REGISTER 0
F339   0610	   706		LD	B,10H		;RUECKSETZEN EXTERNAL STATUS INT
F33B   ED41	   707		OUT	(C),B		;DAMIT DCD UEBERNOMMEN WIRD
F33D   ED40	   708		IN	B,(C)
F33F   CB60	   709		BIT	4,B		;IFSS ?
F341   200A	   710		JR	NZ,SIOR1	;JA
F343   FDCB006E	   711		BIT	5,(IY+FLAG)	;V24 DTR BEHANDELN ?
F347   2804	   712		JR	Z,SIOR1		;NEIN
F349   CB58	   713		BIT	3,B		;DCD, DTR INAKTIV ?
F34B   2805	   714		JR	Z,SIOR2		;JA
F34D   CB50	   715	SIOR1	BIT	2,B		;SENDEPUFFER LEER ?
F34F   2801	   716		JR	Z,SIOR2		;NEIN
F351   2F	   717		CPL
F352   B7	   718	SIOR2	OR	A
F353   C1	   719		POP	BC
F354   C9	   720		RET
		   721
		   722	;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
		   723	;ZEICHEN AUSGEBEN
		   724
F355   CD2DF3	   725	SEND	CALL	SIORDY		;KANN ZEICHEN AUSGEGEBEN WERDEN	?
F358   28FB	   726		JR	Z,SEND		;NEIN, WARTEN
		   727
F35A   C5	   728	SEND1	PUSH	BC
F35B   79	   729		LD	A,C
F35C   FD4E03	   730		LD	C,(IY+SIOADR)
F35F   0C	   731		INC	C		;READ REGISTER 0
F360   ED40	   732	SEND2	IN	B,(C)
F362   CB50	   733		BIT	2,B		;SENDEPUFFER LEER ?
F364   28FA	   734		JR	Z,SEND2
F366   0D	   735		DEC	C		;DATENADRESSE
F367   ED79	   736		OUT	(C),A
F369   C1	   737		POP	BC
F36A   C9	   738		RET
		   739
		   740	;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
		   741	;CO - CONSOLE OUTPUT
		   742	;INPUT:	ZEICHEN	IN C
		   743	;OUTPUT:
		   744	;VERAENDERT A, FLAGS
		   745
F36B   FDE5	   746	CO	PUSH	IY
F36D   FD21F1F1	   747		LD	IY,SIOCON
F371   CD55F3	   748		CALL	SEND
F374   FDCB0066	   749		BIT	4,(IY+FLAG)	;VERZOEGERUNG AUSGESCHALTET ?ŠF378   2020	   750		JR	NZ,SO2

			      BIOS				PAGE  15
 LOC   OBJ CODE	M STMT SOURCE STATEMENT				 ASM 5.9

F37A   E5	   751		PUSH	HL
F37B   79	   752		LD	A,C
F37C   213205	   753		LD	HL,1330		;ca 10ms
F37F   FE0A	   754		CP	0AH
F381   2806	   755		JR	Z,COWAIT
F383   21A00F	   756		LD	HL,4000		;ca 30ms
F386   B7	   757		OR	A
F387   2005	   758		JR	NZ,COEND
F389   2B	   759	COWAIT	DEC	HL
F38A   7D	   760		LD	A,L
F38B   B4	   761		OR	H
F38C   20FB	   762		JR	NZ,COWAIT
F38E   E1	   763	COEND	POP	HL
F38F   1809	   764		JR	SO2
		   765
		   766	;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
		   767	;SO - SERIAL INTERFACE OUTPUT
		   768	;INPUT:	ZEICHEN	IN C
		   769	;OUTPUT:
		   770	;VERAENDERT A, FLAGS
		   771
F391   FDE5	   772	SO	PUSH	IY
F393   FD21E0F1	   773		LD	IY,SIOSI
F397   CD55F3	   774	SO1	CALL	SEND
F39A   FDE1	   775	SO2	POP	IY
F39C   C9	   776		RET
		   777
		   778	;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
		   779	;LO - LIST OUTPUT (DRUCKER)
		   780	;INPUT:	ZEICHEN	IN C
		   781	;OUTPUT:
		   782	;VERAENDERT A, FLAGS, C
		   783
		   784
F39D   FDE5	   785	LO	PUSH	IY
F39F   FD2102F2	   786		LD	IY,LPSTRUCT
F3A3   FDCB16C6	   787		SET	0,(IY+LPFLAG)	;DRUCKER WURDE AUFGERUFEN
F3A7   79	   788		LD	A,C
F3A8   FDCB164E	   789		BIT	1,(IY+LPFLAG)	;LETZTES ZEICHEN ESC ?
F3AC   FDCB168E	   790		RES	1,(IY+LPFLAG)
F3B0   2822	   791		JR	Z,LPGOON	;NEIN
		   792
		   793	;WORDSTAR ANPASSUNG FUER DRUCKER 1152 UND K63..
		   794
F3B2   FE8B	   795		CP	8BH
F3B4   2833	   796		JR	Z,LF6313
F3B6   F5	   797		PUSH	AF
F3B7   0E1B	   798		LD	C,1BH
F3B9   CD55F3	   799		CALL	SEND
F3BC   F1	   800		POP	AF
F3BD   4F	   801		LD	C,A
F3BE   3033	   802		JR	NC,LP0
F3C0   FE80	   803		CP	80H
F3C2   382F	   804		JR	C,LP0
F3C4   F5	   805		PUSH	AF
F3C5   0E5B	   806		LD	C,5BH
F3C7   CD55F3	   807		CALL	SENDŠF3CA   F1	   808		POP	AF

			      BIOS				PAGE  26
 LOC   OBJ CODE	M STMT SOURCE STATEMENT				 ASM 5.9

		  1320	*E
		  1321	;*-----------------------------------------------------------------------------
		  1322	;*
		  1323	;* Floppy Disk Treiber
		  1324	;*
		  1325	;*-----------------------------------------------------------------------------
		  1326
		  1327	*I FLOPPY.S
		  1328	;******************************************************************************
		  1329	;
		  1330	;	Floppydisk Treiber fuer	P8000
		  1331	;
		  1332	;******************************************************************************
		  1333
		  1334
		  1335
		  1336	;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
		  1337	;ENTRY FLOPPY TREIBER
		  1338
F6B3   2A76FA	  1339	FLOPPY	LD	HL,(DSKVSL)	;ist Parametervektor eingetragen ?
F6B6   7C	  1340		LD	A,H
F6B7   B5	  1341		OR	L
F6B8   20F9	  1342		JR	NZ,FLOPPY	;Treiber besetzt --> warten !
F6BA   FD2276FA	  1343		LD	(DSKVSL),IY	;Treiber ist frei --> Parametervektor
		  1344					;eintragen und damit als "besetzt" setzen
F6BE   CDD2F6	  1345		CALL	FLP		;CALL-Aufruf des Treibers
F6C1   FDCB0146	  1346	WAIT	BIT	0,(IY+1)	;zeitgeteilter Verkehr ?
F6C5   C0	  1347	WAIT1	RET	NZ
F6C6   FDCB0A7E	  1348		BIT	7,(IY+10)	;Operation fertig ?
F6CA   18F9	  1349		JR	WAIT1
		  1350
		  1351	;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
		  1352	;RETURN	FUER WARTEN AUF	INTERRUPT
		  1353
F6CC   E1	  1354	SFLPI	POP	HL		;IN HL ADR WO PROGRAMM NACH
F6CD   2278FA	  1355		LD	(INTPNT),HL	;INTERRUPT WEITER LAEUFT
F6D0   FB	  1356		EI			;falls Programm	als ISR	des PIO2 laeuft
		  1357					;kann neuer INT	vom PIO2 erst nach RETI
F6D1   C9	  1358		RET			;erfolgen
		  1359
		  1360	;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
		  1361	;Auswertung UDOS-Requestcode
		  1362
F6D2   FD7E01	  1363	FLP	LD	A,(IY+1)	;Request Code
F6D5   E6FE	  1364		AND	0FEH		;Loeschen der Abarbeitungsart
F6D7   FE0A	  1365		CP	0AH		;Read Binary Request ?
F6D9   2828	  1366		JR	Z,TYPTEST
F6DB   FE0E	  1367		CP	0EH		;Write Binary Request ?
F6DD   2824	  1368		JR	Z,TYPTEST
F6DF   FE50	  1369		CP	50H		;Format
F6E1   2820	  1370		JR	Z,TYPTEST
F6E3   3EC1	  1371		LD	A,0C1H		;'Invalid  Request'
F6E5   CDC3F8	  1372	ERRRTN	CALL	OPFIN
F6E8   FD6609	  1373		LD	H,(IY+9)
F6EB   FD6E08	  1374		LD	L,(IY+8)
F6EE   7C	  1375		LD	A,H
F6EF   B5	  1376		OR	LŠF6F0   2801	  1377		JR	Z,FEHL1

			      BIOS				PAGE  37
 LOC   OBJ CODE	M STMT SOURCE STATEMENT				 ASM 5.9

		  1958
F9BC   F3	  1959		DI			;besser	DI
F9BD   DB1E	  1960		IN	A,(PIO2BD)
F9BF   F60F	  1961		OR	0FH		;Select	altes Laufwerk ruecksetzen
F9C1   A0	  1962		AND	B
F9C2   D31E	  1963		OUT	(PIO2BD),A	;Ausgabe des "Drive select" an PIO/B
F9C4   DB1C	  1964		IN	A,(PIO2AD)
F9C6   A0	  1965		AND	B		;Motor LW an
F9C7   CBEF	  1966		SET	5,A		;Kopf an
F9C9   D31C	  1967		OUT	(PIO2AD),A
F9CB   3EA5	  1968		LD	A,0A5H		;CTCINT	fuer Motor aus programmieren
F9CD   D309	  1969		OUT	(CTC0+1),A
F9CF   3EFF	  1970		LD	A,255		;16,3ms
F9D1   D309	  1971		OUT	(CTC0+1),A
F9D3   210015	  1972		LD	HL,01500H	;Warteschleife
F9D6   010402	  1973	RDY2	LD	BC,0204H	;Kommando "Sense Drive Status"
F9D9   CDDAF8	  1974		CALL	FDCKMD
F9DC   E5	  1975		PUSH	HL
F9DD   CDF4F8	  1976		CALL	RDRES
F9E0   E1	  1977		POP	HL
F9E1   FB	  1978		EI
F9E2   CB6F	  1979		BIT	5,A		;FDD ready ?
F9E4   2005	  1980		JR	NZ,RDY3		;JA
F9E6   2B	  1981		DEC	HL
F9E7   7C	  1982		LD	A,H
F9E8   B5	  1983		OR	L
F9E9   20EB	  1984		JR	NZ,RDY2		;not ready
F9EB   E1	  1985	RDY3	POP	HL
		  1986	 COND UDOS
		  1987		RET	Z		;LW nicht bereit
		  1988		LD	A,(DMASK)
		  1989		OR	(HL)		;bereites LW in	DSEL eintragen
		  1990		LD	(HL),A
		  1991	 ENDC
F9EC   C9	  1992		RET			;mit NZ
		  1993
		  1994	;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
		  1995	;Tabelle der physischen	Floppy-Formate
		  1996
		  1997	;Standard FM 77	Spuren 26 Sektoren a 128 Byte (Typ 1)
		  1998	;8" Laufwerk
		  1999
F9ED   6F	  2000	TYPTAB	DEFB	6FH		;SRT=10ms, HUT=240ms (siehe DOK	8272)
F9EE   28	  2001		DEFB	28H		;HLT=40ms (8MHz)
F9EF   00	  2002		DEFB	0		;N (Sektorlaenge)
F9F0   1A	  2003		DEFB	26		;EOT (letzte Sektornummer)
F9F1   07	  2004		DEFB	07H		;GPL
F9F2   80	  2005		DEFB	128		;DTL - Ende 8272-Daten
F9F3   FF	  2006		DEFB	-1		;PR1 (Praekompensation 1) 200ns
		  2007					;ab Spur .. (hier keine)
F9F4   FF	  2008		DEFB	-1		;PR2 (Praekompensation 2) 400ns
		  2009					;(keine)
F9F5   1B	  2010		DEFB	1BH		;GPL fuer Format
F9F6   4D	  2011		DEFB	77		;Anzahl	der Spuren
F9F7   02	  2012		DEFB	02H		;MT=0, MFM=0, SK=0, 8"=1, DSTEP=0
		  2013					;bit7, bit6,  bit5, bit1, bit0
		  2014Š		  2015	;Mini MFM 40 Spuren 16 Sektoren	a 256 Byte (Typ	2)

			      BIOS				PAGE  38
 LOC   OBJ CODE	M STMT SOURCE STATEMENT				 ASM 5.9

		  2016	;K5600.10, MF1.2
		  2017
F9F8   9F	  2018		DEFB	9FH		;SRT=14ms, HUT=480ms
F9F9   14	  2019		DEFB	14H		;HLT=40ms (4MHz)
F9FA   01	  2020		DEFB	01H		;N
F9FB   10	  2021		DEFB	16		;EOT
F9FC   22	  2022		DEFB	22H		;GPL
F9FD   FF	  2023		DEFB	-1		;DTL
F9FE   14	  2024		DEFB	20		;PR1 ab	Spur 20
F9FF   1D	  2025		DEFB	29		;PR2 ab	Spur 29
FA00   32	  2026		DEFB	32H		;GPL fuer Format
FA01   28	  2027		DEFB	40		;Anzahl	der Spuren
FA02   40	  2028		DEFB	40H		;MT=0, MFM=1, SK=0, 8"=0, DSTEP=0
		  2029
		  2030	;Mini MFM 40 Spuren auf	80 Spur	Drive 16 Sektoren a 256	Byte (Typ 3)
		  2031	;K5600.20, MF1.4 (auch MF1.6)
		  2032
FA03   BF	  2033		DEFB	0BFH		;SRT=10ms, HUT=480ms
FA04   08	  2034		DEFB	08H		;16ms (nur Kopfberuhigungszeit nach Steppen)
FA05   01	  2035		DEFB	01H		;N
FA06   10	  2036		DEFB	16		;EOT
FA07   22	  2037		DEFB	22H		;GPL
FA08   FF	  2038		DEFB	-1		;DTL
FA09   23	  2039		DEFB	35		;PR1 ab	Spur 35	(bezogen auf 80	Spuren)
FA0A   FF	  2040		DEFB	-1		;keine PR2
FA0B   32	  2041		DEFB	32H		;GPL fuer Format
FA0C   28	  2042		DEFB	40		;Anzahl	der Spuren
FA0D   41	  2043		DEFB	41H		;MT=0, MFM=1, SK=0, 8"=0, DSTEP=1
		  2044
		  2045	;Mini MFM 80 Spuren 16 Sektoren	a 256 Byte (Typ	4)
		  2046	;K5600.20, MF1.4 (auch MF1.6)
		  2047
FA0E   BF	  2048		DEFB	0BFH		;SRT=10ms, HUT=480ms
FA0F   08	  2049		DEFB	08H		;HLT=16ms
FA10   01	  2050		DEFB	01H		;N
FA11   10	  2051		DEFB	16		;EOT
FA12   22	  2052		DEFB	22H		;GPL
FA13   FF	  2053		DEFB	-1		;DTL
FA14   23	  2054		DEFB	35		;PR 1 ab Spur 35
FA15   FF	  2055		DEFB	-1		;keine PR 2
FA16   32	  2056		DEFB	32H		;GPL fuer Format
FA17   50	  2057		DEFB	80		;Anzahl	der Spuren
FA18   40	  2058		DEFB	40H		;MT=0, MFM=1, SK=0, 8"=0, DSTEP=0
		  2059
		  2060	;Mini MFM Doppelkopf 80	Spuren 2*16 Sektoren a 256 Byte	(Typ 5)
		  2061	;MF1.6
		  2062
FA19   EF	  2063		DEFB	0EFH		;SRT=4ms, HUT=480ms
FA1A   08	  2064		DEFB	08H		;HLT=16ms
FA1B   01	  2065		DEFB	01H		;N
FA1C   10	  2066		DEFB	16		;EOT
FA1D   22	  2067		DEFB	22H		;GPL
FA1E   FF	  2068		DEFB	-1		;DTL
FA1F   FF	  2069		DEFB	-1		;keine PR1
FA20   FF	  2070		DEFB	-1		;keine PR2
FA21   32	  2071		DEFB	32H		;GPL fuer Format
FA22   50	  2072		DEFB	80		;Anzahl	der SpurenŠFA23   C0	  2073		DEFB	0C0H		;MT=1, MFM=1, SK=0, 8"=0, DSTEP=0

			      BIOS				PAGE  39
 LOC   OBJ CODE	M STMT SOURCE STATEMENT				 ASM 5.9

		  2074
		  2075	;Mini MFM Doppelkopf 80	Spuren 2*9 Sektoren a 512 Byte (Typ 6)
		  2076	;MF1.6
		  2077
FA24   EF	  2078		DEFB	0EFH		;SRT=4ms, HUT=480ms
FA25   08	  2079		DEFB	08H		;HLT=16ms
FA26   02	  2080		DEFB	02H		;N
FA27   09	  2081		DEFB	9		;EOT
FA28   1B	  2082		DEFB	1BH		;GPL
FA29   FF	  2083		DEFB	-1		;DTL
FA2A   FF	  2084		DEFB	-1		;keine PR1
FA2B   FF	  2085		DEFB	-1		;keine PR2
FA2C   54	  2086		DEFB	54H		;GPL fuer Format
FA2D   50	  2087		DEFB	80		;80 Spuren
FA2E   C0	  2088		DEFB	0C0H		;MT=1, MFM=1, SK=0, 8"=0, DSTEP=0
		  2089
		  2090	;Mini MFM Doppelkopf 80	Spuren 2*5 Sektoren a 1024 Byte	(Typ 7)
		  2091	;MF1.6
		  2092
FA2F   EF	  2093		DEFB	0EFH		;SRT=4ms, HUT=480ms
FA30   08	  2094		DEFB	08H		;HLT=16ms
FA31   03	  2095		DEFB	03H		;N
FA32   05	  2096		DEFB	5		;EOT
FA33   35	  2097		DEFB	35H		;GPL
FA34   FF	  2098		DEFB	-1		;DTL
FA35   FF	  2099		DEFB	-1		;keine PR1
FA36   FF	  2100		DEFB	-1		;keine PR2
FA37   74	  2101		DEFB	74H		;GPL fuer Format
FA38   50	  2102		DEFB	80		;80 Spuren
FA39   C0	  2103		DEFB	0C0H		;MT=1, MFM=1, SK=0, 8"=0, DSTEP=0
		  2104
		  2105	;User-Format 1 (entspricht Format 7)  (Typ 8)
		  2106
FA3A   EF	  2107		DEFB	0EFH		;SRT=4ms, HUT=480ms
FA3B   08	  2108		DEFB	08H		;HLT=16ms
FA3C   03	  2109		DEFB	03H		;N
FA3D   05	  2110		DEFB	5		;EOT
FA3E   35	  2111		DEFB	35H		;GPL
FA3F   FF	  2112		DEFB	-1		;DTL
FA40   FF	  2113		DEFB	-1		;keine PR1
FA41   FF	  2114		DEFB	-1		;keine PR2
FA42   74	  2115		DEFB	74H		;GPL fuer Format
FA43   50	  2116		DEFB	80		;80 Spuren
FA44   C0	  2117		DEFB	0C0H		;MT=1, MFM=1, SK=0, 8"=0, DSTEP=0
		  2118
		  2119	;User-Format 2 (entspricht Format 7)  (Typ 9)
		  2120
FA45   EF	  2121		DEFB	0EFH		;SRT=4ms, HUT=480ms
FA46   08	  2122		DEFB	08H		;HLT=16ms
FA47   03	  2123		DEFB	03H		;N
FA48   05	  2124		DEFB	5		;EOT
FA49   35	  2125		DEFB	35H		;GPL
FA4A   FF	  2126		DEFB	-1		;DTL
FA4B   FF	  2127		DEFB	-1		;keine PR1
FA4C   FF	  2128		DEFB	-1		;keine PR2
FA4D   74	  2129		DEFB	74H		;GPL fuer Format
FA4E   50	  2130		DEFB	80		;80 SpurenŠFA4F   C0	  2131		DEFB	0C0H		;MT=1, MFM=1, SK=0, 8"=0, DSTEP=0

			      BIOS				PAGE  40
 LOC   OBJ CODE	M STMT SOURCE STATEMENT				 ASM 5.9

		  2132
		  2133	;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
		  2134	;VEREINBARUNGEN
		  2135	;Kommandotabelle fuer den FDC
		  2136
		  2137	 COND 1-UDOS
		  2138
FA50   00	  2139	FDCTAB	DEFB	00H		;Command Codes (MT,MFM,SK & Read/Write)
FA51   00	  2140		DEFB	00H		;Bit 2,1,0: HDS,DS1,DS0
FA52   00	  2141		DEFB	0		;Spur
FA53   00	  2142		DEFB	00H		;Kopf
FA54   00	  2143		DEFB	00H		;Record
FA55   01	  2144		DEFB	01H		;256 Byte/Sector
FA56   10	  2145		DEFB	16		;EOT (last sector on track)
FA57   0E	  2146		DEFB	0EH		;GPL (Length of	gap 3)
FA58   FF	  2147		DEFB	0FFH		;DTL (N=0 --> Data length)
		  2148
FA59   00	  2149	PR1	DEFB	0
FA5A   00	  2150	PR2	DEFB	0
FA5B   00	  2151	GPL2	DEFB	0
FA5C   00	  2152	TRK	DEFB	0
FA5D   00	  2153	CMD	DEFB	0
		  2154
		  2155	;DMA-Tabelle fuer die Uebernahme/Uebergabe der Daten vom/zum FDC
FA5E   79	  2156	DMALSW	DEFB	79H		;Datenlaenge und Datenadresse ist einzutragen
FA5F   00	  2157		DEFB	0		;Daten-Adresse Low-Teil
FA60   00	  2158		DEFB	0		;Daten-Adresse High-Teil
FA61   00	  2159		DEFB	0		;Datenlaenge Low-Teil
FA62   00	  2160		DEFB	0		;Datenlaenge High-Teil
FA63   CF	  2161		DEFB	0CFH		;Laden der Adresse und Laenge
FA64   05	  2162		DEFB	5		;Richtungsumkehr programmieren
FA65   CF	  2163		DEFB	0CFH		;Laden Richtungsumkehr
FA66   AB	  2164		DEFB	0ABH		;Interruptfreigabe fuer	DMA
FA67   87	  2165		DEFB	87H		;DMA aktivieren
		  2166
		  2167	;Resultats-Tabelle der FDC-Kommandos
		  2168
FA68		  2169	RESUL	DEFS	7
		  2170
		  2171	;Motorzeittabelle
		  2172
FA6F   00	  2173	TTAB	DEFB	0		;Motor 0 aus
FA70   00	  2174		DEFB	0		;Motor 1 aus
FA71   00	  2175		DEFB	0		;Motor 2 aus
FA72   00	  2176		DEFB	0		;Motor 3 aus
		  2177
		  2178	;Fehlerzaehler
FA73   00	  2179	WSCTR	DEFB	0		;Fehlerzaehler Sektor
FA74   00	  2180	WTCTR	DEFB	0		;Fehlerzaehler Spur
FA75   00	  2181	CRCCTR	DEFB	0		;CRC-Fehlerzaehler
		  2182
FA76   0000	  2183	DSKVSL	DEFW	0		;Disk Semaphor
FA78   0000	  2184	INTPNT	DEFW	0		;Adresse fuer Programmfortsetzung
FA7A   00	  2185	DMASK	DEFB	0		;LW Maske fuer Motor aus und ...
FA7B   00	  2186	FDTYP	DEFB	0
FA7C   FF	  2187	CURT0	DEFB	-1		;Tabelle der Kopfpositionen der	Laufwerke
FA7D   FF	  2188		DEFB	-1ŠFA7E   FF	  2189		DEFB	-1

			      BIOS				PAGE  42
 LOC   OBJ CODE	M STMT SOURCE STATEMENT				 ASM 5.9

		  2193	*E
		  2194	;*-----------------------------------------------------------------------------
		  2195	;*
		  2196	;* Kaltstart Prozedure
		  2197	;*
		  2198	;*-----------------------------------------------------------------------------
		  2199
FA80   21FFFF	  2200	BOOT	LD	HL,-1
FA83   2B	  2201	TLOOP	DEC	HL
FA84   7D	  2202		LD	A,L
FA85   B4	  2203		OR	H
FA86   20FB	  2204		JR	NZ,TLOOP
FA88   F3	  2205		DI
FA89   3E03	  2206		LD	A,03H
FA8B   D309	  2207		OUT	(CTC0+1),A
		  2208
FA8D   3EFF	  2209		LD	A,0FFH		;NEUER INT VEKTOR
FA8F   ED47	  2210		LD	I,A
FA91   AF	  2211		XOR	A
FA92   D308	  2212		OUT	(CTC0),A
FA94   31C0FF	  2213		LD	SP,BIOSTAK
		  2214
		  2215	;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
		  2216	;PARAMETER AUS URLADER UEBERNEHMEN
		  2217
FA97   3E03	  2218		LD	A,3		;SIOSTRUCTUREN
FA99   21600C	  2219		LD	HL,0C60H	;ADR IM	URLADER
FA9C   11E0F1	  2220		LD	DE,SIOBASE
FA9F   010900	  2221	L1	LD	BC,9
FAA2   EDB0	  2222		LDIR
FAA4   0608	  2223		LD	B,8
FAA6   13	  2224	L2	INC	DE
FAA7   10FD	  2225		DJNZ	L2
FAA9   3D	  2226		DEC	A
FAAA   20F3	  2227		JR	NZ,L1
		  2228
FAAC   0E03	  2229		LD	C,3		;LPSTRUCT+LINES
FAAE   EDB0	  2230		LDIR
FAB0   2A7E0C	  2231		LD	HL,(0C7EH)	;FDCONF
FAB3   224EF0	  2232		LD	(FDCONF),HL
		  2233
		  2234	;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
		  2235	;CCP vor Ueberschreiben	schuetzen, folgendes Programm eintragen:
		  2236
		  2237	;	ORG	CCP-20H
		  2238	;BDENTRY:
		  2239	;	JP	BDOS
		  2240	;ERR1	DEFW	BDOSERROR1
		  2241	;ERR2	DEFW	BDOSERROR2
		  2242	;ERR3	DEFW	BDOSERROR3
		  2243	;ERR4	DEFW	BDOSERROR4
		  2244	;ERROR1	LD	HL,(ERR1)
		  2245	;	JP	(HL)
		  2246	;ERROR2	LD	HL,(ERR2)
		  2247	;	JP	(HL)
		  2248	;ERROR3	LD	HL,(ERR3)
		  2249	;	JP	(HL)Š		  2250	;ERROR4	LD	HL,(ERR4)

			      BIOS				PAGE  43
 LOC   OBJ CODE	M STMT SOURCE STATEMENT				 ASM 5.9

		  2251	;	JP	(HL)
		  2252
FAB6   2106E2	  2253		LD	HL,BDOS		;BDOS ADRESSEN UEBERNEHMEN
FAB9   11E0D9	  2254		LD	DE,BDENTRY
FABC   010B00	  2255		LD	BC,11
FABF   EDB0	  2256		LDIR
FAC1   EB	  2257		EX	DE,HL
FAC2   11E3D9	  2258		LD	DE,BDENTRY+3	;FEHLER-SPRUENGE ANLEGEN
FAC5   DD2109E2	  2259		LD	IX,BDOS+3	;UND ERRORX ADRESSEN IN	BDOS EINTRAGEN
FAC9   0604	  2260		LD	B,4
FACB   DD7500	  2261	BDLOOP	LD	(IX),L
FACE   DD23	  2262		INC	IX
FAD0   DD7400	  2263		LD	(IX),H
FAD3   DD23	  2264		INC	IX
FAD5   362A	  2265		LD	(HL),2AH	;LD HL,()
FAD7   23	  2266		INC	HL
FAD8   73	  2267		LD	(HL),E
FAD9   23	  2268		INC	HL
FADA   72	  2269		LD	(HL),D
FADB   23	  2270		INC	HL
FADC   36E9	  2271		LD	(HL),0E9H	;JP (HL)
FADE   23	  2272		INC	HL
FADF   13	  2273		INC	DE
FAE0   13	  2274		INC	DE
FAE1   00	  2275		NOP
FAE2   00	  2276		NOP
FAE3   10E6	  2277		DJNZ	BDLOOP
		  2278
		  2279	;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
		  2280	;SIO KANAELE INITIALISIEREN
		  2281
FAE5   3E18	  2282		LD	A,COMM3		;KANAL RESET
FAE7   D325	  2283		OUT	(SIO0AD+1),A
FAE9   D327	  2284		OUT	(SIO0BD+1),A
FAEB   D329	  2285		OUT	(SIO1AD+1),A
FAED   D32B	  2286		OUT	(SIO1BD+1),A
FAEF   219BFB	  2287		LD	HL,SIOITAB	;INT VEKTOR + STATUS BEEINFLUSST VEKTOR
		  2288					;NUR UEBER KANAL B MOEGLICH
FAF2   012704	  2289		LD	BC,0400H+SIO0BD+1
FAF5   EDB3	  2290		OTIR
FAF7   012B04	  2291		LD	BC,0400H+SIO1BD+1
FAFA   EDB3	  2292		OTIR
		  2293
FAFC   21E1F1	  2294		LD	HL,SIOSI+1	;AKTIVE	SIO KANAELE INIT
FAFF   CD36FB	  2295		CALL	SETSIO
FB02   21F2F1	  2296		LD	HL,SIOCON+1
FB05   CD36FB	  2297		CALL	SETSIO
FB08   2103F2	  2298		LD	HL,SIOLP+1
FB0B   CD36FB	  2299		CALL	SETSIO
		  2300
FB0E   CDC3FB	  2301		CALL	IFLOPPY		;INIT des Floppy Treibers
FB11   FB	  2302		EI
FB12   AF	  2303		XOR	A
FB13   320300	  2304		LD	(IOBYTE),A	;INIT IOBYTE
FB16   320400	  2305		LD	(CDISK),A	;INIT CDISK DRIVE A
		  2306
FB19   0619	  2307		LD	B,25		;BILDSCHIRM LOESCHENŠFB1B   0E0D	  2308		LD	C,CR

			      BIOS				PAGE  48
 LOC   OBJ CODE	M STMT SOURCE STATEMENT				 ASM 5.9

		  2527	*E
		  2528	;*-----------------------------------------------------------------------------
		  2529	;*
		  2530	;* Puffer
		  2531	;*
		  2532	;*-----------------------------------------------------------------------------
		  2533
		  2534	;KALTSTART PROGRAMME WERDEN DURCH PUFFER UEBERSCHRIEBEN
		  2535
		  2536	DIRBUF	EQU	BOOT		;DEFS 128
		  2537	CSV0	EQU	DIRBUF+128	;DEFS 64
		  2538					;=CKS BYTES (max 256 Dir Eintraege)
		  2539	CSV1	EQU	CSV0+64		;DEFS 64
		  2540	CSV2	EQU	CSV1+64		;DEFS 48 (192 Dir Eintraege)
		  2541	CSV3	EQU	CSV2+48		;DEFS 48
		  2542	ALV0	EQU	CSV3+48		;DEFS 80
		  2543					;(DSM/8)+1 (max	640 Bloecke pro	Disk)
		  2544	ALV1	EQU	ALV0+80		;DEFS 80
		  2545	ALV2	EQU	ALV1+80		;DEFS 50 (400 Bloecke)
		  2546	ALV3	EQU	ALV2+50		;DEFS 50
		  2547	ALVEND	EQU	ALV3+50
		  2548
FCE4		  2549		ORG	ALVEND
		  2550
FCE4		  2551	CIBUF	DEFS	50
FD16		  2552	LPBUF	DEFS	5
FD1B		  2553	SIBUF	DEFS	160
FDBB		  2554		DEFS	0
		  2555
		  2556	;*-----------------------------------------------------------------------------
		  2557	;*
		  2558	;*	Interrupt Tabelle (von FF00 bis	FF7F)
		  2559	;*
		  2560	;*-----------------------------------------------------------------------------
		  2561
FF00		  2562		ORG	0FF00H
		  2563
FF00   9DF9	  2564	INTTAB	DEFW	ISRDMA		;CTC0
FF02   63F9	  2565		DEFW	ISRCT1
FF04   0000	  2566		DEFW	0
FF06   0000	  2567		DEFW	0
		  2568
FF08   10F9	  2569		DEFW	ISRPB		;CTC1
FF0A   0000	  2570		DEFW	0
FF0C   0000	  2571		DEFW	0
FF0E   0000	  2572		DEFW	0
		  2573
FF10   0000	  2574		DEFW	0
FF12   0000	  2575		DEFW	0
FF14   0000	  2576		DEFW	0
FF16   0000	  2577		DEFW	0
FF18   0000	  2578		DEFW	0
FF1A   0000	  2579		DEFW	0
FF1C   0000	  2580		DEFW	0
FF1E   0000	  2581		DEFW	0
		  2582
FF20   5DF2	  2583		DEFW	NOINT		;SENDER	UND EXT/STATUS INTŠFF22   5DF2	  2584		DEFW	NOINT		;NIE ZUGELASSEN

			      BIOS				PAGE  49
 LOC   OBJ CODE	M STMT SOURCE STATEMENT				 ASM 5.9

FF24   5DF2	  2585		DEFW	NOINT
FF26   5DF2	  2586		DEFW	NOINT
FF28   5DF2	  2587		DEFW	NOINT
FF2A   5DF2	  2588		DEFW	NOINT
FF2C   5DF2	  2589		DEFW	NOINT
FF2E   5DF2	  2590		DEFW	NOINT
		  2591
FF30   5DF2	  2592		DEFW	NOINT
FF32   5DF2	  2593		DEFW	NOINT
FF34   5DF2	  2594		DEFW	NOINT
FF36   5DF2	  2595		DEFW	NOINT
FF38   5DF2	  2596		DEFW	NOINT
FF3A   5DF2	  2597		DEFW	NOINT
FF3C   5DF2	  2598		DEFW	NOINT
FF3E   5DF2	  2599		DEFW	NOINT
		  2600	;REST BIS 7F FREI
		  2601
		  2602
		  2603	;*-----------------------------------------------------------------------------
		  2604	;*
		  2605	;*	Stackerweiterung fuer CCP
		  2606	;*
		  2607	;*-----------------------------------------------------------------------------
		  2608
FF80		  2609		ORG	0FF80H
		  2610
FF80		  2611		DEFS	40H
FFC0		  2612	BIOSTAK	DEFS	0
FFC0		  2613		DEFS	38H
FFF8		  2614	CCPSTAK	DEFS	0
		  2615
		  2616	;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
		  2617
		  2618	;	ENDE DES PROGRAMMS
		  2619
		  2620	;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
