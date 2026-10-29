/**
 * @file sios_check.cpp
 * @brief Schnellpruefung des SIOS-1526-Dateisystems (nur Etiketten, Spur 0).
 *
 * Bewusst klein: ein paar HARTE Konsistenzpruefungen, die an den fuenf Originalabbildern
 * keinen Befund ergeben.  „Falschmeldungen sind schlimmer als fehlende" - alles, was das
 * Formatdokument nicht belegt (ERMAP-Nummern, VOL1-Felder, Bibliotheksfuss NV), wird nicht
 * geprueft.  Reparaturen gibt es keine (die Diskette ist lesend).
 *
 * | Kennung | Schwere | Bedeutung |
 * |---|---|---|
 * | `sios.hdr1.ungueltig` | Fehler | BOE/EOE keine Adressen, in Spur 0, EOE vor BOE oder hinter der Diskette |
 * | `sios.hdr1.ueberlappt` | Fehler | zwei aktive Bereiche teilen sich Sektoren |
 * | `sios.hdr1.eod` | Warnung | EOD fehlt oder liegt ausserhalb des Bereichs |
 *
 * @see doc/sios_diskettenformat.md §4
 */

#include "core/filesystem/check/fs_check.h"
#include "core/filesystem/sios/sios_fs.h"

#include <string>
#include <vector>

FsCheckReport SiosFileSystem::check(FsCheckLevel level, bool nachladen) const {
    (void)nachladen;     // Spur 0 wurde beim Mounten gelesen
    FsCheckReport bericht;
    bericht.level = level;
    FsFindings b(bericht);
    bericht.spuren_gelesen = bericht.spuren_gesamt = 1;

    bericht.schritt("sios.schritt.etiketten",
                    "Dateikennsaetze (HDR1): Adressen, Bereiche und Ueberlappung");
    const std::vector<SiosEtikett> et = etiketten();
    for (const SiosEtikett& e : et) {
        const std::string obj = e.name.empty() ? "Sektor " + std::to_string(e.sektor) : e.name;
        if (!e.gueltig) {
            b.addAt("sios.hdr1.ungueltig", FsSeverity::Fehler, FsLayer::Verwaltung, obj,
                    "Das Etikett in Sektor " + std::to_string(e.sektor) + ": " + e.problem,
                    0, 0, e.sektor);
            continue;
        }
        if (!e.problem.empty())
            b.addAt("sios.hdr1.eod", FsSeverity::Warnung, FsLayer::Verwaltung, obj,
                    "Das Etikett in Sektor " + std::to_string(e.sektor) + ": " + e.problem,
                    0, 0, e.sektor);
    }
    for (size_t i = 0; i < et.size(); ++i) {
        if (!et[i].gueltig) continue;
        for (size_t j = i + 1; j < et.size(); ++j) {
            if (!et[j].gueltig) continue;
            if (et[i].boe <= et[j].eoe && et[j].boe <= et[i].eoe)
                b.addAt("sios.hdr1.ueberlappt", FsSeverity::Fehler, FsLayer::Verwaltung,
                        et[i].name + " / " + et[j].name,
                        "Die Bereiche von '" + et[i].name + "' (" + et[i].boe_text + ".."
                        + et[i].eoe_text + ") und '" + et[j].name + "' (" + et[j].boe_text + ".."
                        + et[j].eoe_text + ") ueberlappen sich",
                        0, 0, et[i].sektor);
        }
    }

    bericht.sortieren();
    return bericht;
}
