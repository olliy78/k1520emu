/**
 * @file geregelter_wagen.h
 * @brief Geregelter Druckwagen des SD 1157 (Gleichstromantrieb mit Lage- und Geschwindigkeitsrueckmeldung).
 *
 * Verhalten nach doc/printer/sd1157/hardware.md §10.1/§15 (Nr. 9-12):
 *  - **LFG 0 -> 1: LAUF sofort 1**, der Wagen laeuft in Richtung VW (die Richtung folgt VW, solange LFG = 1);
 *    je Position ein Impuls auf Takt vorwaerts/rueckwaerts (aktiv high).  Abstand nach VTAB.
 *  - **LFG 1 -> 0:** der Wagen laeuft noch den Bremsweg aus (Richtung bleibt), dann LAUF = 0.  Takte zaehlen
 *    waehrend des Auslaufs weiter.  Eine LFG-Spitze von Mikrosekunden bewegt den Wagen also um den vollen
 *    Bremsweg — die Firmware setzt beim Einschalten genau eine solche Spitze (§11 Nr. 6).
 *  - **Bremsweg** ~ v^2: 20 Takte bei Druckgeschwindigkeit, 70 bei VTAB = 1.  Das sind die Zahlen, die die
 *    Firmware selbst benutzt (LFG = 0 etwa 2 Spalten bzw. 7 Spalten vor dem Ziel, §10.1) und
 *    das Verhaeltnis passt zu (1365/700)^2 = 3,8.
 *  - Randsensoren als Pegel der Position; weiche Anschlaege: der Wagen bleibt dort stehen.
 *
 * **Randbereich:** /VK (Vorwarnkontakt) deckt den ganzen /LR-Bereich ab (`vk_bis >= lr_bis`) — sonst findet die
 * Anfahrt aus dem Randbereich nie die /LR-Flanke (Ableitung in hardware.md §10.1 / merkposten/drucker.md).
 * Kein Sonderfall im Wagen: das Verhalten folgt aus Position und Sensorlage.
 */

#pragma once
#include "core/peripherals/druckwerk/wagen.h"

class GeregelterWagen : public Wagen {
public:
    struct Parameter {
        int lr_bis = 76;          ///< /LR = 0 fuer Position <= 76 [Q:1239-1240] hardware.md §10.1
        int rr_ab = 1420;         ///< /RR = 0 ab Position 1420 (schmal) bzw. 2200 (breit) hardware.md §10.1
        /// /VK = 0 fuer Position <= vk_bis.  [?] hardware.md §15 Nr. 11 nannte 10; abgeleitet: >= lr_bis, sonst
        /// laeuft die Grundstellung aus Positionen 11...76 bis zum Anschlag (Bremsweg 70 bei VTAB = 1).
        int vk_bis = 100;
        int pos_min = -200;       ///< weiche Anschlaege, kein NMI (§15 Nr. 12)
        int pos_max = 2400;
        /// Takt-Abstand bei Druckgeschwindigkeit: 180 Z/s -> 1800 Takte/s [?] -> 2457600/1800 (§10.1)
        int abstand_druck = 1365;
        /// Takt-Abstand bei VTAB = 1: >= 600 T verlangt, 700 gewaehlt [?] (§15 Nr. 9)
        int abstand_tab = 700;
        int bremsweg_druck = 20;  ///< Takte, [?] 8...70 (§15 Nr. 10)
        int bremsweg_tab = 70;    ///< Takte, [?] 8...70 (§15 Nr. 10)
        /// Breite der Takt-Impulse: >= 1 us = 3 T verlangt (§15 Nr. 9); 25 T gewaehlt [?]
        int takt_impuls = 25;
    };

    GeregelterWagen() : GeregelterWagen(Parameter{}) {}
    explicit GeregelterWagen(const Parameter& p) : p_(p) {}

    void halt() override;
    void netzEin(int position) override;
    void eingang(const WagenEingang& e) override;
    bool takt(int ticks) override;
    WagenRueckmeldung rueckmeldung() const override;
    int  position() const override { return pos_; }
    int  richtung() const override { return richtung_; }
    bool laeuft() const override { return laeuft_; }
    const Parameter& parameter() const { return p_; }

private:
    Parameter p_;
    WagenEingang e_;
    int  pos_ = 0, richtung_ = +1;
    bool laeuft_ = false;
    int  bremsrest_ = -1;     ///< -1 = LFG aktiv, sonst verbleibende Positionen bis Stillstand
    int  akku_ = 0;
    int  puls_vor_ = 0, puls_rueck_ = 0;
};
