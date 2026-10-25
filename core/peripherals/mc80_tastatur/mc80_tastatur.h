/**
 * @file mc80_tastatur.h
 * @brief Abgesetzte Tastatur des MC80.3x (eigener U881) auf ZEICHENEBENE an SIO A der ASP.
 *
 * Quellen: doc/mc80/egos.md §3 (Tastaturprotokoll aus der ISR 044BH/046FH),
 * doc/mc80/hardware_3x.md §6.3, Forum robotrontechnik.de threadid=7918 „Tastatur am MC80.3x“
 * (Messungen am Geraet, AE 2012–2014; Auswertung in doc/mc80/tastatur_forum.md).  Die
 * Firmware der Tastatur liegt nicht vor; nachgebildet wird, was EGOS erwartet und was das
 * Forum an der echten Tastatur gemessen hat:
 *
 *   - 9600 Bd 7O2 → ein Zeichen = 11 Bit = 11 × 256 Systemtakte (2,4576 MHz).
 *   - **Praefix** 1CH–1FH gilt fuer alle folgenden Zeichen (FB7CH).  Das Modell sendet es vor
 *     JEDEM Anschlag [Annahme]: das Kommando INIT setzt FB7CH zurueck, eine nur bei Wechsel
 *     sendende Tastatur waere danach stumm, bis ein anderes Praefix kaeme.
 *   - 1CH = grosses Tastenfeld: Zeichentasten so, wie die Kappe es sagt (Kleinbuchstabe ohne
 *     Umschalter).  EGOS tauscht Gross/Klein in der Vorgabe (FB7FH Bit 4) — „a“ erscheint
 *     also als „A“.  Dazu die Nicht-Zeichentasten [Forum]: **↑ 0BH, ↓ 0AH, ← 08H, → 0CH,
 *     CL 7FH, CR/LF 0DH**, Tab 09H, **ENTER = 11H**, **OFF = 13H**.
 *   - 1DH = Ziffernblock (ohne CTRL/Umschaltung): Ziffern und Zeichen, **CL 7FH**, die
 *     unbeschriftete Taste **20H**, „00“ = 30H zweimal [Forum].
 *   - 1EH + Bitmaske: Sondertaste n (0…7) — Maske 40H >> n, n = 7 → 00H.
 *   - 1FH + Code: CTRL war vor der Taste gehalten; EGOS setzt Bit 7 des Codes [Forum].
 *     Welches Zeichen die Tastatur dabei sendet (Kappe oder Steuercode), ist offen [?].
 *   - Rechner → Tastatur [Forum, Beitrag 037]: 07H Piep (und Dauerton aus), 10H Dauerton ein,
 *     04H Matrix sperren (Antwort 15H NAK, Bereit-LED aus), 05H freigeben (Antwort 06H ACK),
 *     0EH/0FH CTRL-/Umschaltung sperren/freigeben, 13H/14H Autorepeat (laut Forum wegen eines
 *     Firmwarefehlers wirkungslos), jedes andere Wort wird mit 18H (CAN) quittiert.
 *
 * **Anschlagabstand [Annahme]:** zwischen zwei TASTEN liegen mindestens 20 ms Maschinenzeit
 * (≈ 50 Anschlaege/s; Praefix und Code folgen dagegen im Zeichenabstand).  EGOS wertet das
 * Zeichen erst im nachgeordneten CTC-K3-Interrupt aus (FB45H) und zeichnet das Echo — kaeme
 * die naechste Taste schneller, ueberschriebe sie FB45H.  Eine echte Tastatur ist nie schneller.
 *
 * Wiederholung: macht die echte Tastatur selbst; hier nicht nachgebildet (ein keyPress = ein
 * Anschlag).
 */

#pragma once
#include <cstdint>
#include <deque>
#include <functional>

class Mc80Tastatur {
public:
    static constexpr int TAKTE_JE_ZEICHEN = 11 * 256;    ///< 9600 Bd bei 2,4576 MHz, 7O2
    static constexpr int TAKTE_JE_TASTE   = 49'152;      ///< 20 ms [Annahme]

