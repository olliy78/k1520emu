;#################################################
;INCLUDE-FILE "PLAYER-ENGINE"
;FÜR STC-PLAYER MIT AY-3-8910 
;Version für A5120 / K8924 u.ä.
;#################################################
;-------------------------------------------------
;ORIGINAL-VERÖFFENTLICHUNGEN:
;"MUSIK-TÜRKLINGEL MIT DEM AUDIO-COPROZESSOR AY8910"
;MAGAZIN 'RADIO', 1998/6, S. 42 + 43 (RUSSISCH)
;http://www.radioman-portal.ru/magazin/radio/1998/6.php
;
;ABSPIELROUTINE FÜR ZX81 MIT AY-3-8910-ADAPTER
;http://www.sinclairzxworld.com/viewtopic.php?t=528&start=100
;-------------------------------------------------
;PRINZIP CTC-TEIL: ENTWICKLERTEAM "M066"
;BEARBEITUNG, INCLUDE UND DOKU: WERO 10/2017
;ASSEMBLER: ASIDE
;-------------------------------------------------
;VERSION 1.1:
;* ISR GEÄNDERT
;* ARBEITSZELLEN STC-PLAYER VOR START GELÖSCHT
;* MISCHERBITS PORTREGISTER: BEI SONG-START/ENDE AUF EINGABE
;VERSION 1.2:
;* MISCHERBIST PORTREGISTER: BLEIBEN IMMER UNANGETASTET
;-------------------------------------------------
;Anpassungen an K1520-SOUND-Karte 
;Bastelsystem mit ZRE K2526 unter CP/A, SCPX 
;SOUND-CTC Interruptsäule-F7D8h I=F7h, IV=D8h
;IO-Adresse der SOUND-Karte = 38h
;SOUND-Karten CTC Kanal 0 als 20ms Zeitgeber
;Version 1.2.13
;KW 19.11.2017 (Klaus Wilfling)
;
;-------------------------------------------------
;ANFORDERUNGEN AN HAUPTPROGRAMM:
;* MUSS ARBEITSZELLENBEGINN MIT LABEL 'WORK' DEFINIEREN!
;* DARF LABELS X0001...X0087 NICHT BENUTZEN
;HARDWAREBEZÜGE-----------------------------------
;* EA-ADRESSEN ANPASSEN:
;
X0001:  EQU #38              ;AY-BASISADRESSE  (K1520/SOUND)
X0002:  EQU #3C              ;CTC-BASISADRESSE (K1520/SOUND)
;
;Adresse der Interruptsäule für SOUND-CTC 
;für CP/A und bei SCPX auf 0F7D8h, IM2 , I-Register F7h
;
X0003:  EQU #F7D8            ;INTERRUPTTABELLE (K1520 -CP/A, SCPX)
;
;
; Variante 1:* CTC K0+K1 KASKADIERT UND MIT 500KHZ AN TRG0 GETAKTET
; Variante 2:* CTC K0 als Zeitgeber verwendet, Systemtakt 2,4576 MHz --> 50Hz
;* CTC-PROGRAMMIERUNG BEI BEDARF ANPASSEN!
;
;-------------------------------------------------
;PLAYER-AREBEITSZELLEN AB WORK
X0004:  EQU WORK    ;WORD   ;WORK     ABLAGE LIEDANFANG   
X0005:  EQU X0004+2 ;BYTE   ;WORK+#02 ABLAGE ANZAHL SONG-WIEDERHOLUNGEN 
X0006:  EQU X0005+1 ;WORD   ;WORK+#03 POSITIONS-ZEIEGR
X0007:  EQU X0006+2 ;WORD   ;WORK+#05 ORNAMENT-ZEIGER      
X0008:  EQU X0007+2 ;WORD   ;WORK+#07 PATTERN-ZEIGER       
X0009:  EQU X0008+2 ;WORD   ;WORK+#09 POINTER 1. SAMPLE NR       
X0010:  EQU X0009+2 ;BYTE   ;WORK+#0B GLOBAL DELAY SONG  
X0011:  EQU X0010+1 ;BYTE   ;WORK+#0C GLOBAL DELAY WORK
X0012:  EQU X0011+1 ;BYTE   ;WORK+#0D ANZAHL  PATTERN IM SONG 
X0013:  EQU X0012+1 ;WORD   ;WORK+#0E ADRESSE PATTERN KANAL A 
X0014:  EQU X0013+2 ;WORD   ;WORK+#10 ADRESSE PATTERN KANAL B
X0015:  EQU X0014+2 ;WORD   ;WORK+#12 ADRESSE PATTERN KANAL C
X0016:  EQU X0015+2 ;BYTE   ;WORK+#14 ENDEMARKE (FF)
X0017:  EQU X0016+1 ;WORD   ;WORK+#15
;KANAL A------------------------------------------
X0018:  EQU X0017+2 ;BYTE   ;WORK+#17 INDEX A
                    ;BYTE   ;WORK+#18 NOTENNNUMMER
                    ;BYTE   ;WORK+#19 PAUSE ZWISCHEN NOTEN
