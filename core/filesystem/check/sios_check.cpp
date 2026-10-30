/**
 * @file sios_check.cpp
 * @brief Schnellpruefung des SIOS-1526-Dateisystems (nur Etiketten, Spur 0).
 *
 * Bewusst schlank: ein paar HARTE Konsistenzpruefungen, die an den fuenf Originalabbildern
 * keinen Befund ergeben.  „Falschmeldungen sind schlimmer als fehlende" - alles, was das
 * Formatdokument nicht belegt (ERMAP-Nummern, VOL1-Felder, Bibliotheksfuss NV), wird nicht
 * geprueft.  Reparaturen gibt es keine (die Diskette ist lesend).
 *
 * | Kennung | Schwere | Bedeutung |
 * |---|---|---|
 * | `sios.hdr1.ungueltig` | Fehler | BOE/EOE keine Adressen, in Spur 0, EOE vor BOE oder hinter der Diskette |
 * | `sios.hdr1.ueberlappt` | Fehler | zwei aktive Bereiche teilen sich Sektoren |
 * | `sios.hdr1.eod` | Warnung | EOD fehlt oder liegt ausserhalb des Bereichs |
 * | `sios.ermap.fehlt` | Fehler | Spur 0 Sektor 5 traegt kein `ERMAP` (nur nach dem Mounten veraendert) |
 * | `sios.vol1.fehlt` | Fehler | Spur 0 Sektor 7 traegt kein `VOL1` |
 * | `sios.bib.verzeichnis` | Warnung | Verzeichnissektor (AADR 2) einer Bibliothek nicht lesbar |
 * | `sios.bib.aadr` | Warnung | AADR eines Eintrags ausserhalb [19, ND) |
 * | `sios.bib.reihenfolge` | Warnung | AADR nicht aufsteigend |
 * | `sios.bib.doppelt` | Fehler | zwei benannte Objekte am selben AADR |
 * | `sios.bib.nd` | Warnung | Fuss-ND vor dem Datenteil oder hinter dem Ende der Bibliothek |
 * | `sios.marke.aktiv` | Hinweis | aktives HDR1 mit geloeschter Datenmarke F8 (Reparatur) |
 * | `sios.marke.frei` | Hinweis | freier/DDR1-Etikettensatz mit normaler Marke FB (Reparatur) |
 *
 * @see doc/sios_diskettenformat.md §4
 */

#include "core/filesystem/check/fs_check.h"
#include "core/filesystem/sios/sios_fs.h"

#include <algorithm>
#include <cstdio>
#include <set>
#include <string>
#include <vector>

