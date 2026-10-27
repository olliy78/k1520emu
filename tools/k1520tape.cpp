/**
 * @file k1520tape.cpp
 * @brief `k1520tape` — Kassettenbaender (`.k52`) anlegen, ansehen, mit Dateien fuellen, Bootband bauen.
 *
 * ```
 * k1520tape create   <band.k52>
 * k1520tape ls       <band.k52> [--side A|B]
 * k1520tape put      <band.k52> <datei> [--side A|B] [--block N] [--no-mark] [--format F]
 * k1520tape get      <band.k52> <datei> [--side A|B] [--file N] [--as F --addr A [--start A] [--name N] [--type C]]
 * k1520tape bootband <binaer> <band.k52> [A|B] [--format F]
 * ```
 * F = auto|roh|headersave|kcc|kctap|hex — die Programmdateiformate von JKCEMU
 * (core/peripherals/kassette/programmdatei.h): `put`/`bootband` streifen den Kopf ab und legen die
 * Nutzbytes aufs Band, `get --as` setzt einen Kopf davor.
 * Nur Rohbloecke: eine Datei ist eine Folge von Bloecken zu je N (Vorgabe 128, hoechstens 256)
 * Datenbytes, der letzte mit 0xE5 aufgefuellt, danach eine Bandmarke.  FEX-Formate sind nicht
 * belegt und werden nicht erzeugt (doc/kassette/treiber.md §6.2).  Ein „File" beim Auslesen ist
 * die Blockfolge bis zur naechsten Bandmarke.
 *
 * Exit-Codes: 0 ok · 1 Fehler (Aufruf, Datei, Band voll).
 * @see doc/kassette/treiber.md §6.1, tools/k1520tape.md
 */

#include "core/peripherals/kassette/k52_codec.h"
#include "core/peripherals/kassette/kassetten_medium.h"
#include "core/peripherals/kassette/programmdatei.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>
#include <vector>

using namespace kassette;
namespace pd = kassette::programmdatei;

