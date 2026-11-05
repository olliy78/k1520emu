/**
 * @file nadeldruckwerk.h
 * @brief Nadeldruckwerk: Wagen + Nadelkopf + Papierweg(e) + Blatt(er) hinter EINEM Gesicht (Entwurf 49 §4.3).
 *
 * Gehoert einer Steuerung (SD 1157, spaeter K6316).  Die Steuerung gibt Wagen-/Papiervorgaben als Pegel rein,
 * liest Rueckmeldungen und loest den Nadelanschlag aus; was gedruckt wird, entsteht hier aus Position und
 * Papierlage.  Kein Bezug auf ein Geraet.  Die Oberflaeche liest nur `blatt()`, `kopfX()`, `papierY()`.
 *
 * **Blattkoordinaten:** x = Wagenposition in Takten mal `x_je_takt` (plus Flugzeit), y = `rand_oben_y + lage * pv_schritt_y` des
 * Papierwegs plus Nadelversatz.  Die Papierlage 0 ist die des Einschaltens.
 */

#pragma once
#include "core/peripherals/druckwerk/blattmodell.h"
#include "core/peripherals/druckwerk/nadelkopf.h"
#include "core/peripherals/druckwerk/nadeldruck_parameter.h"
#include "core/peripherals/druckwerk/papierweg.h"
#include "core/peripherals/druckwerk/wagen.h"
#include <memory>
#include <vector>

class Nadeldruckwerk {
public:
    /// @p papier je Bahn ein Parametersatz (Laenge = Zahl der Bahnen in @p p, fehlende = Vorgabe).
    Nadeldruckwerk(const NadeldruckParameter& p, std::unique_ptr<Wagen> wagen,
                   const Papierweg::Parameter& papier = Papierweg::Parameter{});

    /// Netz ein: Wagen auf @p wagen_pos, Papier auf Lage 0, leere Blaetter.
    void netzEin(int wagen_pos);
    /// /RESET: Antriebe halten an; Position, Papierlage und Blatt bleiben.
    void halt();
    /// @p ticks Maschinentakte; true = eine Rueckmeldung (Wagen oder Papierimpuls) hat gewechselt.
    bool takt(int ticks);

    /// Nadelanschlag an der aktuellen Wagenposition und Papierlage von Bahn 0 (bei 2 Bahnen: TODO WP6b).
    /// @return gesetzte Punkte
    int feuere(uint16_t muster, bool kopf_anliegend);

    Wagen&       wagen()       { return *wagen_; }
    const Wagen& wagen() const { return *wagen_; }
    Nadelkopf&   kopf()        { return kopf_; }
    Papierweg&       papier(int bahn)       { return papier_[bahn & (bahnen() - 1)]; }
    const Papierweg& papier(int bahn) const { return papier_[bahn & (bahnen() - 1)]; }
    Blattmodell&       blatt(int bahn)       { return *blatt_[bahn & (bahnen() - 1)]; }
    const Blattmodell& blatt(int bahn) const { return *blatt_[bahn & (bahnen() - 1)]; }
    int bahnen() const { return static_cast<int>(papier_.size()); }
    const NadeldruckParameter& parameter() const { return p_; }

    int kopfX() const { return wagen_->position(); }                       ///< in Takten
    int kopfXPixel() const { return wagen_->position() * p_.x_je_takt; }   ///< in Blattpixeln
    /// Blattzeile der Nadel 1 bei der aktuellen Papierlage der Bahn.
    int papierY(int bahn) const { return p_.rand_oben_y + papier(bahn).lage() * p_.pv_schritt_y; }
    void blattLeeren();

private:
    void blattStrecken(int bahn);

    NadeldruckParameter p_;
    std::unique_ptr<Wagen> wagen_;
    Nadelkopf kopf_;
    std::vector<Papierweg> papier_;
    std::vector<std::unique_ptr<Blattmodell>> blatt_;
};
