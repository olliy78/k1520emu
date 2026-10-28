/**
 * @file z8kdis.h
 * @brief z8kdis — Kommandozeilen-Disassembler fuer Z8000-Abzuege (Kern, header-only).
 *
 * Das Gegenstueck zu tools/z80_disasm2.py (Entwurf 40) fuer U8001/U8002 (Entwurf 41, AP K3):
 * dieselbe Kartendatei, dieselbe Idee (Abzug + Karte -> reproduzierbare Quelle), dieselbe
 * Pruefung (Rundlauf).  Die Befehle kommen AUSSCHLIESSLICH aus der Tabelle des Emulators
 * (core/primitives/z8000/z8k_table.h ueber tools/z8000/z8k_disasm.h) — es gibt keinen
 * zweiten Dekoder.
 *
 * Ausgabearten (Format):
 *   listing  Adresse, Bytes, Befehl, Kommentar — zum Lesen.
 *   z8k      Quelle fuer tools/z8000/z8k_asm.h (z8kasm).  Wird fuer den Rundlauf
 *            (`pruefen`) benutzt: sie ist das einzige, was wir HIER wieder zu Bytes machen koennen.
 *   as       Quelle im Dialekt des MUTOS-`as` (Zilog PLZ/ASM V1.0), s. doc/mutos_kernel/as_dialekt.md.
 *            Der Rundlauf gegen den Gast-`as` steht noch aus.
 *
 * Kartendatei (Zahlen hexadezimal, auch `%1F`/`0x1F`; `#` beginnt eine Kommentarzeile):
 *   seg  <adr> <datei-offset> <laenge>   ein Ladesegment der Datei
 *   code <adr> [<adr> ...]               Einstiege der Codeverfolgung
 *   lin  <von> <bis>                     linear als Code lesen (ohne Ablaufverfolgung; Pruefstand)
 *   db   <von> <bis>                     Daten, Bytes (bis einschliesslich)
 *   dw   <von> <bis>                     Daten, Woerter (big-endian); Werte auf Marken werden Namen
 *   dwc  <von> <bis>                     wie dw, die Ziele sind zugleich Code-Einstiege
 *   str  <von> <bis>                     Zeichenketten mit vorangestelltem Laengenbyte
 *   txt  <von> <bis>                     Text
 *   psa  <adr> <anzahl>                  Programmstatus-Feld: je 8 Byte [res, FCW, Segment, Offset]
 *   vek  <von> <bis>                     Vektortabelle: je 4 Byte [Kennung, Handler]
 *   name <adr> <marke>                   eigene Marke statt Lxxxx
 *   kom  <adr> <text>                    Kommentarzeile VOR der Stelle (mehrfach moeglich)
 *   ik   <adr> <text>                    Zeilenkommentar HINTER der Zeile
 *   eq   <marke> <wert>                  Marke ausserhalb der Segmente (EQU)
 *
 * Linkkarte (AP K5, Entwurf 41 §4.2; gleiche Datei oder eigene, `--karte` darf mehrfach stehen):
 *   modul <name> <tvon> <tbis> <dvon> <dbis>   ein Modul in Linkreihenfolge: Text [tvon,tbis), Daten [dvon,dbis)
 *                                        (hex, Ende AUSSCHLIESSLICH; `- -` = leer)
 *   bss  <von> <bis>                     Bereich der Common-/BSS-Variablen (Ende = `_end`)
 *   konst <adr> ...                      Befehle, deren Direktwert KEINE Adresse ist
 *   konstwert <wert> ...                 Werte, die nie Adressen sind (z. B. FCW C000)
 *   adr  <adr> ...                       Befehle, deren Direktwert im Textbereich eine Adresse ist
 *   dwp  <von> <bis>                     Woerter (Adresse bis einschl.), die Adressen sind
 *
 * @license MIT
 */
#pragma once
#include "tools/z8000/z8k_asm.h"
#include "tools/z8000/z8k_disasm.h"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace z8kdis {

// ── Karte ────────────────────────────────────────────────────────────────────

enum class Art : uint8_t { Rest, Db, Dw, Dwc, Str, Txt };

struct KarteSeg { uint32_t adr = 0, off = 0, len = 0; };
struct KarteBereich { Art art; uint32_t von, bis; };

struct Karte {
    std::vector<KarteSeg> segs;
    std::vector<uint32_t> code;
    std::vector<std::pair<uint32_t, uint32_t>> lin;
    std::vector<KarteBereich> daten;
    std::vector<std::pair<uint32_t, uint32_t>> psa;   ///< Adresse, Anzahl Eintraege
    std::vector<std::pair<uint32_t, uint32_t>> vek;   ///< von, bis
    std::map<uint32_t, std::string> name;
    std::multimap<uint32_t, std::string> kom, ik;
    std::vector<std::pair<std::string, uint32_t>> eq;
    // Linkkarte
    struct Modul { std::string name; uint32_t tvon = 0, tbis = 0, dvon = 0, dbis = 0; };
    std::vector<Modul> moduln;
    bool bssGesetzt = false;
    uint32_t bssVon = 0, bssBis = 0;
    std::set<uint32_t> konst, konstWert, adr;
    std::vector<std::pair<uint32_t, uint32_t>> dwp;

    static bool zahl(const std::string& s, uint32_t& v) {
        std::string t = s;
        if (!t.empty() && t[0] == '%') t = t.substr(1);
        else if (t.size() > 2 && t[0] == '0' && (t[1] == 'x' || t[1] == 'X')) t = t.substr(2);
        if (t.empty()) return false;
        char* e = nullptr;
        unsigned long x = std::strtoul(t.c_str(), &e, 16);
        if (*e) return false;
        v = uint32_t(x);
        return true;
    }

    /// Text einlesen; Fehler als Meldung mit Zeilennummer.  false = Fehler.
    bool lesen(const std::string& text, std::string& err) {
        std::istringstream in(text);
        std::string zeile;
        int n = 0;
        while (std::getline(in, zeile)) {
            ++n;
            if (!zeile.empty() && zeile.back() == '\r') zeile.pop_back();
            size_t b = zeile.find_first_not_of(" \t");
            if (b == std::string::npos || zeile[b] == '#') continue;
            std::istringstream ls(zeile.substr(b));
            std::string kw; ls >> kw;
            auto fail = [&](const char* m) {
                err = "Karte Zeile " + std::to_string(n) + ": " + m + " (" + kw + ")";
                return false;
            };
            std::vector<std::string> a;
            std::string t;
            auto rest = [&]() {                       // Rest der Zeile als Text
                std::string r; std::getline(ls, r);
                size_t p = r.find_first_not_of(" \t");
                return p == std::string::npos ? std::string() : r.substr(p);
            };
            uint32_t x, y, z;
            if (kw == "seg") {
                while (ls >> t && t[0] != '#') a.push_back(t);
                if (a.size() != 3 || !zahl(a[0], x) || !zahl(a[1], y) || !zahl(a[2], z)) return fail("seg adr offset laenge");
                segs.push_back({x, y, z});
            } else if (kw == "code") {
                while (ls >> t && t[0] != '#') { if (!zahl(t, x)) return fail("Zahl erwartet"); code.push_back(x); }
            } else if (kw == "lin" || kw == "db" || kw == "dw" || kw == "dwc" || kw == "str" || kw == "txt" || kw == "vek") {
                while (ls >> t && t[0] != '#') a.push_back(t);
                if (a.size() != 2 || !zahl(a[0], x) || !zahl(a[1], y) || y < x) return fail("von bis erwartet");
                if (kw == "lin") lin.push_back({x, y});
                else if (kw == "vek") vek.push_back({x, y});
                else daten.push_back({kw == "db" ? Art::Db : kw == "dw" ? Art::Dw : kw == "dwc" ? Art::Dwc
                                      : kw == "str" ? Art::Str : Art::Txt, x, y});
            } else if (kw == "psa") {
                while (ls >> t && t[0] != '#') a.push_back(t);
                if (a.size() != 2 || !zahl(a[0], x) || !zahl(a[1], y)) return fail("adr anzahl erwartet");
                psa.push_back({x, y});
            } else if (kw == "name") {
                ls >> t; std::string m; ls >> m;
                if (!zahl(t, x) || m.empty()) return fail("adr marke erwartet");
                name[x] = m;
            } else if (kw == "kom" || kw == "ik") {
                ls >> t;
                if (!zahl(t, x)) return fail("adr text erwartet");
                (kw == "kom" ? kom : ik).insert({x, rest()});
            } else if (kw == "modul") {
                while (ls >> t && t[0] != '#') a.push_back(t);
                auto num = [&](const std::string& q, uint32_t& v) { if (q == "-") { v = 0; return true; } return zahl(q, v); };
                Modul m;
                if (a.size() != 5 || !num(a[1], m.tvon) || !num(a[2], m.tbis) || !num(a[3], m.dvon) || !num(a[4], m.dbis) ||
                    m.tbis < m.tvon || m.dbis < m.dvon)
                    return fail("modul name tvon tbis dvon dbis");
                m.name = a[0];
                moduln.push_back(m);
            } else if (kw == "bss") {
                while (ls >> t && t[0] != '#') a.push_back(t);
                if (a.size() != 2 || !zahl(a[0], x) || !zahl(a[1], y) || y < x) return fail("von bis erwartet");
                bssGesetzt = true; bssVon = x; bssBis = y;
            } else if (kw == "konst" || kw == "konstwert" || kw == "adr") {
                while (ls >> t && t[0] != '#') {
                    if (!zahl(t, x)) return fail("Zahl erwartet");
                    (kw == "konst" ? konst : kw == "konstwert" ? konstWert : adr).insert(x);
                }
            } else if (kw == "dwp") {
                while (ls >> t && t[0] != '#') a.push_back(t);
                if (a.size() != 2 || !zahl(a[0], x) || !zahl(a[1], y) || y < x) return fail("von bis erwartet");
                dwp.push_back({x, y});
            } else if (kw == "eq") {
                std::string m; ls >> m >> t;
                if (m.empty() || !zahl(t, x)) return fail("marke wert erwartet");
                eq.push_back({m, x});
            } else {
                return fail("unbekanntes Schluesselwort");
            }
        }
        return true;
    }
};

