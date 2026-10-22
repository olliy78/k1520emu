/**
 * @file rtc.h
 * @brief Echtzeituhr-Karte des P8000 (nur 16-Bit-Teil, E/A-Seite 0xFDxx/0xFExx): gemeinsame
 *        Schnittstelle `P8000Rtc`, die gemeinsame Zeitbasis `P8000RtcBasis` und die beiden
 *        Auspraegungen **RTC-72421** (Epson-Nachbau, pofo.de) und **U130X** (Originalkarte 1988,
 *        Armbanduhr-Chip ohne Registerschnittstelle; AP R2, Annahmen [U1]-[U9] weiter unten).
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
 * **Zeitmodell (AP R2b, „Batterie" in Maschinenzeit):** Die Uhr zaehlt in **Maschinenzeit** der Karte16
 * (Takte / `takt_hz`), wie die Kernuhr des Gasts — bei 5facher Emulation also 5 Uhrsekunden je Wirtssekunde;
 * angehaltene Maschine = Uhr steht.  Grund: WEGA liest die RTC nur beim Booten und zaehlt danach ueber die CTC;
 * folgte die RTC der Wirtszeit, sprang die Uhr beim naechsten Boot hinter die (vorauseilenden) Dateizeiten zurueck
 * (`make` haelt Quellen fuer neuer).  Die Uhr darf beim Neustart nie zurueckspringen.
 *  - Erststart ohne Batteriedatei: Startwert = Wirtszeit (Zeitquelle, injizierbar; Vorgabe `std::time`;
 *    Ortszeit des Wirts bzw. UTC je `utc`).
 *  - Batteriedatei (`zustand`, Text, Kopf `K1520-P8000-RTC 2`): `stand=` (Uhrstand als Wandzeit, **ohne**
 *    Jahresversatz) und `wirt=` (Unix-Sekunden UTC beim Speichern).  Geschrieben: beim Zerstoeren der Karte, beim
 *    Stellen (Gast, von aussen, Laden eines Zwischenstands) und je 60 s Maschinenzeit (kleine Datei, nie je Befehl).
 *    Das alte Format 1 (`versatz=`) wird gelesen und wie „Wirtszeit + Versatz" als Stand genommen.
 *  - **Startregel:** Startwert = max(gespeicherter Stand + (Wirtszeit jetzt − Wirtszeit beim Speichern),
 *    Wirtszeit jetzt).  Eine vorausgelaufene Uhr holt auf, sobald die Wirtszeit sie ueberholt; negative Abwesenheit
 *    (Wirtsuhr zurueckgestellt) zaehlt als 0.  Folge: auch eine vom Gast ZURUECKgestellte Uhr gilt nur bis zum
 *    naechsten Start, sofern sie hinter der Wirtszeit liegt.  Beim Wechsel Ortszeit/UTC oder einer Sommerzeitgrenze in
 *    der Abwesenheit rechnet die Regel in der Wandzeit des gespeicherten Modus (Abweichung hoechstens die Zonendifferenz,
 *    die Uhr springt auch dann nicht zurueck).
 *  - Der Gast stellt die Uhr (STOP/RESET setzen, Ziffern schreiben, beide wieder loeschen): der neue Stand gilt, auch
 *    rueckwaerts, und wird sofort gespeichert.  `jahresversatz` verschiebt NUR, was der Gast liest/schreibt; gespeichert
 *    und gerechnet wird unversetzt (ein 29.2. in einem Nicht-Schaltjahr wird 28.2.).
 *  - Von aussen: `gastZeit()`/`stellen()` und die statischen `startzeit()`/`stellenDatei()` (C-ABI `k1520_rtc_*`).
 *    Kodierung: Kalenderzeit als Unix-Sekunden (die Wanduhr, als waere sie UTC).  `gastZeit()`/`startzeit()` liefern sie
 *    **gastsichtig** (mit Jahresversatz), `stellen()`/`stellenDatei()` nehmen sie **im Jahr des Wirts** (unversetzt,
 *    wie gespeichert); die Karte wendet den Versatz beim Lesen an.
 *  - Save-State: Uhrstand (Sekunden + Resttakte) im Kartenabschnitt.  Laden stellt den Stand wieder her, wie er war
 *    (konsistent zur geladenen Kernzeit), und die Batteriedatei wird danach damit fortgeschrieben.  Staende aus R1/R2
 *    (Versatz) laden als „Wirtszeit + Versatz".
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
 * Save-State: eigener Abschnitt der Maschine (Version 2; Version 1 mit `versatz_` bleibt ladbar).
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
    /// Maschinenzeit der Karte um @p n Takte weiter (die Uhr zaehlt mit).
    virtual void takte(int n) = 0;
    virtual P8000RtcTyp typ() const = 0;
    /// Abstand „Uhr − Wirtszeit jetzt" in Sekunden (Tests, Anzeige) — wandert mit der Emulationsgeschwindigkeit.
    virtual int64_t versatz() const = 0;
    /// Uhr wie der Gast sie sieht (mit Jahresversatz), Kalenderzeit als Unix-Sekunden kodiert.
    virtual int64_t gastZeit() const = 0;
    /// Uhr von aussen stellen: Kalenderzeit im Jahr des Wirts (ohne Jahresversatz); Batteriedatei wird geschrieben.
    virtual void stellen(int64_t wand) = 0;

    /// Zeit, mit der eine Karte mit dieser Batteriedatei JETZT starten wuerde (Startregel), gastsichtig; ohne Karte.
    static int64_t startzeit(const std::string& zustand, int jahresversatz, bool utc,
                             const std::function<int64_t()>& zeitquelle = {});
    /// Batteriedatei auf @p wand (Kalenderzeit im Jahr des Wirts) stellen; false = nicht schreibbar.
    static bool stellenDatei(const std::string& zustand, int64_t wand,
                             const std::function<int64_t()>& zeitquelle = {});
    /// Zustand der Karte als Text (Debugger `rtc`): Typ, Uhrzeit, Versatz, Typ-Besonderheiten.
    virtual std::string text() const = 0;
    virtual void serialize(std::vector<uint8_t>& out) const = 0;
    virtual bool deserialize(const uint8_t*& p, const uint8_t* end) = 0;
};

/// Gemeinsame Zeitbasis beider Auspraegungen: Zeitquelle, Uhrstand in Maschinenzeit, Batteriedatei.
class P8000RtcBasis : public P8000Rtc {
public:
    explicit P8000RtcBasis(const P8000RtcConfig& cfg);
    ~P8000RtcBasis() override;
    int64_t versatz() const override { return sek_ - wirtWand(); }
    int64_t gastZeit() const override;
    void stellen(int64_t wand) override;

protected:
    void zaehle(int n);                         ///< Maschinenzeit weiter: Uhrstand, Speicherintervall
    std::string uhrText() const;                ///< "JJJJ-MM-TT hh:mm:ss" der Uhr (Anzeigejahr mit Jahresversatz)
    int64_t wirtWand() const;                   ///< Wirtszeit als Wandzeit (Ort/UTC), ohne Versatz
    int64_t uhrWand() const { return sek_; }    ///< Uhrstand als Wandzeit (Ort/UTC), ohne Jahresversatz
    void sichern() const;                       ///< Batteriedatei schreiben (falls `zustand` gesetzt)
    void setzeUhr(int64_t wand, bool sekunde0); ///< Uhr stellen + sichern (@p sekunde0: Resttakte loeschen)

    P8000RtcConfig cfg_;
    int64_t sek_ = 0;                           ///< Uhrstand in ganzen Sekunden (Wandzeit)
    uint64_t rest_ = 0;                         ///< Takte seit dem letzten Sekundenwechsel der Uhr
    mutable uint64_t seit_sicher_ = 0;                  ///< Takte seit dem letzten Schreiben der Batteriedatei
    static constexpr int SICHER_SEKUNDEN = 60;  ///< Speicherintervall (Maschinenzeit)
};

/// Epson RTC-72421 (Nachbau 2009–2016).
class P8000Rtc72421 final : public P8000RtcBasis {
public:
    explicit P8000Rtc72421(const P8000RtcConfig& cfg);
    bool lesen(uint16_t port, uint8_t& d) override;
    bool schreiben(uint16_t port, uint8_t d) override;
    void takte(int n) override;
    P8000RtcTyp typ() const override { return P8000RtcTyp::Rtc72421; }
    std::string text() const override;
    void serialize(std::vector<uint8_t>& out) const override;
    bool deserialize(const uint8_t*& p, const uint8_t* end) override;

    static constexpr uint16_t BASIS = 0xFDE1;   ///< Nibble 0 (S1)
    static constexpr int BUSY_US = 122;         ///< [R2]

private:
    bool angehalten() const { return (f_ & 0x03) != 0; }
    void ladeNibbel(int64_t wand, uint8_t* n13) const;   ///< Nibbelregister aus einer Wandzeit (24/12 h)
    bool wandAusNibbeln(int64_t& wand) const;   ///< false = Nibbel unbrauchbar
    void festschreiben();                       ///< gestellte Zeit → Uhrstand, Datei
    void minuteRunden();

    uint8_t d_ = 0, e_ = 0, f_ = 0x04;          ///< Register D (HOLD/IRQ), E, F
    bool hold_ = false;
    uint8_t zeit_[13] = {};                     ///< Momentaufnahme (HOLD) bzw. Stellregister (STOP)
    bool stell_geaendert_ = false;
    uint64_t phase_ = 0;                        ///< Takte seit Sekundenanfang (Maschinenzeit)
    uint64_t busy_takte_ = 0;
};

/**
 * RFT U130X (Original, 1988): Armbanduhr-Chip; die Karte sieht nur die LCD-Segmentausgaenge
 * (sechs Stellen, je 5 Segmente + COM) und treibt die Tasten HT/VT/ST ueber ein Schieberegister.
 *
 * Anbindung (Stromlaufplan `doc/p8000/uhr/U130_Stromlaufplan.pdf`, Treiber `u130.c`/`timer.c`):
 * @code
 *   FEF1  lesen = 0xBF (Kennung)      schreiben = Tasten: Bit0 VT, Bit1 HT, Bit2 ST (gehalten, 1 = gedrueckt)
 *   FEF5 FEF7 | FEF9 FEFB | FEFD FEFF   = Stelle 1/2 | 3/4 | 5/6 (Einer, Zehner)
 *     Zeit : Sekunde | Minute | Stunde        Datum : Jahr | Tag | Monat
 *   Lesewert: Bit0-4 Segmente, Bit5 = COM (Wechselspannungsphase), Segment aktiv <=> Bit != COM;
 *             FEFF: Bit3 = Datums-Symbol (gleiche Regel), Bit7 = Sekundentakt (kein Wechselsignal),
 *             nur Bit0-2 sind Segmente (Zehner Stunde/Monat: 1 = 0x02, 2 = 0x05, leer = 0).
 *   Segmentcodes (Bit0-4, aktiv = 1): 0=0F 1=02 2=17 3=13 4=1A 5=19 6=1D 7=03 8=1F 9=1B, leer=00
 * @endcode
 *
 * Benannte Annahmen (kein Datenblatt der Karte; belegt ist nur, was die Treiber tun):
 *  [U1] Tasten: Zuordnung Bit0 VT, Bit1 HT, Bit2 ST aus Stromlaufplan (4035 Q1 = VT, Q2 = HT, Q3 = ST)
 *       und Treiber (0x02 = HT = Datum, 0x01 = VT, 0x03 = HT+VT).  ST (Stoppuhr) ist unbelegt.
 *       Wirksam ist nur die Flanke 0 -> 1 eines Tastenbits; HT wird vor VT ausgewertet.
 *  [U2] COM wechselt alle 1 ms Maschinenzeit (der echte Chip taktet mit rund 32 Hz; beobachtbar ist nur,
 *       dass der Wert zwischen zwei Lesungen kippen kann).  Sekundentakt (Bit7 FEFF): erste Haelfte jeder
 *       Maschinensekunde 1.  Beides in Maschinenzeit der Karte, nicht in Wirtszeit.
 *  [U3] Zeitwerte aus dem Uhrstand (Maschinenzeit) wie beim RTC-72421; Jahr zweistellig 00-99 (die Karte
 *       bildet nicht die Grenze 2019 des Chips nach), `jahresversatz` nur in der Anzeige.
 *  [U4] Anzeige: Stunde Zehner wird bei 0 leer gezeigt (sonst nicht lesbar, der Treiber kennt keinen
 *       Code dafuer); alle anderen Stellen zeigen die Null.
 *  [U5] Zustandsautomat (nur die vom Treiber benutzten Folgen):
 *         Zeit --HT--> Datum (faellt nach 2 s Maschinenzeit zurueck; HT erneut = sofort Zeit)
 *         Datum+VT(bei gehaltenem HT) --> Korrektur Monat --VT--> Tag-Zehner --> Tag-Einer --> Jahr-Zehner
 *                 --> Jahr-Einer; dann HT = Korrektur Ende (Datum wird uebernommen, Tag auf die Monatslaenge
 *                 begrenzt) --> Zeit;  (VT nach Jahr-Einer: dasselbe)
 *         Zeit/Datum --VT(ohne HT)--> Zeit-Korrektur, Uhr STEHT; HT = Sekunden auf 0 (>= 30 s: Minute + 1)
 *                 --VT--> (Zwischenstufe, unbelegt) --VT--> Stunde --VT--> Minute-Zehner --VT--> Minute-Einer
 *                 --VT--> Halt (Zeit gestellt, Uhr steht) --VT--> Uhr laeuft ("Start der Zeit").
 *       Die Korrekturstelle zaehlt selbstaendig weiter (Stunde 0-23, Minute-Zehner 0-5, Monat 1-12, Rest 0-9,
 *       Tag-Zehner 0-3) - ohne Tastendruck, ein Schritt je 150 ms Maschinenzeit [U6].  Die Stelle blinkt nicht.
 *  [U6] Schrittdauer 150 ms (6,7/s): unbelegt (der Treiber pollt nur); beeinflusst die Dauer des Stellens, nicht das
 *       Ergebnis.  Absichtlich KEIN ganzzahliges Vielfaches des Sekundentakts: die leere Stundenzehnerstelle
 *       liest der Treiber beim Stellen nur bei gesetztem Sekundentakt als 0 — mit einem ganzzahligen Vielfachen waere
 *       der Rundlauf der Stunde phasenstarr, und Stunde 1-9 liesse sich je nach Startphase nie einstellen.
 *  [U7] Nicht aufgefuehrte Tasten (HT in Stundenkorrektur, ST, ...) bleiben ohne Wirkung.
 *  [U8] FEF3 und gerade Adressen antworten nicht (offener Bus); Schreiben ausser FEF1 wird verworfen.
 *  [U9] Zeit-Korrektur uebernimmt Stunde/Minute; die Sekunden sind 0 (HT in der Sekunden-Stufe),
 *       ein Stellen ohne diesen HT behaelt die eingefrorenen Sekunden.
 *  [U10] Stellenpaar-Haltung: beim Lesen der Einerstelle wird die zugehoerige Zehnerstelle mit festgehalten
 *       und bei der folgenden Zehnerlesung (binnen 400 Takten = 100 us) herausgegeben.  Die echte Anzeige wechselt
 *       beide Stellen im selben Augenblick; die Treiber lesen nacheinander und wuerden sonst beim Umlauf einen
 *       zerrissenen Wert sehen (59 -> 9, Monat 12 -> 2, Stunde 9 -> 19).
 * Save-State: Abschnitt-Kennung 0x82 (0x81 = R2-Fassung mit Versatz, ladbar; der RTC-72421 nutzt 1/2, die Typen laden sich nicht gegenseitig).
 */
