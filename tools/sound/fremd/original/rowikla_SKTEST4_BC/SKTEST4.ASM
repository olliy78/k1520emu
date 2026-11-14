;TESTPROGRAMM SOUND-K1520 mit AY-3-8910/12 und CTC
;Version für A5120/K8924 u.ä.
;-------------------------------------------------
;WERO 09/2017
;ERZEUGT CA. 1KHZ RECHTECK BEI 1,75 / 1,79 /2 MHZ AY-TAKT
;FÜR EXAKTE ODER ANDERE TONHÖHE GGF. DEN REGISTERWERT 
;"7DH" BEI HÖHE A, /B, /C VARIIEREN!
;-------------------------------------------------
;Version 1.2.0.4 für CP/A, SCPX
;KW 19.11.2017
;
;Anpassung an K1520-Soundkarte mit CTC unter NANOS EPOS 
;- Auswahl AY-Taktfrequenz
;- Test Tonausgabe, Geräusche  
;- Test parallele AY-Port(s)  Eingabe und Ausgabe
;- Test CTC  (alle Kanäle kaskadiert)
;- Test AY-SOUND über CTC-Interrupt
;   Anpassung der AYPLAY.INC an den jeweiligen Rechner notwendig!!!!
;   speziell die Adresse Interruptsäule, da I-Register bei manchen Rechner
;   nicht verändert werden darf !!!
;-------------------------------------------------
;kleine Routinen (CP/M Aufrufe) vom Heiko Poppe verwendet
;-------------------------------------------------
;ANPASSEN AN JEWEILIGES RECHNERSYSTEM:
;
AYBASE:  EQU     038H            ;BASISADRESSE AY-3
CTCBASE: EQU     AYBASE +4       ;BASISADRESSE CTC Kanal 0

START:   EQU     100h            ;PROGRAMMSTART
WORK:    EQU     START+10h       ;BEGINN ARBEITSZELLEN (50h)
PLAYER:  EQU     WORK+50h        ;BEGINN PLAYER-ENGINE (480h)
PROGGI:  EQU     PLAYER+480h     ;PROGRAMMBEGINN
RAUS:    EQU     0               ;RÜCKSPRUMG ZUM OS
WDH:     EQU     WORK+2          ;WIEDERHOLUNGEN

ANZ_WDH: EQU     2               ;ANZAHL WIEDERHOLUNGEN

;(ANGEGEBENE WERTE GÜLTIG FÜR CP/M)
bdos:    equ     5               ;BDOS Aufruf
esc:     equ     1bh
conout:  equ     02h             ;ein Zeichen ausgeben (E)
zkout:   equ     09h             ;Ausgabe Zeichenkette (DE), Ende mit $
;-------------------------------------------------
         ORG     START           ;Anfang CP/M
         JP      start0
         ;0103h...0107h          ;FREI
         ;0108h...010Fh          ;INTERRUPTTABELLE CTC
         ;0108h IN INCLUDE EINTRAGEN!       
;-------------------------------------------------
ORG      WORK                    ;ARBEITSZELLEN, BEZÜGE:
         ;WORK                   ;WORD, ÜBERGABE ANFANGSADESSE SONG
         ;WORK+2                 ;BYTE, ÜBERGABE ANZAHL ABSPIELE
                                ;(1...255, 0 BEI SONGENDE)
;-------------------------------------------------
ORG      PLAYER                  ;BEZÜGE:
         ;PLAYER+0               ;SONG LADEN+ABSPIELEN
         ;PLAYER+3               ;GERÄUSCH ABSPIELEN
         ;PLAYER+6               ;SOUND AUS
;-------------------------------------------------
INCLUDE  AYPBC.INC               ;AY-PLAYER-ENGINE
;
start0:  ld      de,TEXTU
         call    disp_str        ;Zeichenkette ausgeben
         ld      de,TEXT0 
         call    disp_str
         call    kbwait          ;Taste - Zeichen in A