// ── Analyse ──────────────────────────────────────────────────────────────────

enum class Format { Listing, Z8k, As };

struct Optionen {
    bool segmentiert = false;       ///< U8001-Modus (Vorgabe nichtsegmentiert)
    Format format = Format::Listing;
    std::string modul = "mutos";    ///< Modulname der As-Ausgabe
    bool teil = false;              ///< As-Ausgabe nur fuer [teilVon, teilBis) (AP K4: Kernel in Stuecken)
    uint32_t teilVon = 0, teilBis = 0;
};

struct Statistik {
    uint32_t gesamt = 0;            ///< Bytes im Abbild
    uint32_t code = 0, befehle = 0; ///< als Code erreichte Bytes / Befehle
    uint32_t daten = 0;             ///< von der Karte als Daten erklaert (db/dw/dwc/str/txt/psa/vek)
    uint32_t unerreicht = 0;        ///< weder Code noch erklaert
    uint32_t einstiege = 0;         ///< Einstiegspunkte der Ablaufverfolgung
    uint32_t ungueltig = 0;         ///< Einstiege/Sprungziele, an denen kein Befehl steht
    uint32_t ueberlappung = 0;      ///< Ablauf stoesst auf Daten, Fremdes oder die Mitte eines Befehls
    uint32_t nicht_kanonisch = 0;   ///< Befehle, die der Assembler nicht bitgleich zurueckbaut (werden Daten)
    uint32_t marken_mitten = 0;     ///< Marken, die in keinem Zeilenanfang liegen (als EQU ausgegeben)
};

/// Eine Ausgabeeinheit (Befehl oder Datenzeile).
struct Einheit {
    uint32_t adr = 0;
    uint32_t len = 0;
    enum Typ { Befehl, Db, Dw, Str, Txt } typ = Db;
    z8k::Line line;                 // nur Befehl
    std::vector<bool> wortMarke;    // nur Dw: Wort darf Marke werden
    bool codeBereich = false;       // Teil der Ablaufverfolgung (Befehl) oder Daten der Karte
};

class Analyse {
public:
    Optionen opt;
    Karte karte;
    Statistik stat;

    /// Abbild laden: `daten` ab Dateioffset 0, Ladeadresse `adr` — oder die Segmente der Karte.
    void laden(const std::vector<uint8_t>& daten, uint32_t adr) {
        if (karte.segs.empty()) karte.segs.push_back({adr, 0, uint32_t(daten.size())});
        for (const auto& s : karte.segs) {
            segList_.push_back(s);
            for (uint32_t i = 0; i < s.len && s.off + i < daten.size(); ++i)
                mem_[s.adr + i] = daten[s.off + i];
        }
        std::sort(segList_.begin(), segList_.end(), [](const KarteSeg& a, const KarteSeg& b) { return a.adr < b.adr; });
        stat.gesamt = uint32_t(mem_.size());
    }

    bool imAbbild(uint32_t a) const { return mem_.count(a) != 0; }

    /// Ablaufverfolgung, Marken, Zeilenaufteilung.
    void analysieren() {
        deklarieren();
        // Einstiege sammeln
        std::vector<uint32_t> arbeit;
        auto einstieg = [&](uint32_t a) { if (imAbbild(a)) arbeit.push_back(a); };
        for (uint32_t a : karte.code) einstieg(a);
        for (uint32_t a : wortCode_) {
            uint32_t v;
            if (wort(a, v)) einstieg(zielAdr(a, v));
        }
        // Ablaufverfolgung (Arbeitsliste, Ziele kommen dazu)
        for (size_t i = 0; i < arbeit.size(); ++i) {
            ++stat.einstiege;
            verfolge(arbeit[i], arbeit);
        }
        for (const auto& l : karte.lin) linear(l.first, l.second);
        for (const auto& kv : code_) gesehen_.insert(kv.second.dec.insn->index);
        kanonischPruefen();
        // Marken: alles, worauf gezeigt wird
        for (const auto& kv : karte.name) gewuenscht_.insert(kv.first);
        for (uint32_t a : karte.code) gewuenscht_.insert(a);
        link_ = !karte.moduln.empty();
        if (link_) linkVorbereiten();
        for (auto& kv : code_) {
            const z8k::Line& l = kv.second;
            if (l.hasTarget) gewuenscht_.insert(lin(l.targetSeg, l.target));
            if (link_) { linkOperanden(kv.first, l); continue; }
            for (int i = 0; i < l.dec.nops; ++i) {
                const z8k::Operand& o = l.dec.op[i];
                // Indizierte Adressen unterhalb von 0100 sind meist Strukturversaetze (`4(R2)`),
                // keine Tabellen -- sie bekommen keine eigene Marke.
                if (o.kind == z8k::Kind::DA || (o.kind == z8k::Kind::X && o.value >= 0x100))
                    if (!o.shortSeg) gewuenscht_.insert(lin(l.dec.seg ? o.seg : 0, o.value));
            }
        }
        if (link_) linkWoerter();
        for (const auto& kv : wortArt_) {
            uint32_t v;
            // Art 1 (dw): nur Namen einsetzen, wo es die Marke ohnehin gibt -- sonst wuerde jede
            // kleine Zahl (0000, 4000 ...) zur Marke; Art 2 (dwc, psa, vek) legt die Marke an.
            if (kv.second == 2 && wort(kv.first, v)) gewuenscht_.insert(zielAdr(kv.first, v));
        }
        for (auto it = gewuenscht_.begin(); it != gewuenscht_.end();)
            it = imAbbild(*it) ? std::next(it) : gewuenscht_.erase(it);
        aufteilen();
        // nutzbare Marken = gewuenschte Marken auf einem Zeilenanfang
        for (uint32_t a : gewuenscht_) {
            if (einheitStart_.count(a)) {
                auto n = karte.name.find(a);
                marke_[a] = n != karte.name.end() ? n->second : autoName(a);
                // Linkmodus: der as/ld unterscheidet nur 8 Zeichen -- laengere Namen bekommen die Adressmarke
                if (link_) {
                    // nur die ersten 8 Zeichen zaehlen: zu lange oder in 8 Zeichen doppelte Namen -> Adressmarke
                    std::string k = marke_[a].substr(0, 8);
                    if (marke_[a].size() > 8 || !acht_.insert(k).second) { langName_[a] = marke_[a]; marke_[a] = autoName(a); }
                }
            } else {
                ++stat.marken_mitten;
            }
        }
        statistik();
    }

