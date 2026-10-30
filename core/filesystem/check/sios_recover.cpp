/**
 * @file sios_recover.cpp
 * @brief Rettung geloeschter SIOS-1526-Dateien und Bibliotheksobjekte.
 *
 * ### Warum das geht
 * SIOS loescht, ohne Nutzdaten anzufassen (Formatdokument §4.3, §7.3):
 *
 * * **Physische Datei:** `HDR1` -> `DDR1` (ein Byte, dazu die Datenmarke F8).  Name, Satzlaenge,
 *   BOE, EOE und EOD bleiben stehen, der Bereich ist unberuehrt.  Eine geloeschte Datei ist damit
 *   vollstaendig beschrieben; sie wird nur nicht mehr gefunden.
 * * **Bibliotheksobjekt:** der Name wird blank, AADR und ML bleiben, der Datenteil auch.
 *   Verloren ist **allein der Name**.
 *
 * ### Zwei Arten von Fund
 * 1. **DDR1-Etikett mit erhaltenem Bereich** (`d == 0`).  Auf SG1 stehen 17 DDR1-Platzhalter
 *    `DATA10..DATA26` mit Bereich 01001..73026 - hinter dem Ende der Diskette.  Ein Bereich ausserhalb
 *    der Diskette ist KEIN Fund (kein Befund, kein Eintrag): es gibt dort nichts, was man holen koennte.
 * 2. **Geloeschtes Objekt einer lebenden Bibliothek** (`d == 1`, Name blank, AADR/ML da).
 *
 * ### Wiederherstellen (der Ausnahmefall; Retten geht vor)
 * * Etikett: nur wenn kein lebendes Etikett den Bereich beruehrt und der Name frei ist (sonst mit
 *   `--restore N=NAME`).  Derselbe Etikettensektor wird wieder `HDR1` (DDR1 -> HDR1, Datenmarke FB);
 *   ein freier Etikettensatz ist damit sicher da.  Unmittelbar vor dem Schreiben werden alle
 *   Bedingungen noch einmal geprueft.
 * * Objekt: nur mit einem vom Anwender vergebenen Namen (`--restore N=NAME`) - aus dem Fund laesst
 *   er sich nicht ableiten.  Der Verzeichnisplatz muss noch den AADR tragen und blank sein.
 *
 * @see doc/design/15_dateisystempruefung.md §13, doc/sios_diskettenformat.md §4.3, §7
 */

#include "core/filesystem/check/fs_recover.h"
#include "core/filesystem/sios/sios_fs.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <set>
#include <string>

namespace {

FsRecoverOrt ortVon(int linear) {
    FsRecoverOrt o;
    o.cyl = linear / kSiosSektoren;
    o.head = 0;
    o.sector = linear % kSiosSektoren + 1;
    return o;
}

std::string hex4(int v) { char b[16]; std::snprintf(b, sizeof b, "%04X", v); return b; }

std::string gross(std::string s) {
    for (char& c : s) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return s;
}

/// @brief Dateiname fuer Linux aus einem Etikettennamen (kein Pfadanteil, nie leer).
std::string vorschlagAus(std::string n, const std::string& ersatz) {
    for (char& c : n) if (c == '/' || c == '\\' || c == ':' || c == ' ') c = '_';
    return n.empty() ? ersatz : n;
}

}  // namespace