;
;AY-Takt wählen
start1:  ld      de,TEXTU
         call    disp_str  
         ld      de,TEXT1
         call    disp_str        ;Zeichenkette ausgeben
         call    kbwait          ;Taste - Zeichen in A
         ld      b,06Dh          ;1,75MHz AY-Takt 
         cp      "A"             
         jr      z,taset          
         cp      "a"
         jr      z,taset
         ld      b,06Fh          ;1,79MHz AY-Takt
         cp      "B"
         jr      z,taset
         cp      "b"
         jr      z,taset
         ld      b,07Dh          ;2,00MHz AY-Takt
         cp      "C"
         jr      z,taset
         cp      "c"
         jr      z,taset
         jp      nz,start1
taset:   ld      a,b             ;Wert in Registertabelle schreiben
         ld      (TONA),a
         ld      (TONB+2),a
         ld      (TONC+4),a
         ld      (HUBI),a
start2:  ld      de,TEXTU
         call    disp_str 
         ld      de,TEXT2
         call    disp_str        ;Zeichenkette ausgeben
         call    kbwait          ;Taste - Zeichen in A
         cp      "1"             ;Taste "1"?
         call    z,ayt           ;ja => 
         cp      "2"
         call    z,ayppt
         cp      "3"
         call    z,ctct
         cp      "4"
         call    z,ayint
         cp      "0"
         jp      z,raus          ;zurück zum OS
         jp      nz,start2
;        
;Tonausgabemenü
ayt:     ld      de,TEXT3
         call    disp_str        ;Zeichenkette ausgeben
         call    kbwait          ;Taste - Zeichen in A
         LD      HL,TONA         ;ADRESSE REGISTERWERTE
         LD      (WORK),HL
         CP      "A"             ;TASTE "A"?
         CALL    Z,PLAYER+3      ;JA => TON STARTEN
         CP      "a"
         CALL    Z,PLAYER+3
         LD      HL,TONB  
         LD      (WORK),HL
         CP      "B"
         CALL    Z,PLAYER+3
         CP      "b"
         CALL    Z,PLAYER+3 
         LD      HL,TONC
         LD      (WORK),HL  
         CP      "C"
         CALL    Z,PLAYER+3
         CP      "c"
         CALL    Z,PLAYER+3
         LD      HL,HUBI
         LD      (WORK),HL
         CP      "H"
         CALL    Z,PLAYER+3
         CP      "h"
         CALL    Z,PLAYER+3 
         LD      HL,MEER
         LD      (WORK),HL
         CP      "M"
         CALL    Z,PLAYER+3
         CP      "m"
         CALL    Z,PLAYER+3
         LD      HL,SCHUSS
         LD      (WORK),HL
         CP      "S"
         CALL    Z,PLAYER+3
         CP      "s"
         CALL    Z,PLAYER+3 
         LD      HL,AUS
         LD      (WORK),HL  
         CP      "0"
         JR      z,ayt1 
         JR      NZ,ayt
ayt1:    CALL    PLAYER+3         ;AY AUS!
         xor     a
         ret
;-------------------------------------------------        
;Test Parallel Port(s) vom AY Chip
ayppt:   LD      DE,TEXT4
         call    disp_str        ;Zeichenkette ausgeben
         call    kbwait          ;Taste - Zeichen in A
         cp      "1"             ;Taste "1"?
         call    z,ppaw          ;ja => 
         cp      "2"
         call    z,ppar
         cp      "3"
         call    z,ppbw
         cp      "4"
         call    z,ppbr
         cp      "0"
         jr      z,ayppt1        ;zurück
         jr      nz,ayppt
ayppt1:  xor     a
         ret
;
;IO-PortA 
;Register7 Bit6 =0 > Port Eingabe
;Register7 Bit6 =1 > Port Ausgabe
;IO-PortB
;Register7 Bit7 =0 > Port Eingabe
;Register7 Bit7 =1 > Port Ausgabe
;
;Register 14 (E) und 15 (F) speichert Daten die von CPU geschrieben wurden
;IO Ports haben Pullup Widerstände (60k..600k)
;
ppaw:    ld      c,aybase
         ld      a,07h           ;Register 7
         out     (c),a
         inc     c               ;aybase+1
         ld      a,01000000b     ;bit6=1... PortA=Ausgabe
         out     (c),a 
         dec     c               ;aybase
         ld      a,0Eh           ;Register 14 
         out     (c),a           ;an aybase
         inc     c               ;aybase+1