    /// Kandidaten fuer weitere Code-Einstiege: Zeigerwerte (Woerter im Abbild und Direktwerte von
    /// Befehlen), die auf noch nicht erreichte, gerade Stellen zeigen und dort mindestens `minBefehle`
    /// gueltige Befehle bis zu einem Ablaufende (RET/JP/JR/IRET) ergeben.  Nur ein HINWEIS fuer die
    /// Karte -- ein Zahlenwert, der wie ein Zeiger aussieht, ist keiner (Entwurf 41 §4.2).
    struct Vorschlag { uint32_t adr; uint32_t quelle; int befehle; };
    std::vector<Vorschlag> vorschlaege(int minBefehle = 4) const {
        std::map<uint32_t, uint32_t> kand;                       // Ziel -> erste Quelle
        for (const auto& kv : mem_) {
            uint32_t a = kv.first;
            if (!gerade(a) || codeStart_.count(a) || decl_.count(a)) continue;
            uint32_t v;
            if (wort(a, v)) { uint32_t t = zielAdr(a, v); if (!kand.count(t)) kand[t] = a; }
        }
        for (const auto& kv : code_) {
            const z8k::Line& l = kv.second;
            for (int i = 0; i < l.dec.nops; ++i)
                if (l.dec.op[i].kind == z8k::Kind::IMW && !kand.count(l.dec.op[i].value & 0xFFFF))
                    kand[l.dec.op[i].value & 0xFFFF] = kv.first;
        }
        std::vector<Vorschlag> r;
        for (const auto& k : kand) {
            uint32_t a = k.first;
            if (!imAbbild(a) || !gerade(a) || codeStart_.count(a) || decl_.count(a)) continue;
            int n = 0; bool ok = false;
            for (uint32_t c = a; n < 2000; ) {
                if (code_.count(c)) { ok = n > 0; break; }             // trifft bekannten Code
                if (!imAbbild(c) || decl_.count(c) || codeStart_.count(c)) break;
                z8k::Line l = holen(c);
                if (!l.dec.insn) break;
                bool frei = true;
                for (int i = 0; i < l.bytes; ++i)
                    if (!imAbbild(c + i) || decl_.count(c + i) || codeStart_.count(c + i)) frei = false;
                if (!frei) break;
                ++n;
                if (endetAblauf(l)) { ok = true; break; }
                c += l.bytes;
            }
            if (ok && n >= minBefehle) r.push_back({a, k.second, n});
        }
        return r;
    }

    /// Alle Operanden der Befehle, die Adressen sein koennen (DA, X, IMW, IML): eine Zeile je Operand
    /// `ADR ART WERT BEFEHL` -- Grundlage der Linkkarte (AP K5, Entwurf 41 §4.2).
    std::string referenzen() const {
        std::string o; char b[160];
        for (const auto& kv : code_) {
            const z8k::Line& l = kv.second;
            for (int i = 0; i < l.dec.nops; ++i) {
                const z8k::Operand& op = l.dec.op[i];
                const char* art = op.kind == z8k::Kind::DA ? "DA" : op.kind == z8k::Kind::X ? "X" :
                                  op.kind == z8k::Kind::IMW ? "IMW" : op.kind == z8k::Kind::IML ? "IML" : nullptr;
                if (!art) continue;
                std::snprintf(b, sizeof b, "%04X %s %04X %s\n", unsigned(kv.first & 0xFFFF), art,
                              unsigned(op.value & 0xFFFF), l.text.c_str());
                o += b;
            }
        }
        return o;
    }

    /// Tabellenzeilen (Insn::index), die als Code dekodiert wurden (vor der Kanonisch-Pruefung) -- fuer Tests.
    const std::set<int>& gesehen() const { return gesehen_; }
    const std::vector<Einheit>& einheiten() const { return einheiten_; }
    const std::map<uint32_t, std::string>& marken() const { return marke_; }

    // ── Ausgabe ──────────────────────────────────────────────────────────────
    std::string ausgeben() const {
        switch (opt.format) {
        case Format::Listing: return textQuelle(false);
        case Format::Z8k:     return textQuelle(true);
        case Format::As:      return asQuelle();
        }
        return {};
    }

    /// Rundlauf: Z8k-Quelle assemblieren und mit dem Abbild vergleichen.
    /// Rueckgabe: Anzahl abweichender Bytes (-1 = Assemblerfehler); `bericht` erhaelt die Einzelheiten.
    int pruefen(std::string& bericht) const {
        Optionen o2 = opt; o2.format = Format::Z8k;
        Analyse kopie = *this; kopie.opt = o2;
        std::string quelle = kopie.ausgeben();
        z8k::AsmOptions ao; ao.seg = opt.segmentiert;
        z8k::AsmResult r = z8k::assemble(quelle, ao);
        std::ostringstream os;
        if (!r.ok()) {
            for (size_t i = 0; i < r.errors.size() && i < 10; ++i)
                os << "Zeile " << r.errors[i].line << ": " << r.errors[i].text << "\n";
            bericht = os.str();
            return -1;
        }
        int diff = 0; uint32_t vergl = 0;
        for (const auto& kv : mem_) {
            uint32_t schluessel = opt.segmentiert ? kv.first : (kv.first & 0xFFFF);
            auto it = r.image.find(schluessel);
            ++vergl;
            if (it == r.image.end() || it->second != kv.second) {
                if (diff < 10) {
                    char b[96];
                    std::snprintf(b, sizeof b, "Abweichung bei %04X: Abbild %02X, Rundlauf %s%02X\n",
                                  unsigned(kv.first), kv.second, it == r.image.end() ? "fehlt " : "", it == r.image.end() ? 0 : it->second);
                    os << b;
                }
                ++diff;
            }
        }
        os << "Rundlauf: " << vergl << " Bytes verglichen, " << diff << " abweichend\n";
        bericht = os.str();
        return diff;
    }

    std::string statistikText() const {
        char b[512];
        std::snprintf(b, sizeof b,
            "Abbild      %6u Bytes\n"
            "Code        %6u Bytes (%u Befehle) %5.1f %%\n"
            "Daten (Karte)%5u Bytes %5.1f %%\n"
            "Unerreicht  %6u Bytes %5.1f %%\n"
            "Einstiege %u, ungueltig %u, Ueberlappung %u, nicht kanonisch (-> Daten) %u, Marken mitten im Befehl %u, Marken %zu\n",
            stat.gesamt, stat.code, stat.befehle, stat.gesamt ? 100.0 * stat.code / stat.gesamt : 0.0,
            stat.daten, stat.gesamt ? 100.0 * stat.daten / stat.gesamt : 0.0,
            stat.unerreicht, stat.gesamt ? 100.0 * stat.unerreicht / stat.gesamt : 0.0,
            stat.einstiege, stat.ungueltig, stat.ueberlappung, stat.nicht_kanonisch, stat.marken_mitten, marke_.size());
        return b;
    }

private:
    std::map<uint32_t, uint8_t> mem_;
    std::vector<KarteSeg> segList_;
    std::map<uint32_t, z8k::Line> code_;       // Befehlsanfang -> Befehl
    std::map<uint32_t, uint32_t> codeStart_;   // jedes Byte eines Befehls -> Anfang
    std::map<uint32_t, Art> decl_;             // erklaerte Datenbytes
    std::map<uint32_t, uint8_t> wortArt_;      // Wortadresse -> 0 nur Zahl, 1 Marke, 2 Marke + Code
    std::set<uint32_t> wortCode_;              // Woerter, deren Ziel Code ist
    std::set<uint32_t> gewuenscht_;
    std::vector<Einheit> einheiten_;
    std::set<uint32_t> einheitStart_;
    std::map<uint32_t, std::string> marke_;
    // ── Linkmodus (AP K5) ──
    bool link_ = false;
    uint32_t datenAb_ = 0, bssVon_ = 0, bssBis_ = 0;
    std::set<uint32_t> bssRef_;                    ///< gerade Basisadressen der referenzierten BSS-Variablen
    std::set<uint32_t> callZiel_;                  ///< Ziele von CALL (Funktionsanfaenge)
    std::set<std::string> acht_;
    std::map<uint32_t, std::string> langName_;     ///< Namen ueber 8 Zeichen (nur Kommentar)

