/**
 * @file bs600_check.cpp
 * @brief Die Pruefung des BS600-Dateisystems (PRG 700/710/710-1) und ihre Reparaturen.
 *
 * Umsetzung von `doc/design/15_dateisystempruefung.md` fuer das Format aus
 * `doc/bs600_diskettenformat.md`.  Die Pruefung **schreibt nie**; geaendert wird nur ueber
 * @ref Bs600FileSystem::repair, aufgerufen von @ref DiskVolume::applyRepairs.
 *
 * Alles liegt auf der Ebene **Verwaltung** (Spur 0: kein Zusatzzugriff auf Datenspuren, die
 * Schnellpruefung beim Oeffnen kostet nichts); nur `bs600.datei.unlesbar` (Ebene Medium) braucht die
 * Datenspuren und laeuft erst bei der Vollpruefung.
 *
 * | Kennung | Schwere | Bedeutung | Reparatur |
 * |---|---|---|---|
 * | `bs600.kennsatz.fehlt` | Fehler | Sektor 7 der Spur 0 traegt kein `VOL1` (EBCDIC) | - |
 * | `bs600.kennsatz.prg` | Fehler | im Kennsatz fehlt die Marke `PRG` (Byte 11..13) | - |
 * | `bs600.urlader.prg700` | Hinweis | Urlader `BOOTSTRAP FOR BS600` = Bauart des PRG 700 | - |
 * | `bs600.name.fehlt` | Fehler | Eintrag ohne Namen | `bs600.eintrag.loeschen` |
 * | `bs600.name.ungueltig` | Warnung | Steuer-/Nicht-ASCII-Zeichen im Namen | - |
 * | `bs600.name.doppelt` | Fehler | zwei Eintraege gleichen Namens | - |
 * | `bs600.eintrag.lage` | Fehler | Spur/Sektor ausserhalb der Diskette (Spur 1..39, Sektor 1..15) | `bs600.eintrag.loeschen` |
 * | `bs600.eintrag.ende` | Fehler | Ende der Lage vor ihrem Anfang | `bs600.eintrag.loeschen` |
 * | `bs600.eintrag.nutzspur` | Warnung | Datei reicht hinter Spur 37 (die Allokation vergibt sie nie) | - |
 * | `bs600.eintrag.typ` | Warnung | Typ weder `P` noch `L` | - |
 * | `bs600.eintrag.ueberlappt` | Gefahr | zwei Dateien teilen sich Sektoren | - |
 * | `bs600.adresse.ende` | Fehler | Lade-/Endadresse kein Hexfeld oder Ende < Anfang | - |
 * | `bs600.adresse.laenge` | Warnung | Sektorzahl passt nicht zur Laenge aus den Adressen | - |
 * | `bs600.datum.unplausibel` | Hinweis | Datum kein gueltiges `TTMMJJ` | - |
 * | `bs600.tabelle.fehlt` | Gefahr | Sektoren einer Datei stehen in der Belegungstabelle als frei | `bs600.tabelle.nachtragen` |
 * | `bs600.tabelle.ueberzaehlig` | Warnung | Tabelle fuehrt Sektoren belegt, die keine Datei hat | `bs600.tabelle.freigeben` |
 * | `bs600.luecke` | Hinweis | freie Sektoren zwischen den Dateien (Dateien liegen unfragmentiert) | `bs600.reorganisieren` |
 * | `bs600.datei.unlesbar` | Fehler | Sektor einer Datei nicht lesbar oder mit falscher CRC (nur voll) | - |
 *
 * **Falschmeldungen sind schlimmer als fehlende** (E10): an beiden Originalabzuegen
 * (`tests/fixtures/bs600/`) liefert die Pruefung keinen einzigen Befund; alles, was das Formatdokument
 * nicht belegt (Typ `L`, Rest[10], die Sektoren 4..6 der Spur 0), wird nicht beurteilt.
 *
 * @author Olaf Krieger
 * @date 2026
 * @license MIT License
 */

#include "core/filesystem/bs600/bs600_fs.h"
#include "core/filesystem/check/fs_check.h"

#include <algorithm>
#include <set>
#include <string>
#include <vector>