ppaw1:   ld      a,055h          ;Testmuster 01010101b
         out     (c),a           ;an aybase+1
         call    outx 
         call    warten
         ld      a,0AAh          ;Testmuster 10101010b
         out     (c),a           ;an aybase+1
         call    outx            ;als HEX-Wert ausgeben
         call    warten 
         call    kbds
         cp      "0"             ;bis Taste "0" gedrückt
         jr      nz,ppaw1 
         ld      c,aybase
         ld      a,07h           ;Register 7
         out     (c),a           
         inc     c               ;aybase+1
         ld      a,00000000b     ;bit6=0... PortA=Eingabe
         out     (c),a 
         ld      a,082h          ;Cursor an
         call    print
         ret

ppar:    ld      c,aybase
         ld      a,0Eh           ;Register 14  
         out     (c),a
ppar1:   in      a,(c)           ;lesen
         call    outx            ;als HEX-Wert ausgeben
         call    kbds
         cp      "0"             ;bis Taste "0" gedrückt
         jr      nz,ppar1 
         ld      a,082h          ;Cursor an
         call    print
         ret

ppbw:    ld      c,aybase
         ld      a,07h           ;Register 7
         out     (c),a
         inc     c               ;aybase+1
         ld      a,10000000b     ;bit7=1... PortB=Ausgabe
         out     (c),a 
         dec     c               ;aybase
         ld      a,0Fh           ;Register 15 
         out     (c),a           ;an aybase
         inc     c               ;aybase+1
ppbw1:   ld      a,055h          ;Testmuster 01010101b
         out     (c),a           ;an aybase+1
         call    outx 
         call    warten
         ld      a,0AAh          ;Testmuster 10101010b
         out     (c),a           ;an aybase+1
         call    outx            ;als HEX-Wert ausgeben
         call    warten 
         call    kbds
         cp      "0"             ;bis Taste "0" gedrückt
         jr      nz,ppbw1 
         ld      c,aybase
         ld      a,07h           ;Register 7
         out     (c),a
         inc     c               ;aybase+1
         ld      a,00000000b     ;bit6=0... PortA=Eingabe
         out     (c),a 
         ld      a,082h          ;Cursor an
         call    print
         ret

ppbr:    ld      c,aybase
         ld      a,0Fh           ;Register 15 
         out     (c),a
ppbr1:   in      a,(c)           ;lesen
         call    outx            ;als HEX-Wert ausgeben
         call    kbds
         cp      "0"             ;bis Taste "0" gedrückt
         jr      nz,ppbr1
         ld      a,082h          ;Cursor an
         call    print
         ret     
;-------------------------------------------------   
;Test der CTC   (kaskadiert)
;Kanal 0 als Zeitgeber 
;Kanal 1 2 3 als Zähler
;Kanal 3 Zählerstand wird angezeigt (Rückwärtszähler)
;
ctct:    ld      de,TEXT5
         call    disp_str        ;Zeichenkette ausgeben
         ld      c,ctcbase       ;Kanal 0
        
         ;Kanal 0 als Zeitgeber
         ld      a,00000111b     ;VT16
         out     (c),a
         ld      a,0FFh
         out     (c),a           ;Kanal 0
         ;ca. 600 Hz
        
         ;Kanal 1 als Zähler
         inc     c               ;Kanal 1
         ld      a,01011101b     
         out     (c),a
         ld      a,60
         out     (c),a           ;Kanal 1
         ;ca. 10Hz
        
         ;Kanal 2 als Zähler
         inc     c               ;Kanal 2
         ld      a,01011101b     
         out     (c),a
         ld      a,05
         out     (c),a           ;Kanal 2
         ;ca. 2Hz
         
         inc     c               ;Kanal 3
         ld      a,57h           ;0101 1101 CTC initialisieren
                                 ;Int. sperre, Zähler,
         out     (c),a           ;Kanal 3
         ld      a,0FFh          ;ZK 255 setzen
         out     (c),a           ;von da an rückwärts zählen