X0019:  EQU X0018+3 ;WORD   ;WORK+#1A SAMPLEADRESSE
X0020:  EQU X0019+2 ;WORD   ;WORK+#1C ORNAMENTADRESSE
X0021:  EQU X0020+2 ;3XBYTE ;WORK+#1E 
;KANAL B------------------------------------------
X0022:  EQU X0021+3 ;BYTE   ;WORK+#21 INDEX B
                    ;BYTE   ;WORK+#22 NOTENNNUMMER
                    ;BYTE   ;WORK+#23 PAUSE ZWISCHEN NOTEN
X0023:  EQU X0022+3 ;WORD   ;WORK+#24 SAMPLEADRESSE
X0024:  EQU X0023+2 ;WORD   ;WORK+#26 ORNAMENTADRESSE
X0025:  EQU X0024+2 ;3XBYTE ;WORK+#28
;KANAL C------------------------------------------
X0026:  EQU X0025+3 ;BYTE   ;WORK+#2B INDEX C
                    ;BYTE   ;WORK+#2C NOTENNNUMMER
                    ;BYTE   ;WORK+#2D PAUSE ZWISCHEN NOTEN
X0027:  EQU X0026+3 ;WORD   ;WORK+#2E SAMPLEADRESSE
X0028:  EQU X0027+2 ;WORD   ;WORK+#30 ORNAMENTADRESSE
X0029:  EQU X0028+2 ;3XBYTE ;WORK+#32
;-------------------------------------------------
X0030:  EQU X0029+3 ;BYTE   ;WORK+#35 ABGESPIELTE PATTERNANZAHL
X0031:  EQU X0030+1 ;BYTE   ;WORK+#36 AKTUELLE PATTERNNUMMER
;-------------------------------------------------
;AUSZUGEBENDER BLOCK MIT AY3-PROGRAMMIERWERTEN
X0032:  EQU X0031+1 ;BYTE   ;WORK+#37 REG. 0=TONHÖHE KANAL A FEIN
                    ;BYTE   ;WORK+#38 REG. 1=TONHÖHE KANAL A GROB
X0033:  EQU X0032+2 ;BYTE   ;WORK+#39 REG. 2=TONHÖHE KANAL B FEIN
                    ;BYTE   ;WORK+#3A REG. 3=TONHÖHE KANAL B GROB
X0034:  EQU X0033+2 ;BYTE   ;WORK+#3B REG. 4=TONHÖHE KANAL C FEIN
                    ;BYTE   ;WORK+#3C REG. 5=TONHÖHE KANAL C GROB
X0035:  EQU X0034+2 ;BYTE   ;WORK+#3D REG. 6=TONHÖHE RAUSCHEN
X0036:  EQU X0035+1 ;BYTE   ;WORK+#3E REG. 7=MISCHER             
X0037:  EQU X0036+1 ;BYTE   ;WORK+#3F REG. 8=LAUTSTÄRKE KANAL A  
X0038:  EQU X0037+1 ;BYTE   ;WORK+#40 REG. 9=LAUTSTÄRKE KANAL B  
X0039:  EQU X0038+1 ;BYTE   ;WORK+#41 REG.10=LAUTSTÄRKE KANAL C  
X0040:  EQU X0039+1 ;BYTE   ;WORK+#42 REG.11=DAUER HÜLLKURVE FEIN
                    ;BYTE   ;WORK+#43 REG.12=DAUER HÜLLKURVE GROB    
