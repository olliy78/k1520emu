/**
 * @file ausgabe_backend.h
 * @brief Innere Schnittstelle: ein Wirts-Tongeraet (nur fuer TonAusgabe und die Backends).
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

namespace k1520::audio {

class AusgabeBackend {
public:
    virtual ~AusgabeBackend() = default;

    /// Geraet oeffnen (48 kHz, S16, Stereo).  Bei Fehler false + Grund (ein Satz).
    virtual bool oeffnen(std::string& fehler) = 0;

    /// Einen Block schreiben.  DARF blockieren (das ist der Takt des Ausgabefadens),
    /// soll dabei aber hoechstens etwa einen Pufferinhalt lang warten.
    virtual void schreibe(const int16_t* lr, std::size_t frames) = 0;

    /// Geraet schliessen (idempotent).
    virtual void schliessen() = 0;

    /// Hoert man etwas?  `false` fuer das Stumm-Backend.
    virtual bool hoerbar() const = 0;
};

std::unique_ptr<AusgabeBackend> neuesBackendStumm();
#if !defined(_WIN32)
/// @param bibliothek Name fuer dlopen (leer = "libasound.so.2")
std::unique_ptr<AusgabeBackend> neuesBackendAlsa(const std::string& bibliothek);
#else
std::unique_ptr<AusgabeBackend> neuesBackendWaveOut();
#endif

}  // namespace k1520::audio
