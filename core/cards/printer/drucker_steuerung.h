/**
 * @file drucker_steuerung.h
 * @brief Gemeinsame Schnittstelle der Druckersteuerungen (SD 1157, K6316) zur `PrinterMachine` (Entwurf 49 §4.1).
 *
 * Eine Steuerungsklasse je Geraet (Entwurf 49 §3 Nr. 4); die Maschine kennt nur das, was beide gemeinsam haben:
 * CPU, Lebenslauf, Takt, Druckwerk, serielle Anschluesse nach aussen.  Tasten, Schalter und Lampen sind
 * geraetespezifisch und stehen in den Klassen selbst (die C-ABI verzweigt nach dem Typ).
 */

#pragma once
#include "core/bus/k1520_bus.h"
#include "core/peripherals/druckwerk/nadeldruckwerk.h"
#include "core/primitives/z80.h"
#include "core/serial/anschluss.h"
#include <cstdint>
#include <functional>

class DruckerSteuerung {
public:
    virtual ~DruckerSteuerung() = default;

    virtual Z80& cpu() = 0;
    virtual void attachToBus(K1520Bus& bus) = 0;
    virtual void powerOn() = 0;
    virtual void reset() = 0;
    /// Takte fortschreiben; true = Interruptzustand kann sich geaendert haben.
    virtual bool clockTick(int ticks) = 0;
    virtual Nadeldruckwerk& druck() = 0;
    virtual const Nadeldruckwerk& druck() const = 0;
    /// Die Steuerung als Interruptkette fuer den Bus (CTC/PIO/SIO stehen in ihr).
    virtual InterruptSlave& unterbrechung() = 0;

    // Serielle Schnittstellen nach aussen: je SIO-Kanal ein Anschluss (IFSS und V.24 gibt es immer).
    virtual k1520::serial::SerialAnschluss& serialAnschluss(bool v24) = 0;
    virtual void setzeHubAktiv(bool aktiv) = 0;
    virtual bool nimmSeriellGeaendert() = 0;
    virtual void serialAbnehmer(bool v24, std::function<void(uint8_t)> cb) = 0;
    virtual void serialEinspeisen(bool v24, uint8_t byte) = 0;
};
