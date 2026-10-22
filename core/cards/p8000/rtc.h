/**
 * @file rtc.h
 * @brief Echtzeituhr-Karte des P8000 (nur 16-Bit-Teil, E/A-Seite 0xFDxx/0xFExx): gemeinsame
 *        Schnittstelle `P8000Rtc` und die Auspraegung **RTC-72421** (Epson-Nachbau, pofo.de).
 *        Die Auspraegung U130X (Originalkarte 1988) folgt in AP R2; der Typ ist schon angelegt.
 *
 * Quellen: doc/design/37_p8000_rtc.md; Treiber `sa.timer/common/rtc72421.{h,c}` (BSD, O. Lehmann);
 * Plan `P8000_Echtzeituhr_v1.2_Plan.pdf` (doc/p8000/uhr/, KiCad Rev 1.7, 2016-07-11).
 *
 * @code
 *   Anbindung (Plan): D1 74LS133 dekodiert A5–A15 = 0xFDE0–0xFDFF, A0 = 1 (E/A-Byte auf ungeraden
 *   Adressen); A1–A4 = Registernummer des 72421; /RD, /WR; D0–D3 des Bausteins ueber den 74LS245
 *   an AD0–AD3, die oberen vier Bit liest der Treiber als Kennung 0x30
 *   (`(inb(0xFDE1) & 0xF0) == 0x30`).
 *   Register (Nibble n, Adresse = 0xFDE1 + 2*n):
 *     0 S1  1 S10  2 MI1  3 MI10  4 H1  5 H10 (Bit 2 = PM im 12-h-Betrieb)  6 D1  7 D10
 *     8 MO1  9 MO10  A Y1  B Y10  C Wochentag  D (HOLD b0, BUSY b1, IRQ-Flag b2, 30s-ADJ b3)
 *     E (MASK b0, ITRPT/STND b1, t0 b2, t1 b3 — unbenutzt)  F (RESET b0, STOP b1, 24/12 b2, TEST b3)
 * @endcode
 *
 * **Zeitmodell („Batterie"):** Uhr = Wirtszeit + `versatz_` (Sekunden).  Stellt der Gast die Uhr
 * (STOP/RESET setzen, Ziffern schreiben, beide wieder loeschen), wird aus der gestellten Zeit
 * `versatz_ = gestellt − Wirtszeit` und — bei gesetztem `zustand` — in die Batteriedatei
 * geschrieben.  Ohne gestellte Uhr laeuft sie mit der Wirtszeit.  Die Zeitquelle ist
 * injizierbar (Tests); Vorgabe `std::time`.  Anzeige in der Ortszeit des Wirts (`utc = false`)
 * oder UTC.  `jahresversatz` verschiebt NUR die angezeigte Jahreszahl (Rueckfall, falls WEGA 2026
 * nicht mag; ein 29.2. in einem Nicht-Schaltjahr wird 28.2.).
 *
 * Benannte Annahmen (ein Datenblatt des RTC-72421 liegt nicht vor; belegt ist nur, was Treiber
 * und Plan zeigen):
 *  [R1] Registerfunktionen nach dem Epson-Datenblatt (Gedaechtnis des Entwurfs), D/E/F wie im
 *       Treiber `rtc72421.h`.  Wochentag (Nibble C): beim Lesen aus dem Datum errechnet
 *       (0 = Sonntag); geschrieben wird er nur gespeichert; kein Treiber benutzt ihn.
 *  [R2] BUSY: je Sekunde Maschinenzeit 122 us hoch (Phase beginnt mit der Maschinenzeit der Karte,
 *       ohne Bezug zur Wirtssekunde), unabhaengig von HOLD.  HOLD nimmt beim Setzen eine
 *       Momentaufnahme der Zeit; Lesen der Zeitregister liefert sie, bis HOLD faellt.
 *  [R3] Zeitregister werden nur bei STOP oder RESET gestellt (wie der Treiber); Schreiben bei
 *       laufender Uhr bleibt ohne Wirkung.  Waehrend STOP/RESET zeigen die Register den beim
 *       Anhalten uebernommenen (und ggf. ueberschriebenen) Stand; beim Loeschen beider Bit zaehlt
 *       die Uhr ab dem gestellten Wert weiter.  Ungueltige Eingabe (Monat 0, Tag > Monatslaenge,
 *       Stunde > 23 …) wird auf den naechstgueltigen Wert begrenzt.
 *  [R4] Jahr: nur zwei Ziffern.  Beim Stellen waehlt die Karte das Jahrhundert so, dass das
 *       gestellte Jahr dem angezeigten am naechsten liegt.
 *  [R5] Startzustand: Register D = 0, E = 0, F = 24-h-Betrieb (04H), Uhr laeuft (Batterie da).
 *  [R6] 30s-ADJ (D b3): rundet auf die volle Minute (>= 30 s auf, sonst ab), Bit selbstloeschend.
 *  [R7] Gerade Adressen und Adressen ausserhalb 0xFDE1–0xFDFF antworten nicht (offener Bus).
 *
 * Save-State: eigener Abschnitt der Maschine (Version 1): `versatz_` und Registerzustand.
 * Die Wirtszeit laeuft nie zurueck, ein geladener Versatz bleibt daher richtig.
 */

