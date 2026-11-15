/**
 * @file drucker_rom.h
 * @brief Test-Hilfe: ROM-Saetze der Drucker aus dem Katalog `data/drucker_roms.yaml` (WP9, Entwurf 49 §12).
 *
 * Die Karten haben kein eingebettetes Abbild mehr; Tests, die eine Karte (nicht die PrinterMachine) direkt bauen,
 * holen sich hier die Bytes.  Ein fehlender/fehlerhafter Katalog wirft mit der Meldung des Lesers.
 */
#pragma once
#include <stdexcept>
#include <string>
#include <vector>

#include "core/machines/printer/drucker_rom_katalog.h"

namespace k1520test {

/// Bytes des Katalogeintrags (Schluessel oder Alias) des Typs; ohne Schluessel die Vorgabe des Typs.
inline std::vector<uint8_t> druckerRom(const std::string& typ, const std::string& schluessel = "")
{
    const auto kat = DruckerRomKatalog::gemeinsam();
    if (!kat->ok()) throw std::runtime_error(kat->fehler());
    const DruckerRomEintrag* e = schluessel.empty() ? kat->vorgabe(typ) : kat->finde(typ, schluessel);
    if (!e || !e->daten) throw std::runtime_error("Drucker-ROM-Katalog: kein Eintrag " + typ + "/" + schluessel);
    return *e->daten;
}

}  // namespace k1520test