X0041:  EQU X0040+2 ;BYTE   ;WORK+#44 REG.13=FORM HÜLLKURVE   
X0042:  EQU X0041+1 ;WORD   ;WORK+#45 EFFEKT1
X0043:  EQU X0042+2 ;BYTE   ;WORK+#47 EFFEKT2
;
;LETZTE ARBEITSZELLE: WORK+#47 = 00C7 BEI BEGINN AUF 0080
;-------------------------------------------------
;PROGRAMMANFANG PROG 
;-------------------------------------------------
        JP      X0048           ;CALL PROG   = SONG LADEN+ABSPIELEN
        JP      X0045           ;CALL PROG+3 = GERÄUSCH LADEN+ABSPIELEN
        JP      X0046           ;CALL PROG+6 = SOUND OFF   
        DEFS    3,0             ;RESERVE
        DEFS    3,0             ;RESERVE       
;REGISTERWERTE FUER "ALLES AUS"-------------------
X0044:  DEFB    #00,#00,#00,#00,#00,#00,#00
        DEFB    #FF,#00,#00,#00,#00,#00,#01 
;GERÄUSCH LADEN+ABSPIELEN-------------------------
X0045:  LD      HL,(X0004)      ;TABELLE REGISTERWERTE
        JR      X0047
        
;#############################################################        
;Variante 1: UP "SOUND STOPPEN"-------------------------------
;; X0046:  ;CTC-INTERRUPT K1 AUS, GGF ANPASSEN!
;;        LD      A,#43           ;STEUERWORT (INT AUS)
;;        OUT     (X0002+1),A     ;CTC K1   
;;        

;Variante 2: UP "SOUND STOPPEN"-------------------------------
X0046:  ;CTC-INTERRUPT K0 AUS, GGF ANPASSEN!
        LD      A,#43           ;STEUERWORT (INT AUS)
        OUT     (X0002),A       ;CTC K0   
        LD      HL,X0044        ;REGISTERWERTE FUER ANHALTEN
;##############################################################

X0047:  LD      DE,X0032        ;
        LD      BC,14           ;14 REGISTERWERTE
        LDIR                    ;UMLADEN
        JP      X0084           ;IN AY3 SCHREIBEN
;-------------------------------------------------
;SONGDATEN LADEN, CTC PROGRAMMIEREN, STARTEN
X0048:  DI
        LD      A,#0A
        LD      (X0042),A       ;STARTWERT EFFEKT1
        XOR     A
        LD      (X0043),A       ;STARTWERT EFFEKT2 
        DEC     A               ;FF
        LD      (X0016),A       ;1. ENDE-WERT SETZEN!   
        ;KOPFDATEN DES LIEDES AUSLESEN------------
        LD      HL,(X0004)      ;LIEDANFANG
        LD      A,(HL)          ;1. ZEICHEN = "GLOBAL DELAY"
        LD      (X0010),A       ;ABLEGEN
        INC     HL              ;POSITIONSZEIGER
        CALL    X0055           ;HOLEN =>DE
        LD      A,(DE)          ;ANZAHL PATTERN IM SONG
        INC     DE              ;DE=ADR. 1. PATTERNNNUMMER
        INC     A               ;+1
        LD      (X0012),A       ;ANZAHL PATTERN IM SONG + 1 ABLEGEN
        LD      (X0006),DE      ;ADRESSE 1. PATTERNNUMMER
        
        CALL    X0055           ;ORNAMENTZEIGER HOLEN
        LD      (X0007),DE      ;ABLEGEN
        PUSH    DE              ;MERKEN

        CALL    X0055           ;PATTERNZEIGER HOLEN
        LD      (X0008),DE      ;ABLEGEN
        
        LD      HL,#001B        ;OFFSET 1. SAMPLENUMMER
        CALL    X0056           ;+LIEDANFANG
        EX      DE,HL           ;HL=ABSOLUTADRESSE RAM
        LD      (X0009),HL      ;ABLEGEN
        
        LD      HL,X0016        ;DORT FF,00
        LD      (X0013),HL
        
        LD      HL,X0017        ;BEREICH
        LD      DE,X0017+1
        LD      BC,#002C
        LD      (HL),B          ;LÖSCHEN
        LDIR
        POP     HL              ;AB HL=ORNAMENTZEIGER, 
                                ;JEDES ORNAMENT IST 33 BYTES LANG
        LD      BC,#0021        ;IN 33ER SCHRITTEN
        XOR     A               ;00
        CALL    X0054           ;SUCHEN
        DEC     A
        LD      (X0021),A       ;#FF = ENDE ORNAMENT SETZEN
        LD      (X0025),A       ;#FF
        LD      (X0029),A       ;#FF
        LD      A,#01
        LD      (X0011),A       ;STARTWERT GLOBAL DELAY WORK
        INC     HL
        LD      (X0020),HL
        LD      (X0024),HL
        LD      (X0028),HL
        CALL    X0084            ;REGISTER BESCHREIBEN
        
 ;;       ;#########################################
 ;; Variante 1:     50-HZ-INTERRUPT PER CTC ERZEUGEN:
 ;;       ;CTC ALS ZÄHLER PROGRAMMIEREN, 500 KHZ EINGANG
 ;;       ;MIN. K0+K1 KASKADIERT 500kHz/80/125 = 50Hz
 ;;       ;BEI BEDARF ANPASSEN!
 ;;       LD      HL,X0049        ;ADR. PLAYERROUTINE
 ;;       LD      (X0003+2),HL    ;ALS INTERRUPT-ADRESSE  K1
 ;;       LD      A,LO X0003      ;IV (01)C8
 ;;       OUT     (X0002),A       ;K0
 ;;       LD      A,#47           ;STEUERWORT, KEIN INT
 ;;       OUT     (X0002),A       ;K0   
 ;;       LD      A,#50           ;ZK=80
 ;;       OUT     (X0002),A      
 ;;       LD      A,#C7           ;STEUERWORT, INT!
 ;;       OUT     (X0002+1),A     ;K1   
 ;;       LD      A,#7D           ;ZK=125
 ;;       OUT     (X0002+1),A  
 ;;       ;#########################################
 ;;       EI                      ;SOUND ON!
 ;;       RET

        ;#########################################
