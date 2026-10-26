;06.09.13
;
; Z80
;---------------------
; MC80.30
;---------------------
; pcmc.asm    
;
; PCMC v.0.2      (c)Willisoft 2007
; Datenuebertragung  PC <-> MC80.30  
;
;++++++++++++++++++++++++++++++++++++++++++++++++++++++++
; Verbindung PC <-> MC80.30 über USB (COM) - IFSS 
;    COM von PC (COM3) <-> SIO auf ASP (IFSS)
;    Parameter: 4800/8/N/1
;++++++++++++++++++++++++++++++++++++++++++++++++++++++++ 
;

;Programm läuft auch im ROM, aber es sind 8 Byte im RAM (ramz1, ramz2) für
;die Ausgabe der Anfangs und Endadresse als String nötig
;Hier wurde RAM ab Adresse auf 0F300h !!! gesetzt.
;--- muss evtl. angepasst werden!!! 
;
;############################################
;Version für ROM ab Adresse 2800h
;############################################
;
;PROGRAMM-STARTADRESSE !!!!
;
bind:         equ    02800h        ;Programm Startadresse im ROM
kett:         equ    04000h        ;RAM-Kettung auf baf8 (4000 bei ROM)
;
ramz1:        equ    0f300h        ;RAM Zellen (evtl. ändern !!!)
ramz2:        equ    0f304h
;
; Vereinbarungen
;
zeitk:        equ    0ffffh
;
;
; mc80.30 spezifisch
;
wbn:          equ    00bc1h        ;text an bildschirm
bout:         equ    00bbeh        ;zeichen an bildschirm
clb:          equ    00bd9h        ;bildschirm loeschen
tst:          equ    00bcdh        ;tastaturabfrage polling
hbs:          equ    00bebh        ;hexbyte
;
; Tasten
;
cr:           equ    00dh
lf:           equ    00ah
et:           equ    011h
off:          equ    013h
;
; IO Bausteine
;
ctc5_as:      equ    0d5h               ;CTC aus ASP
sio5_ad:      equ    0d2h               ;SIO Kanal A auf ASP
sio5_as:      equ    0d3h
                                        ;SIO Kanal B für Tastatur
                                        ;reserviert

pio5_ad:      equ    0d8H               ;PIO  auf ASP - Platine
pio5_as:      equ    0d9H            
pio5_bd:      equ    0daH
pio5_bs:      equ    0dbH
;
;----------------------------------------------------------------
;HEADER für Datei (Adresse. Läange)
;
              setbase   bind

;nur bei Übertragung vom PC in RAM vom MC80 notwendig !!!

;              defw      bind            ;Startadresse
;anzahl:       defw      ende-begin-1    ;Programmlänge
;       
;----------------------------------------------------------------
;HEADER für Ramkettung
;
              setbase   bind
;Kennblock speziell für MC80.30 (Ramkettung)
;
begin:        defb      001h            ;1 byte = Kennbyte
              defw      kett-dist       ;2 byte = Distanz für 
                                        ;Ramkettung auf 4000h (Ramanfang)
                                        ; (0BAF8h _Dirctory)
                                        ;+ 4 Byte (Header)
dist:         defm      "PCMC"          ;n byte = Dateiname (4byte)
              defb      0ffh            ;1 byte = Trennbyte
;                                       ;gesamt 8 byte Header
;
;PROGRAMMSTART
;
;
start:        call      systemini
              call      clb
              call      copyright
              ld        hl,text1
              ld        bc,000b2h
              call      wbn
tast:         call      tst
              or        a
              jr        z,tast
              cp        045h            ;E
              call      z,empfang
              cp        053h            ;S
              call      z,senden
              cp        013h            ;OFF
              ret       z
              jr        start
              ret
;
; UP Empfang
;
empfang:      call      clb
              call      copyright
              ld        hl,texta
              ld        de,00605h
              ld        bc,00018h
              call      bout
              ld        hl,text10
              ld        de,00805h
              ld        bc,00025h
              call      bout
              call      systemini
              call      blockempf
              ld        hl,text20
              ld        de,00c05h
              ld        bc,00025h
              call      bout
              call      wait
              ret
;
; UP Senden
;
senden:       call      clb
              call      copyright
              ld        hl,textb
              ld        de,00605h
              ld        bc,00018h
              call      bout
              ld        hl,text30
              ld        de,00805h
              ld        bc,00025h
              call      bout
              call      systemini
              call      blocksend
              ld        hl,text40
              ld        de,00c05h
              ld        bc,00025h
              call      bout
              call      wait
              ret
;
; UP Block-Empfang  
;
blockempf:    call      ezeich
              ld        l,a             ;startadr. l-teil
              call      szeich
              call      ezeich
              ld        h,a             ;startadr. h-teil
              push      hl
              ld        de,hl           ;DE=anfadr
              call      szeich
              call      ezeich
              ld        c,a             ;proglaenge l-teil
              call      szeich
              call      ezeich
              ld        b,a             ;proglaenge h-teil
              push      bc
              add       hl,bc
              ld        bc,hl           ;BC=endadr
              call      hexaus
              call      szeich
              pop       bc
              pop       hl
              inc       bc
blockempf1:   call      ezeich
              ld        (hl),a
              call      szeich
              inc       hl
              dec       bc
              ld        a,b
              or        c
              jr        nz,blockempf1
              call      zeit
              ret                 
;
; UP Block-Senden
;
blocksend:    call      ezeich
              ld        l,a             ;startadr. l-teil
              call      ezeich
              ld        h,a             ;startadr. h-teil
              push      hl
              ld        de,hl           ;DE=anfadr
              call      ezeich
              ld        c,a             ;endadresse l-teil
              call      ezeich
              ld        b,a             ;endadresse h-teil
                                        ;BC=endadr
              call      hexaus
              ld        de,bc
              ex        de,hl
              sbc       hl,de           ;dateilaenge berechnen
              ld        b,h             ;BC Dateilänge
              ld        c,l
              inc       bc
              pop       hl