    bool inBss(uint32_t v) const { return v >= bssVon_ && v < bssBis_; }

    std::string bssName(uint32_t basis) const {
        auto n = karte.name.find(basis);
        return n != karte.name.end() && n->second.size() <= 8 ? n->second : autoName(basis);
    }

    /// Adresse v als Symbolausdruck, wenn sie im Linkmodus ein Symbol ist (Marke, BSS-Variable, `_end`).
    bool symbolText(uint32_t v, std::string& t) const {
        if (v == bssBis_ && bssBis_) { t = "_end"; return true; }
        if (inBss(v)) { t = bssName(v & ~1u) + ((v & 1) ? "+%1" : ""); return true; }
        auto m = marke_.find(v);
        if (m == marke_.end()) return false;
        t = m->second;
        return true;
    }

    /// Ist der Direktwert-Operand i des Befehls bei pc eine Adresse?  (Regeln: Kopf der Karte / Entwurf 41 §4.2)
    bool immAdresse(uint32_t pc, const z8k::Line& l, int i) const {
        const z8k::Operand& o = l.dec.op[i];
        if (o.kind != z8k::Kind::IMW) return false;
        uint32_t v = o.value & 0xFFFF;
        if (karte.konst.count(pc)) return false;
        if (karte.adr.count(pc)) return true;
        if (karte.konstWert.count(v)) return false;
        if (v >= datenAb_ && v <= bssBis_) return true;
        if (v < 0x100) return false;
        const char* mn = l.dec.insn->mn;
        // Funktionszeiger werden nur mit PUSH/LD uebergeben; `AND R2,#%7F00` trifft zufaellig einen Funktionsanfang
        return (!std::strcmp(mn, "PUSH") || !std::strcmp(mn, "LD")) &&
               (callZiel_.count(v) || karte.name.count(v) || std::find(karte.code.begin(), karte.code.end(), v) != karte.code.end());
    }

    void linkAdresse(uint32_t v) {                 // Ziel einer Adresse vormerken
        if (v == bssBis_ && bssBis_) return;
        if (inBss(v)) bssRef_.insert(v & ~1u);
        else if (imAbbild(v)) gewuenscht_.insert(v);
    }

    void linkVorbereiten() {
        datenAb_ = ~0u;
        for (const auto& m : karte.moduln) if (m.dbis > m.dvon) datenAb_ = std::min(datenAb_, m.dvon);
        if (datenAb_ == ~0u) datenAb_ = 0;
        bssVon_ = karte.bssGesetzt ? karte.bssVon : 0;
        bssBis_ = karte.bssGesetzt ? karte.bssBis : 0;
        for (const auto& kv : code_) {
            const z8k::Line& l = kv.second;
            if (l.hasTarget && l.dec.insn && !std::strcmp(l.dec.insn->mn, "CALL")) callZiel_.insert(l.target);
        }
        for (const auto& m : karte.moduln) {          // Modulgrenzen sind Zeilengrenzen
            if (imAbbild(m.tvon)) gewuenscht_.insert(m.tvon);
            if (m.dbis > m.dvon && imAbbild(m.dvon)) gewuenscht_.insert(m.dvon);
        }
    }

    void linkOperanden(uint32_t pc, const z8k::Line& l) {
        for (int i = 0; i < l.dec.nops; ++i) {
            const z8k::Operand& o = l.dec.op[i];
            uint32_t v = o.value & 0xFFFF;
            if (o.kind == z8k::Kind::DA && !o.shortSeg) {
                linkAdresse(v);
            } else if (o.kind == z8k::Kind::X && !o.shortSeg) {
                if (v >= datenAb_ && v < bssBis_) linkAdresse(v);
            } else if (immAdresse(pc, l, i)) {
                linkAdresse(v);
            }
        }
    }

    /// Datenwoerter, die Adressen sind: Marken bzw. BSS-Symbole vormerken.
    void linkWoerter() {
        for (const auto& kv : wortArt_) {
            uint32_t v;
            if (!wort(kv.first, v)) continue;
            if (kv.second == 1 && v >= datenAb_ && v <= bssBis_ && !karte.konstWert.count(v)) linkAdresse(v);
        }
        for (const auto& r : karte.dwp)
            for (uint32_t a = r.first; a <= r.second; a += 2) {
                uint32_t v;
                if (wort(a, v)) linkAdresse(v);
            }
    }

    /// Ist das Datenwort bei a (Wert v) eine Adresse?  (nach der Markenbildung)
    bool wortIstAdresse(uint32_t a, uint32_t v) const {
        for (const auto& r : karte.dwp) if (a >= r.first && a <= r.second) return true;
        auto w = wortArt_.find(a);
        if (w == wortArt_.end() || w->second == 0) return false;
        if (w->second == 2) return true;
        if (karte.konstWert.count(v)) return false;
        if (v >= datenAb_ && v <= bssBis_) return true;
        return v >= 0x400 && marke_.count(v) && (callZiel_.count(v) || karte.name.count(v) || code_.count(v));
    }

    static uint32_t lin(unsigned seg, uint32_t off) { return (uint32_t(seg) << 16) | (off & 0xFFFF); }
    uint32_t zielAdr(uint32_t wortAdr, uint32_t v) const {
        return opt.segmentiert ? lin((wortAdr >> 16) & 0x7F, v) : v;
    }
    bool wort(uint32_t a, uint32_t& v) const {
        auto h = mem_.find(a), l = mem_.find(a + 1);
        if (h == mem_.end() || l == mem_.end()) return false;
        v = (uint32_t(h->second) << 8) | l->second;
        return true;
    }
    std::string autoName(uint32_t a) const {
        char b[24];
        if (opt.segmentiert) std::snprintf(b, sizeof b, "L%02X%04X", unsigned(a >> 16), unsigned(a & 0xFFFF));
        else std::snprintf(b, sizeof b, "L%04X", unsigned(a & 0xFFFF));
        return b;
    }

    void deklarieren() {
        for (const auto& d : karte.daten)
            for (uint32_t a = d.von; a <= d.bis; ++a) {
                decl_[a] = d.art;
                if ((d.art == Art::Dw || d.art == Art::Dwc) && ((a - d.von) % 2 == 0)) {
                    wortArt_[a] = d.art == Art::Dwc ? 2 : 1;
                    if (d.art == Art::Dwc) wortCode_.insert(a);
                }
            }
        // dwp: Woerter, die Adressen sind -- als Wortdaten fuehren (auch wo die Karte `db` sagt)
        for (const auto& r : karte.dwp)
            for (uint32_t a = r.first; a <= r.second; a += 2) {
                decl_[a] = Art::Dw; decl_[a + 1] = Art::Dw;
                wortArt_[a] = 1;
            }
        // PSA: je 8 Byte [reserviert, FCW, Segment, Offset]; der Offset ist ein Code-Einstieg.
        static const char* const psaName[8] = {"Reset", "Erweiterter Befehl", "Privilegierter Befehl",
                                               "Systemaufruf", "Segmentfehler", "NMI", "NVI", "VI"};
        for (const auto& p : karte.psa)
            for (uint32_t i = 0; i < p.second; ++i) {
                uint32_t e = p.first + 8 * i;
                for (uint32_t k = 0; k < 8; ++k) decl_[e + k] = Art::Dw;
                wortArt_[e] = 0; wortArt_[e + 2] = 0; wortArt_[e + 4] = 0;
                wortArt_[e + 6] = 2; wortCode_.insert(e + 6);
                if (i < 8) psaKom_.insert({e, std::string("PSA-Eintrag ") + psaName[i] + ": reserviert, FCW, Segment, Handler"});
            }
        for (const auto& v : karte.vek)
            for (uint32_t e = v.first; e + 3 <= v.second; e += 4) {
                for (uint32_t k = 0; k < 4; ++k) decl_[e + k] = Art::Dw;
                wortArt_[e] = 0; wortArt_[e + 2] = 2; wortCode_.insert(e + 2);
            }
    }

    bool gerade(uint32_t a) const { return ((a & 1) == 0); }