#pragma once
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

enum class P8000RtcTyp { Aus, U130X, Rtc72421 };

struct P8000RtcConfig {
    P8000RtcTyp typ = P8000RtcTyp::Aus;
    int jahresversatz = 0;                ///< Jahre, nur Anzeige
    bool utc = false;                     ///< false = Ortszeit des Wirts
    std::string zustand;                  ///< Batteriedatei, "" = nur im Speicher
    std::function<int64_t()> zeitquelle;  ///< Unix-Sekunden UTC; leer = std::time
    uint32_t takt_hz = 4'000'000;         ///< Takt, in dem `takte()` zaehlt (Maschinenzeit)
};

class P8000Rtc {
public:
    virtual ~P8000Rtc() = default;
    /// Legt die Karte an; @c nullptr + @p fehler bei Typ Aus bzw. noch nicht verfuegbarem Typ.
    static std::unique_ptr<P8000Rtc> anlegen(const P8000RtcConfig& cfg, std::string& fehler);

    /// E/A-Lesen; @c false = die Karte antwortet nicht (offener Bus, der Aufrufer liefert `leer8`).
    virtual bool lesen(uint16_t port, uint8_t& d) = 0;
    virtual bool schreiben(uint16_t port, uint8_t d) = 0;
    /// Maschinenzeit der Karte um @p n Takte weiter.
    virtual void takte(int n) = 0;
    virtual P8000RtcTyp typ() const = 0;
    /// Versatz „Uhr − Wirtszeit" in Sekunden (Tests, Anzeige).
    virtual int64_t versatz() const = 0;
    virtual void serialize(std::vector<uint8_t>& out) const = 0;
    virtual bool deserialize(const uint8_t*& p, const uint8_t* end) = 0;
};

/// Epson RTC-72421 (Nachbau 2009–2016).
class P8000Rtc72421 final : public P8000Rtc {
public:
    explicit P8000Rtc72421(const P8000RtcConfig& cfg);
    bool lesen(uint16_t port, uint8_t& d) override;
    bool schreiben(uint16_t port, uint8_t d) override;
    void takte(int n) override;
    P8000RtcTyp typ() const override { return P8000RtcTyp::Rtc72421; }
    int64_t versatz() const override { return versatz_; }
    void serialize(std::vector<uint8_t>& out) const override;
    bool deserialize(const uint8_t*& p, const uint8_t* end) override;

    static constexpr uint16_t BASIS = 0xFDE1;   ///< Nibble 0 (S1)
    static constexpr int BUSY_US = 122;         ///< [R2]

private:
    bool angehalten() const { return (f_ & 0x03) != 0; }
    int64_t wirtWand() const;                   ///< Wirtszeit als Wandzeit (Ort/UTC), ohne Versatz
    int64_t uhrWand() const { return wirtWand() + versatz_; }
    void ladeNibbel(int64_t wand, uint8_t* n13) const;   ///< Nibbelregister aus einer Wandzeit (24/12 h)
    bool wandAusNibbeln(int64_t& wand) const;   ///< false = Nibbel unbrauchbar
    void festschreiben();                       ///< gestellte Zeit → versatz_, Datei
    void sichern() const;
    void laden();
    void minuteRunden();

    P8000RtcConfig cfg_;
    int64_t versatz_ = 0;
    uint8_t d_ = 0, e_ = 0, f_ = 0x04;          ///< Register D (HOLD/IRQ), E, F
    bool hold_ = false;
    uint8_t zeit_[13] = {};                     ///< Momentaufnahme (HOLD) bzw. Stellregister (STOP)
    bool stell_geaendert_ = false;
    uint64_t phase_ = 0;                        ///< Takte seit Sekundenanfang (Maschinenzeit)
    uint64_t busy_takte_ = 0;
};