    static constexpr uint8_t PRAEFIX_ZEICHEN  = 0x1C;
    static constexpr uint8_t PRAEFIX_EBENE2   = 0x1D;
    static constexpr uint8_t PRAEFIX_SONDER   = 0x1E;
    static constexpr uint8_t PRAEFIX_FUNKTION = 0x1F;
    static constexpr uint8_t ENTER = 0x11;
    static constexpr uint8_t OFF   = 0x13;
    static constexpr uint8_t ACK = 0x06, NAK = 0x15, CAN = 0x18;   ///< Antworten der Tastatur

    /// Tasten, die es nur in der Bildschirmtastatur gibt (kein Qt-Gegenstueck): 0x02000000 + n.
    static constexpr uint32_t TASTE_CRLF       = 0x02000001;   ///< CR/LF im grossen Feld
    static constexpr uint32_t TASTE_ZB_CL      = 0x02000002;   ///< CL im Ziffernblock
    static constexpr uint32_t TASTE_ZB_00      = 0x02000003;   ///< „00“ im Ziffernblock
    static constexpr uint32_t TASTE_ZB_LEER    = 0x02000004;   ///< unbeschriftete Taste (SPACE)
    static constexpr uint32_t TASTE_ZB_ZEICHEN = 0x02000100;   ///< + ASCII: Zeichen im Ziffernblock

    /// Leitung zum Rechner: frei? / Zeichen uebergeben.
    std::function<bool()>        empfaengerFrei;
    std::function<void(uint8_t)> senden;

    void reset();   ///< RES-Taste/Netz ein: Warteschlange leer

    /// Zeichentaste (Praefix 1CH): Code so, wie er gesendet wird.
    void zeichen(uint8_t code);
    /// Sondertaste 0…7 (Praefix 1EH + Bitmaske).
    void sondertaste(int n);
    /// Taste des Ziffernblocks (Praefix 1DH).
    void ziffernblock(uint8_t code);
    /// CTRL + Taste (Praefix 1FH + Code; EGOS setzt Bit 7) [Forum].
    void funktionstaste(uint8_t code);
    /// Text tippen: jedes Zeichen als Zeichentaste, '\n'/'\r' = ENTER.
    void tippe(const char* text);

    /**
     * Taste der Oberflaeche (Qt-Code oder ASCII wie k1520_key_press).  Buchstaben: ohne
     * Umschalter klein, mit gross (Tastenkappe), Strg+Buchstabe = Code & 1FH; Return/Enter
     * = ENTER, Escape = OFF, Pfeile = 0BH/0AH/08H/0CH, Entf = CL (7FH), Rueckschritt = 08H,
     * Tab = 09H, F1…F8 = Sondertaste 0…7, TASTE_* = Tasten der Bildschirmtastatur.
     * Unbekanntes wird verworfen.  @return true, wenn ein Anschlag entstand.
     */
    bool qtTaste(uint32_t keycode, bool shift, bool ctrl);

    /// Maschinentakte verstreichen lassen; liefert Zeichen aus, wenn die Leitung frei ist.
    /// @return true, wenn ein Zeichen uebergeben wurde.
    bool takt(int takte);

    /// Zeichen vom Rechner (SIO A Senden).
    void vomRechner(uint8_t b);

    bool     leer() const { return queue_.empty(); }
    size_t   wartend() const { return queue_.size(); }
    uint32_t summerZaehler() const { return summer_; }
    bool     anzeige() const { return bereit_; }   ///< Bereit-LED (grün)
    bool     bereit() const { return bereit_; }
    bool     dauerton() const { return dauerton_; }
    bool     umschaltungGesperrt() const { return umschaltung_gesperrt_; }

private:
    struct Eintrag { uint8_t byte; bool tastenanfang; };
    void anschlag(uint8_t praefix, uint8_t code);
    void antwort(uint8_t b) { queue_.push_back({b, false}); }

    std::deque<Eintrag> queue_;
    int64_t  bis_zeichen_ = 0;  ///< Takte bis zum naechsten moeglichen Zeichen
    int64_t  bis_taste_ = 0;    ///< Takte bis zur naechsten moeglichen Taste
    uint32_t summer_ = 0;
    bool     bereit_ = true;                ///< nach dem Einschalten: ACK + gruene LED
    bool     dauerton_ = false;
    bool     umschaltung_gesperrt_ = false; ///< 0EH/0FH
};