    // Fluss: endet der Ablauf nach diesem Befehl?
    static bool endetAblauf(const z8k::Line& l) {
        const z8k::Insn& in = *l.dec.insn;
        if (!std::strcmp(in.mn, "IRET")) return true;
        if (std::strcmp(in.mn, "JP") && std::strcmp(in.mn, "JR") && std::strcmp(in.mn, "RET")) return false;
        for (int i = 0; i < l.dec.nops; ++i)
            if (l.dec.op[i].kind == z8k::Kind::CC) return l.dec.op[i].value == 8;
        return true;                                           // ohne Bedingung = immer
    }

    z8k::Line holen(uint32_t a) const {
        unsigned sg = opt.segmentiert ? (a >> 16) & 0x7F : 0;
        auto rd = [&](uint16_t off) -> uint16_t {
            uint32_t v;
            return wort(lin(sg, off), v) ? uint16_t(v) : uint16_t(0);
        };
        return z8k::disasm(rd, uint16_t(a & 0xFFFF), opt.segmentiert, uint8_t(sg));
    }

    // Zeile eintragen; false = Konflikt
    bool eintragen(uint32_t a, const z8k::Line& l) {
        for (int i = 0; i < l.bytes; ++i) {
            uint32_t b = a + i;
            if (!imAbbild(b) || decl_.count(b)) { ++stat.ueberlappung; return false; }
            auto c = codeStart_.find(b);
            if (c != codeStart_.end() && !(i == 0 && c->second == a)) { ++stat.ueberlappung; return false; }
        }
        code_[a] = l;
        for (int i = 0; i < l.bytes; ++i) codeStart_[a + i] = a;
        return true;
    }

    void verfolge(uint32_t a, std::vector<uint32_t>& arbeit) {
        while (true) {
            if (code_.count(a)) return;                          // schon bekannt
            if (!imAbbild(a) || !gerade(a)) { ++stat.ungueltig; return; }
            if (decl_.count(a)) { ++stat.ueberlappung; return; }
            z8k::Line l = holen(a);
            if (!l.dec.insn) { ++stat.ungueltig; return; }
            if (!eintragen(a, l)) return;
            if (l.hasTarget) {
                uint32_t t = lin(l.targetSeg, l.target);
                if (imAbbild(t) && !code_.count(t)) arbeit.push_back(t);
            }
            if (endetAblauf(l)) return;
            a += l.bytes;
        }
    }

    /// Nur Befehle, deren Text der Assembler wieder zu EXAKT denselben Bytes macht, bleiben Code: es gibt
    /// Kodierungen, die die CPU gleich behandelt, der Assembler aber anders schreibt (nicht ausgewertete
    /// Felder, Zweitkodierungen, ungerade Registerpaare).  Richtig ist, was beim Neubau BYTEGLEICH ist --
    /// ob eine Stelle als Code oder Daten gefuehrt wird, darf falsch sein (wie bei z80_disasm2.py).
    void kanonischPruefen() {
        std::vector<uint32_t> weg;
        for (const auto& kv : code_) {
            const z8k::Line& l = kv.second;
            std::vector<uint16_t> w; std::string err;
            unsigned sg = opt.segmentiert ? (kv.first >> 16) & 0x7F : 0;
            bool gleich = z8k::assembleLine(l.text, opt.segmentiert, uint8_t(sg), uint16_t(kv.first & 0xFFFF), w, err)
                          && int(w.size()) * 2 == l.bytes;
            for (int i = 0; gleich && i < l.bytes; i += 2) {
                uint32_t v = 0;
                gleich = wort(kv.first + i, v) && uint16_t(v) == w[size_t(i / 2)];
            }
            if (!gleich) weg.push_back(kv.first);
        }
        for (uint32_t a : weg) {
            const z8k::Line& l = code_[a];
            demotiert_[a] = l.text;
            for (int i = 0; i < l.bytes; ++i) codeStart_.erase(a + i);
            code_.erase(a);
            ++stat.nicht_kanonisch;
        }
    }

    void linear(uint32_t von, uint32_t bis) {
        uint32_t a = von;
        while (a <= bis && imAbbild(a)) {
            if (code_.count(a)) { a += code_[a].bytes; continue; }
            z8k::Line l = holen(a);
            if (!l.dec.insn || !gerade(a) || !eintragen(a, l)) { a += 2; continue; }
            a += l.bytes;
        }
    }

    // ── Zeilenaufteilung ─────────────────────────────────────────────────────
    void aufteilen() {
        std::set<uint32_t> wunsch = gewuenscht_;                  // Schnittstellen der Datenzeilen
        for (const auto& d : demotiert_) wunsch.insert(d.first);
        for (const auto& s : segList_) {
            uint32_t a = s.adr, ende = s.adr + s.len;
            while (a < ende) {
                auto c = code_.find(a);
                if (c != code_.end()) {
                    Einheit e; e.adr = a; e.len = uint32_t(c->second.bytes); e.typ = Einheit::Befehl; e.line = c->second;
                    e.codeBereich = true;
                    einheiten_.push_back(e); einheitStart_.insert(a);
                    a += e.len;
                    continue;
                }
                // Datenlauf bis zum naechsten Befehl bzw. Artwechsel
                auto d = decl_.find(a);
                Art art = d == decl_.end() ? Art::Rest : d->second;
                uint32_t b = a;
                while (b < ende && !code_.count(b)) {
                    auto db = decl_.find(b);
                    Art ba = db == decl_.end() ? Art::Rest : db->second;
                    if (ba != art) break;
                    ++b;
                }
                datenLauf(a, b, art, wunsch);
                a = b;
            }
        }
    }

    void datenLauf(uint32_t a, uint32_t b, Art art, const std::set<uint32_t>& wunsch) {
        auto neue = [&](uint32_t von, uint32_t len, Einheit::Typ t) {
            Einheit e; e.adr = von; e.len = len; e.typ = t; e.codeBereich = (art != Art::Rest);
            if (t == Einheit::Dw) {
                for (uint32_t k = 0; k < len; k += 2) {
                    auto w = wortArt_.find(von + k);
                    e.wortMarke.push_back(w != wortArt_.end() && w->second != 0);
                }
            }
            einheiten_.push_back(e); einheitStart_.insert(von);
        };
        if (art == Art::Str) {
            uint32_t c = a;
            while (c < b) {
                uint32_t n = mem_[c];
                if (c + 1 + n > b) break;
                neue(c, 1 + n, Einheit::Str);
                c += 1 + n;
            }
            a = c;                                         // Rest unregelmaessig -> Bytes
            art = Art::Db;
        }
        if (art == Art::Txt) {
            uint32_t c = a;
            while (c < b) {
                uint32_t e = std::min(b, c + 32);
                for (uint32_t k = c; k < e; ++k) if (mem_[k] == 0x0A) { e = k + 1; break; }
                neue(c, e - c, Einheit::Txt);
                c = e;
            }
            return;
        }
        uint32_t c = a;
        bool woerter = (art == Art::Dw || art == Art::Dwc || art == Art::Rest);
        while (c < b) {
            uint32_t e = std::min(b, c + 8);
            for (uint32_t k = c + 1; k < e; ++k) if (wunsch.count(k)) { e = k; break; }
            if (woerter && gerade(c) && e - c >= 2) {
                if ((e - c) & 1) --e;
                neue(c, e - c, Einheit::Dw);
            } else if (woerter && !gerade(c)) {
                neue(c, 1, Einheit::Db);
                e = c + 1;
            } else {
                neue(c, e - c, Einheit::Db);
            }
            c = e;
        }
    }

    void statistik() {
        for (const auto& s : segList_)
            for (uint32_t a = s.adr; a < s.adr + s.len; ++a) {
                if (codeStart_.count(a)) ++stat.code;
                else if (decl_.count(a)) ++stat.daten;
                else ++stat.unerreicht;
            }
        stat.befehle = uint32_t(code_.size());
    }

    // ── Text ─────────────────────────────────────────────────────────────────
    static std::string hex(uint32_t v, int n) {
        char b[16]; std::snprintf(b, sizeof b, "%%%0*X", n, unsigned(v)); return b;
    }

    std::string zielText(uint32_t a, const std::string& zahlText) const {
        auto m = marke_.find(a);
        return m == marke_.end() ? zahlText : m->second;
    }

