/**
 * @file papierweg.h
 * @brief Papiervorschub einer Bahn: Schritte je Richtung, Papierende, Rueckmeldeimpulse (Entwurf 49 §4.3).
 *
 * Hardware-Schrittgeber mit Rueckmeldung (hardware.md §10.2): solange die Steuerung **Freigabe** gibt, liefert
 * der Geber Impulse, je Impuls laeuft das Papier einen Schritt in der vorgegebenen **Richtung**; die Steuerung
 * zaehlt sie mit.  Der Impulsabstand haengt vom Hochlauf-Pegel ab (Anlauf langsamer als Lauf).  Bleibt ein
 * Impuls aus, ist das Sache der Steuerung (Waechter) — nicht des Papierwegs.
 *
 * Ein Schritt wird mit dem **Beginn** des Impulses vollzogen.  Alle Zeiten in Maschinentakten.
 * Kein Bezug auf ein Geraet; die Impulsform (aktiv low o. ae.) legt die Steuerung fest.
 */

#pragma once
#include <cstdint>

class Papierweg {
public:
    struct Parameter {
        /// Impulsabstand bei Hochlauf-Pegel = 1 (Lauf): 2...4 ms -> 3 ms = 7373 T bei 2,4576 MHz [?] hardware.md §15 Nr. 13
        int abstand_schnell = 7373;
        /// Impulsabstand bei Hochlauf-Pegel = 0 (Anlauf): 4...5 ms -> 4,5 ms = 11059 T [?]; beide < 7,08 ms (Waechter)
        int abstand_anlauf = 11059;
        /// Impulsbreite: >= 10 us = 25 T verlangt, 50 T gewaehlt [?] hardware.md §15 Nr. 13
        int impuls = 50;
        /// Papierlaenge in Schritten ab Lage 0; < 0 = unbegrenzt.  Bei Lage >= Laenge ist das Papier zu Ende.
        int laenge = -1;
    };

    Papierweg() : Papierweg(Parameter{}) {}
    explicit Papierweg(const Parameter& p) : p_(p) {}

    /// Netz ein: Lage 0, Zaehler leer, keine Freigabe.
    void netzEin();
    /// /RESET: Freigabe weg, Impuls laeuft aus; das Papier bleibt, wo es ist.
    void halt();

    /// Vorgaben der Steuerung (Pegel): Freigabe (XPFG), Richtung vorwaerts (XPWV), Hochlauf (FRH).
    void ausgaenge(bool freigabe, bool vorwaerts, bool schnell);
    /// @p ticks Takte; true = der Impulspegel hat gewechselt.
    bool takt(int ticks);

    bool impuls() const { return puls_ > 0; }          ///< 1 waehrend eines Rueckmeldeimpulses
    int  lage() const { return lage_; }                ///< Schritte netto, vorwaerts = +
    uint32_t schritteVor() const { return n_vor_; }
    uint32_t schritteRueck() const { return n_rueck_; }
    uint32_t schritte() const { return n_vor_ + n_rueck_; }

    // Papierende (Sensor /PE)
    bool papierDa() const { return !ende_erzwungen_ && (p_.laenge < 0 || lage_ < p_.laenge); }
    void setzePapierEnde(bool ende) { ende_erzwungen_ = ende; }   ///< Anwender: Papier entnommen/leer
    void setzeLaenge(int schritte) { p_.laenge = schritte; }
    const Parameter& parameter() const { return p_; }

private:
    Parameter p_;
    bool frei_ = false, vor_ = true, schnell_ = false, ende_erzwungen_ = false;
    int  akku_ = 0, puls_ = 0, lage_ = 0;
    uint32_t n_vor_ = 0, n_rueck_ = 0;
};
