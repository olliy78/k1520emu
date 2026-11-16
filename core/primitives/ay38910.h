/**
 * @file ay38910.h
 * @brief AY-3-8910 / AY-3-8912 Tongenerator (PSG) - Chipmodell nach Datenblatt
 *
 * Eigener Nachbau nach dem Datenblatt (Entwurf 51, Abschnitt 3.5/4.1); kein fremder Emulatorcode.
 *
 * Zeitmodell: Der Chip rechnet in ABSOLUTEN Takten seines eigenen Takts (Eingangstakt, z. B. 1789772 Hz).
 * Intern gibt es ein Raster von Takt/8 (ein "Rasterschritt"):
 *  - Tonzaehler zaehlen jeden Rasterschritt und kippen bei Erreichen der Periode TP
 *    -> Tonfrequenz = f / (16 * TP), Periode 0 wirkt wie 1;
 *  - Rauschzaehler zaehlen jeden 2. Rasterschritt (Takt/16), Schiebetakt = f / (16 * NP)
 *    (17-Bit-LFSR, Rueckkopplung Bit0 XOR Bit3, Periode 2^17-1);
 *  - Huellkurve: ein Schritt alle EP Takt/16-Zaehlungen = 16 * EP Takte, 16 Stufen
 *    -> Huellkurvenperiode 256 * EP / f.
 *
 * "Lazy catch-up": rechneBis(takt) holt den Chip bis zum Zeitpunkt nach.  JEDER Registerzugriff
 * mit Zeitangabe (datenSchreiben(d, takt)) rechnet vorher nach, danach aendert sich das Register.
 * Die Variante ohne Zeitangabe wirkt zum zuletzt berechneten Zeitpunkt - der Aufrufer ruft dann
 * vorher selbst rechneBis().
 *
 * Ausgabe: je Kanal A/B/C Mono-Abtastwerte 0..1 (keine Stereomischung - die macht die Karte).
 * ausgabeEin(rate) schaltet die Erzeugung ein: je Ausgabewert wird ueber ALLE Rasterschritte
 * gemittelt (Kastenfilter), davor optional ein Tiefpass auf dem Raster (Gegenmittel gegen
 * Spiegeltoene).  ausgabeAus() = "nur Register nachfuehren", ohne Abtastwerte (schnelles Tempo).
 */
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <deque>

class Ay38910 {
public:
    enum class Variante { AY8910, AY8912 };

    /// Ein Abtastwert je Kanal, 0..1.
    struct Abtastwert { float a, b, c; };

    /// Pegeltabelle (16 Stufen, Stufe 0 = 0, Stufe 15 = 1.0).
    using Pegeltabelle = std::array<float, 16>;

    /// Vorgabe: logarithmisch, ca. 3 dB je Stufe (Faktor 1/sqrt(2)), Stufe 0 = still.
    /// Herkunft: Datenblatt-Kennlinie (Amplitude ~ 2^(-(15-n)/2)); bewusst leicht austauschbar
    /// (setzePegeltabelle), die echte Karte wird spaeter per Gehoer verglichen.
    static Pegeltabelle vorgabePegel();

    explicit Ay38910(Variante v = Variante::AY8910, uint32_t takt = 1789772);

    /// /RESET: alle Register 0, Zaehler/Huellkurve zurueck, Latch 0.  Zeit und Ausgabe-Einstellung bleiben.
    void reset();

    // --- Busschnittstelle (wie /BDIR, BC1 dekodiert von der Karte) ---
    void adresseSchreiben(uint8_t a);           ///< Latch; >= 16 = abgewaehlt
    void datenSchreiben(uint8_t d);             ///< zum zuletzt berechneten Zeitpunkt
    void datenSchreiben(uint8_t d, uint64_t chipTakt);  ///< erst rechneBis(chipTakt), dann schreiben
    uint8_t datenLesen() const;                 ///< abgewaehlt: 0xFF

    uint8_t latch() const { return latch_; }
    uint8_t reg(int n) const { return reg_[n & 15]; }   ///< Rohwert (ohne Lesemaske)

    // --- Zeit ---
    void rechneBis(uint64_t chipTakt);
    uint64_t takt() const { return rasterNr_ * 8; }     ///< bis hierher berechnet (Vielfaches von 8)
    uint32_t chipTakt() const { return takt_; }

    // --- E/A-Ports ---
    void setzePortEingang(int port, uint8_t wert) { pin_[port & 1] = wert; }  ///< 0 = A, 1 = B
    /// Pegel an den Pins: Ausgang = Registerwert, Eingang = Pull-up (FFH).
    uint8_t portPins(int port) const;

    // --- Ausgabe ---
    /// rate = Ausgaberate in Hz (bezogen auf die Chipzeit), tiefpassHz = Grenzfrequenz des
    /// Vorfilters (zwei einpolige Stufen) auf dem Raster; 0 = aus.
    void ausgabeEin(uint32_t rate, float tiefpassHz = 0.0f);
    void ausgabeAus();
    /// Chiptakt im Lauf umstellen (JP2).  Chipzeit (rasterNr_) bleibt monoton; die Ausgabewert-Grenzen werden auf
    /// die neue Rasterdauer neu ausgerichtet (angefangener Wert verfaellt), Filterzustaende bleiben.
    void setzeTakt(uint32_t takt);
    /// Chipvariante im Lauf wechseln: Register bleiben, nur die Port-B-Pins entfallen/erscheinen (8912 ohne Port B).
    void setzeVariante(Variante v) { variante_ = v; }
    bool ausgabeAktiv() const { return rate_ != 0; }
    size_t abholbereit() const { return fertig_.size(); }
    /// Holt bis zu max fertige Abtastwerte ab; liefert die Anzahl.
    size_t abholen(Abtastwert* ziel, size_t max);
    void setzePegeltabelle(const Pegeltabelle& t) { pegel_ = t; }

    Variante variante() const { return variante_; }
    int huellkurvePegel() const { return hkPegel_; }   ///< aktuelle Stufe 0..15 (Test/Diagnose)

private:
    void rasterSchritt();
    void huellkurveStart();
    void huellkurveSchritt();
    void abtasten();
    uint8_t pegelKanal(int k) const;

    Variante variante_;
    uint32_t takt_;
    uint8_t latch_ = 0;
    std::array<uint8_t, 16> reg_{};
    std::array<uint8_t, 2> pin_{{0xFF, 0xFF}};

    uint64_t rasterNr_ = 0;
    // Ton
    std::array<uint32_t, 3> tonZ_{};
    std::array<uint8_t, 3> tonBit_{};
    // Rauschen
    uint32_t rauschZ_ = 0;
    uint32_t lfsr_ = 1;
    uint8_t rauschBit_ = 1;
    bool halbTakt_ = false;   ///< Raster-Paritaet fuer Takt/16-Zaehler
    // Huellkurve
    uint32_t hkZ_ = 0;
    int hkStufe_ = 0;          ///< 0..15 innerhalb des Durchlaufs
    bool hkAuf_ = false;
    bool hkHalt_ = false;
    int hkHaltPegel_ = 0;
    int hkPegel_ = 0;

    Pegeltabelle pegel_;

    // Ausgabe
    uint32_t rate_ = 0;
    float tpKoef_ = 0.0f;
    float tpHz_ = 0.0f;   ///< Vorfilter-Grenzfrequenz (fuer setzeTakt)
    std::array<std::array<float, 2>, 3> tpZ_{};
    std::array<double, 3> summe_{};
    uint64_t abtastNr_ = 0;
    std::deque<Abtastwert> fertig_;
};