    /// Befehlstext mit Marken statt Adressen.  `as`: Schreibweise des MUTOS-as.
    std::string befehlText(const z8k::Line& l, uint32_t pc, bool as) const {
        if (!l.dec.insn) return l.text;
        const z8k::Insn& in = *l.dec.insn;
        std::string s = as ? asKlein(in.mn) : std::string(in.mn);
        if (in.has(z8k::Z8K_ALT)) s += ".L";
        bool omitT = (!std::strcmp(in.mn, "JP") || !std::strcmp(in.mn, "JR") || !std::strcmp(in.mn, "RET"));
        std::string ops;
        uint16_t next = uint16_t(pc + l.bytes);
        unsigned pcSeg = opt.segmentiert ? (pc >> 16) & 0x7F : 0;
        for (int i = 0; i < l.dec.nops; ++i) {
            const z8k::Operand& o = l.dec.op[i];
            if (omitT && o.kind == z8k::Kind::CC && o.value == 8) continue;
            std::string t;
            auto fest = [&](const std::string& x) { return as ? asKlein(x) : x; };   // nur Nicht-Marken
            switch (o.kind) {
            case z8k::Kind::CC: t = as ? asBedingung(z8k::ccName(o.value)) : std::string(z8k::ccName(o.value)); break;
            case z8k::Kind::DA: case z8k::Kind::X: {
                if (as && link_ && !o.shortSeg) {
                    // Linkmodus: nur Adressen im Abbild/BSS werden Symbole; Strukturversaetze (X < Datenanfang) und
                    // absolute Adressen oberhalb des BSS (u-Bereich, Postfach) bleiben Zahlen
                    uint32_t v = o.value & 0xFFFF;
                    std::string st;
                    if ((o.kind == z8k::Kind::DA || (v >= datenAb_ && v < bssBis_)) && symbolText(v, st)) {
                        t = st;
                        if (o.kind == z8k::Kind::X) t += "(" + fest(z8k::detail::regName(z8k::Kind::RW, o.reg)) + ")";
                    } else {
                        t = fest(z8k::formatOperand(o, l.dec.seg, pcSeg, next));
                    }
                    break;
                }
                uint32_t ad = lin(l.dec.seg ? o.seg : 0, o.value);
                auto m = marke_.find(ad);
                bool strukturVersatz = (o.kind == z8k::Kind::X && o.value < 0x100);   // s. analysieren()
                if (o.shortSeg || strukturVersatz || m == marke_.end()) {
                    t = fest(z8k::formatOperand(o, l.dec.seg, pcSeg, next));
                } else {
                    t = m->second;
                    if (o.kind == z8k::Kind::X) t += "(" + fest(z8k::detail::regName(z8k::Kind::RW, o.reg)) + ")";
                }
                break;
            }
            case z8k::Kind::RA16: case z8k::Kind::RA8: case z8k::Kind::RA7: case z8k::Kind::RA12: {
                uint16_t tg = uint16_t(next + o.disp);
                t = zielText(lin(pcSeg, tg), z8k::formatOperand(o, l.dec.seg, pcSeg, next));
                break;
            }
            case z8k::Kind::IMW: {
                std::string st;
                if (as && link_ && immAdresse(pc, l, i) && symbolText(o.value & 0xFFFF, st)) t = "#" + st;
                else t = fest(z8k::formatOperand(o, l.dec.seg, pcSeg, next));
                break;
            }
            default: t = fest(z8k::formatOperand(o, l.dec.seg, pcSeg, next));
            }
            if (t.empty()) continue;
            if (!ops.empty()) ops += ",";
            ops += t;
        }
        if (!ops.empty()) s += " " + ops;
        return s;
    }

    /// Kleinschreibung wie `cc -S` fuer Mnemonik, Register und Bedingungen; Hexzahlen (`%ED4D`)
    /// bleiben, wie sie sind (cc schreibt sie gross).  Marken fasst das nicht an (sie werden nie hierdurch geschickt).
    static std::string asKlein(const std::string& x) {
        std::string r;
        for (size_t i = 0; i < x.size(); ++i) {
            if (x[i] == '%') {                                  // Hexzahl unveraendert
                r += x[i++];
                while (i < x.size() && std::isxdigit(static_cast<unsigned char>(x[i]))) r += x[i++];
                --i;
            } else {
                r += char(std::tolower(static_cast<unsigned char>(x[i])));
            }
        }
        // nichtsegmentiertes Steuerregister 5: der as kennt nur PSAPOFF (PSAP fehlt in seinen Zeichenketten)
        if (r == "ldctl psap" || r.find("psap") != std::string::npos) {
            size_t p = r.find("psap");
            if (r.compare(p, 7, "psapoff") != 0 && r.compare(p, 7, "psapseg") != 0) r.replace(p, 4, "psapoff");
        }
        return r;
    }

    /// Bedingungsnamen des as: er kennt EQ/NE/ULT/UGE (cc benutzt sie), Z/C und T/F stehen nicht in seiner
    /// Bedingungsliste -- gleiche Kodierung, deshalb abgebildet.
    static std::string asBedingung(const std::string& cc) {
        static const std::map<std::string, std::string> m = {{"Z", "eq"}, {"NZ", "ne"}, {"C", "ult"}, {"NC", "uge"}};
        auto it = m.find(cc);
        return it != m.end() ? it->second : asKlein(cc);
    }

    static std::string zeichen(uint8_t c) {
        if (c == '\'') return "%27";
        char b[8];
        std::snprintf(b, sizeof b, "'%c'", c);
        return b;
    }
    static bool druckbar(uint8_t c) { return c >= 0x20 && c < 0x7F && c != '\'' && c != '\\' && c != '"' && c != ';'; }

    /// Datenzeile.  as: je Wert eine eigene Anweisung `BV %xx;BV %xx` bzw. `WV %xxxx;WV ...` wie `cc -S`
    /// (belegt, doc/mutos_kernel/as_dialekt.md); sonst z8kasm-Schreibweise `DB a,b` / `DW a,b`.
    std::string datenText(const Einheit& e, bool as) const {
        std::string s;
        auto bv = [&](const std::string& z, bool erst) {
            return std::string(erst ? "" : (as ? ";" : ",")) + (as ? "BV " : (erst ? "DB " : "")) + z;
        };
        auto wv = [&](const std::string& z, bool erst) {
            return std::string(erst ? "" : (as ? ";" : ",")) + (as ? "WV " : (erst ? "DW " : "")) + z;
        };
        switch (e.typ) {
        case Einheit::Db:
            for (uint32_t k = 0; k < e.len; ++k) s += bv(hex(mem_.at(e.adr + k), 2), k == 0);
            break;
        case Einheit::Dw:
            for (uint32_t k = 0; k < e.len; k += 2) {
                uint32_t v = 0; wort(e.adr + k, v);
                std::string z = hex(v, 4);
                if (as && link_) {
                    std::string st;
                    if (wortIstAdresse(e.adr + k, v) && symbolText(v, st)) z = st;
                } else if (e.wortMarke[k / 2]) {
                    z = zielText(zielAdr(e.adr + k, v), z);
                }
                s += wv(z, k == 0);
            }
            break;
        case Einheit::Str:
        case Einheit::Txt:
            for (uint32_t k = 0; k < e.len; ++k) {
                uint8_t c = mem_.at(e.adr + k);
                // as: nur Zahlen (cc schreibt auch Text als `BV %68`); z8kasm: druckbare Zeichen als 'c'
                bool zeichenOk = !as && druckbar(c) && !(e.typ == Einheit::Str && k == 0);
                s += bv(zeichenOk ? zeichen(c) : hex(c, 2), k == 0);
            }
            break;
        default: break;
        }
        return s;
    }

    std::string bytesHex(uint32_t a, uint32_t n) const {
        std::string s; char b[8];
        for (uint32_t k = 0; k < n && k < 8; ++k) {
            std::snprintf(b, sizeof b, "%02X", mem_.at(a + k));
            s += b;
            if (k & 1) s += ' ';
        }
        return s;
    }

    std::string adrText(uint32_t a) const {
        char b[24];
        if (opt.segmentiert) std::snprintf(b, sizeof b, "<<%u>>%%%04X", unsigned(a >> 16), unsigned(a & 0xFFFF));
        else std::snprintf(b, sizeof b, "%04X", unsigned(a & 0xFFFF));
        return b;
    }