class P8000RtcU130 final : public P8000RtcBasis {
public:
    explicit P8000RtcU130(const P8000RtcConfig& cfg);
    bool lesen(uint16_t port, uint8_t& d) override;
    bool schreiben(uint16_t port, uint8_t d) override;
    void takte(int n) override;
    P8000RtcTyp typ() const override { return P8000RtcTyp::U130X; }
    std::string text() const override;
    void serialize(std::vector<uint8_t>& out) const override;
    bool deserialize(const uint8_t*& p, const uint8_t* end) override;

    static constexpr uint16_t BASIS = 0xFEF1;   ///< Tasten (schreiben) / Kennung (lesen)
    static constexpr uint8_t KENNUNG = 0xBF;

    /// Anzeigezustand (Tests, Debugger).
    enum class Z : uint8_t { Zeit, Datum, KMonat, KTagZ, KTagE, KJahrZ, KJahrE, ZSek, ZVor, KStd, KMinZ, KMinE, Halt, ANZAHL };
    Z zustand() const { return z_; }
    static const char* zustandName(Z z);

private:
    bool datumAnzeige() const { return z_ == Z::Datum || (z_ >= Z::KMonat && z_ <= Z::KJahrE); }
    bool korrDatum() const { return z_ >= Z::KMonat && z_ <= Z::KJahrE; }
    bool korrZeit() const { return z_ >= Z::KStd && z_ <= Z::KMinE; }
    bool eingefroren() const { return z_ >= Z::ZSek && z_ <= Z::Halt; }
    void ziffern(int* pos) const;                ///< sechs Stellen (0-9, -1 = leer)
    uint8_t lesenStelle(int idx);
    void taste(uint8_t neu);
    void ht();
    void vt(bool htGehalten);
    void schritt();
    void datumUebernehmen();
    void ladeDatumStell();
    void ladeZeitStell();
    bool com() const { return ((total_ / com_takte_) & 1) != 0; }

    Z z_ = Z::Zeit;
    uint8_t tasten_ = 0;
    uint64_t total_ = 0;                         ///< Maschinentakte seit Anlegen
    uint64_t com_takte_ = 4000;                  ///< halbe COM-Periode [U2]
    uint64_t schritt_takte_ = 500000;            ///< [U6]
    uint64_t datum_bis_ = 0, schritt_bis_ = 0;
    uint8_t mo_ = 1, dz_ = 0, de_ = 1, jz_ = 0, je_ = 0;   ///< Stellstellen Datum (Monat 1-12, Ziffern)
    uint8_t std_ = 0, mz_ = 0, me_ = 0;                    ///< Stellstellen Zeit
    int64_t frozen_ = 0;                         ///< Wandzeit der stehenden Uhr
    int zehner_idx_ = -1, zehner_ = -1;          ///< festgehaltener Zehner [U10] (nicht im Save-State)
    uint64_t zehner_stempel_ = 0;
};