; Variante 2:     50-HZ-INTERRUPT PER CTC ERZEUGEN:
        ;CTC ALS ZEITGEBER PROGRAMMIEREN, 2,5476 MHZ SYSTEMTAKT
        ;NUR K0 BENUTZT, 2,5476MHz/256/192 = 50Hz
        ;BEI BEDARF ANPASSEN!
        DI
        LD      HL,X0049        ;ADR. PLAYERROUTINE
        LD      (X0003),HL      ;ALS INTERRUPT-ADRESSE  K0
        LD      A,LO X0003      ;IV (HI=F7) LO=D8
        OUT     (X0002),A       ;K0
        LD      A,#A7           ;STEUERWORT, MIT INT , VT 256
        OUT     (X0002),A       ;K0   
        LD      A,#C0           ;ZK=192
        OUT     (X0002),A      
        ;#########################################
        EI                      ;SOUND ON!
        RET
;-------------------------------------------------
;ROUTINE WIRD ALLER 1/50 SEKUNDEN AUFGERUFEN UND 
;VERANLASST 1 PROGRAMMIERUNG DES AY-3-8910 
X0049:  PUSH    IY  
        PUSH    HL
        PUSH    DE
        PUSH    BC
        PUSH    AF
        CALL    X0051           ;1XAY PROGRAMMIEREN
        LD      A,(X0005)       ;WIEDERHOLUNG SONG?
        OR      A               ;FERTIG?
        JR      NZ,X0050        ;NEIN
        CALL    X0046           ;JA: CTC-INTERRUPT AUS!
X0050:  POP     AF
        POP     BC
        POP     DE
        POP     HL  
        POP     IY
        EI
        RETI
;-----------------------------------------------------
X0051:  LD      A,(X0011)       ;GLOBAL DELAY WORK (IN INTERRUPTS)
        DEC     A               ;VERRINGERN
        LD      (X0011),A       ;ABLEGEN
        JP      NZ,X0078        ;ARBEITSBEREICH FÜLLEN

        LD      A,(X0010)       ;GLOBAL DELAY
        LD      (X0011),A       ;WORK NEU LADEN

        LD      IY,X0018        ;INDEX KANAL A
        CALL    X0062           ;TEST, OB ETWAS AUSZUGEBEN
        JP      P,X0052         ;NEIN!
            
        ;KANAL A VERARBEITEN----------------------
        LD      HL,(X0013)      ;ADRESSE PATTERN KANAL A
        LD      A,(HL)
        INC     A               ;WAR FF? (ENDE PATTERN)
        CALL    Z,X0060         ;JA: NÄCHSTE PATTERN-NR./-DATEN HOLEN 
        LD      HL,(X0013)      ;ADRESSE PATTERN KANAL A
        CALL    X0063           ;AUSWERTUNG PATTERNBYTE
        LD      (X0013),HL
        ;KANAL B VERARBEITEN----------------------
