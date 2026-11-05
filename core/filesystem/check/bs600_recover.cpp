/**
 * @file bs600_recover.cpp
 * @brief Rettung von BS600-Daten ohne Verzeichniseintrag - rein lesend.
 *
 * ### Was geht und was nicht
 * Ein BS600-Eintrag traegt ALLES, was die Datei beschreibt (Name, Typ, Lage, Adressen, Datum, Signum), und das
 * Loeschen ueberschreibt den ganzen Eintrag mit 42 Blanks (so tut es `Bs600FileSystem::erase`; den Original-
 * Loeschvorgang hat B0 im Programmcode nicht gefunden - Annahme). Damit bleibt von einer geloeschten Datei
 * **nur der Inhalt** - ohne Namen, ohne Ladeadresse, ohne Laenge. Namen und Adressen lassen sich nicht aus den
 * Bytes ableiten (ein Z80-Abzug traegt seine Ladeadresse nicht in sich); wer eine solche Datei zurueckhaben will,
 * muss sie mit `put --load ADR --entry ADR` neu benennen.
 *
 * Deshalb:
 * * **Ein Fund ist ein Bereich**, kein Dateiersatz: `wiederherstellbar` ist immer `false`, `recoverRestore` lehnt ab.
 *   Herausholen (`recover --extract`, Rohbytes ganzer Sektoren) geht immer, auch schreibgeschuetzt.
 * * Stufe **Verzeichnis**: Sektoren, die die Belegungstabelle als belegt fuehrt und denen kein Eintrag gehoert
 *   (Guete *wahrscheinlich*: die Lage ist belegt, nur die Dateigrenzen sind unbekannt).
 * * Stufe **Oberflaeche**: freie Sektoren (weder Tabelle noch Eintrag), die kein Fuellmuster tragen - zusammenhaengende
 *   Laeufe (Guete *Bruchstueck*: ob und wie viele Dateien dort lagen, ist geraten).
 *
 * @see doc/design/15_dateisystempruefung.md §13, doc/bs600_diskettenformat.md §6
 */

#include "core/filesystem/bs600/bs600_fs.h"
#include "core/filesystem/check/fs_recover.h"

#include <algorithm>
#include <cstdio>
#include <set>
#include <string>
#include <vector>

namespace {

FsRecoverOrt ortVon(int lin) {
    FsRecoverOrt o;
    o.cyl = lin / kBs600Sektoren + 1;
    o.head = 0;
    o.sector = lin % kBs600Sektoren + 1;
    return o;
}

std::string sektorText(int lin) {
    return "Spur " + std::to_string(lin / kBs600Sektoren + 1) + "/" + std::to_string(lin % kBs600Sektoren + 1);
}

}  // namespace