ctct1:   in      a,(c)           ;Zählerstand lesen
         cp      0FFh            ;erster Impuls gekommen
         jr      z,ctct1         ;nein -> warten
ctct2:   in      a,(c)           ;Zählerstand lesen
         call    outx            ;als HEX Wert ausgeben
         push    af
         call    kbds            ;bis Taste "0" gedrückt
         cp      "0"
         jr      z,ctct3 
         pop     af
         cp      0FFh
         jr      nz,ctct2
ctct3:   pop     af
         ld      a,082h          ;Cursor an
         call    print
         ret
;
;-------------------------------------------------
;Test AY-SOUND über Interruptroutine
;Anpassung der AYPLAY.INC an den jeweiligen Rechner notwendig!!!!
;speziell die Adresse Interruptsäule, da I-Register bei manchen Rechner
;nicht verändert werden darf
;
ayint:   ld     de,TEXT6
         call   disp_str        ;Zeichenkette ausgeben
        ;SONG LADEN, SPIELEN, STOPPEN:
         LD     HL,SONG         ;ANFANGSADRESSE SONG
         LD     (WORK),HL       ;ÜBERGEBEN
         LD     A,2             ;ANZAHL ABSPIELE (Z.B. 2x)
         LD     (WORK+2),A      ;ÜBERGEBEN
         CALL   PLAYER          ;SONG ABSPIELEN
         ;...SONG LÄUFT SELSTBSTÄNDIG WEITER!
ayint1:  call   kbds
         cp     "0"             ;bis Taste "0" gedrückt
         jr     nz,ayint1 
         CALL   PLAYER+6        ;SONG VORZEITIG STOPPEN 
         RET                    ;RÜCKKEHR ZUM OS
;
;--------------------------------------------------
;UNTERPROGRAMME:  (nicht alle verwendet)
;--------------------------------------------------
;
;Tastaturabfrage (wartet)
; OUTPUT: Zeichen in A und auf BS
kbwait:   push  bc
          push  de
          push  hl
kbwait1:  ld    c,6            ;zeichenübernahme in a
          ld    e,0FFh         ;direkte konsolen e/a
          call  bdos           ;bdos
          cp    0
          jr    z,kbwait1
          pop   hl
          pop   de
          pop   bc
          ret
;
;Tastaturabfrage
; OUTPUT: Zeichen in A und auf BS
kbd:      push  hl
          push  de
          push  bc
          ld    c,1             ;Konsoleneingabe
          call  bdos
          pop   bc
          pop   de
          pop   hl
          ret
;
;Tastaturabfrage ohne Cursor
; OUTPUT: Zeichen in A und auf BS
kbds:     push  hl
          push  de
          push  bc
          ld    c,6             ;Konsoleneingabe
          ld    e,0FFh
          call  bdos
          pop   bc
          pop   de
          pop   hl
          ret
;
;UP Zeichen über conout ausgeben
;Zeichen in E
cconout:  push   af
          ld     c,2             ;conout
          call   bdos
          pop    af
          ret
;
;UP Zeichenkette ausgeben
;Anfangsadresse in DE
;Ende mit "$" = 024h
disp_str: push  hl
          push  de
          push  bc
          push  af
          ld    c,9             ;zeichenkette ausgeben
          call  BDOS            ;bdos
          pop   af
          pop   bc
          pop   de
          pop   hl
          ret
;
;Cursor positionieren
; IN: HL = Cursorposition (H = Zeile, L = Spalte)
cset:     push   af
          push   bc
          push   hl
          ld     a,h
          or     80h
          ld     (zeipos),a
          ld     a,l
          or     80h
          ld     (spapos),a
          ld     de,punkt1
          ld     c,zkout
          call   bdos
          pop    hl
          pop    bc
          pop    af
          ret