X0052:  LD      IY,X0022        ;INDEX KANAL B
        CALL    X0062           ;TEST, OB ETWAS AUSZUGEBEN
        JP      P,X0053         ;NEIN
        LD      HL,(X0014)      ;ADRESSE PATTERN KANAL B
        CALL    X0063           ;AUSWERTUNG PATTERNBYTE
        LD      (X0014),HL
        ;KANAL C VERARBEITEN----------------------
X0053:  LD      IY,X0026        ;INDEX KANAL C
        CALL    X0062           ;TEST, OB ETWAS AUSZUGEBEN
        JP      P,X0078         ;ARBEITSBEREICH FÜLLEN
        LD      HL,(X0015)      ;ADRESSE PATTERN KANAL C
        CALL    X0063           ;AUSWERTUNG PATTERNBYTE
        LD      (X0015),HL
        JP      X0078           ;ARBEITSBEREICH FÜLLEN
;-------------------------------------------------
X0054:  CP      (HL)            ;SUCHEN
        RET     Z
        ADD     HL,BC
        JR      X0054
;-------------------------------------------------
X0055:  LD      E,(HL)
        INC     HL
        LD      D,(HL)          ;POINTER DEREFERENZIEREN
        INC     HL
        EX      DE,HL           ;HL=POINTER
X0056:  LD      BC,(X0004)      ;
        ADD     HL,BC           ;=> ABS. POSI DES BEREICHS IM RAM
        EX      DE,HL           ;DE=
        RET
;SAMPLE-ANALYSE-----------------------------------
X0057:  LD      D,0
        LD      E,A
        ADD     A,A
        ADD     A,E
        LD      E,A
        ADD     IY,DE
        LD      A,(IY+1)        ;NOTENNUMMER HOLEN
        BIT     7,A
        LD      C,#10
        JR      NZ,X0058
        LD      C,D
X0058:  BIT     6,A
        LD      B,#02
        JR      NZ,X0059
        LD      B,D
X0059:  AND     #1F
        LD      H,A
        LD      E,(IY+2)
        LD      A,(IY+0)
        PUSH    AF
        AND     #F0
        RRCA
        RRCA
        RRCA
        RRCA
        LD      D,A
        POP     AF
        AND     #0F
        LD      L,A
        BIT     5,(IY+1)        ;NOTENNUMMER >20H
        RET     Z               ;JA
        SET     4,D
        RET
;NÄCHSTES PATTERN AUFBEREITEN---------------------
X0060:  LD      A,(X0030)       ;BISHER GESPIELTE PATTERNANZAHL
        LD      C,A             ;MERKEN
        LD      HL,X0012        ;ADRESSE PATTERNANZAHL IM SONG
        CP      (HL)            ;LETZTES PATTERN ERREICHT?
        JR      C,X0061         ;NEIN
        ;HIER IST SONG ZUENDE, LETZTES PATTERN WURDE GESPIELT!
        ;WENN SONG NICHT WIEDERHOLT WERDEN SOLL:
        ;X0005 AUSWERTEN!
        LD      A,(X0005)       ;WIEDERHOLUNGEN
        DEC     A
        LD      (X0005),A
        ;-----------------------------------------
        XOR     A               ;JA
        LD      C,A             ;
X0061:  INC     A               ;NÄCHSTE PATTERN-NR. 
        LD      (X0030),A       ;ABLEGEN 
        LD      L,C
        LD      H,0             ;HL=LFD. PATTERN 
        ADD     HL,HL           ;*2
        LD      DE,(X0006)      ;+ANFANG PATTERN-LISTE   
        ADD     HL,DE           ;=ADRESSE PATTERNNUMMER 
        LD      C,(HL)          ;NR. HOLEN
        INC     HL              ;
        LD      A,(HL)          ;EFFEKT
        LD      (X0043),A       ;ABLEGEN 
        LD      A,C             ;ZU SPIELENDE PATTERN-NR.
        LD      (X0031),A       ;NUMMER ABLEGEN
        
        LD      HL,(X0008)      ;IN PATTERNTABELLE
        LD      BC,#0007        ;IN 7ER SCHRITTEN
        CALL    X0054           ;PATTERN-NUMMER SUCHEN
        
        INC     HL              ;OFFSET PATTERNDATEN A
        CALL    X0055           ;DEREFERENZIEREN UND ECHTADRESSE HOLEN
        LD      (X0013),DE      ;PATTERN KANAL A
        CALL    X0055 
        LD      (X0014),DE      ;PATTERN KANAL B
        CALL    X0055
        LD      (X0015),DE      ;PATTERN KANAL C
        RET