namespace {
std::string hex4(int v) { char b[16]; std::snprintf(b, sizeof b, "%04X", v); return b; }
}  // namespace

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

    // ── VOL1 / ERMAP ─────────────────────────────────────────────────────────
    // Beim Mounten verlangt die Erkennung beide; ein Befund entsteht deshalb nur, wenn
    // die Etiketten NACH dem Mounten verschwinden (Tests, physische Diskette).
    bericht.schritt("sios.schritt.kopf", "Datentraegerkennsatz VOL1 und Fehlspurkarte ERMAP");
    if (etikettenText(5).compare(0, 5, "ERMAP") != 0)
        b.addAt("sios.ermap.fehlt", FsSeverity::Fehler, FsLayer::Verwaltung, "Sektor 5",
                "Spur 0 Sektor 5 traegt kein ERMAP", 0, 0, 5);
    if (etikettenText(7).compare(0, 4, "VOL1") != 0)
        b.addAt("sios.vol1.fehlt", FsSeverity::Fehler, FsLayer::Verwaltung, "Sektor 7",
                "Spur 0 Sektor 7 traegt kein VOL1 - der Datentraeger hat keinen Namen", 0, 0, 7);

    // ── Bibliotheken: Verzeichnis gegen Belegung ────────────────────────────
    bericht.schritt("sios.schritt.bibliotheken",
                    "Bibliotheksverzeichnisse: AADR, Reihenfolge, Fuss (ND)");
    for (const SiosEtikett& e : et) {
        if (!e.gueltig || e.satzlaenge != kSiosSektor || e.name.size() < 3
            || e.name.compare(e.name.size() - 2, 2, "LB") != 0)
            continue;
        const int gesamt = e.eoe - e.boe + 1;       // der ganze reservierte Bereich
        if (gesamt < 19) continue;                  // zu klein fuer eine Bibliothek: kein Befund
        const int dl = e.boe + 1;                   // AADR 2
        SectorData sd;
        if (!space_.readSector(static_cast<uint8_t>(dl / kSiosSektoren), 0,
                               static_cast<uint8_t>(dl % kSiosSektoren + 1), sd)
            || sd.data.size() != kSiosSektor) {
            b.addAt("sios.bib.verzeichnis", FsSeverity::Warnung, FsLayer::Verwaltung, e.name,
                    "Das Verzeichnis (AADR 2) von '" + e.name + "' ist nicht lesbar",
                    dl / kSiosSektoren, 0, dl % kSiosSektoren + 1);
            continue;
        }
        const int cyl = dl / kSiosSektoren, sek = dl % kSiosSektoren + 1;
        const std::vector<uint8_t>& d = sd.data;
        const int nd = d[122] | (d[123] << 8);
        if (nd < 19 || nd > gesamt + 1) {
            b.addAt("sios.bib.nd", FsSeverity::Warnung, FsLayer::Verwaltung, e.name,
                    "Der Verzeichnisfuss von '" + e.name + "' nennt ND " + hex4(nd)
                    + " (erwartet 0013 .. " + hex4(gesamt + 1) + ")", cyl, 0, sek);
            continue;      // ohne brauchbares ND ist ein Vergleich der AADR wertlos
        }
        struct Slot { int slot, aadr; bool benannt; };
        std::vector<Slot> slots;
        for (int slot = 0; slot < 15; ++slot) {
            const uint8_t* p = &d[static_cast<size_t>(slot) * 8];
            if (std::all_of(p, p + 8, [](uint8_t x) { return x == 0x03; })) continue;   // unbenutzt
            const bool benannt = std::any_of(p, p + 5, [](uint8_t x) { return x != 0 && x != 0x20; });
            slots.push_back({slot + 1, p[5] | (p[6] << 8), benannt});
        }
        int vorher = -1;
        for (const Slot& s : slots) {
            if (s.aadr < 19 || s.aadr >= nd)
                b.addAt("sios.bib.aadr", FsSeverity::Warnung, FsLayer::Verwaltung, e.name,
                        "Verzeichnisplatz " + std::to_string(s.slot) + " von '" + e.name
                        + "': AADR " + hex4(s.aadr) + " liegt ausserhalb des Datenteils 0013 .. "
                        + hex4(nd - 1), cyl, 0, sek);
            else if (vorher >= 0 && s.aadr < vorher)
                b.addAt("sios.bib.reihenfolge", FsSeverity::Warnung, FsLayer::Verwaltung, e.name,
                        "Verzeichnisplatz " + std::to_string(s.slot) + " von '" + e.name
                        + "': AADR " + hex4(s.aadr) + " steht vor dem des Vorgaengers "
                        + hex4(vorher), cyl, 0, sek);
            if (s.aadr >= 19 && s.aadr < nd) vorher = std::max(vorher, s.aadr);
        }
        // Geloeschte Eintraege (Name blank) zaehlen nicht: sie behalten AADR und ML (§7.3).
        for (size_t i = 0; i < slots.size(); ++i)
            for (size_t j = i + 1; j < slots.size(); ++j)
                if (slots[i].benannt && slots[j].benannt && slots[i].aadr == slots[j].aadr)
                    b.addAt("sios.bib.doppelt", FsSeverity::Fehler, FsLayer::Verwaltung, e.name,
                            "Die Verzeichnisplaetze " + std::to_string(slots[i].slot) + " und "
                            + std::to_string(slots[j].slot) + " von '" + e.name
                            + "' zeigen beide auf AADR " + hex4(slots[i].aadr), cyl, 0, sek);
    }

    // ── Datenmarken der Etikettensektoren (§4.4) ────────────────────────────
    // Nur wo der Container Marken kennt: traegt KEIN Sektor der Etikettenzone F8, ist die
    // Diskette ein .img (oder alles ist aktiv) - dann gibt es nichts zu beurteilen.
    bericht.schritt("sios.schritt.marken", "Datenmarken der Etikettensektoren (aktiv FB, frei F8)");
    {
        bool marken_da = false;
        for (int sek = 8; sek <= kSiosSektoren; ++sek) marken_da = marken_da || markeGeloescht(sek);
        if (marken_da) {
            std::set<int> aktiv;
            for (const SiosEtikett& e : et) aktiv.insert(e.sektor);
            for (int sek = 8; sek <= kSiosSektoren; ++sek) {
                const bool geloescht = markeGeloescht(sek);
                const bool ist_aktiv = aktiv.count(sek) > 0;
                if (ist_aktiv == !geloescht) continue;
                FsFinding& f = b.addAt(ist_aktiv ? "sios.marke.aktiv" : "sios.marke.frei",
                    FsSeverity::Info, FsLayer::Verwaltung, "Sektor " + std::to_string(sek),
                    ist_aktiv ? "Das aktive Etikett in Sektor " + std::to_string(sek)
                                + " traegt die geloeschte Datenmarke F8 (die Originale: aktiv FB)"
                              : "Der freie Etikettensatz in Sektor " + std::to_string(sek)
                                + " traegt die normale Datenmarke FB (die Originale: frei F8)",
                    0, 0, sek);
                FsRepair r;
                r.kind = "sios.marke.setzen";
                r.text = std::string("Datenmarke von Sektor ") + std::to_string(sek) + " auf "
                       + (ist_aktiv ? "FB (normal)" : "F8 (geloescht)") + " setzen; die Daten bleiben";
                r.empfohlen = true;
                r.a = sek;
                r.b = ist_aktiv ? 0 : 1;
                f.repairs.push_back(r);
            }
        }
    }

    bericht.sortieren();
    return bericht;
}

bool SiosFileSystem::repair(const FsRepair& r) {
    if (r.kind != "sios.marke.setzen")
        return fail("SIOS kennt keine Reparatur '" + r.kind + "'");
    if (r.a < 8 || r.a > kSiosSektoren) return fail("Etikettensektor " + std::to_string(r.a) + " gibt es nicht");
    SectorData s;
    if (!space_.readSector(0, 0, static_cast<uint8_t>(r.a), s) || s.data.size() != kSiosSektor)
        return fail("Spur 0 Sektor " + std::to_string(r.a) + " ist nicht lesbar");
    // Unmittelbar vor dem Schreiben: nur setzen, was zum Inhalt PASST (aktiv <=> HDR1).
    const bool aktiv = dkoi(s.data[0]) == 'H' && dkoi(s.data[1]) == 'D' && dkoi(s.data[2]) == 'R'
                       && dkoi(s.data[3]) == '1';
    if (aktiv == (r.b != 0))
        return fail("Die Marke von Sektor " + std::to_string(r.a) + " passt nicht mehr zum Inhalt");
    if (!space_.writeSectorDam(0, 0, static_cast<uint8_t>(r.a), s.data, r.b != 0))
        return fail(space_.lastError());
    return true;
}