;
;ASCII-Zeichen aus Eingabe in Hexwert
; DE = Beginn Zeichenkette
; HL = Ergebnis Hex-Wert
inhex:    ld     h,0
          call   ashex
          rla
          rla
          rla
          rla
          and    0F0h
          ld     h,a
          call   ashex
          or     h
          ld     h,a
inhx2:    ld     l,0
          call   ashex
          rla
          rla
          rla
          rla
          and    0F0h
          ld     l,a
inhx1:    call   ashex
          or     l
          ld     l,a
          ret
;
;ASCII zu HEX
; DE = Zeiger auf Zeichen
ashex:    ld     a,(de)
          inc    de
          sub    30h
          cp     0Ah
          ret    c
          sub    7
          ret
;
;Akku dezimal anzeigen
; A = Zahl
ADEZ:     PUSH  HL
          LD    L,A
          LD    H,0
          CALL  HLDEZ
          POP   HL
          RET
;
;HL dezimal anzeigen, fuehrende Nullen weglassen
; HL = Zahl
HLDEZ:    PUSH  HL
          PUSH  DE
          LD    DE,0
HLDZ1:    PUSH  BC
          PUSH  AF
          LD    BC,10000        ;Zehntausender
          CALL  DEZ             
          LD    BC,1000         ;Tausender
          CALL  DEZ
          LD    BC,100
          CALL  DEZ             ;Hunderter
          LD    BC,10
          CALL  DEZ             ;Zehner
          LD    A,L
          ADD   A,'0'
          call  ochr            ;Einer
          POP   AF
          POP   BC
          POP   DE
          POP   HL
          RET
;
DEZ:      LD    A,'0'
DEZ1:     AND   A
          SBC   HL,BC
          JR    C,DEZ2          ;keine weiteren Teile
          INC   A
          JR    DEZ1
DEZ2:     ADD   HL,BC
          CP    '0'
          JR    NZ,DEZ3         ;Ziffer ungleich '0'
          INC   D
          DEC   D
          JR    NZ,DEZ3         ;Anzeige laeuft
          INC   E
          DEC   E
          RET   Z               ;fuehrende Nullen weglassen
          LD    A,' '           ;oder durch SPC ersetzen
          DEC   D
DEZ3:     call  ochr
          INC   D
          RET  
;
;Ausgabe eines Byte als HEX an Position
; A = Byte an Position Z=10, SP=18
outx:     push  af
          ld    a,083h          ;Cursor aus
          call  print
          pop   af
          ld    h,10            ;Zeile
          ld    l,18            ;Spalte 
          call  cset
          call  outa
       ;push af
       ;ld   a,082h          ;Cursor an
       ;call print
       ;pop  af         
          ret
;
;Ausgabe Zeichen 
; A = Zeichen                                                           
print:    ld    (gchar),a        ;Zeichen
          push  de
          push  bc
          ld    de,gchar         ;Textzeichen
          ld    c,zkout          ;Zeichenkette ausgeben
          call  BDOS
          pop   bc
          pop   de       
          ret
;
;Ausgabe von A als HEX-Wert
outa:     push  af
          rra
          rra
          rra
          rra
          call  ahexo
          pop   af
ahexo:    and   0Fh
          add   a,90h
          daa
          adc   a,40h
          daa
          call  ochr
          ret
;
;Ausgabe von HL als HEX-Wert
outhl:    push  af
          ld    a,h
          call  outa
          ld    a,l
          call  outa
          pop   af
          ret
;
;Ausgabe eines Zeichens aus A
ochr:     push  hl
          push  de
          push  bc
          push  af
          ld    e,a
          ld    c,conout
          call  bdos
          pop   af
          pop   bc
          pop   de
          pop   hl
          ret

;UP Warteschleife
warten:  push  af
         push  bc
         push  de
         push  hl
         ld    bc,0FFFFh
warten1: dec   bc
         ld    a,b
         or    c
         jr    nz,warten1
         pop   hl
         pop   de
         pop   bc
         pop   af
         ret