;-------------------------------------------------
X0062:  DEC     (IY+2)          ;ENDEMARKE FF ERREICHT?
        RET     P               ;JA
        LD      A,(IY-1)        ;
        LD      (IY+2),A        ;NEUER ENDEWERT
        RET
;PRÜFT PATTERNWERT UND VERANLASST AKTIVITÄT-------
X0063:  LD      A,(HL)
        CP      #60             ;60...6F?
        JR      C,X0064         ;NEIN, KLEINER => NOTENNUMMER 00...5F
        CP      #70             ;70...7F?
        JR      C,X0066         ;NEIN, KLEINER => SAMPLENUMMER 60...6F
        CP      #80             ;KOMMANDO
        JR      C,X0070         ;NEIN, KLEINER => ORNAMENTNUMMER 70...7F 
        JR      Z,X0067         ;80= PAUSE
        CP      #81             ;LEERE NOTE?
        JR      Z,X0065         ;JA, WEITER ZUR NÄCHSTEN POSI
        CP      #82             ;82= SPERREN?
        JR      Z,X0069         ;JA, SPERREN HÜLLKURVE UND ORNAMENT
        CP      #8F             ;
        JR      C,X0072         ;KLEINER => HÜLLKURVENWERT 83...8E
        SUB     #A1             ;A1...FE => PAUSE ZWISCHEN NOTEN
        LD      (IY+2),A        ;0...5D
        LD      (IY-1),A
        INC     HL              ;NÄCHSTEN WERT
        JR      X0063           ;TESTEN
;NOTENNUMMER--------------------------------------
X0064:  LD      (IY+1),A        ;NOTENNUMMER ABLEGEN
        LD      (IY+0),0
        LD      (IY+7),#20      ;KEIN ENDE
X0065:  INC     HL              ;LEERE NOTE
        RET
;SAMPLENUMMER-------------------------------------
X0066:  SUB     #60
        PUSH    HL          
        LD      BC,#0063        ;IN 63H-SCHRITTEN
        LD      HL,(X0009)      ;AB SAMPLENUMMER
        CALL    X0054           ;SUCHEN
        INC     HL
        LD      (IY+3),L        ;SAMPLEADRESSE ABLEGEN
        LD      (IY+4),H
        POP     HL
        INC     HL
        JR      X0063           ;PATTERNTEST
;-------------------------------------------------
X0067:  INC     HL              ;NÄCHSTE POSI
X0068:  LD      (IY+7),#FF      ;ENDE MARKIEREN
        RET
;-------------------------------------------------
X0069:  XOR     A               ;HÜLLKURVE+ORNAMENT AUS
        JR      X0071
;-------------------------------------------------
X0070:  SUB     #70             ;A= 0...F = ORNAMENT
X0071:  PUSH    HL
        LD      BC,#0021        ;IN 21H-SCHRITTEN
        LD      HL,(X0007 )     ;AB ORNAMENTS POINTER
        CALL    X0054           ;SUCHEN
        INC     HL
        LD      (IY+5),L        ;ORNAMENTADRESSE ABLEGEN
        LD      (IY+6),H
        LD      (IY-2),0
        POP     HL
        INC     HL
        JR      X0063          ;PATTERNTEST
;HÜLLKURVE----------------------------------------
X0072:  SUB     #80             ;A=0...F
        LD      (X0041),A       ;FORM HÜLLKURVE
        INC     HL
        LD      A,(HL)
        INC     HL
        LD      (X0040),A       ;DAUER HÜLLKURVE FEIN
        LD      (IY-2),1
        PUSH    HL
        XOR     A               ;00
        LD      BC,#0021        ;IM 33ER ABSTAND
        LD      HL,(X0007)      ;AB ORNAMENT-POINTER
        CALL    X0054           ;SUCHEN
        INC     HL
        LD      (IY+5),L        ;ORNAMENTADRESSE ABLEGEN
        LD      (IY+6),H
        POP     HL
        JP      X0063           ;PATTERNTEST