blocksend1:   ld        a,(hl)
              call      szeich
              ld        d,a
              push      bc
              call      ezeich
              pop       bc
              cp        d
              jr        nz,sendfehler
              inc       hl
              dec       bc
              ld        a,b
              or        c
              jr        nz,blocksend1
              jr        blocksendx
sendfehler:   ld        hl,text90
              ld        de,0081ch
              ld        bc,00016h
              call      bout
blocksendx:   call      zeit
              ret                 

;senden
szeich:       push      af
szeich1:      in        a,(sio5_as)     ;Puffer leer?
              bit       2,a
              jr        z,szeich1
              pop       af
              out       (sio5_ad),a     ;Zeichen senden
              ret
;empfang
ezeich:       in        a,(sio5_as)     ;Zeichen im Puffer?
              bit       0,a
              jr        nz,ezeich1
              jr        ezeich
ezeich1:      in        a,(sio5_ad)     ;Zeichen lesen
              ret
;       
; UP Initialisierung Systembausteine
;
systemini:    
init:         ld        hl,itab
              ld        c,ctc5_as
              ld        b,002h          ;ctc  auf asp
              otir    
              ld        c,sio5_as
              ld        b,007h          ;sio  aus asp
              otir    
              in        a,(sio5_ad)     ;Empangspuffer leeren
              ret
;
; UP Auf Tastendruck warten
;           
wait:         call      tst
              or        a
              jr        z,wait
              ret                 
;
; UP Hexausgabe
; DE = (anfadr)
; BC = (endadr)
; KILL:  
;
hexaus:       push      af
              push      hl
              push      de
              push      bc
              ld        hl,ramz1    ;Anfangsadresse
              ld        a,d
              call      hbs         ;in HEX-String wandeln
              ld        a,e
              call      hbs
              ld        hl,ramz2    ;Endadresse
              ld        a,b
              call      hbs
              ld        a,c
              call      hbs
              ld        hl,textadr  ; "Von: - Bis: "
              ld        de,000a12h
              ld        bc,0000bh
              call      bout
              ld        hl,ramz1    ; "0000"
              ld        de,000a1eh
              ld        bc,00004h
              call      bout
              ld        hl,textadr1 ; "H - "
              ld        de,000a22h
              ld        bc,00004h
              call      bout
              ld        hl,ramz2    ; "0000"
              ld        de,000a26h
              ld        bc,00004h
              call      bout
              ld        hl,textadr2 ; "H", piep
              ld        de,000a2ah
              ld        bc,00002h
              call      bout
              pop       bc
              pop       de                 
              pop       hl
              pop       af
              ret
;
; UP lange Schleife
;
zeit:         push      bc
              push      af
              ld        bc,zeitk
zeit1:        dec       bc
              ld        a,b
              or        c
              jr        nz,zeit1
              pop       af
              pop       bc
              ret
;    
; UP kurze Schleife
;
zss:          push      bc
              push      af                  
              ld        b,03h           ;ms
zss1:         djnz      zss1
              pop       af
              pop       bc
              ret
;
; UP Copyright ausgeben
;
copyright:    ld        hl,textcop
              ld        de,00219h
              ld        bc,00024h
              call      bout
              ret
;        
; INITIALISIERUNGSTABELLE FUER SYSTEM - BAUSTEINE
;
;SIO und CTC auf ASP
;SIO Kanal B (Kanal A ist für Tastatur reserviert)
;CTC Kanal ?
itab:         defb      017h           ;ctc auf asp 
              defb      001h           ;takt-teiler x1
                                       ;
              ;Achtung CTC Takt = CPU-Takt / 2 !!!     
              ;Wichtig! Bei SIO Taktteiler x16 unbedingt 
              ;einstellen, da sonst Übertragungsprobleme !!!!
              ;Baudrate 4800 über IFSS
              ;
              defb      018h           ;sio auf asp   
              defb      004h           ;WR4
              defb      044h           ;Takt x16 !!!, 1Stop, keine Parität
              defb      003h           ;WR3
              defb      0c1h           ;8 bit, Empfängerfreigabe   
              defb      005h           ;WR5
              defb      068h           ;8 bit, Senderfreigabe
;
; TEXTE
text1:        defb      00dh
              defb      00dh
              defm      "HAUPTMENUE: "
              defb      00dh
              defm      "----------- "
              defb      00dh
              defb      00dh  
texta:        defm      "FILE VOM PC EMPFANGEN   ...E"
              defb      00dh
              defb      00dh
textb:        defm      "FILE ZUM PC SENDEN      ...S"
              defb      00dh
              defb      00dh
              defm      "ENDE                    .OFF"
              defb      00dh
              defb      00dh
              defb      0ffh
textcop:      defm      "PC-MC  v.0.2      (c)WILLISOFT 2007  "
text10:       defm      "Senden auf PC Starten - Empfang ...  "
text20:       defm      "Empfang beendet ! ... Taste druecken",07h
text30:       defm      "Empfang auf PC starten ...           "
text40:       defm      "Sendung beendet ! ... Taste druecken",07h  
text90:       defm      "Uebertragungsfehler !",07h
              defb      0ffh

textadr:      defm      "Von - Bis: "
textadr1:     defm      "H - "
textadr2:     defm      "H",07h
;
              defm      0ffh
;++++++++++++++++++++++++++++++++++++++++++++++++++++++++
;
ende:         end
;
;++++++++++++++++++++++++++++++++++++++++++++++++++++++++