;
;--------------------------------------------------
TEXTU:  DEFB    0CH
        DEFM    "* TEST SOUND-K1520 MIT AY-3-8910 UND CTC  *",0DH,0AH
        DEFM    "*  Version 1.2.0.4  -  WERO, KW 10/2017    *",0DH,0AH
        DEFM    "-------------------------------------------",0DH,0AH,0DH,0AH,024H
     
TEXT0:  DEFM    "STANDARD IO-ADRESSE: 38h - JP1.. A6,A7 gesteckt",0DH,0AH,0DH,0AH
        DEFM    "AY-Control  = 38h",0DH,0AH
        DEFM    "AY-Data     = 39h",0DH,0AH
        DEFM    "CTC-Kanal 0 = 3Ch",0DH,0AH
        DEFM    "CTC Kanal 1 = 3Dh",0DH,0AH
        DEFM    "CTC Kanal 2 = 3Eh",0DH,0AH
        DEFM    "CTC Kanal 3 = 3Fh",0DH,0AH,0DH,0AH
        DEFM    "WICHTIG bei 'AY Sound mit CTC Interrupt testen:'",0DH,0AH
        DEFM    "Interruptsaeule liegt bei F7D8h (CPA, SCPX )",0DH,0AH,0DH,0AH
        DEFM    "WEITER mit Taste....",024h

TEXT1:  DEFM    "AUSWAHL DER TAKTFREQUENZ DES AY-CHIP:",0DH,0AH,0DH,0AH
        DEFM    "  A= 1,75MHz",0DH,0AH
        DEFM    "  B= 1,79MHz",0DH,0AH
        DEFM    "  C= 2,00MHz",0DH,0AH,0DH,0AH
        DEFM    "WAHL?",024h
;
TEXT2:  DEFM    "AUSWAHL DES TESTS:",0DH,0AH,0DH,0AH
        DEFM    "  1= Testton / Geraeusch AY-3-8910 ausgeben" ,0DH,0AH
        DEFM    "  2= Parallel-PORT(s) vom AY CHIP testen",0DH,0AH
        DEFM    "  3= CTC (kaskadierte Kanaele) testen",0DH,0AH
        DEFM    "  4= AY Sound mit CTC Interrupt testen ",0Dh,0Ah
        DEFM    "  0= Zurueck zum OS",0DH,0AH,0DH,0AH
        DEFM    "WAHL?",024h
;        
TEXT3:  DEFB    0CH
        DEFM    "* TESTTON AY-3-8910 : RECHTECK CA. 1 KHZ *",0DH,0AH
        DEFM    "LAUTSTAERKE: MAXIMAL",0DH,0AH
        DEFM    "------------------------------------------",0DH,0AH
        DEFM    "  A= RECHTS",0DH,0AH
        DEFM    "  B= BEIDE",0DH,0AH
        DEFM    "  C= LINKS",0DH,0AH,0DH,0AH
        DEFM    "* TEST SOUNDEFFEKTE *",0DH,0AH
        DEFM    "------------------------------------------",0DH,0AH
        DEFM    "  H= HUBSCHRAUBER",0DH,0AH
        DEFM    "  M= MEERESRAUSCHEN",0DH,0AH
        DEFM    "  S= SCHUSS",0DH,0AH,0DH,0AH
        DEFM    "  0= Zurueck zum Testmenue",0DH,0AH,0DH,0AH
        DEFM    "WAHL?",024h
;
TEXT4:  DEFB    0CH
        DEFM    "* TEST DER/DES IO-PORT(S) VOM AY-3-8910/8912 *",0DH,0AH
        DEFM    "-----------------------------------------------------",0DH,0AH,0AH,0DH
        DEFM    "  1= PORT A: 55h/AAh wird abwechselnd auf PORT A ausgegeben",0DH,0AH
        DEFM    "  2= PORT A: Lesen... ",0DH,0AH
        DEFM    "  3= PORT B: 55h/AAh wird abwechselnd auf PORT B ausgegeben (nur AY-3-8910)",0DH,0AH
        DEFM    "  4= PORT B: Lesen...                                       (nur AY-3-8910)",0DH,0AH,0DH,0AH
        DEFM    "  0= Zurueck zum Testmenue",0DH,0AH,0DH,0AH
        DEFM    "   ANZEIGE BYTE:   ",024h