namespace {

using bs600::datumNormal;

constexpr const char* kSchrittKennsatz  = "bs600.schritt.kennsatz";
constexpr const char* kSchrittUrlader   = "bs600.schritt.urlader";
constexpr const char* kSchrittVerz      = "bs600.schritt.verzeichnis";
constexpr const char* kSchrittBelegung  = "bs600.schritt.belegung";
constexpr const char* kSchrittDateien   = "bs600.schritt.dateien";

std::string lage(const Bs600Eintrag& e) {
    return "Spur " + std::to_string(e.anf_spur) + "/" + std::to_string(e.anf_sek) + " bis "
         + std::to_string(e.end_spur) + "/" + std::to_string(e.end_sek);
}

std::string objektVon(const Bs600Eintrag& e) {
    return e.name.empty() || e.name[0] == '?' ? "Platz " + std::to_string(e.platz) : e.name;
}

int verzSektor(const Bs600Eintrag& e) { return 8 + e.platz / 3; }

std::string sektorText(int lin) {
    return "Spur " + std::to_string(lin / kBs600Sektoren + 1) + "/" + std::to_string(lin % kBs600Sektoren + 1);
}

}  // namespace

FsCheckReport Bs600FileSystem::check(FsCheckLevel level, bool nachladen) const {
    FsCheckReport bericht;
    bericht.level = level;
    FsFindings b(bericht);

    // Spur 0 ist beim Mounten gelesen; Datenspuren zaehlen nur in der Vollpruefung.
    std::set<int> gewollt{0}, da{0};
    auto abschluss = [&] {
        bericht.spuren_gelesen = static_cast<int>(da.size());
        bericht.spuren_gesamt  = static_cast<int>(gewollt.size());
    };

    // ═══ Kennsatz ════════════════════════════════════════════════════════════
    bericht.schritt(kSchrittKennsatz, "Datentraegerkennsatz: VOL1 (EBCDIC) und Marke PRG");
    {
        std::vector<uint8_t> v;
        if (!sektorLesen(0, 7, v)) {
            b.addAt("bs600.kennsatz.fehlt", FsSeverity::Fehler, FsLayer::Verwaltung, "Sektor 7",
                    "Spur 0 Sektor 7 (Datentraegerkennsatz) ist nicht lesbar", 0, 0, 7);
        } else {
            if (!(v[0] == 0xE5 && v[1] == 0xD6 && v[2] == 0xD3 && v[3] == 0xF1))
                b.addAt("bs600.kennsatz.fehlt", FsSeverity::Fehler, FsLayer::Verwaltung, "Sektor 7",
                        "Spur 0 Sektor 7 traegt kein VOL1 (EBCDIC E5 D6 D3 F1) - der Datentraeger hat keinen Kennsatz",
                        0, 0, 7);
            if (!(v[11] == 'P' && v[12] == 'R' && v[13] == 'G'))
                b.addAt("bs600.kennsatz.prg", FsSeverity::Fehler, FsLayer::Verwaltung, "Sektor 7",
                        "Im Kennsatz fehlt die Marke 'PRG' (Byte 11..13) - BS600 meldet dann KEINE PRG-DISK",
                        0, 0, 7);
        }
    }

    // ═══ Urlader (nur Hinweis, nie ein Mangel: eine Datendiskette braucht keinen) ═══
    bericht.schritt(kSchrittUrlader, "Urlader (Sektor 1..3): Bauart und Zielgeraet");
    {
        const Bs600Urlader u = urlader();
        if (u.art == Bs600Urlader::Art::Bootstrap)
            b.addAt("bs600.urlader.prg700", FsSeverity::Info, FsLayer::Verwaltung, "Sektor 1..3",
                    u.text + " (der NKM-Lader des 1-KB-ROM verlangt 'SY' an 0402H und bootet diese Diskette nicht)",
                    0, 0, 1);
    }

    // ═══ Verzeichnis ═════════════════════════════════════════════════════════
    bericht.schritt(kSchrittVerz, "Verzeichnis: Namen, Lage, Typ, Adressen, Datum");
    const std::vector<Bs600Eintrag> ein = eintraege();
    const int spuren = tracks_;

    for (const Bs600Eintrag& e : ein) {
        const std::string obj = objektVon(e);
        const int sek = verzSektor(e);
        auto at = [&](const char* id, FsSeverity sv, const std::string& text) -> FsFinding& {
            return b.addAt(id, sv, FsLayer::Verwaltung, obj, text, 0, 0, sek);
        };
        auto loeschen = [&](FsFinding& f, const std::string& warum) {
            FsRepair r;
            r.kind = "bs600.eintrag.loeschen";
            r.text = "Verzeichniseintrag auf Platz " + std::to_string(e.platz) + " ('" + obj
                   + "') mit Blanks ueberschreiben (" + warum + "); die Belegungstabelle bleibt unberuehrt";
            r.datenverlust = true;      // der Verweis auf die Datei geht verloren
            r.empfohlen = false;
            r.a = e.platz;
            r.s = e.name;
            f.repairs.push_back(r);
        };

        if (e.name.empty() || e.name[0] == '?') {
            loeschen(at("bs600.name.fehlt", FsSeverity::Fehler,
                        "Verzeichnisplatz " + std::to_string(e.platz) + " traegt Angaben, aber keinen Namen"),
                     "Name fehlt");
        } else if (std::any_of(e.name.begin(), e.name.end(),
                               [](char c) { return static_cast<unsigned char>(c) < 0x20 || static_cast<unsigned char>(c) > 0x7E; })) {
            at("bs600.name.ungueltig", FsSeverity::Warnung,
               "Der Name auf Platz " + std::to_string(e.platz) + " enthaelt Steuer- oder Nicht-ASCII-Zeichen");
        }

        if (e.typ != 'P' && e.typ != 'L')
            at("bs600.eintrag.typ", FsSeverity::Warnung,
               std::string("Typ '") + (e.typ >= 0x20 && e.typ < 0x7F ? std::string(1, e.typ) : std::string("?"))
               + "' - BS600 kennt nur P und L");

        // Lage.  Die Ursache wird neu bestimmt (parse() nennt nur einen Klartext, die Befunde brauchen Kennungen).
        const bool lage_aus = e.anf_spur < 1 || e.anf_spur >= spuren || e.end_spur < 1 || e.end_spur >= spuren
                              || e.anf_sek < 1 || e.anf_sek > kBs600Sektoren
                              || e.end_sek < 1 || e.end_sek > kBs600Sektoren;
        if (lage_aus && !e.name.empty() && e.name[0] != '?') {
            loeschen(at("bs600.eintrag.lage", FsSeverity::Fehler,
                        "Die Lage " + lage(e) + " liegt ausserhalb der Diskette (Spur 1.." + std::to_string(spuren - 1)
                        + ", Sektor 1..15)"), "Lage ausserhalb der Diskette");
        } else if (!lage_aus && e.linEnde() < e.linAnfang()) {
            loeschen(at("bs600.eintrag.ende", FsSeverity::Fehler,
                        "Das Ende der Lage (" + lage(e) + ") liegt vor ihrem Anfang"), "Ende vor Anfang");
        } else if (!lage_aus && e.end_spur > kBs600LetzteNutzSpur) {
            at("bs600.eintrag.nutzspur", FsSeverity::Warnung,
               "Die Datei reicht bis Spur " + std::to_string(e.end_spur) + " (" + lage(e)
               + "); BS600 vergibt nur Spur 1.." + std::to_string(kBs600LetzteNutzSpur));
        }

        // Adressen: nur wo die Lage brauchbar ist (sonst folgt der Befund der Lage zuerst).
        if (!lage_aus && e.linEnde() >= e.linAnfang()) {
            if (e.ladeadr < 0 || e.endadr < 0) {
                at("bs600.adresse.ende", FsSeverity::Fehler,
                   "Anfangs- oder Endadresse ist kein vierstelliges Hexfeld");
            } else if (e.endadr < e.ladeadr) {
                at("bs600.adresse.ende", FsSeverity::Fehler,
                   "Die Endadresse " + std::to_string(e.endadr) + " liegt vor der Anfangsadresse "
                   + std::to_string(e.ladeadr));
            } else {
                const long len = static_cast<long>(e.endadr) - e.ladeadr + 1;
                const long soll = (len + static_cast<long>(kBs600Sektor) - 1) / static_cast<long>(kBs600Sektor);
                if (soll != e.sektoren())
                    at("bs600.adresse.laenge", soll > e.sektoren() ? FsSeverity::Fehler : FsSeverity::Warnung,
                       "Die Adressen nennen " + std::to_string(len) + " Byte = " + std::to_string(soll)
                       + " Sektor(en), die Lage (" + lage(e) + ") belegt " + std::to_string(e.sektoren())
                       + (soll > e.sektoren() ? " - die Datei wird abgeschnitten gelesen" : ""));
            }
        }

        // Datum: nur beurteilen, was da steht (ein leeres Feld ist kein falsches).
        {
            std::string d = e.datum;
            while (!d.empty() && d.back() == ' ') d.pop_back();
            if (!d.empty() && datumNormal(e.datum).empty())
                at("bs600.datum.unplausibel", FsSeverity::Info,
                   "Das Datum '" + e.datum + "' ist kein gueltiges TTMMJJ");
        }
    }

    // Doppelte Namen (nur gueltig benannte).
    for (size_t i = 0; i < ein.size(); ++i)
        for (size_t j = i + 1; j < ein.size(); ++j) {
            if (ein[i].name.empty() || ein[i].name[0] == '?' || ein[i].name != ein[j].name) continue;
            b.addAt("bs600.name.doppelt", FsSeverity::Fehler, FsLayer::Verwaltung, ein[i].name,
                    "Der Name '" + ein[i].name + "' steht auf Platz " + std::to_string(ein[i].platz) + " und Platz "
                    + std::to_string(ein[j].platz), 0, 0, verzSektor(ein[j]));
        }

    // Ueberlappung (nur Eintraege mit brauchbarer Lage).
    for (size_t i = 0; i < ein.size(); ++i)
        for (size_t j = i + 1; j < ein.size(); ++j) {
            const Bs600Eintrag &x = ein[i], &y = ein[j];
            if (!x.gueltig || !y.gueltig) continue;
            if (x.linAnfang() <= y.linEnde() && y.linAnfang() <= x.linEnde())
                b.addAt("bs600.eintrag.ueberlappt", FsSeverity::Gefahr, FsLayer::Verwaltung,
                        objektVon(x) + " / " + objektVon(y),
                        "'" + objektVon(x) + "' (" + lage(x) + ") und '" + objektVon(y) + "' (" + lage(y)
                        + ") teilen sich Sektoren - der naechste Schreibvorgang auf einer zerstoert die andere",
                        y.anf_spur, 0, y.anf_sek);
        }

    // ═══ Belegungstabelle gegen Verzeichnis ══════════════════════════════════
    bericht.schritt(kSchrittBelegung, "Belegungstabelle (Sektor 26) gegen das Verzeichnis; Luecken");
    {
        const std::vector<uint8_t> tab = belegungstabelle();          // [Spur*16 + Sektor], nur Tabelle
        std::vector<uint8_t> soll(tab.size(), 0);                      // aus den Eintraegen
        for (const Bs600Eintrag& e : ein) {
            if (!e.gueltig) continue;
            for (int l = e.linAnfang(); l <= e.linEnde(); ++l) {
                const size_t i = static_cast<size_t>(l / kBs600Sektoren + 1) * 16 + static_cast<size_t>(l % kBs600Sektoren + 1);
                if (i < soll.size()) soll[i] = 1;
            }
        }
        bool ungueltiger = std::any_of(ein.begin(), ein.end(), [](const Bs600Eintrag& e) { return !e.gueltig; });

        // Eintrag -> Tabelle: je Datei EIN Befund.
        for (const Bs600Eintrag& e : ein) {
            if (!e.gueltig) continue;
            int fehlt = 0;
            for (int l = e.linAnfang(); l <= e.linEnde(); ++l) {
                const size_t i = static_cast<size_t>(l / kBs600Sektoren + 1) * 16 + static_cast<size_t>(l % kBs600Sektoren + 1);
                if (i < tab.size() && !tab[i]) ++fehlt;
            }
            if (!fehlt) continue;
            FsFinding& f = b.addAt("bs600.tabelle.fehlt", FsSeverity::Gefahr, FsLayer::Verwaltung, objektVon(e),
                "In der Belegungstabelle stehen " + std::to_string(fehlt) + " von " + std::to_string(e.sektoren())
                + " Sektoren von '" + objektVon(e) + "' (" + lage(e) + ") als frei - die naechste Datei wird darueber geschrieben",
                0, 0, 26);
            FsRepair r;
            r.kind = "bs600.tabelle.nachtragen";
            r.text = "Die Sektoren aller Dateien in der Belegungstabelle als belegt eintragen (" + std::to_string(fehlt)
                   + " fehlende Bits bei '" + objektVon(e) + "'); sonst bleibt alles stehen";
            r.empfohlen = true;
            f.repairs.push_back(r);
        }

        // Tabelle -> Eintrag: zusammenhaengende Laeufe fuehren NICHT verzeichneter Sektoren.
        int lauf_start = -1, lauf_len = 0;
        auto lauf_ende = [&] {
            if (lauf_len <= 0) return;
            FsFinding& f = b.addAt("bs600.tabelle.ueberzaehlig", FsSeverity::Warnung, FsLayer::Verwaltung,
                sektorText(lauf_start),
                "Die Belegungstabelle fuehrt " + std::to_string(lauf_len) + " Sektor(en) ab " + sektorText(lauf_start)
                + " als belegt, aber keine Datei gehoert dazu (verlorener Platz; ein Suchlauf zeigt, ob dort Daten liegen)",
                lauf_start / kBs600Sektoren + 1, 0, lauf_start % kBs600Sektoren + 1);
            FsRepair r;
            r.kind = "bs600.tabelle.freigeben";
            r.text = "Diese und alle anderen von keiner Datei belegten Sektoren in der Belegungstabelle freigeben";
            r.empfohlen = false;       // dort koennten Daten einer Datei ohne Eintrag liegen: erst ansehen
            if (ungueltiger) {
                r.gesperrt = true;
                r.warum = "ein unbrauchbarer Verzeichniseintrag belegt womoeglich Sektoren, die dabei freigegeben wuerden - "
                          "erst den Eintrag klaeren";
            }
            f.repairs.push_back(r);
            lauf_start = -1; lauf_len = 0;
        };
        for (int l = 0; l < (spuren - 1) * kBs600Sektoren; ++l) {
            const size_t i = static_cast<size_t>(l / kBs600Sektoren + 1) * 16 + static_cast<size_t>(l % kBs600Sektoren + 1);
            if (i < tab.size() && tab[i] && !soll[i]) {
                if (lauf_start < 0) lauf_start = l;
                ++lauf_len;
            } else {
                lauf_ende();
            }
        }
        lauf_ende();

        // Luecken: freie Sektoren HINTER keiner Datei sind der Normalfall; davor eine Zerstueckelung.
        int hoechster = -1;
        for (const Bs600Eintrag& e : ein) if (e.gueltig) hoechster = std::max(hoechster, e.linEnde());
        int frei = 0, laeufe = 0;
        bool im_lauf = false;
        for (int l = 0; l < hoechster; ++l) {
            const size_t i = static_cast<size_t>(l / kBs600Sektoren + 1) * 16 + static_cast<size_t>(l % kBs600Sektoren + 1);
            if (i < soll.size() && !soll[i]) { ++frei; if (!im_lauf) { ++laeufe; im_lauf = true; } }
            else im_lauf = false;
        }
        if (frei > 0) {
            FsFinding& f = b.addAt("bs600.luecke", FsSeverity::Info, FsLayer::Verwaltung, "Belegung",
                std::to_string(frei) + " freie Sektoren liegen in " + std::to_string(laeufe)
                + " Luecke(n) zwischen den Dateien; BS600 legt jede Datei zusammenhaengend ab - eine grosse Datei kann daran "
                "scheitern, obwohl insgesamt Platz ist.  Reorganisieren schliesst die Luecken.", 0, 0, 26);
            FsRepair r;
            r.kind = "bs600.reorganisieren";
            r.text = "Alle Dateien lueckenlos ab Spur 1 neu legen (verschiebt Dateien; Namen und Inhalt bleiben)";
            r.empfohlen = false;
            f.repairs.push_back(r);
        }
    }

    // ═══ Datenspuren (nur voll) ══════════════════════════════════════════════
    if (level == FsCheckLevel::Voll) {
        bericht.schritt(kSchrittDateien, "Dateien: jeder Sektor lesbar, CRC in Ordnung");
        for (const Bs600Eintrag& e : ein) {
            if (!e.gueltig) continue;
            int schlecht = 0, erster = -1;
            for (int l = e.linAnfang(); l <= e.linEnde(); ++l) {
                const int cyl = l / kBs600Sektoren + 1, id = l % kBs600Sektoren + 1;
                gewollt.insert(cyl);
                if (!nachladen && !space_.trackKnown(static_cast<uint8_t>(cyl), 0)) {
                    bericht.vollstaendig = false;
                    continue;
                }
                da.insert(cyl);
                SectorData s;
                if (!space_.readSector(static_cast<uint8_t>(cyl), 0, static_cast<uint8_t>(id), s)
                    || s.data.size() != kBs600Sektor || !s.ok()) {
                    ++schlecht;
                    if (erster < 0) erster = l;
                }
            }
            if (schlecht)
                b.addAt("bs600.datei.unlesbar", FsSeverity::Fehler, FsLayer::Medium, objektVon(e),
                        std::to_string(schlecht) + " Sektor(en) von '" + objektVon(e) + "' sind nicht lesbar oder tragen eine "
                        "falsche CRC (erster: " + sektorText(erster) + ")",
                        erster / kBs600Sektoren + 1, 0, erster % kBs600Sektoren + 1);
        }
    }

    bericht.schrittEnde();
    abschluss();
    bericht.sortieren();
    return bericht;
}

