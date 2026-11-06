/**
 * @file schrittwagen.h
 * @brief Druckwagen des K6316: Schrittmotor, den die Firmware per Phasenmuster selbst treibt (Entwurf 49 §4.3).
 *
 * Die Steuerung gibt die Phasen (/SW1 bis /SW4, @ref WagenEingang::phasen, Bit k = Phase k+1 bestromt) rein; die
 * Taktscheibe liefert /G1 (@ref WagenRueckmeldung::g1) zurueck.  6-Grad-Motor, 60 Vollschritte je Umdrehung, die
 * Taktscheibe hat 30 Schlitze (hardware.md §6 [H]): je Vollschritt wechselt /G1 einmal.  Mechanische Anschlaege
 * links und rechts halten den Rotor an (Firmware: blockierter Wagen = Havarie).
 *
 * **Einheit der Position:** Halbschritte (ein Halbschritt = 1/100 Zoll bei 0,508 mm je Vollschritt, Handbuch 2.1
 * Nr. 5: Grundteilung 50 Punkte/Zoll).  Ein Vollschritt sind zwei Einheiten, /G1 wechselt je zwei Einheiten.
 * Die Phasen wirken sofort (die Firmware sieht die Taktscheibe /G1 im selben Augenblick).  Die Kopflage fuer den Nadel-
 * anschlag (@ref positionPixel) liegt zwischen den Schritten: die Firmware feuert zwischen zwei Motorschritten (Drucktakt
 * = 3/4 Schritt beim Zeichenabstand 1/10"), der Rotor erreicht die neue Stellung gleichmaessig binnen eines Schrittabstands.
 */

#pragma once
#include "core/peripherals/druckwerk/phasenmotor.h"
#include "core/peripherals/druckwerk/wagen.h"

class SchrittWagen : public Wagen {
public:
    struct Parameter {
        int pos_min = 0;        ///< linker Anschlag (Halbschritte); der Wagen kann nicht weiter
        int pos_max = 1800;     ///< rechter Anschlag (12 Zoll Weg, Annahme [?])
        /// true: steigende Phasennummer = Wagen nach links.  Servicehandbuch Tabelle 1 (Synchronisation, Rueckwaerts
        /// gegen den linken Rand): /SW2+/SW3, /SW3+/SW4, /SW4+/SW1, /SW1+/SW2 — die Phasennummer steigt.
        bool umgekehrt = true;
    };

    SchrittWagen() : SchrittWagen(Parameter{}) {}
    explicit SchrittWagen(const Parameter& p) : p_(p) {}

    void halt() override { e_ = WagenEingang{}; }   // Phasen aus, Lage bleibt (Netz/RESET versetzen den Wagen nicht)
    void netzEin(int position) override;
    void eingang(const WagenEingang& e) override;
    bool takt(int ticks) override { t_ += ticks; return false; }
    int  positionPixel(int x_je_takt) const override;
    WagenRueckmeldung rueckmeldung() const override;
    int  position() const override { return pos_; }
    int  richtung() const override { return richtung_; }
    bool laeuft() const override { return false; }
    /// Rotor steht an einem Anschlag und das Feld wollte weiter (Schritte gingen verloren).
    bool blockiert() const { return blockiert_; }
    uint64_t schritte() const { return n_; }        ///< Zahl der Halbschritt-Aenderungen (auch am Anschlag verlorene)
    const Parameter& parameter() const { return p_; }

private:
    Parameter p_;
    WagenEingang e_;
    PhasenMotor motor_;
    int  pos_ = 0, richtung_ = +1;
    bool blockiert_ = false;
    uint64_t n_ = 0;
    // Lage zwischen den Schritten: der Rotor erreicht die neue Stellung nach einem Schrittabstand (gleichmaessige Fahrt)
    uint64_t t_ = 0;             ///< Maschinentakte seit Netz-Ein
    uint64_t t_schritt_ = 0;     ///< Zeitpunkt des letzten Schritts
    int      letzter_ = 0;       ///< dessen Verschiebung (Halbschritte, vorzeichenbehaftet)
    uint64_t abstand_ = 3300;    ///< letzter Schrittabstand (Takte), Vorgabe 750 Hz
};
