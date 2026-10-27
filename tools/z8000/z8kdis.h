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
        for (auto& kv : code_) {
            const z8k::Line& l = kv.second;
            if (l.hasTarget) gewuenscht_.insert(lin(l.targetSeg, l.target));
            for (int i = 0; i < l.dec.nops; ++i) {
                const z8k::Operand& o = l.dec.op[i];
                // Indizierte Adressen unterhalb von 0100 sind meist Strukturversaetze (`4(R2)`),
                // keine Tabellen -- sie bekommen keine eigene Marke.
                if (o.kind == z8k::Kind::DA || (o.kind == z8k::Kind::X && o.value >= 0x100))
                    if (!o.shortSeg) gewuenscht_.insert(lin(l.dec.seg ? o.seg : 0, o.value));
            }
        }
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
                if (e.wortMarke[k / 2]) z = zielText(zielAdr(e.adr + k, v), z);
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

inline std::string Analyse::asQuelle() const {
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
        auto pk = psaKom_.equal_range(e.adr);
        for (auto it = pk.first; it != pk.second; ++it) rumpf += "! " + it->second + " !\n";
        auto kk = karte.kom.equal_range(e.adr);
        for (auto it = kk.first; it != kk.second; ++it) rumpf += "! " + it->second + " !\n";
        auto dm = demotiert_.find(e.adr);
        if (dm != demotiert_.end()) rumpf += "! nicht kanonisch kodiert: " + dm->second + " !\n";
        auto m = marke_.find(e.adr);
        if (m != marke_.end()) rumpf += m->second + ":\n";
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
        rumpf += "\t" + text + ik + "\n";
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