;
TEXT5:  DEFB    0CH
        DEFM    "* TEST VON CTC KANAL 0-3 auf der SOUND-K1520 *",0DH,0AH
        DEFM    "-----------------------------------------------",0DH,0AH,0AH,0DH
        DEFM    "  CTC Kanal 0 arbeitet als Zeitgeber",0DH,0AH
        DEFM    "  Kanal 1 2 3 arbeiten als Zaehler",0DH,0AH
        DEFM    "  Kanal 0->1->2->3 kaskadiert",0DH,0AH
        DEFM    "  Zaehlerstand Kanal 3 wird gelesen (ca. 2Hz)",0DH,0AH,0AH,0DH
        DEFM    "  0= Zurueck zum Testmenue",0DH,0AH,0DH,0AH
        DEFM    " GELESENES BYTE:   ",024h
;
TEXT6:  DEFB    0CH
        DEFM    "* TESTSONG AY-3-8910 MIT CTC INTERRUPT *",0DH,0AH
        DEFM    "----------------------------------------",0DH,0AH,0DH,0AH
        DEFM    "Programmteil funktioniert nur, wenn der Interruptbetrieb",0DH,0AH
        DEFM    "dem Rechner angepasst wurde (A5120/K8924 mit CPA,SCPX)",0DH,0AH,0DH,0AH
        DEFM    ">>>> Song wird gespielt :-) <<<< ",0DH,0AH,0DH,0AH
        DEFM    "Taste  0= Abbruch! Zurueck zum Testmenue"
        DEFM    024h
;
ctcmerk: defb    0
;
punkt1: defb   esc
zeipos: defb   80h             ;Zeile+80h
spapos: defb   80h             ;Spalte+80h
        defb   '$'
;
grafz:  defb   esc             ;Ausgabe Grafikzeichen
        defb   5Fh             ;Grafikzeichen folgt
gchar:  defb   0               ;Grafikzeichen
        defb   '$'
;
;--------------------------------------------------
;ACHTUNG!!! REGISTER 7 ... Bit 7 und Bit 6 auf 0 (Ports auf Eingabe)
; 
;FÜR AY-TAKT=2 MHZ: REGISTERWERT #7D FÜR CA. 1KHZ
;REGISTER       0     1   2   3   4   5   6   7    8   9   A   B   C   D
;               -HÖHEA-   -HÖHEB- -HÖHEC- R MIX   LA  LB  LC  HDF HDG HKF
TONA:   DEFB    7DH,  0,  0,  0,  0,  0,  0,0FEH,0FH,  0,  0,  0,  0,  0     
TONB:   DEFB      0,  0,7DH,  0,  0,  0,  0,0FDH,  0,0FH,  0,  0,  0,  0     
TONC:   DEFB      0,  0,  0,  0,7DH,  0,  0,0FBH,  0,  0,0FH,  0,  0,  0     
AUS:    DEFB      0,  0,  0,  0,  0,  0,  0,0FFH,  0,  0,  0,  0,  0,  0      
;
;AUCH ALS GERÄUSCHGENERATOR ZU BENUTZEN, BEISPIELE:
HUBI:   DEFB    6FH,  0,  0,  0,  0,  0,1Eh,0F6H,10H,  0,  0,2CH,  1,0EH 
MEER:   DEFB      0,  0,  0,  0,  0,  0,14H,0CFH,  0,10H,08H,  0,32H,0EH
SCHUSS: DEFB      0,  0,  0,  0,  0,  0,14H,0EFH,  0,10H,  0,  0,1FH,01H
;--------------------------------------------------
;
SONG:   
INCLUDEBINARY MUSIC.STC        ;HIER ABZUSPIELENDEN SONG EINBINDEN      
;END