namespace {

constexpr uint32_t kBlockLuecke_um = 10'000;  // Luecke zwischen Bloecken [?] (nicht belegt)
constexpr uint8_t kFuell = 0xE5;

int fehler(const std::string& s) {
    std::fprintf(stderr, "k1520tape: %s\n", s.c_str());
    return 1;
}

bool leseDatei(const std::string& p, std::vector<uint8_t>& out) {
    std::ifstream f(p, std::ios::binary);
    if (!f) return false;
    out.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
    return true;
}
bool schreibeDatei(const std::string& p, const std::vector<uint8_t>& d) {
    std::ofstream f(p, std::ios::binary | std::ios::trunc);
    if (!f) return false;
    f.write(reinterpret_cast<const char*>(d.data()), static_cast<std::streamsize>(d.size()));
    return static_cast<bool>(f);
}

std::optional<KassettenMedium> ladeBand(const std::string& p) {
    std::vector<uint8_t> d;
    if (!leseDatei(p, d)) { fehler("kann '" + p + "' nicht lesen"); return std::nullopt; }
    std::string f;
    auto m = k52::dekodieren(d, f);
    if (!m) fehler(p + ": " + f);
    return m;
}
bool speichereBand(const std::string& p, const KassettenMedium& m) {
    if (!schreibeDatei(p, k52::kodieren(m))) { fehler("kann '" + p + "' nicht schreiben"); return false; }
    return true;
}

bool seiteParsen(const std::string& s, Seite& out) {
    if (s == "A" || s == "a") { out = Seite::A; return true; }
    if (s == "B" || s == "b") { out = Seite::B; return true; }
    return false;
}

std::string endung(const std::string& p) {
    const size_t i = p.rfind('.');
    return i == std::string::npos ? "" : p.substr(i + 1);
}

/// Eingabedatei → Abbild.  Ohne --format: am Inhalt erkennen, sonst (KCC ist nicht erkennbar) an der Endung;
/// ein Quellformat mit Adresse wird nie als Roh ueberlesen, wenn die Endung es ausdruecklich benennt.
std::optional<pd::Abbild> leseProgramm(const std::string& pfad, const std::string& format, int& rc) {
    std::vector<uint8_t> d;
    if (!leseDatei(pfad, d)) { rc = fehler("kann '" + pfad + "' nicht lesen"); return std::nullopt; }
    if (d.empty()) { rc = fehler("Datei ist leer"); return std::nullopt; }
    pd::Format f = pd::Format::Auto;
    if (!format.empty()) {
        auto g = pd::formatVonName(format);
        if (!g) { rc = fehler("unbekanntes Format '" + format + "' (auto|roh|headersave|kcc|kctap|hex)"); return std::nullopt; }
        f = *g;
    }
    if (f == pd::Format::Auto) {
        f = pd::erkenne(d);
        auto e = pd::formatVonName(endung(pfad));
        if (f == pd::Format::Roh && e && *e == pd::Format::Kcc) f = pd::Format::Kcc;
    }
    std::string fe;
    bool weitere = false;
    auto a = pd::lesen(d, f, fe, &weitere);
    if (!a) { rc = fehler(pfad + ": " + fe); return std::nullopt; }
    if (weitere) std::fprintf(stderr, "k1520tape: Hinweis: Multi-KC-TAP, nur die erste Teildatei wird uebernommen\n");
    return a;
}

/// Haengt hinter den letzten Block an; die erste Luecke ist die Initialluecke.
bool anhaengen(KassettenMedium& m, Seite s, const std::vector<uint8_t>& bytes) {
    uint32_t l = m.anzahlBloecke(s) == 0 ? m.geometrie().initialluecke_um : kBlockLuecke_um;
    return m.haengeAn(s, l, bytes);
}

int cmdCreate(const std::vector<std::string>& a) {
    if (a.size() != 1) return fehler("create <band.k52>");
    KassettenMedium m;
    return speichereBand(a[0], m) ? 0 : 1;
}

int cmdLs(const std::vector<std::string>& a) {
    Seite nur = Seite::A;
    bool alle = true;
    std::vector<std::string> pos;
    for (size_t i = 0; i < a.size(); ++i) {
        if (a[i] == "--side" && i + 1 < a.size()) {
            if (!seiteParsen(a[++i], nur)) return fehler("--side erwartet A oder B");
            alle = false;
        } else pos.push_back(a[i]);
    }
    if (pos.size() != 1) return fehler("ls <band.k52> [--side A|B]");
    auto m = ladeBand(pos[0]);
    if (!m) return 1;
    for (Seite s : {Seite::A, Seite::B}) {
        if (!alle && s != nur) continue;
        std::printf("Seite %c: %zu Bloecke%s\n", s == Seite::A ? 'A' : 'B', m->anzahlBloecke(s),
                    m->schreibgeschuetzt(s) ? " (schreibgeschuetzt)" : "");
        for (size_t i = 0; i < m->anzahlBloecke(s); ++i) {
            const Block& b = m->block(s, i);
            if (b.bandmarke()) std::printf("  %3zu  Bandmarke\n", i);
            else std::printf("  %3zu  %4zu Bytes, %3zu Daten, CRC %s\n", i, b.bytes.size(),
                             b.bytes.size() >= 4 ? b.bytes.size() - 4 : 0, crcStimmt(b.bytes) ? "ok" : "FALSCH");
        }
    }
    return 0;
}

int cmdPut(const std::vector<std::string>& a) {
    Seite s = Seite::A;
    size_t bl = 128;
    bool marke = true;
    std::string format;
    std::vector<std::string> pos;
    for (size_t i = 0; i < a.size(); ++i) {
        if (a[i] == "--side" && i + 1 < a.size()) {
            if (!seiteParsen(a[++i], s)) return fehler("--side erwartet A oder B");
        } else if (a[i] == "--block" && i + 1 < a.size()) {
            bl = std::strtoul(a[++i].c_str(), nullptr, 0);
            if (bl < 1 || bl > 256) return fehler("--block muss 1 bis 256 sein");
        } else if (a[i] == "--format" && i + 1 < a.size()) format = a[++i];
        else if (a[i] == "--no-mark") marke = false;
        else pos.push_back(a[i]);
    }
    if (pos.size() != 2) return fehler("put <band.k52> <datei> [--side A|B] [--block N] [--no-mark] [--format F]");
    auto m = ladeBand(pos[0]);
    if (!m) return 1;
    int rc = 1;
    auto abbild = leseProgramm(pos[1], format, rc);
    if (!abbild) return rc;
    const std::vector<uint8_t>& d = abbild->daten;
    KassettenMedium neu = *m;  // Transaktion: bei vollem Band bleibt die Datei unveraendert
    for (size_t o = 0; o < d.size(); o += bl) {
        std::vector<uint8_t> teil(d.begin() + static_cast<long>(o),
                                  d.begin() + static_cast<long>(std::min(d.size(), o + bl)));
        teil.resize(bl, kFuell);
        if (!anhaengen(neu, s, blockBauen(teil))) return fehler("Band voll");
    }
    if (marke && !anhaengen(neu, s, bandmarkeBytes())) return fehler("Band voll");
    return speichereBand(pos[0], neu) ? 0 : 1;
}

int cmdGet(const std::vector<std::string>& a) {
    Seite s = Seite::A;
    size_t nr = 0;
    std::string als, adr, start, name, typ;
    std::vector<std::string> pos;
    for (size_t i = 0; i < a.size(); ++i) {
        if (a[i] == "--side" && i + 1 < a.size()) {
            if (!seiteParsen(a[++i], s)) return fehler("--side erwartet A oder B");
        } else if (a[i] == "--file" && i + 1 < a.size()) nr = std::strtoul(a[++i].c_str(), nullptr, 0);
        else if (a[i] == "--as" && i + 1 < a.size()) als = a[++i];
        else if (a[i] == "--addr" && i + 1 < a.size()) adr = a[++i];
        else if (a[i] == "--start" && i + 1 < a.size()) start = a[++i];
        else if (a[i] == "--name" && i + 1 < a.size()) name = a[++i];
        else if (a[i] == "--type" && i + 1 < a.size()) typ = a[++i];
        else pos.push_back(a[i]);
    }
    if (pos.size() != 2)
        return fehler("get <band.k52> <datei> [--side A|B] [--file N] [--as F --addr A [--start A] [--name N] [--type C]]");
    pd::Format fmt = pd::Format::Roh;
    if (!als.empty()) {
        auto g = pd::formatVonName(als);
        if (!g || *g == pd::Format::Auto) return fehler("--as erwartet roh|headersave|kcc|kctap|hex");
        fmt = *g;
    }
    if (fmt != pd::Format::Roh && adr.empty()) return fehler("--as " + als + " braucht --addr (Anfangsadresse im Speicher)");
    auto m = ladeBand(pos[0]);
    if (!m) return 1;
    // Datei N = Blockfolge zwischen den Bandmarken (Marken selbst gehoeren nicht dazu).
    size_t n = m->anzahlBloecke(s), i = 0, dateien = 0;
    std::vector<uint8_t> out;
    bool gefunden = false;
    while (i < n) {
        size_t j = i;
        std::vector<uint8_t> teil;
        bool hat = false;
        while (j < n && !m->block(s, j).bandmarke()) {
            const Block& b = m->block(s, j);
            if (!crcStimmt(b.bytes)) return fehler("Block " + std::to_string(j) + ": CRC falsch");
            teil.insert(teil.end(), b.bytes.begin() + 1, b.bytes.end() - 3);
            hat = true;
            ++j;
        }
        if (hat) {
            if (dateien == nr) { out = teil; gefunden = true; break; }
            ++dateien;
        }
        i = j + 1;
    }
    if (!gefunden) return fehler("Datei " + std::to_string(nr) + " nicht auf dem Band");
    if (fmt != pd::Format::Roh) {
        pd::Abbild ab;
        ab.anfang = static_cast<uint16_t>(std::strtoul(adr.c_str(), nullptr, 0));
        if (!start.empty()) ab.start = static_cast<uint16_t>(std::strtoul(start.c_str(), nullptr, 0));
        ab.name = name;
        ab.typ = typ.empty() ? 0 : typ[0];
        ab.daten = out;
        std::string fe;
        auto r = pd::schreiben(ab, fmt, fe);
        if (!r) return fehler(fe);
        out = *r;
    }
    return schreibeDatei(pos[1], out) ? 0 : fehler("kann '" + pos[1] + "' nicht schreiben");
}

int cmdBootband(const std::vector<std::string>& a) {
    std::string format;
    std::vector<std::string> p;
    for (size_t i = 0; i < a.size(); ++i) {
        if (a[i] == "--format" && i + 1 < a.size()) format = a[++i];
        else p.push_back(a[i]);
    }
    if (p.size() < 2 || p.size() > 3) return fehler("bootband <binaer> <band.k52> [A|B] [--format F]");
    Seite s = Seite::A;
    if (p.size() == 3 && !seiteParsen(p[2], s)) return fehler("Seite erwartet A oder B");
    int rc = 1;
    auto abbild = leseProgramm(p[0], format, rc);
    if (!abbild) return rc;
    // Das Lade-ROM legt das Band nach 0400H.  Eine Datei, die eine andere Anfangsadresse traegt,
    // wuerde dort falsch liegen (Spruenge/Daten wuerden verschoben) — abweisen statt raten.
    if (abbild->mit_adresse && abbild->anfang != 0x0400)
        return fehler("Anfangsadresse " + std::to_string(abbild->anfang) + " ist nicht 0400H - das Lade-ROM laedt das Band nach 0400H");
    std::vector<uint8_t> d = abbild->daten;
    if (d.empty() || d.size() > 512) return fehler("Bootprogramm muss 1 bis 512 Bytes lang sein");
    d.resize(512, 0x00);
    // Lade-ROM prueft (0400H..0402H) = 'SYL' (treiber.md §6.1); Einsprung bei Versatz 34H.
    if (d[0] != 'S' || d[1] != 'Y' || d[2] != 'L')
        return fehler("Bootprogramm muss mit 'SYL' beginnen (Pruefung des Lade-ROMs); Einsprung bei Versatz 34H");
    KassettenMedium m;
    for (size_t b = 0; b < 4; ++b) {
        std::vector<uint8_t> teil(d.begin() + static_cast<long>(b * 128),
                                  d.begin() + static_cast<long>(b * 128 + 128));
        if (!anhaengen(m, s, blockBauen(teil))) return fehler("Band voll");
    }
    return speichereBand(p[1], m) ? 0 : 1;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "Aufruf: k1520tape create|ls|put|get|bootband ... (siehe tools/k1520tape.md)\n");
        return 1;
    }
    std::string c = argv[1];
    std::vector<std::string> a(argv + 2, argv + argc);
    if (c == "create") return cmdCreate(a);
    if (c == "ls") return cmdLs(a);
    if (c == "put") return cmdPut(a);
    if (c == "get") return cmdGet(a);
    if (c == "bootband") return cmdBootband(a);
    return fehler("unbekanntes Kommando '" + c + "'");
}