// ─── Reparatur ───────────────────────────────────────────────────────────────

bool Bs600FileSystem::repair(const FsRepair& r) {
    // Jede Reparatur liest den Zustand UNMITTELBAR vor dem Schreiben neu: zwischen Bericht und Entscheidung kann die
    // Diskette beschrieben worden sein.
    if (r.kind == "bs600.eintrag.loeschen") {
        if (r.a < 0 || r.a >= kBs600Eintraege) return fail("Verzeichnisplatz " + std::to_string(r.a) + " gibt es nicht");
        Bs600Eintrag gefunden;
        bool da = false;
        for (const Bs600Eintrag& e : eintraege())
            if (e.platz == r.a) { gefunden = e; da = true; }
        if (!da) return fail("Platz " + std::to_string(r.a) + " ist inzwischen leer");
        if (gefunden.gueltig && !gefunden.name.empty() && gefunden.name[0] != '?')
            return fail("Der Eintrag auf Platz " + std::to_string(r.a) + " ('" + gefunden.name + "') ist inzwischen in Ordnung - nichts geloescht");
        return eintragSchreiben(r.a, std::vector<uint8_t>(kBs600EintragGroesse, 0x20));
    }

    if (r.kind == "bs600.tabelle.nachtragen" || r.kind == "bs600.tabelle.freigeben") {
        const bool frei = r.kind == "bs600.tabelle.freigeben";
        std::vector<uint8_t> tab;
        if (!sektorLesen(0, 26, tab)) return fail("Belegungstabelle (Spur 0 Sektor 26) nicht lesbar");
        const std::vector<Bs600Eintrag> ein = eintraege();
        if (frei)
            for (const Bs600Eintrag& e : ein)
                if (!e.gueltig)
                    return fail("Der Eintrag '" + e.name + "' ist unbrauchbar - sein Platzbedarf ist unbekannt, "
                                "freigeben verweigert");
        std::vector<uint8_t> soll(static_cast<size_t>(std::max(tracks_, 1)) * 16, 0);
        for (const Bs600Eintrag& e : ein) {
            if (!e.gueltig) continue;
            for (int l = e.linAnfang(); l <= e.linEnde(); ++l)
                soll[static_cast<size_t>(l / kBs600Sektoren + 1) * 16 + static_cast<size_t>(l % kBs600Sektoren + 1)] = 1;
        }
        for (int s = 1; s < tracks_; ++s)
            for (int k = 1; k <= kBs600Sektoren; ++k) {
                const bool will = soll[static_cast<size_t>(s) * 16 + static_cast<size_t>(k)] != 0;
                if (will) bs600::tabBit(tab, s, k, true);
                else if (frei) bs600::tabBit(tab, s, k, false);
            }
        return tabelleSchreiben(tab);
    }

    if (r.kind == "bs600.reorganisieren") return reorganize(nullptr);

    return fail("BS600 kennt keine Reparatur '" + r.kind + "'");
}