;TEST AUF WIEDERHOLUNGEN--------------------------
X0073:  LD      A,(IY+7)        ;ENDE (FF)?
        INC     A
        RET     Z               ;JA
        DEC     A
        DEC     A
        LD      (IY+7),A
        PUSH    AF
        LD      A,(IY+0)
        LD      C,A
        INC     A
        AND     #1F
        LD      (IY+0),A
        POP     AF
        RET     NZ
        LD      E,(IY+3)        ;SAMPLEADRESSE HOLEN
        LD      D,(IY+4)
        LD      HL,#0060
        ADD     HL,DE
        LD      A,(HL)
        DEC     A
        JP      M,X0068
        LD      C,A
        INC     A
        AND     #1F
        LD      (IY+0),A
        INC     HL
        LD      A,(HL)
        INC     A
        LD      (IY+7),A
        RET
;-------------------------------------------------
X0074:  LD      A,C
        OR      A
        RET     NZ
        LD      A,H
        LD      (X0035),A
        RET
;-------------------------------------------------
X0075:  LD      A,(IY+7)        ;ENDE (FF) ?
        INC     A
        RET     Z               ;JA
        LD      A,(IY-2)
        OR      A
        RET     Z
        CP      #02
        JR      Z,X0076
        LD      (IY-2),#02
        JR      X0077
X0076:  XOR     A               ;HÜLLKURVE
        LD      (X0041),A       ;FORM 0
X0077:  SET     4,(HL)
        RET
;ARBEITSBEREICH AUFBAUEN--------------------------
X0078:  LD      IY,X0018        ;INDEX
        CALL    X0073           ;TEST AUF WIEDERHOLUNGEN
        LD      A,C
        LD      (X0042),A       ;EFFEKT ABLEGEN
        LD      IY,(X0019)
        CALL    X0057           ;SAMPLE-ANALYSE
        LD      A,C
        OR      B
        RRCA
        LD      (X0036),A       ;MISCHER
        LD      IY,X0018
        LD      A,(IY+7)        ;ENDE (FF)?
        INC     A
        JR      Z,X0079         ;JA!
        CALL    X0074
        CALL    X0082
        LD      (X0032),HL      ;TONHÖHE A SETZEN
X0079:  LD      HL,X0037        ;LAUTSTÄRKE A
        LD      (HL),A          ;SETZEN
        CALL    X0075
        ;KANAL B----------------------------------
        LD      IY,X0022        ;INDEX
        CALL    X0073           ;TEST AUF WIEDERHOLUNGEN
        LD      A,(IY+7)        ;ENDE (FF)?
        INC     A
        JR      Z,X0080         ;JA!
        LD      A,C
        LD      (X0042),A       ;EFFEKT ABLEGEN
        LD      IY,(X0023)
        CALL    X0057           ;SAMPLE-ANALYSE
        LD      A,(X0036)       ;MISCHER
        OR      C
        OR      B
        LD      (X0036),A       ;MISCHER
        CALL    X0074
        LD      IY,X0022        ;INDEX
        CALL    X0082
        LD      (X0033),HL      ;TONHÖHE B SETZEN
X0080:  LD      HL,X0038        ;LAUTSTÄRKE B
        LD      (HL),A          ;SETZEN
        CALL    X0075
        ;KANAL C----------------------------------
        LD      IY,X0026
        CALL    X0073           ;TEST AUF WIEDERHOLUNGEN
        LD      A,(IY+7)        ;ENDE (FF)?
        INC     A
        JR      Z,X0081         ;JA
        LD      A,C
        LD      (X0042),A       ;EFFEKT ABLEGEN
        LD      IY,(X0027)
        CALL    X0057           ;SAMPLE-ANALYSE
        LD      A,(X0036)       ;MISCHER
        RLC     C
        RLC     B
        OR      B
        OR      C
        LD      (X0036),A       ;MISCHER
        CALL    X0074
        LD      IY,X0026
        CALL    X0082           ;
        LD      (X0034),HL      ;TONHÖHE C SETZEN
X0081:  LD      HL,X0039        ;LAUTSTÄRKE C
        LD      (HL),A          ;SETZEN
        CALL    X0075
        JR      X0084           ;REGISTER SCHREIBEN
