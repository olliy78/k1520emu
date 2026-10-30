"""Namen und Kommentare fuer modul.py (SYSORG09-Module).  Offsets relativ zum Modulanfang.
[Code] = aus dem Listing gelesen; [?] = Deutung.  Befund: doc/sios/module_k7633_kut.md."""

KOPF_K7633 = """; ============================================================================
; t_k7633.prn - SIOS 1526, Modul T-K7633 (SYSORG09 Position 22, 3 Saetze, Variante 3)
; Terminaltastatur K7633 an der AUP 062-8531 (E/A 40H-43H) + Kundentastatur-Empfang.
; Quelle: tools/sios_a5120/original/SIOS1526_SG1.hfe, Datei SYSORG09.  Erzeugt mit
;   python3 tools/sios_a5120/modul.py prn T-K7633 doc/sios/t_k7633.prn --org 2000H
; Ladeadresse ist NICHT festgelegt (SGEN legt das Modul in 1-KB-Bloecke); hier 2000H wie T-K7637.
; Der Code ist lageunabhaengig: D = Seite des Moduls beim Aufruf von Block+2; relative Spruenge
; (LD DE,n / CALL 002BH) und Aufrufe (CALL 003BH) gelten ab Folgeadresse.
; [Code] aus dem Listing gelesen, [?] Deutung.  Ports: 41H/42H lesen, 43H schreiben, 00H = INT-BS.
; ============================================================================"""

KOPF_KUT = """; ============================================================================
; kut.prn - SIOS 1526, Modul KUT (SYSORG09 Position 25, 1 Satz, Variante 1)
; Geraetetreiber Kundentastatur (Geraetenummer 8, Eintrag 01A8H).  Erzeugt mit
;   python3 tools/sios_a5120/modul.py prn KUT doc/sios/kut.prn --org 2C00H
; Hat KEINEN eigenen Eingabeport: die Codes liest der Empfangsteil im Modul T-K7633 (2B1D).
; Einziger E/A: OUT (43H) fuer das Lampenbyte (GET-LED).  Befund: doc/sios/module_k7633_kut.md
; ============================================================================"""

