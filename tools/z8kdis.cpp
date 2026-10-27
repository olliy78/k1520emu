/**
 * @file z8kdis.cpp
 * @brief z8kdis — Z8000-Disassembler auf der Kommandozeile (Entwurf 41, AP K3).
 *
 *   z8kdis [optionen] abbild.bin
 *
 * Alles Inhaltliche steckt in tools/z8000/z8kdis.h (Kern, getestet in
 * tests/debugtools/test_z8kdis.cpp); hier nur Aufruf, Ein- und Ausgabe.
 * Exit: 0 = gut, 1 = Fehler in Eingabe/Karte oder Rundlauf weicht ab, 2 = Aufruf falsch.
 *
 * @license MIT
 */
#include "tools/z8000/z8kdis.h"

#include <cstdio>
#include <fstream>
#include <iostream>
#include <sstream>

namespace {

void usage() {
    std::fprintf(stderr,
        "Aufruf: z8kdis [optionen] abbild.bin\n"
        "  --base ADR            Ladeadresse (hex, Vorgabe 0); ohne --seg/Karte-seg ein Segment\n"
        "  --seg ADR:OFFSET:LEN  Ladesegment (hex, mehrfach); Adresse = Ladeadresse, Offset in der Datei\n"
        "  --karte DATEI         Kartendatei (Kopf von tools/z8000/z8kdis.h)\n"
        "  --entry ADR           Einstieg der Codeverfolgung (hex, mehrfach)\n"
        "  --psa ADR[:ANZAHL]    Programmstatus-Feld ab ADR (je 8 Byte; Vorgabe 8 Eintraege)\n"
        "  --format listing|as|z8k   Ausgabe (Vorgabe listing)\n"
        "                        as  = Quelle im Dialekt des MUTOS-as (PLZ/ASM)\n"
        "                        z8k = Quelle fuer z8kasm (Zilog-Syntax)\n"
        "  --modul NAME          Modulname der as-Ausgabe (Vorgabe mutos)\n"
        "  --segmentiert         U8001-Modus (Vorgabe nichtsegmentiert)\n"
        "  --statistik           Statistik (Code/Daten/unerreicht) auf stderr\n"
        "  --vorschlaege         Kandidaten fuer weitere Code-Einstiege als Kartenzeilen (nur Hinweis);\n"
        "                        die Statistik auf stderr zeigt zusaetzlich den Stand MIT allen Vorschlaegen\n"
        "  --pruefen             Rundlauf: Ausgabe (z8k) wieder zu Bytes, vergleichen; keine Ausgabe\n"
        "  -o DATEI              Ausgabe in Datei statt Standardausgabe\n");
}

bool lies(const std::string& p, std::string& out) {
    std::ifstream f(p, std::ios::binary);
    if (!f) return false;
    std::ostringstream ss; ss << f.rdbuf();
    out = ss.str();
    return true;
}

} // namespace