FsRecoverReport SiosFileSystem::recoverScan(FsRecoverLevel level, bool nachladen) const {
    FsRecoverReport r;
    r.level = level;
    std::set<int> gebraucht{0}, gelesen{0};
    // Spur ansehen, wenn das erlaubt ist (physische Diskette: nur Bekanntes ohne `nachladen`).
    auto spurDa = [&](int cyl) {
        gebraucht.insert(cyl);
        if (nachladen || space_.trackKnown(static_cast<uint8_t>(cyl), 0)) { gelesen.insert(cyl); return true; }
        r.vollstaendig = false;
        return false;
    };

    const std::vector<SiosEtikett> lebend = etiketten();
    auto lebendeUeberlappung = [&](int boe, int eoe) {
        std::vector<std::string> n;
        for (const SiosEtikett& l : lebend)
            if (l.gueltig && boe <= l.eoe && l.boe <= eoe) n.push_back(l.name);
        return n;
    };
    auto lebtName = [&](const std::string& name) {
        for (const SiosEtikett& l : lebend) if (gross(l.name) == gross(name)) return true;
        return false;
    };

    // Oberflaeche: Inhalt ansehen (kostet die Datenspuren).  Liefert die Zahl der Sektoren, die
    // nicht lesbar sind oder eine falsche CRC tragen; -1 = nicht angesehen.  `fuell` = nur Fuellbytes.
    auto inhaltPruefen = [&](int erster, int anzahl, bool& fuell, std::string& einordnung) {
        fuell = false;
        if (level != FsRecoverLevel::Oberflaeche) return -1;
        for (int t = erster / kSiosSektoren; t <= (erster + anzahl - 1) / kSiosSektoren; ++t)
            if (!spurDa(t)) return -1;
        std::vector<uint8_t> inhalt;
        int schlecht = 0;
        for (int i = 0; i < anzahl; ++i) {
            const int lin = erster + i;
            SectorData s;
            if (!space_.readSector(static_cast<uint8_t>(lin / kSiosSektoren), 0,
                                   static_cast<uint8_t>(lin % kSiosSektoren + 1), s)
                || s.data.size() != kSiosSektor) {
                ++schlecht;
                inhalt.insert(inhalt.end(), kSiosSektor, 0xE5);
                continue;
            }
            if (!s.ok()) ++schlecht;
            inhalt.insert(inhalt.end(), s.data.begin(), s.data.end());
        }
        fuell = fsRecoverFuellmuster(inhalt);
        einordnung = fsRecoverEinordnung(inhalt);
        return schlecht;
    };

    // ── 1. DDR1-Etiketten ────────────────────────────────────────────────────
    r.schritt("sios.suche.ddr1", "Geloeschte Dateien: DDR1-Etiketten mit erhaltenem Bereich");
    const std::vector<SiosEtikett> tot = geloeschteEtiketten();
    for (const SiosEtikett& e : tot) {
        // Bereich ausserhalb der Diskette (SG1: DATA10.. mit 01001..73026), in Spur 0 oder ohne
        // Adressen: kein Fund.
        if (!e.gueltig) continue;
        const int n = e.anzahl();
        if (n <= 0) continue;                       // eine leere Datei ist nichts zu retten

        FsRecoverFind f;
        f.name = e.name;
        f.type = "Datei";
        f.origin = "Etikett Sektor " + std::to_string(e.sektor) + " (DDR1)";
        f.vorschlag = vorschlagAus(e.name, "sios_s" + std::to_string(e.sektor));
        f.size = static_cast<uint64_t>(n) * kSiosSektor;
        f.cyl = e.boe / kSiosSektoren; f.head = 0; f.sector_index = e.boe % kSiosSektoren + 1;
        for (int i = 0; i < n && f.orte.size() < kFsRecoverMaxOrte; ++i) f.orte.push_back(ortVon(e.boe + i));
        f.d = 0; f.a = e.sektor; f.b = e.boe; f.c = n; f.s = e.name;

        std::vector<std::string> belege;
        FsRecoverQuality q = FsRecoverQuality::Sicher;
        const std::vector<std::string> nutzer = lebendeUeberlappung(e.boe, e.eoe);
        for (const std::string& l : nutzer) belege.push_back("der Bereich gehoert inzwischen zu '" + l + "'");
        for (const SiosEtikett& anderer : tot)
            if (anderer.sektor != e.sektor && anderer.gueltig && e.boe <= anderer.eoe && anderer.boe <= e.eoe)
                belege.push_back("der Bereich teilt sich Sektoren mit dem geloeschten '" + anderer.name + "'");
        if (!nutzer.empty()) q = FsRecoverQuality::Bruchstueck;
        else if (!belege.empty()) q = FsRecoverQuality::Wahrscheinlich;

        bool fuell = false;
        std::string einordnung;
        const int schlecht = inhaltPruefen(e.boe, n, fuell, einordnung);
        if (schlecht > 0) {
            belege.push_back(std::to_string(schlecht) + " Sektor(en) nicht lesbar oder mit falscher CRC");
            q = std::min(q, FsRecoverQuality::Wahrscheinlich);
        }
        if (fuell) {
            belege.push_back("der Bereich enthaelt nur Fuellbytes");
            q = FsRecoverQuality::Bruchstueck;
        }
        if (!einordnung.empty() && einordnung != "unklar") f.type = einordnung;
        f.quality = q;
        fsRecoverBelege(f.detail, belege, "Vorbehalte");

        f.wiederherstellbar = nutzer.empty() && e.eoe / kSiosSektoren <= letzteNutzSpur();
        if (!nutzer.empty())
            f.warum_nicht = "der Bereich gehoert inzwischen zu '" + nutzer.front() + "'";
        else if (!f.wiederherstellbar)
            f.warum_nicht = "der Bereich liegt in den Reservespuren";
        if (f.wiederherstellbar && lebtName(e.name)) {
            if (!f.detail.empty()) f.detail += "; ";
            f.detail += "der Name '" + e.name
                      + "' gehoert einer lebenden Datei - nur unter anderem Namen (--restore N=NAME)";
        }
        r.hinzu(std::move(f));
    }

    // ── 2. Geloeschte Objekte lebender Bibliotheken ──────────────────────────
    r.schritt("sios.suche.objekte", "Geloeschte Bibliotheksobjekte (Name blank, AADR/ML erhalten)");
    for (const SiosEtikett& lib : lebend) {
        if (!lib.gueltig || lib.satzlaenge != kSiosSektor || lib.name.size() < 3
            || lib.name.compare(lib.name.size() - 2, 2, "LB") != 0)
            continue;
        if (!spurDa((lib.boe + 1) / kSiosSektoren)) continue;
        std::vector<SiosObjekt> objekte;
        if (!bibliothek(lib, objekte)) continue;
        for (const SiosObjekt& o : objekte) {
            if (!o.name.empty() || o.laenge <= 0) continue;
            const int erster = lib.boe + (o.aadr - 1);
            FsRecoverFind f;
            f.type = "Objekt";
            f.origin = "Bibliothek " + lib.name + ", AADR " + hex4(o.aadr) + " (Name geloescht)";
            f.vorschlag = vorschlagAus(lib.name, "lib") + "_" + hex4(o.aadr);
            f.size = static_cast<uint64_t>(o.laenge) * kSiosSektor;
            f.cyl = erster / kSiosSektoren; f.head = 0; f.sector_index = erster % kSiosSektoren + 1;
            for (int i = 0; i < o.laenge && f.orte.size() < kFsRecoverMaxOrte; ++i)
                f.orte.push_back(ortVon(erster + i));
            f.d = 1; f.a = o.aadr; f.b = erster; f.c = o.laenge; f.s = lib.name;
            f.teile = {lib.sektor};
            f.quality = FsRecoverQuality::Wahrscheinlich;
            std::vector<std::string> belege{
                "der Name ist verloren; die Laenge (" + std::to_string(o.laenge)
                + " Sektoren) ist der Abstand zum naechsten AADR"};
            bool fuell = false;
            std::string einordnung;
            const int schlecht = inhaltPruefen(erster, o.laenge, fuell, einordnung);
            if (schlecht > 0)
                belege.push_back(std::to_string(schlecht) + " Sektor(en) nicht lesbar oder mit falscher CRC");
            if (fuell) {
                belege.push_back("der Bereich enthaelt nur Fuellbytes");
                f.quality = FsRecoverQuality::Bruchstueck;
            }
            if (!einordnung.empty() && einordnung != "unklar") f.type = einordnung;
            fsRecoverBelege(f.detail, belege, "Vorbehalte");
            f.wiederherstellbar = true;      // nur mit vergebenem Namen - das prueft recoverRestore
            r.hinzu(std::move(f));
        }
    }
    r.schrittEnde();
    r.spuren_gesamt = static_cast<int>(gebraucht.size());
    r.spuren_gelesen = static_cast<int>(gelesen.size());
    return r;
}