MODULE = {
    "T-K7633": {
        "kopf": KOPF_K7633,
        "einstiege": [2, 0x4F, 0x4AA, 0x7A4, 0x783, 0x6F2, 0x824, 0x9E0, 0xA94, 0xAFE, 0xB1D, 0xB61,
                      0xB84, 0xBA0, 0xBC7, 0xBE2, 0x1D4, 0x37E, 0x45A, 0x48F],
        "namen": {
            0x002: "INIT", 0x04F: "GERAET_EREIGNIS", 0x1D4: "STEUERCODE_10H", 0x4AA: "GERAET_BEFEHL",
            0x7A4: "ISR_ZEIT", 0x7BF: "ISR_ABFRAGE", 0x8C7: "TASTE_CODE", 0xAFE: "SCHLUESSEL_SL",
            0xB1D: "KUT_CODE", 0x954: "SCHLUESSEL_GRUPPE_A0", 0xBE2: "FEHLER_TYP",
        },
        "kommentar": {
            0x002: "INIT (Block+2): Zeiger (0110H)=Ereignisroutine +004FH, (010EH)=Befehlsroutine +04AAH; Geraetetabelle (0301H)=0EH (Geraet 1 = Dialog, Eintrag 010EH)",
            0x01B: "(0115H)=2 (Zeitgeber des Geraeteeintrags), (00FDH)=E6H",
            0x027: "IM-2-Tabelle: Slot 027CH (CTC-Kanal 2, Vektor 7CH) := ISR_ZEIT (+07A4H) - Abfrage im Zeittakt, KEIN AUP-Interrupt",
            0x02E: "A=FFH: OUT (43H),FFH = RESET AUP (Kommandobyte)",
            0x033: "Typabfrage: IN A,(42H) bis High-Nibble = 80H (Zaehler HL=FFFFH, ca. 65535 Lesezugriffe); KEIN Test von IN 41H davor",
            0x041: "Zeitablauf: relativer Sprung nach 2BE2H (Fehlerausgabe BC=2032H DE=424BH)",
            0x047: "Low-Nibble des Typzeichens -> (00B1H)",
            0x04F: "Geraeteroutine (Ereignis/Abtastung durch ISR 1E48H): Lampenbyte neu bilden",
            0x060: "(00CAH): Bit0 = SEL-Zustand, Bit1 = ALPHA-Zustand (getoggelt von den Codes A0H/A1H); A = 03H XOR (00CAH) OR FCH",
            0x069: "(00CBH) Bit7 (Fehlersperre) gesetzt -> Lampenbit 5 loeschen (= Fehlerlampe EIN, 0 = ein)",
            0x06F: "(01BAH) Bit0 (Kundeneingabe aktiv) -> Lampenbit 4 loeschen (= GET-LED EIN)",
            0x078: "(004EH) Bit2 (Hupanforderung) -> gelesen+geloescht, Lampenbit 6 loeschen (= Hupe EIN, einmalig)",
            0x083: "Vergleich mit Abbild (076AH); nur bei Aenderung: (076AH):=A, OUT (43H),A = Kommandobyte (Bit7 immer 1)",
            0x1D4: "[?] Steuercode 10H (Ereignis-Abschluss: Eingabe/Ende-Text-Behandlung)",
            0x4AA: "Geraetebefehl-Routine (SVC-Parameter ab 0050H: Befehlscode); Befehl 01H = Hupe: (004EH) Bit2 setzen",
            0x7A4: "ISR Zeittakt (CTC-Kanal 2): EI, Register retten",
            0x7BF: "IN A,(41H) AND 03H: Bit1 -> Terminaltaste (TASTE_CODE), nur Bit0 -> Kundentastatur (KUT_CODE); beide Bits: erst Bit1",
            0x7CB: "Bit1: relativer Aufruf von TASTE_CODE (28C7H)",
            0x7ED: "Bit0: relativer Sprung nach KUT_CODE (2B1DH)",
            0x7D9: "Zeitgeber-Zelle (00FBH) herabzaehlen (Fortsetzung der ISR), RETI am Ende (281EH)",
            0x8C7: "IN A,(42H) = Tastencode (Lesen loescht Gueltigkeit); in (006FH) abgelegt, Flag (006EH) Bit3 geloescht",
            0x8D6: "High-Nibble A0H (Bit3 gesetzt = A8H-AFH) -> SCHLUESSEL_SL, sonst weiter",
            0x954: "Codes A0H-A7H: Bit n von (00CAH) toggeln (n = Code-A0H): A0H = SEL (F10), A1H = ALPHA (F05); Codes A8H-AFH: (00CBH) siehe SCHLUESSEL_SL",
            0x974: "Zifferneingabe: Zeichen '-' (2DH) und 30H-39H gueltig; Eingabepuffer ab 009FH abwaerts",
            0xAFE: "Codes A8H-AFH: AFH = (00CBH) Bit7 loeschen (Fehlerlampe/Sperre, Taste F12); A8H = Bit0 toggeln; sonst (Code AND 3)<<1 XOR (00CBH): A9H (SS2) kippt Bit1, AAH (SS1) kippt Bit2",
            0xB1D: "KUT_CODE: nur wenn (01BAH) Bit0 (Kundeneingabe aktiv) - sonst Rueckkehr zur ISR (Code bleibt unabgeholt)",
            0xB2C: "IN A,(42H) = Code der Kundentastatur",
            0xB2E: "Code 19H = Korrektur: Puffer (01B4H, Laenge 01B6H) mit 00H loeschen, Zeiger/Zaehler zurueck",
            0xB4A: "sonst Zeichen in Puffer (01BBH)++, Restzaehler (01BDH)--; bei 0: (01ACH)=FFH, OUT (00H) = Eingabe fertig",
            0xBE2: "Fehlerausgabe TYP-Zeitablauf: EI, BC=2032H DE=424BH, Fehlerbehandlung 002BH-Tabelle L=2BH/13H",
        },
    },
    "KUT": {
        "kopf": KOPF_KUT,
        "einstiege": [2, 0x1B, 0x6E, 0x90, 0x124],
        "namen": {0x002: "INIT", 0x01B: "GERAET_EREIGNIS", 0x06E: "EINGABE_ENDE", 0x090: "GERAET_BEFEHL"},
        "kommentar": {
            0x002: "INIT: (01AAH)=Ereignisroutine +001BH, (01A8H)=Befehlsroutine +0090H; Geraetetabelle (0308H)=A8H = Geraet 8 (Eintrag 01A8H)",
            0x01B: "Ereignis (ISR 1E48H): (01ADH) Bit2 gesetzt? Eingabe-Zustand (01B1H): 0 = laeuft, FFH = abbrechen",
            0x04F: "Befehl 01H in (01B1H): Eingabe starten: (01BAH) Bit0 := 1 (aktiv -> GET-LED ueber T-K7633), Puffer (01B4H, Laenge 01B6H) loeschen",
            0x06E: "Eingabe beenden: (01BAH) Bit0 := 0, Lampenbyte (076AH) Bit4 setzen (GET inaktiv), OUT (43H)",
            0x07A: "OUT (43H),A mit gesetztem Bit4 (GET-LED aus), sonst Bits aus (076AH)",
            0x090: "Befehlsroutine (SVC, Befehlscode in (0050H)): 2DH = Eingabe mit Laenge (0051H) in Puffer (0052H); 2EH = SVC-Block (Geraet 08H, Befehl 00H), sonst Fehler",
            0x0B8: "Befehl 2DH: Pufferlaenge pruefen, Fehler '06' ueber 0042H-Tabelle (BC=3630H L=27H)",
            0x0F3: "SVC-Block 0104H.. aufbauen (Geraet 08H, Befehl 01H), (0108H)=Puffer, (010AH)=Laenge, RST 08H Kennbyte 23H",
        },
    },
}