int main(int argc, char** argv) {
    z8kdis::Analyse an;
    std::string datei, kartePfad, aus;
    uint32_t base = 0;
    bool stat = false, pruefen = false, vorschlag = false;
    std::vector<uint32_t> entries;
    std::vector<std::pair<uint32_t, uint32_t>> psa;
    std::vector<z8kdis::KarteSeg> segs;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        auto arg = [&]() -> std::string { if (i + 1 >= argc) { usage(); std::exit(2); } return argv[++i]; };
        uint32_t x, y, z;
        if (a == "--base") { if (!z8kdis::Karte::zahl(arg(), base)) { usage(); return 2; } }
        else if (a == "--seg") {
            std::string s = arg();
            size_t p1 = s.find(':'), p2 = p1 == std::string::npos ? p1 : s.find(':', p1 + 1);
            if (p2 == std::string::npos || !z8kdis::Karte::zahl(s.substr(0, p1), x) ||
                !z8kdis::Karte::zahl(s.substr(p1 + 1, p2 - p1 - 1), y) || !z8kdis::Karte::zahl(s.substr(p2 + 1), z)) {
                std::fprintf(stderr, "z8kdis: --seg ADR:OFFSET:LEN erwartet\n"); return 2;
            }
            segs.push_back({x, y, z});
        }
        else if (a == "--karte") kartePfad = arg();
        else if (a == "--entry") { if (!z8kdis::Karte::zahl(arg(), x)) { usage(); return 2; } entries.push_back(x); }
        else if (a == "--psa") {
            std::string s = arg(); uint32_t n = 8;
            size_t p = s.find(':');
            if (p != std::string::npos && !z8kdis::Karte::zahl(s.substr(p + 1), n)) { usage(); return 2; }
            if (!z8kdis::Karte::zahl(s.substr(0, p), x)) { usage(); return 2; }
            psa.push_back({x, n});
        }
        else if (a == "--format") {
            std::string f = arg();
            if (f == "listing") an.opt.format = z8kdis::Format::Listing;
            else if (f == "as") an.opt.format = z8kdis::Format::As;
            else if (f == "z8k") an.opt.format = z8kdis::Format::Z8k;
            else { usage(); return 2; }
        }
        else if (a == "--modul") an.opt.modul = arg();
        else if (a == "--segmentiert") an.opt.segmentiert = true;
        else if (a == "--statistik") stat = true;
        else if (a == "--pruefen") pruefen = true;
        else if (a == "--vorschlaege") vorschlag = true;
        else if (a == "-o") aus = arg();
        else if (a == "-h" || a == "--help") { usage(); return 0; }
        else if (!a.empty() && a[0] == '-') { std::fprintf(stderr, "z8kdis: unbekannte Option %s\n", a.c_str()); usage(); return 2; }
        else datei = a;
    }
    if (datei.empty()) { usage(); return 2; }

    std::string bin;
    if (!lies(datei, bin)) { std::fprintf(stderr, "z8kdis: %s nicht lesbar\n", datei.c_str()); return 1; }
    if (!kartePfad.empty()) {
        std::string k, err;
        if (!lies(kartePfad, k)) { std::fprintf(stderr, "z8kdis: Karte %s nicht lesbar\n", kartePfad.c_str()); return 1; }
        if (!an.karte.lesen(k, err)) { std::fprintf(stderr, "z8kdis: %s\n", err.c_str()); return 1; }
    }
    for (const auto& s : segs) an.karte.segs.push_back(s);
    for (uint32_t e : entries) an.karte.code.push_back(e);
    for (const auto& p : psa) an.karte.psa.push_back(p);
    std::vector<uint8_t> daten(bin.begin(), bin.end());
    an.laden(daten, base);
    an.analysieren();

    if (vorschlag) {
        auto v = an.vorschlaege();
        for (const auto& e : v)
            std::printf("code %04X   # Zeiger bei %04X, %d Befehle\n", unsigned(e.adr & 0xFFFF), unsigned(e.quelle & 0xFFFF), e.befehle);
        // Gegenprobe: derselbe Abzug mit allen Vorschlaegen als Einstiege
        z8kdis::Analyse mit;
        mit.opt = an.opt;
        mit.karte = an.karte;
        for (const auto& e : v) mit.karte.code.push_back(e.adr);
        mit.laden(daten, base);
        mit.analysieren();
        std::fprintf(stderr, "%zu Vorschlaege; Stand mit allen Vorschlaegen:\n%s", v.size(), mit.statistikText().c_str());
        return 0;
    }
    if (pruefen) {
        std::string bericht;
        int d = an.pruefen(bericht);
        std::fputs(bericht.c_str(), stdout);
        if (stat) std::fputs(an.statistikText().c_str(), stderr);
        return d == 0 ? 0 : 1;
    }
    std::string text = an.ausgeben();
    if (aus.empty()) std::fputs(text.c_str(), stdout);
    else {
        std::ofstream f(aus, std::ios::binary);
        if (!f) { std::fprintf(stderr, "z8kdis: %s nicht schreibbar\n", aus.c_str()); return 1; }
        f << text;
    }
    if (stat) std::fputs(an.statistikText().c_str(), stderr);
    return 0;
}