FsRecoverReport Bs600FileSystem::recoverScan(FsRecoverLevel level, bool nachladen) const {
    FsRecoverReport r;
    r.level = level;
    std::set<int> gebraucht{0}, gelesen{0};
    auto spurDa = [&](int cyl) {
        gebraucht.insert(cyl);
        if (nachladen || space_.trackKnown(static_cast<uint8_t>(cyl), 0)) { gelesen.insert(cyl); return true; }
        r.vollstaendig = false;
        return false;
    };

    const std::vector<uint8_t> tab = belegungstabelle();
    std::vector<uint8_t> soll(tab.size(), 0);
    for (const Bs600Eintrag& e : eintraege()) {
        if (!e.gueltig) continue;
        for (int l = e.linAnfang(); l <= e.linEnde(); ++l) {
            const size_t i = static_cast<size_t>(l / kBs600Sektoren + 1) * 16 + static_cast<size_t>(l % kBs600Sektoren + 1);
            if (i < soll.size()) soll[i] = 1;
        }
    }
    const int gesamt = (tracks_ - 1) * kBs600Sektoren;
    auto idx = [](int l) { return static_cast<size_t>(l / kBs600Sektoren + 1) * 16 + static_cast<size_t>(l % kBs600Sektoren + 1); };

    auto fund = [&](int start, int n, bool aus_tabelle) {
        FsRecoverFind f;
        f.type = "Rohbereich";
        f.origin = std::string(aus_tabelle ? "Belegungstabelle ohne Eintrag, " : "freier Bereich mit Inhalt, ")
                 + sektorText(start) + (n > 1 ? " bis " + sektorText(start + n - 1) : "");
        char nm[48];
        std::snprintf(nm, sizeof nm, "bs600_s%02d_%02d_%dsek.bin", start / kBs600Sektoren + 1, start % kBs600Sektoren + 1, n);
        f.vorschlag = nm;
        f.size = static_cast<uint64_t>(n) * kBs600Sektor;
        const FsRecoverOrt o = ortVon(start);
        f.cyl = o.cyl; f.head = 0; f.sector_index = o.sector;
        for (int i = 0; i < n && f.orte.size() < kFsRecoverMaxOrte; ++i) f.orte.push_back(ortVon(start + i));
        f.a = start; f.b = n; f.d = aus_tabelle ? 0 : 1;
        f.wiederherstellbar = false;
        f.warum_nicht = "Name, Typ, Adressen und Dateigrenzen stehen nur im Verzeichniseintrag und sind verloren - "
                        "den Inhalt herausholen und mit 'put --load ADR --entry ADR' neu ablegen";
        return f;
    };

    // ── 1. Belegungstabelle ohne Eintrag ─────────────────────────────────────
    r.schritt("bs600.suche.tabelle", "Belegungstabelle: als belegt gefuehrte Sektoren ohne Verzeichniseintrag");
    for (int l = 0; l < gesamt;) {
        if (!(tab[idx(l)] && !soll[idx(l)])) { ++l; continue; }
        int n = 0;
        while (l + n < gesamt && tab[idx(l + n)] && !soll[idx(l + n)]) ++n;
        FsRecoverFind f = fund(l, n, true);
        f.quality = FsRecoverQuality::Wahrscheinlich;
        f.detail = "die Lage ist belegt, aber ob dort eine oder mehrere Dateien lagen, ist unbekannt";
        if (level == FsRecoverLevel::Oberflaeche) {
            std::vector<uint8_t> probe;
            if (spurDa(l / kBs600Sektoren + 1)) {
                std::vector<uint8_t> s;
                if (sektorLesen(l / kBs600Sektoren + 1, l % kBs600Sektoren + 1, s)) probe = s;
            }
            if (!probe.empty()) f.type = fsRecoverEinordnung(probe);
        }
        r.hinzu(std::move(f));
        l += n;
    }

    // ── 2. Freie Sektoren mit Inhalt (nur Oberflaeche) ───────────────────────
    if (level == FsRecoverLevel::Oberflaeche) {
        r.schritt("bs600.suche.frei", "Freie Sektoren mit Inhalt (Reste geloeschter Dateien; Namen sind verloren)");
        int lauf = -1, n = 0;
        std::vector<uint8_t> daten;
        auto abschliessen = [&] {
            if (lauf < 0) return;
            FsRecoverFind f = fund(lauf, n, false);
            f.quality = FsRecoverQuality::Bruchstueck;
            f.type = fsRecoverEinordnung(daten);
            f.detail = "frei gefuehrte Sektoren mit Inhalt: Reste einer geloeschten Datei, Grenzen und Zahl der Dateien geraten ("
                     + f.type + ")";
            r.hinzu(std::move(f));
            lauf = -1; n = 0; daten.clear();
        };
        for (int l = 0; l < kBs600LetzteNutzSpur * kBs600Sektoren; ++l) {
            if (tab[idx(l)] || soll[idx(l)]) { abschliessen(); continue; }
            if (!spurDa(l / kBs600Sektoren + 1)) { abschliessen(); continue; }
            std::vector<uint8_t> s;
            if (!sektorLesen(l / kBs600Sektoren + 1, l % kBs600Sektoren + 1, s) || fsRecoverFuellmuster(s)) {
                abschliessen();
                continue;
            }
            if (lauf < 0) lauf = l;
            ++n;
            if (daten.size() < 4096) daten.insert(daten.end(), s.begin(), s.end());
        }
        abschliessen();
    }

    r.schrittEnde();
    r.spuren_gesamt = static_cast<int>(gebraucht.size());
    r.spuren_gelesen = static_cast<int>(gelesen.size());
    r.sortieren();
    return r;
}

bool Bs600FileSystem::recoverRead(const FsRecoverFind& f, std::vector<uint8_t>& out) const {
    out.clear();
    if (f.b <= 0 || f.a < 0) return fail("Der Fund nennt keinen Bereich");
    out.reserve(static_cast<size_t>(f.b) * kBs600Sektor);
    for (int i = 0; i < f.b; ++i) {
        std::vector<uint8_t> s;
        const int l = f.a + i;
        if (!sektorLesen(l / kBs600Sektoren + 1, l % kBs600Sektoren + 1, s)) s.assign(kBs600Sektor, 0xE5);   // Offsets halten
        out.insert(out.end(), s.begin(), s.end());
    }
    return true;
}

bool Bs600FileSystem::recoverRestore(const FsRecoverFind& f, const std::string&) {
    (void)f;
    return fail("BS600: ein Verzeichniseintrag laesst sich nicht wiederherstellen - Name, Typ, Adressen und Dateigrenzen "
                "sind mit dem Eintrag verloren. Den Inhalt herausholen (recover --extract) und mit "
                "'put --load ADR --entry ADR' neu ablegen.");
}
