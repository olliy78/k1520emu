/**
 * @file nadelkopf.h
 * @brief Nadelkopf: Nadelmuster + Wagenposition + Papierlage ergibt Punkte (Entwurf 49 §4.3).
 *
 * Gemeinsam fuer alle Nadeldrucker, **ohne Bezug auf ein Geraet**: wer feuert, gibt das Muster (Bit k = Nadel
 * k+1 schlaegt an), die Wagenposition in Takten, die Papierlage in Rasterzeilen und die Richtung.  Die
 * Nadeln stehen untereinander (Nadel 1 oben); Teilung, Lage der Nadel 1, Punktmass und ein Versatz je
 * Druckrichtung stehen im Parametersatz.
 */

#pragma once
#include "core/peripherals/druckwerk/blattmodell.h"
#include "core/peripherals/druckwerk/nadeldruck_parameter.h"
#include <cstdint>

class Nadelkopf {
public:
    static constexpr int NADELN = 9;

    explicit Nadelkopf(const NadeldruckParameter& p) : p_(p) {}

    /**
     * @brief Ein Anschlag aller gesetzten Nadeln.
     * @param muster    Bit k = Nadel k+1 schlaegt an (Bit 0..8)
     * @param x         Wagenposition in Takten (das Blatt hat x_je_takt Pixel je Takt; die Flugzeit verschiebt je Richtung)
     * @param y_papier  Zeile der Papierlage (Nadel 1 sitzt bei y_papier + nadel1_y)
     * @param richtung  +1 vorwaerts, -1 rueckwaerts (waehlt den x-Versatz)
     * @param anliegend false = Kopf nicht angeschwenkt: nichts erreicht das Papier (die Punkte werden gezaehlt)
     * @return gesetzte Punkte (ausserhalb des Blatts Verworfene zaehlen nicht)
     */
    int feuere(uint16_t muster, int x, int y_papier, int richtung, bool anliegend, Blattmodell& blatt)
    {
        return feuerePixel(muster, x * p_.x_je_takt, y_papier, richtung, anliegend, blatt);
    }
    /// Wie @ref feuere, die Wagenposition schon in Blattpixeln (Schrittmotor-Wagen: Lage zwischen den Schritten).
    int feuerePixel(uint16_t muster, int x_pixel, int y_papier, int richtung, bool anliegend, Blattmodell& blatt);

    uint64_t anschlaege() const { return anschlaege_; }       ///< Aufrufe mit mindestens einer Nadel
    uint64_t punkteGesetzt() const { return gesetzt_; }
    uint64_t abgewiesen() const { return abgewiesen_; }       ///< Nadeln bei nicht angeschwenktem Kopf
    void zaehlerLoeschen() { anschlaege_ = gesetzt_ = abgewiesen_ = 0; }

private:
    NadeldruckParameter p_;
    uint64_t anschlaege_ = 0, gesetzt_ = 0, abgewiesen_ = 0;
};