bool SiosFileSystem::recoverRead(const FsRecoverFind& f, std::vector<uint8_t>& out) const {
    out.clear();
    if (f.c <= 0 || f.b < kSiosSektoren) return fail("Der Fund nennt keinen Bereich");
    out.reserve(static_cast<size_t>(f.c) * kSiosSektor);
    for (int i = 0; i < f.c; ++i) {
        std::vector<uint8_t> d;
        if (!sektorLesen(f.b + i, d)) d.assign(kSiosSektor, 0xE5);   // Offsets der uebrigen erhalten
        out.insert(out.end(), d.begin(), d.end());
    }
    return true;
}

bool SiosFileSystem::recoverRestore(const FsRecoverFind& f, const std::string& name) {
    const std::string ziel = gross(name);

    if (f.d == 0) {
        // ── Etikett: DDR1 -> HDR1 ────────────────────────────────────────────
        // Alles unmittelbar vor dem Schreiben noch einmal: zwischen Suchlauf und Entscheidung
        // kann die Diskette beschrieben worden sein.
        const std::vector<SiosEtikett> alle = geloeschteEtiketten();
        const SiosEtikett* tot = nullptr;
        for (const SiosEtikett& e : alle) if (e.sektor == f.a) tot = &e;
        if (!tot || !tot->gueltig || tot->name != f.s || tot->boe != f.b || tot->anzahl() != f.c)
            return fail("Der Etikettensatz in Sektor " + std::to_string(f.a)
                        + " ist nicht mehr derselbe geloeschte Satz");
        const std::vector<SiosEtikett> lebend = etiketten();
        for (const SiosEtikett& l : lebend)
            if (l.gueltig && tot->boe <= l.eoe && l.boe <= tot->eoe)
                return fail("Der Bereich gehoert inzwischen zu '" + l.name
                            + "' - ein Eintrag darauf erzeugte eine Ueberlappung");
        if (tot->eoe / kSiosSektoren > letzteNutzSpur())
            return fail("Der Bereich liegt in den Reservespuren");
        const std::string neu = ziel.empty() ? gross(tot->name) : ziel;
        if (neu.empty()) return fail("Der Fund hat keinen Namen - bitte --restore N=NAME angeben");
        for (const SiosEtikett& l : lebend)
            if (gross(l.name) == neu) return fail("Eine Datei '" + neu + "' steht bereits auf der Diskette");
        SectorData s;
        if (!space_.readSector(0, 0, static_cast<uint8_t>(f.a), s) || s.data.size() != kSiosSektor)
            return fail("Spur 0 Sektor " + std::to_string(f.a) + " ist nicht lesbar");
        s.data[0] = 0xC8;                                   // 'H': DDR1 -> HDR1
        if (!ziel.empty()) {
            if (ziel.size() > 8) return fail("Dateiname '" + ziel + "' ungueltig: hoechstens 8 Zeichen");
            for (size_t i = 0; i < 17; ++i) {
                uint8_t b = 0x40;
                if (i < ziel.size() && (ziel[i] == ' ' || !dkoiKodieren(ziel[i], b)))
                    return fail("Dateiname '" + ziel + "' ungueltig: Zeichen ausserhalb DKOI");
                s.data[5 + i] = b;
            }
        }
        // Datenmarke FB: aktiver Satz (§4.4).
        if (!space_.writeSectorDam(0, 0, static_cast<uint8_t>(f.a), s.data, false))
            return fail(space_.lastError());
        return true;
    }

    // ── Objekt: Name in den noch vorhandenen Verzeichnisplatz ────────────────
    std::string objname = ziel;
    const size_t schraeg = objname.find('/');
    if (schraeg != std::string::npos) {
        if (objname.substr(0, schraeg) != gross(f.s))
            return fail("Der Name nennt eine andere Bibliothek als '" + f.s + "'");
        objname = objname.substr(schraeg + 1);
    }
    if (objname.empty())
        return fail("Der Name eines geloeschten Objekts ist verloren und laesst sich nicht ableiten -"
                    " bitte --restore N=NAME angeben");
    if (objname.size() > 5
        || std::any_of(objname.begin(), objname.end(),
                       [](char c) { return c <= 0x20 || c > 0x7E || c == '/'; }))
        return fail("Objektname '" + objname + "' ungueltig: 1 bis 5 sichtbare ASCII-Zeichen");
    const SiosEtikett* lib = nullptr;
    const std::vector<SiosEtikett> lebend = etiketten();
    const int libsektor = f.teile.empty() ? -1 : f.teile[0];
    for (const SiosEtikett& l : lebend) if (l.sektor == libsektor) lib = &l;
    if (!lib || lib->name != f.s)
        return fail("Die Bibliothek '" + f.s + "' ist inzwischen nicht mehr dieselbe");
    const int dl = lib->boe + 1;
    SectorData sd;
    if (!space_.readSector(static_cast<uint8_t>(dl / kSiosSektoren), 0,
                           static_cast<uint8_t>(dl % kSiosSektoren + 1), sd)
        || sd.data.size() != kSiosSektor)
        return fail("Das Verzeichnis von '" + lib->name + "' ist nicht lesbar");
    int slot = -1;
    for (int i = 0; i < 15; ++i) {
        const uint8_t* p = &sd.data[static_cast<size_t>(i) * 8];
        if (std::all_of(p, p + 8, [](uint8_t x) { return x == 0x03; })) continue;   // unbenutzt
        std::string n;
        for (int k = 0; k < 5 && p[k] != 0; ++k) n += static_cast<char>(p[k]);
        while (!n.empty() && n.back() == ' ') n.pop_back();
        if (n == objname) return fail("'" + lib->name + "/" + objname + "' existiert bereits");
        if (n.empty() && (p[5] | (p[6] << 8)) == f.a) slot = i;
    }
    if (slot < 0)
        return fail("Der Verzeichnisplatz mit AADR " + hex4(f.a) + " ist nicht mehr geloescht");
    uint8_t* p = &sd.data[static_cast<size_t>(slot) * 8];
    for (size_t k = 0; k < 5; ++k) p[k] = k < objname.size() ? static_cast<uint8_t>(objname[k]) : 0x00;
    if (!space_.writeSector(static_cast<uint8_t>(dl / kSiosSektoren), 0,
                            static_cast<uint8_t>(dl % kSiosSektoren + 1), sd.data))
        return fail(space_.lastError());
    return true;
}