;EFFEKT-------------------------------------------
X0082:  LD      A,L
        PUSH    AF
        PUSH    DE
        LD      L,(IY+5)        ;ORNAMENTADRESSE HOLEN
        LD      H,(IY+6)
        LD      DE,(X0042)      ;EFFEKT1 (ANZAHL MOD.)
        ADD     HL,DE           ;
        LD      A,(IY+1)        ;NOTENNUMMER HOLEN
        ADD     A,(HL)
        PUSH    BC
        LD      B,A
        LD      A,(X0043)
        ADD     A,B            
        POP     BC
        ADD     A,A             ;*2
        LD      E,A
        LD      D,0             ;DE=
        LD      HL,X0087        ;LISTE NOTENFREQUENZEN
        ADD     HL,DE           ;POSITION AKT. NOTENFREQUENZ 
        LD      E,(HL)
        INC     HL
        LD      D,(HL)
        EX      DE,HL           ;HL=NOTENFREQUENZ SELBST
        POP     DE
        POP     AF
        BIT     4,D             
        JR      Z,X0083
        RES     4,D
        ADD     HL,DE
        RET
X0083:  AND     A
        SBC     HL,DE
        RET
;-------------------------------------------------
;AY3-REGISTER PROGRAMMIEREN, JEWEILS 13 ODER 10 BYTES
;ENTHÄLT DER ZIELWERT FÜR REGISTER 13 (HÜLLKURVENFORM)
;EINE 00, SO WIRD DIE BISHERIGE HÜLLKURVE (DAUER+FORM)
;BEIBEHALTEN,D.H. ES WERDEN NUR 10 REGISTER BESCHRIEBEN
;
X0084: ;MISCHER-REGISTER: PORTZUSTAENDE AUSLESEN+SICHERN:
        LD      A,7             ;MISCHER
        OUT     (X0001),A       ;ADRESSIEREN
        IN      A,(X0001)       ;UND AUSLESEN
        AND     A,#C0           ;NUR PORTBITS
        LD      C,A             ;MERKEN
        LD      A,(X0036)       ;MISCHER-INHALT AUS PUFFER
        AND     A,#3F           ;NUR RAUSCHEN+TON-BITS
        OR      C               ;+ALTE PORTSBITS
        LD      (X0036),A       ;ALS NEUER MISCHERPUFFER
; 
        LD      HL,X0041        ;BEREICHSANFANG (RÜCKWÄRTS)
        XOR     A
        OR      (HL)            ;ANFANG=0?
        LD      A,#0D           ;AB REGISTER 13
        JR      NZ,X0085        ;NEIN 
        SUB     03H             ;JA=> AB REGISTER 10 (OHNE HÜLLKURVE)
        DEC     HL
        DEC     HL
        DEC     HL
X0085: ;KOMMT MIT A=REGISTERNUMMER 13 ODER 10
        LD      C,X0001+1       ;"REGISTERINHALT SCHREIBEN"
X0086:  OUT     (X0001),A       ;REGISTER AUSWÄHLEN
        OUTD                    ;BYTE AUF (HL) IN AKT. REG. SCHREIBEN
        DEC     A               ;NÄCHSTES REGISTER (RÜCKWÄRTS)
        JP      P,X0086         ;BIS REGISTER 0
        RET
;LISTE NOTENFREQUENZEN----------------------------
X0087:  DEFW    #0EF8,#0E10,#0D60,#0C80
        DEFW    #0BD8,#0B28,#0A88,#09F0
        DEFW    #0960,#08E0,#0858,#07E0
        DEFW    #077C,#0708,#06B0,#0640
        DEFW    #05EC,#0594,#0544,#04F8
        DEFW    #04B0,#0470,#042C,#03F0
        DEFW    #03BE,#0384,#0358,#0320
        DEFW    #02F6,#02CA,#02A2,#027C
        DEFW    #0258,#0238,#0216,#01F8
        DEFW    #01DF,#01C2,#01AC,#0190
        DEFW    #017B,#0165,#0151,#013E
        DEFW    #012C,#011C,#010B,#00FC
        DEFW    #00EF,#00E1,#00D6,#00C8
        DEFW    #00BD,#00B2,#00A8,#009F
        DEFW    #0096,#008E,#0085,#007E
        DEFW    #0077,#0070,#006B,#0064
        DEFW    #005E,#0059,#0054,#004F
        DEFW    #004B,#0047,#0042,#003F
        DEFW    #003B,#0038,#0035,#0032
        DEFW    #002F,#002C,#002A,#0027
        DEFW    #0025,#0023,#0021,#001F
        DEFW    #001D,#001C,#001A,#0019
        DEFW    #0017,#0016,#0015,#0013
        DEFW    #0012,#0011,#0010,#000F
;-------------------------------------------------
DEFM    "ENDE ENGINE"