    /// Listing (`z8k=false`) bzw. z8kasm-Quelle (`z8k=true`).
    std::string textQuelle(bool z8kQuelle) const {
        std::string o;
        char b[64];
        if (z8kQuelle) {
            o += "; z8kdis: Rundlauf-Quelle (z8kasm-Syntax)\n";
            if (opt.segmentiert) o += "SEG\n";
        }
        for (const auto& eq : karte.eq) {
            std::snprintf(b, sizeof b, "%s EQU %%%X\n", eq.first.c_str(), unsigned(eq.second));
            o += b;
        }
        // Marken, die in keinem Zeilenanfang liegen: als EQU (im Listing nur als Hinweis)
        for (uint32_t a : gewuenscht_) {
            if (marke_.count(a)) continue;
            std::snprintf(b, sizeof b, "%s EQU %%%X\n", autoName(a).c_str(), unsigned(a & 0xFFFF));
            o += z8kQuelle ? std::string(b) : std::string("; Marke mitten im Befehl: ") + b;
        }
        uint32_t letzterEnde = ~0u;
        for (const Einheit& e : einheiten_) {
            if (z8kQuelle && e.adr != letzterEnde) {
                if (opt.segmentiert) std::snprintf(b, sizeof b, "ORG <<%u>>%%%X\n", unsigned(e.adr >> 16), unsigned(e.adr & 0xFFFF));
                else std::snprintf(b, sizeof b, "ORG %%%X\n", unsigned(e.adr & 0xFFFF));
                o += b;
            }
            letzterEnde = e.adr + e.len;
            auto pk = psaKom_.equal_range(e.adr);
            for (auto it = pk.first; it != pk.second; ++it) o += "; " + it->second + "\n";
            auto kk = karte.kom.equal_range(e.adr);
            for (auto it = kk.first; it != kk.second; ++it) o += "; " + it->second + "\n";
            auto dm = demotiert_.find(e.adr);
            if (dm != demotiert_.end()) o += "; nicht kanonisch kodiert: " + dm->second + "\n";
            auto m = marke_.find(e.adr);
            if (m != marke_.end()) o += m->second + ":\n";
            std::string text = e.typ == Einheit::Befehl ? befehlText(e.line, e.adr, false) : datenText(e, false);
            std::string ik;
            auto ii = karte.ik.equal_range(e.adr);
            for (auto it = ii.first; it != ii.second; ++it) ik += (ik.empty() ? "  ; " : " ") + it->second;
            if (z8kQuelle) {
                o += "    " + text + ik + "\n";
            } else {
                std::string h = bytesHex(e.adr, e.len);
                h.resize(20, ' ');
                o += adrText(e.adr) + "  " + h + "  " + text + ik + "\n";
            }
        }
        if (z8kQuelle) o += "END\n";
        return o;
    }

    // ── Dialekt des MUTOS-as (PLZ/ASM) ───────────────────────────────────────
    std::string asQuelle() const;
    std::string asEinheit(const Einheit& e) const;
    bool asModul(std::string& aus) const;

public:
    /// Schnittstellen fuer die As-Ausgabe in Stuecken (`opt.teil`): Bereiche [von, bis) von hoechstens
    /// `maxBytes` Byte (ein einzelner Brocken darf groesser sein), geschnitten nur an Einheitengrenzen,
    /// die kein relativer Sprung (JR/DJNZ/DBJNZ/CALR) ueberspannt -- der as kennt ueber Stuecke hinweg nur
    /// absolute Konstanten, und fuer einen relativen Sprung ginge das nicht (`ILLEGAL VALUE`, gemessen).
    std::vector<std::pair<uint32_t, uint32_t>> schnitte(uint32_t maxBytes) const {
        std::set<uint32_t> verboten;                   // Schnitt c ist verboten, wenn lo < c <= hi
        std::vector<std::pair<uint32_t, uint32_t>> iv;
        for (const Einheit& e : einheiten_) {
            if (e.typ != Einheit::Befehl || !e.line.dec.insn || !e.line.hasTarget) continue;
            bool rel = false;
            for (int i = 0; i < e.line.dec.nops; ++i) {
                z8k::Kind k = e.line.dec.op[i].kind;
                if (k == z8k::Kind::RA16 || k == z8k::Kind::RA8 || k == z8k::Kind::RA7 || k == z8k::Kind::RA12) rel = true;
            }
            if (!rel) continue;
            uint32_t tg = lin(e.line.targetSeg, e.line.target);
            iv.push_back({std::min(e.adr, tg), std::max(e.adr, tg)});
        }
        std::sort(iv.begin(), iv.end());
        std::vector<std::pair<uint32_t, uint32_t>> r;
        if (einheiten_.empty()) return r;
        uint32_t von = einheiten_.front().adr, ende = einheiten_.back().adr + einheiten_.back().len;
        size_t k = 0;
        uint32_t reichweite = 0;                       // groesstes hi der Intervalle mit lo < c
        uint32_t letzterSchnitt = von;
        for (const Einheit& e : einheiten_) {
            uint32_t c = e.adr;
            while (k < iv.size() && iv[k].first < c) reichweite = std::max(reichweite, iv[k++].second);
            if (c == von) continue;
            bool frei = !(reichweite >= c);
            if (frei && c - letzterSchnitt >= maxBytes) { r.push_back({letzterSchnitt, c}); letzterSchnitt = c; }
        }
        r.push_back({letzterSchnitt, ende});
        return r;
    }
private:

    /// JR mit Wortabstand <= -127 oder >= 127: der Gast-as lehnt ihn ab (nur -127 im Kernel belegt; die Grenzen
    /// -128/+127 sind aus Vorsicht mit erfasst, ein JR dort kommt im Kernel nicht vor).
    static bool asLehntSprungAb(const Einheit& e) {
        if (!e.line.dec.insn || std::strcmp(e.line.dec.insn->mn, "JR") || !e.line.hasTarget) return false;
        int d = (int(e.line.target) - int(e.adr + e.len)) / 2;
        return d <= -127 || d >= 127;
    }

    std::multimap<uint32_t, std::string> psaKom_;
    std::set<int> gesehen_;
    std::map<uint32_t, std::string> demotiert_;   // Befehle ohne kanonische Kodierung (Text zur Anzeige)
};


