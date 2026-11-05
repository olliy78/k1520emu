/**
 * @file wagen.h
 * @brief Schnittstelle des Druckwagens (Entwurf 49 §4.3): Eingaenge der Steuerung rein, Rueckmeldungen raus.
 *
 * Zwei Antriebe, ein Kopf: der SD 1157 hat einen **geregelten** Wagen (Firmware gibt Lauf/Richtung/Tempo vor,
 * die Hardware regelt und meldet LAUF und Takte; @ref GeregelterWagen), der K6316 einen **Schrittmotor** (die
 * Firmware schaltet Phasen selbst und liest die Taktscheibe /G1; `SchrittWagen`, noch nicht gebaut).  Darum
 * traegt @ref WagenEingang BEIDE Arten von Vorgaben und @ref WagenRueckmeldung beide Arten von Rueckmeldung;
 * jeder Wagen wertet nur seine aus.  Ein `SchrittWagen` braucht also keinen Umbau der Schnittstelle:
 * Phasen rein (`phasen`), /G1 raus (`g1`).
 *
 * Alles in **Maschinentakten** (CPU-Takt des Geraets).  Die Position zaehlt in **Takten der Taktscheibe**;
 * 1 Takt = ein Pixel des Blatts in x-Richtung (@ref NadeldruckParameter::takte_je_zoll).
 */

#pragma once
#include <cstdint>

/// Vorgaben der Steuerung an den Antrieb (Pegel).
struct WagenEingang {
    bool    lfg = false;         ///< SD 1157: Lauffreigabe (2A.6)
    bool    vorwaerts = true;    ///< SD 1157: Richtung VW (2A.4), 1 = vorwaerts (nach rechts)
    bool    schnell = false;     ///< SD 1157: VTAB (2A.2), 1 = Tabulatorgeschwindigkeit
    uint8_t phasen = 0;          ///< K6316: Phasenbits des Schrittmotors (nur SchrittWagen)
};

/// Rueckmeldungen des Antriebs und seiner Sensoren, wie die Firmware sie als Pegel liest.
struct WagenRueckmeldung {
    bool lauf = false;                       ///< SD 1157: LAUF (2A.5)
    bool takt_vor = false, takt_rueck = false;   ///< SD 1157: Takt vorwaerts/rueckwaerts (aktiv high)
    bool g1 = false;                         ///< K6316: Taktscheibe /G1 (nur SchrittWagen)
    bool vk = true, lr = true, rr = true;    ///< /VK, /LR, /RR (0 = aktiv)
    bool operator==(const WagenRueckmeldung& o) const {
        return lauf == o.lauf && takt_vor == o.takt_vor && takt_rueck == o.takt_rueck && g1 == o.g1 &&
               vk == o.vk && lr == o.lr && rr == o.rr;
    }
    bool operator!=(const WagenRueckmeldung& o) const { return !(*this == o); }
};

class Wagen {
public:
    virtual ~Wagen() = default;
    /// Wagen steht still, Eingaenge aus; die Position bleibt (ein /RESET versetzt den Wagen nicht).
    virtual void halt() = 0;
    /// Netz ein: Position setzen, alles zurueck.
    virtual void netzEin(int position) = 0;
    /// Neue Eingaenge, sofort wirksam (LAUF binnen Mikrosekunden).
    virtual void eingang(const WagenEingang& e) = 0;
    /// @p ticks Maschinentakte fortschreiben; true = eine Rueckmeldung hat gewechselt.
    virtual bool takt(int ticks) = 0;
    virtual WagenRueckmeldung rueckmeldung() const = 0;
    /// Position in Takten der Taktscheibe (ganzzahlig, wie die Firmware sie mitzaehlt).
    virtual int  position() const = 0;
    /// +1 = vorwaerts (nach rechts), -1 = rueckwaerts: Richtung der letzten Bewegung.
    virtual int  richtung() const = 0;
    virtual bool laeuft() const = 0;
};