inline bool Analyse::asModul(std::string& aus) const {
    // Modulausgabe (AP K5): Text [tvon,tbis) als EIN Unterprogramm, Daten [dvon,dbis) dahinter im Datenabschnitt.
    // Fremde Marken sind fuer `as -u` undefinierte Symbole (Aufloesung durch ld); eigene, anderswo benutzte werden GLOBAL.
    const auto& mods = karte.moduln;
    auto sauber = [](const std::string& n) { std::string r = "p_"; for (char c : n) r += std::isalnum(static_cast<unsigned char>(c)) ? c : '_'; return r; };
    char b[160];
    auto eigener = [&](const Karte::Modul& m, uint32_t a) {
        return (a >= m.tvon && a < m.tbis) || (a >= m.dvon && a < m.dbis);
    };
    auto rumpfVon = [&](const Karte::Modul& m, std::string& text, std::string& daten) {
        for (const Einheit& e : einheiten_) {
            if (e.adr >= m.tvon && e.adr < m.tbis) text += asEinheit(e);
            else if (e.adr >= m.dvon && e.adr < m.dbis) daten += asEinheit(e);
        }
    };
    auto tokens = [](const std::string& t, std::set<std::string>& out) {
        std::string tok;
        for (size_t i = 0; i <= t.size(); ++i) {
            char c = i < t.size() ? t[i] : ' ';
            if (std::isalnum(static_cast<unsigned char>(c)) || c == '_') tok += c;
            else { if (!tok.empty()) out.insert(tok); tok.clear(); }
        }
    };
    if (opt.modul == "bss") {
        // Gemeinsame Variablen (BSS): eine Vereinbarung je Symbol, Groesse = Abstand zum naechsten
        std::set<uint32_t> basen = bssRef_;
        basen.insert(bssVon_);
        // Der ld teilt echte Gemeinsame Variablen in Hash-Reihenfolge zu (gemessen) -- nicht in der des Abbilds.  Deshalb
        // werden sie hier als nullgefuellte GLOBAL-Vereinbarungen in der Adressfolge ans Ende des Datenabschnitts gelegt;
        // das Abbild ist nur Text+Daten (die Nullen dahinter sind im Original BSS), `_end` stimmt ebenfalls.
        aus = "bss module\n$NONSEGMENTED\nconstant\n\tG\t:= global\n\tA\t:= array\n\tB\t:= byte\n";
        for (auto it = basen.begin(); it != basen.end(); ++it) {
            auto nx = std::next(it);
            uint32_t ende = nx == basen.end() ? bssBis_ : *nx;
            std::snprintf(b, sizeof b, "G\t%s\tA[%%%X B]\n", bssName(*it).c_str(), unsigned(ende - *it));
            aus += b;
        }
        aus += "end bss\n";
        return true;
    }
    const Karte::Modul* mm = nullptr;
    for (const auto& m : mods) if (m.name == opt.modul) mm = &m;
    if (!mm) return false;
    std::string text, daten;
    rumpfVon(*mm, text, daten);
    // fremd benutzte eigene Marken -> GLOBAL
    std::set<std::string> fremd;
    for (const auto& m : mods) {
        if (&m == mm) continue;
        std::string t, d; rumpfVon(m, t, d);
        tokens(t, fremd); tokens(d, fremd);
    }
    std::set<std::string> eigen;
    tokens(text, eigen); tokens(daten, eigen);
    std::string o = mm->name + " module\n$NONSEGMENTED\nconstant\n"
        "\tFP\t:= r14\n\tSP\t:= r15\n\tX\t:= external\n\tG\t:= global\n\tI\t:= internal\n\tA\t:= array\n"
        "\tPR\t:= procedure\n\tE\t:= entry\n\tB\t:= byte\n\tW\t:= word\n\tBV\t:= bval\n\tWV\t:= wval\n"
        "\tLV\t:= lval\n\tL\t:= label\n";
    std::set<std::string> definiert;
    std::string globale;
    for (const auto& kv : marke_) {
        if (!eigener(*mm, kv.first)) continue;
        definiert.insert(kv.second);
        if (fremd.count(kv.second)) globale += "G\n" + kv.second + "\tL\n";
    }
    for (const auto& eq : karte.eq)
        if (eigen.count(eq.first)) { std::snprintf(b, sizeof b, "\t%s\t:= %%%X\n", eq.first.c_str(), unsigned(eq.second)); o += b; }
    for (uint32_t a : gewuenscht_) {
        if (marke_.count(a)) continue;
        std::string n = autoName(a);
        if (eigen.count(n)) { std::snprintf(b, sizeof b, "\t%s\t:= %%%X\n", n.c_str(), unsigned(a & 0xFFFF)); o += b; }
    }
    o += globale;
    std::string pn = "_" + sauber(mm->name);
    if (!text.empty()) {
        o += "G\t" + pn + "\tPR\nE\n" + text + "end " + pn + "\n";
    } else {
        o += "G\t" + pn + "\tPR\nE\nend " + pn + "\n";
    }
    if (!daten.empty()) o += "I\n" + daten;
    o += "end " + mm->name + "\n";
    aus = o;
    return true;
}

inline std::string Analyse::asEinheit(const Einheit& e) const {
    std::string r;
        auto pk = psaKom_.equal_range(e.adr);
        for (auto it = pk.first; it != pk.second; ++it) r += "! " + it->second + " !\n";
        auto kk = karte.kom.equal_range(e.adr);
        for (auto it = kk.first; it != kk.second; ++it) r += "! " + it->second + " !\n";
        auto dm = demotiert_.find(e.adr);
        if (dm != demotiert_.end()) r += "! nicht kanonisch kodiert: " + dm->second + " !\n";
        auto m = marke_.find(e.adr);
        if (m != marke_.end()) r += m->second + ":\n";
        std::string text;
        if (e.typ == Einheit::Befehl && (e.line.dec.insn->has(z8k::Z8K_ALT) || asLehntSprungAb(e))) {
            // Zweitkodierung (LDB.L): der as hat keine Schreibweise dafuer -> Woerter.  Ebenso ein JR an der
            // Reichweitengrenze (K4, gemessen: der as meldet fuer -127 Woerter `ILLEGAL VALUE`, die CPU kann es).
            Einheit w = e; w.typ = Einheit::Dw; w.wortMarke.assign(e.len / 2, false);
            text = "! " + befehlText(e.line, e.adr, true) + " !\n\t" + datenText(w, true);
        } else {
            text = e.typ == Einheit::Befehl ? befehlText(e.line, e.adr, true) : datenText(e, true);
        }
        std::string ik;
        auto ii = karte.ik.equal_range(e.adr);
        for (auto it = ii.first; it != ii.second; ++it) ik += (ik.empty() ? "\t! " : " ") + it->second;
        if (!ik.empty()) ik += " !";
        // Mnemonik und Operanden durch Tabulator getrennt wie bei cc
        size_t sp = text.find(' ');
        if (e.typ == Einheit::Befehl && sp != std::string::npos && text.compare(0, 1, "!") != 0) text[sp] = '\t';
        r += "\t" + text + ik + "\n";
    return r;
}

inline std::string Analyse::asQuelle() const {
    if (link_ && !opt.modul.empty()) { std::string m; if (asModul(m)) return m; }
    // Geruest nach `cc -S` (Beleg: doc/mutos_kernel/as_dialekt.md): Modulkopf, Abkuerzungen, EIN globales
    // Unterprogramm um das ganze Abbild (alle Marken sind dort lokal), Daten inline als BV/WV.
    // UNBELEGT bleibt: ob der as Daten innerhalb eines Unterprogramms und `$ABS` so annimmt.
    std::string o;
    char b[128];
    o += opt.modul + (opt.segmentiert ? " module\n$SEGMENTED\n" : " module\n$NONSEGMENTED\n");
    o += "constant\n"
         "\tFP\t:= r14\n\tSP\t:= r15\n\tX\t:= external\n\tG\t:= global\n\tI\t:= internal\n\tA\t:= array\n"
         "\tPR\t:= procedure\n\tE\t:= entry\n\tB\t:= byte\n\tW\t:= word\n\tBV\t:= bval\n\tWV\t:= wval\n"
         "\tLV\t:= lval\n\tL\t:= label\n";
    for (const auto& eq : karte.eq) {
        std::snprintf(b, sizeof b, "\t%s\t:= %%%X\n", eq.first.c_str(), unsigned(eq.second));
        o += b;
    }
    for (uint32_t a : gewuenscht_) {
        if (marke_.count(a)) continue;                         // Marke mitten im Befehl: Konstante
        std::snprintf(b, sizeof b, "\t%s\t:= %%%X\n", autoName(a).c_str(), unsigned(a & 0xFFFF));
        o += b;
    }
    std::string rumpf = "G\t_" + opt.modul + "\tPR\nE\n";
    for (const Einheit& e : einheiten_) {
        if (opt.teil && (e.adr < opt.teilVon || e.adr >= opt.teilBis)) continue;
        // `$ABS` wird NICHT geschrieben (K4, gegen den Gast-as gemessen): der as legt dann gar keinen Text an.
        // Luecken gibt es nicht -- was die Karte nicht als Code fuehrt, bleibt DW/DB, das Abbild ist lueckenlos.
        rumpf += asEinheit(e);
    }
    if (opt.teil) {
        // Stueckweise Ausgabe: Marken ausserhalb des Stuecks sind fuer den as absolute Konstanten
        // (die Stuecke werden spaeter auf dem Host auf ihre Ladeadresse gesetzt; Marken IM Stueck
        // bleiben echte Marken und werden dort mit der Relokation verschoben).
        std::map<std::string, uint32_t> aussen;
        for (const auto& kv : marke_)
            if (kv.first < opt.teilVon || kv.first >= opt.teilBis) aussen[kv.second] = kv.first;
        std::set<std::string> benutzt;
        std::string tok;
        for (size_t i = 0; i <= rumpf.size(); ++i) {
            char c = i < rumpf.size() ? rumpf[i] : ' ';
            if (std::isalnum(static_cast<unsigned char>(c)) || c == '_') tok += c;
            else { if (!tok.empty() && aussen.count(tok)) benutzt.insert(tok); tok.clear(); }
        }
        for (const std::string& n : benutzt) {
            std::snprintf(b, sizeof b, "\t%s\t:= %%%X\n", n.c_str(), unsigned(aussen[n] & 0xFFFF));
            o += b;
        }
    }
    o += rumpf;
    o += "end _" + opt.modul + "\nend " + opt.modul + "\n";
    return o;
}

} // namespace z8kdis
