/**
 * @file drucker_rom_katalog.cpp
 * @brief Leser des Drucker-ROM-Katalogs (WP9, Entwurf 49 §12): YAML + Intel-HEX.
 */
#include "core/machines/printer/drucker_rom_katalog.h"

#include <algorithm>
#include <cstdlib>
#include <cstdio>
#include <filesystem>
#include <map>
#include <mutex>
#include <set>
#include <sstream>

#include "core/util/yaml_lite.h"

#if defined(_WIN32)
#  include <windows.h>
#else
#  include <dlfcn.h>
#  include <unistd.h>
#endif

// Vom Build gesetzt (CMake): Katalog im Quellbaum; Release-Bauten setzen ihn leer (kein Baurechnerpfad im Binaerabbild).
#ifndef K1520_PRINTER_ROMS_DEFAULT
#define K1520_PRINTER_ROMS_DEFAULT ""
#endif

namespace {

constexpr const char* kDateiname = "drucker_roms.yaml";

/// Anker im eigenen Modul - nur seine ADRESSE wird gebraucht (wie in format_catalog.cpp).
const char kModuleAnchor = 0;

std::string moduleDir()
{
#if defined(_WIN32)
    HMODULE mod = nullptr;
    if (!::GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                              reinterpret_cast<LPCWSTR>(&kModuleAnchor), &mod))
        return {};
    wchar_t buf[MAX_PATH];
    const DWORD n = ::GetModuleFileNameW(mod, buf, MAX_PATH);
    if (n == 0 || n >= MAX_PATH) return {};
    return std::filesystem::path(buf).parent_path().string();
#else
    ::Dl_info info{};
    if (::dladdr(&kModuleAnchor, &info) && info.dli_fname && *info.dli_fname)
        return std::filesystem::path(info.dli_fname).parent_path().string();
    char buf[4096];
    const ssize_t n = ::readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (n <= 0) return {};
    buf[n] = '\0';
    return std::filesystem::path(buf).parent_path().string();
#endif
}

constexpr char kListTrenner =
#if defined(_WIN32)
    ';';
#else
    ':';
#endif

bool umgebungGesetzt(std::vector<std::string>* pfade)
{
    const char* env = std::getenv("K1520_PRINTER_ROMS");
    if (!env || !*env) return false;
    std::stringstream ss(env);
    std::string item;
    while (std::getline(ss, item, kListTrenner)) {
        if (item.empty()) continue;
        std::error_code ec;
        pfade->push_back(std::filesystem::is_directory(item, ec) ? item + "/" + kDateiname : item);
    }
    return true;
}

bool schluesselOk(const std::string& s)
{
    if (s.empty()) return false;
    auto klein = [](char c) { return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9'); };
    if (!klein(s[0])) return false;
    return std::all_of(s.begin(), s.end(), [&](char c) { return klein(c) || c == '_' || c == '.' || c == '-'; });
}

int hexWert(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
}

/// Intel-HEX-Block -> Bytes.  Fehler als Text mit absoluter Dateizeile (`ersteZeile` = Zeile von `text`[0]).
bool hexLesen(const std::string& text, int ersteZeile, std::vector<uint8_t>& out, std::string& fehler, int& fehlerZeile)
{
    out.clear();
    out.reserve(text.size() / 3);
    bool ende = false;
    int nr = ersteZeile - 1;
    size_t pos = 0;
    while (pos < text.size()) {
        size_t nl = text.find('\n', pos);
        if (nl == std::string::npos) nl = text.size();
        ++nr;
        const char* z = text.data() + pos;
        size_t n = nl - pos;
        pos = nl + 1;
        while (n > 0 && (z[n - 1] == ' ' || z[n - 1] == '\r')) --n;
        if (n == 0) continue;
        fehlerZeile = nr;
        if (ende) { fehler = "Daten hinter dem Endsatz :00000001FF"; return false; }
        if (z[0] != ':' || n < 11 || (n % 2) == 0) { fehler = "kein Intel-HEX-Satz (':' + gerade Zahl Hexziffern, mindestens LLAAAATTCC)"; return false; }
        uint8_t b[5 + 255];
        const size_t nb = (n - 1) / 2;
        if (nb > sizeof b) { fehler = "Satz zu lang"; return false; }
        unsigned summe = 0;
        for (size_t i = 0; i < nb; ++i) {
            const int h = hexWert(z[1 + 2 * i]), l = hexWert(z[2 + 2 * i]);
            if (h < 0 || l < 0) { fehler = "Zeichen " + std::to_string(2 + 2 * i) + " ist keine Hexziffer"; return false; }
            b[i] = static_cast<uint8_t>(h * 16 + l);
            summe += b[i];
        }
        if (summe & 0xFF) { fehler = "Pruefsumme falsch"; return false; }
        if (nb != static_cast<size_t>(b[0]) + 5) { fehler = "Laengenfeld " + std::to_string(b[0]) + " passt nicht zur Satzlaenge"; return false; }
        const unsigned adr = (static_cast<unsigned>(b[1]) << 8) | b[2];
        if (b[3] == 1) {
            if (b[0] != 0) { fehler = "Endsatz mit Daten"; return false; }
            ende = true;
        } else if (b[3] == 0) {
            if (adr != out.size()) {
                char m[96];
                std::snprintf(m, sizeof m, "Adresse %04XH, erwartet %04XH (Luecke oder Ueberlappung)", adr, static_cast<unsigned>(out.size()));
                fehler = m;
                return false;
            }
            out.insert(out.end(), b + 4, b + 4 + b[0]);
        } else {
            fehler = "Satzart " + std::to_string(b[3]) + " nicht unterstuetzt (nur 00 Daten, 01 Ende)";
            return false;
        }
    }
    fehlerZeile = std::max(nr, ersteZeile);
    if (!ende) { fehler = "Endsatz :00000001FF fehlt"; return false; }
    return true;
}

}  // namespace

const std::vector<std::string>& DruckerRomKatalog::typen()
{
    static const std::vector<std::string> t = {"sd1157", "k6316-ifss", "k6316-v24", "k6313", "k6311", "k6313-ifss", "k6311-ifss"};
    return t;
}

std::vector<size_t> DruckerRomKatalog::erlaubteGroessen(const std::string& typ)
{
    if (typ == "sd1157") return {10624};                               // 0000H-297FH
    if (typ == "k6316-ifss" || typ == "k6316-v24") return {10240};     // fuenf Chips zu 2 KiB
    if (typ == "k6313" || typ == "k6313-ifss") return {10240, 16384};  // kurz / lang (ein 27128)
    if (typ == "k6311" || typ == "k6311-ifss") return {10240, 8192};   // fuenf oder vier Chips (Rest FFH)
    return {};
}

std::vector<std::string> DruckerRomKatalog::searchPaths()
{
    std::vector<std::string> p;
    if (umgebungGesetzt(&p)) return p;   // ausdruecklich gewaehlt: kein stiller Rueckfall auf einen anderen Katalog
    if (std::string e = moduleDir(); !e.empty()) {
        p.push_back(e + "/../share/k1520emu/" + kDateiname);
        p.push_back(e + "/data/" + kDateiname);
    }
    if (std::string d = K1520_PRINTER_ROMS_DEFAULT; !d.empty()) p.push_back(d);
    p.push_back(std::string("data/") + kDateiname);
    return p;
}

std::shared_ptr<const DruckerRomKatalog> DruckerRomKatalog::gemeinsam()
{
    static std::mutex m;
    static std::shared_ptr<const DruckerRomKatalog> cache;
    static std::string cachePfad;
    static std::filesystem::file_time_type cacheZeit;
    static std::uintmax_t cacheGroesse = 0;
    static std::string cacheSuche;
    std::lock_guard<std::mutex> lock(m);

    const std::vector<std::string> kand = searchPaths();
    std::string such;
    for (const auto& k : kand) such += k + "\n";
    std::string gefunden;
    for (const auto& k : kand) {
        std::error_code ec;
        if (std::filesystem::is_regular_file(k, ec)) { gefunden = k; break; }
    }
    std::error_code ec;
    std::filesystem::file_time_type zeit{};
    std::uintmax_t groesse = 0;
    if (!gefunden.empty()) {
        zeit = std::filesystem::last_write_time(gefunden, ec);
        groesse = std::filesystem::file_size(gefunden, ec);
    }
    if (cache && such == cacheSuche && gefunden == cachePfad && zeit == cacheZeit && groesse == cacheGroesse) return cache;

    DruckerRomKatalog k;
    if (gefunden.empty()) {
        k.fehler_ = "Drucker-ROM-Katalog drucker_roms.yaml nicht gefunden. Gesucht:";
        for (const auto& c : kand) k.fehler_ += "\n  " + c;
        k.fehler_ += "\nAbhilfe: data/drucker_roms.yaml bereitstellen oder K1520_PRINTER_ROMS=<datei> setzen.";
    } else {
        k = laden(gefunden);
    }
    cache = std::make_shared<const DruckerRomKatalog>(std::move(k));
    cacheSuche = such; cachePfad = gefunden; cacheZeit = zeit; cacheGroesse = groesse;
    return cache;
}

DruckerRomKatalog DruckerRomKatalog::laden(const std::string& pfad)
{
    yaml::Node n;
    yaml::Error e;
    if (!yaml::parseFile(pfad, n, e)) {
        DruckerRomKatalog k;
        k.pfad_ = pfad;
        k.fehler_ = "Drucker-ROM-Katalog: " + e.format(pfad);
        return k;
    }
    return ausKnoten(n, pfad);
}

DruckerRomKatalog DruckerRomKatalog::ausText(const std::string& text, const std::string& name)
{
    yaml::Node n;
    yaml::Error e;
    if (!yaml::parse(text, n, e)) {
        DruckerRomKatalog k;
        k.pfad_ = name;
        k.fehler_ = "Drucker-ROM-Katalog: " + e.format(name);
        return k;
    }
    return ausKnoten(n, name);
}

DruckerRomKatalog DruckerRomKatalog::ausKnoten(const yaml::Node& wurzel, const std::string& name)
{
    DruckerRomKatalog kat;
    kat.pfad_ = name;
    std::vector<std::string> fehler;
    auto fail = [&](int zeile, const std::string& eintrag, const std::string& msg) {
        if (fehler.size() >= 20) return;
        std::string t = name + ":" + std::to_string(zeile) + ": ";
        if (!eintrag.empty()) t += "Eintrag '" + eintrag + "': ";
        fehler.push_back(t + msg);
    };

    if (!wurzel.isMap()) { fail(1, "", "oberste Ebene ist keine Abbildung"); }
    else {
        long v = 0;
        const yaml::Node* ver = wurzel.find("version");
        if (!ver || !ver->isScalar() || !yaml::toInt(ver->scalar, v) || v != 1) fail(ver ? ver->line : 1, "", "version muss 1 sein");
    }
    const yaml::Node* liste = wurzel.isMap() ? wurzel.find("eintraege") : nullptr;
    if (wurzel.isMap() && (!liste || !liste->isList() || liste->items.empty())) fail(liste ? liste->line : 1, "", "'eintraege' fehlt oder ist leer");
    if (!liste || !liste->isList()) { kat.fehler_ = ""; for (auto& f : fehler) kat.fehler_ += (kat.fehler_.empty() ? "" : "\n") + f; if (kat.fehler_.empty()) kat.fehler_ = name + ": Katalog unbrauchbar"; return kat; }

    // Eigene Daten je Schluessel, damit `daten_von` aufloesen kann (Reihenfolge der Datei ist frei).
    std::map<std::string, std::shared_ptr<const std::vector<uint8_t>>> eigene;
    struct Verweis { size_t index; std::string ziel; int zeile; };
    std::vector<Verweis> verweise;
    std::set<std::string> schluessel;

    auto text = [&](const yaml::Node& e, const char* feld, std::string& out, bool pflicht, const std::string& ek) {
        const yaml::Node* f = e.find(feld);
        if (!f) { if (pflicht) fail(e.line, ek, std::string("Feld '") + feld + "' fehlt"); return false; }
        if (!f->isScalar()) { fail(f->line, ek, std::string("Feld '") + feld + "' muss Text sein"); return false; }
        out = f->scalar;
        return true;
    };
    auto textListe = [&](const yaml::Node& e, const char* feld, std::vector<std::string>& out, const std::string& ek) {
        const yaml::Node* f = e.find(feld);
        if (!f || f->isNull()) return;
        if (!f->isList()) { fail(f->line, ek, std::string("'") + feld + "' muss eine Liste sein"); return; }
        for (const auto& i : f->items) {
            if (!i.isScalar() || i.scalar.empty()) { fail(i.line, ek, std::string("'") + feld + "': leerer oder verschachtelter Wert"); continue; }
            out.push_back(i.scalar);
        }
    };

    for (const auto& e : liste->items) {
        DruckerRomEintrag d;
        d.zeile = e.line;
        if (!e.isMap()) { fail(e.line, "", "Eintrag ist keine Abbildung"); continue; }
        text(e, "schluessel", d.schluessel, true, "");
        const std::string ek = d.schluessel.empty() ? std::string("?") : d.schluessel;
        if (!d.schluessel.empty()) {
            if (!schluesselOk(d.schluessel)) fail(e.line, ek, "Schluessel nicht erlaubt (a-z, 0-9, _ . -)");
            if (!schluessel.insert(d.schluessel).second) fail(e.line, ek, "Schluessel doppelt");
        }
        text(e, "typ", d.typ, true, ek);
        text(e, "titel", d.titel, true, ek);
        text(e, "guete", d.guete, true, ek);
        text(e, "pgm", d.pgm, false, ek);
        text(e, "zeichensatz", d.zeichensatz, false, ek);
        text(e, "hinweis", d.hinweis, false, ek);
        textListe(e, "schnittstellen", d.schnittstellen, ek);
        textListe(e, "aliases", d.aliases, ek);
        if (const yaml::Node* h = e.find("herkunft")) {
            if (h->isScalar()) d.herkunft = h->scalar;
            else if (h->isList()) for (const auto& i : h->items) d.herkunft += (d.herkunft.empty() ? "" : ", ") + i.scalar;
        }
        if (!d.typ.empty() && std::find(typen().begin(), typen().end(), d.typ) == typen().end())
            fail(e.line, ek, "typ '" + d.typ + "' unbekannt");
        if (!d.guete.empty() && d.guete != "ok" && d.guete != "unvollstaendig" && d.guete != "defekt" && d.guete != "unbekannt")
            fail(e.line, ek, "guete '" + d.guete + "' unbekannt (ok | unvollstaendig | defekt | unbekannt)");
        for (const auto& s : d.schnittstellen)
            if (s != "ifss" && s != "v24") fail(e.line, ek, "schnittstellen: '" + s + "' unbekannt (ifss | v24)");
        if (const yaml::Node* vg = e.find("vorgabe"); !vg || !vg->isScalar() || !yaml::toBool(vg->scalar, d.vorgabe))
            fail(vg ? vg->line : e.line, ek, "Feld 'vorgabe' fehlt oder ist kein Wahrheitswert");
        long groesse = 0;
        const yaml::Node* gn = e.find("groesse");
        if (!gn || !gn->isScalar() || !yaml::toInt(gn->scalar, groesse) || groesse <= 0) fail(gn ? gn->line : e.line, ek, "Feld 'groesse' fehlt oder ist keine positive Zahl");

        const yaml::Node* daten = e.find("daten");
        const yaml::Node* von = e.find("daten_von");
        if (daten && von) fail(e.line, ek, "'daten' und 'daten_von' zugleich");
        else if (!daten && !von) fail(e.line, ek, "weder 'daten' noch 'daten_von'");
        else if (von) {
            if (!von->isScalar() || von->scalar.empty()) fail(von->line, ek, "'daten_von' muss ein Schluessel sein");
            else verweise.push_back({kat.eintraege_.size(), von->scalar, von->line});
        } else if (!daten->isScalar()) {
            fail(daten->line, ek, "'daten' muss ein Blockwert '|' mit Intel-HEX sein");
        } else {
            auto bytes = std::make_shared<std::vector<uint8_t>>();
            std::string msg;
            int zl = 0;
            if (!hexLesen(daten->scalar, daten->line + 1, *bytes, msg, zl)) fail(zl, ek, "daten: " + msg);
            else if (groesse > 0 && bytes->size() != static_cast<size_t>(groesse))
                fail(daten->line, ek, "daten haben " + std::to_string(bytes->size()) + " Bytes, groesse sagt " + std::to_string(groesse));
            else { eigene[d.schluessel] = bytes; d.daten = bytes; }
        }
        kat.eintraege_.push_back(std::move(d));
        kat.groessen_.push_back(groesse);
    }

    for (const auto& v : verweise) {
        auto& d = kat.eintraege_[v.index];
        auto it = eigene.find(v.ziel);
        if (it == eigene.end()) { fail(v.zeile, d.schluessel, "daten_von '" + v.ziel + "' ist kein Eintrag mit eigenen Daten"); continue; }
        if (static_cast<size_t>(kat.groessen_[v.index]) != it->second->size())
            fail(v.zeile, d.schluessel, "groesse " + std::to_string(kat.groessen_[v.index]) + " passt nicht zu den Daten von '" + v.ziel + "' (" + std::to_string(it->second->size()) + " Bytes)");
        d.daten = it->second;
    }

    // Typregeln: Laenge je Typ, genau eine Vorgabe je Typ, Aliase
    std::map<std::string, int> vorgaben;
    std::map<std::pair<std::string, std::string>, std::string> alias;
    for (const auto& d : kat.eintraege_) {
        if (d.daten) {
            const auto ok = erlaubteGroessen(d.typ);
            if (!ok.empty() && std::find(ok.begin(), ok.end(), d.daten->size()) == ok.end()) {
                std::string erw;
                for (size_t g : ok) erw += (erw.empty() ? "" : " oder ") + std::to_string(g);
                fail(d.zeile, d.schluessel, "Daten haben " + std::to_string(d.daten->size()) + " Bytes, Typ " + d.typ + " erwartet " + erw);
            }
        }
        if (d.vorgabe) ++vorgaben[d.typ];
        for (const auto& a : d.aliases) {
            if (schluessel.count(a) && a != d.schluessel) fail(d.zeile, d.schluessel, "Alias '" + a + "' kollidiert mit einem Schluessel");
            auto ins = alias.emplace(std::make_pair(d.typ, a), d.schluessel);
            if (!ins.second) fail(d.zeile, d.schluessel, "Alias '" + a + "' ist schon Alias von '" + ins.first->second + "'");
        }
    }
    for (const auto& t : typen()) {
        const int n = vorgaben.count(t) ? vorgaben[t] : 0;
        bool da = std::any_of(kat.eintraege_.begin(), kat.eintraege_.end(), [&](const DruckerRomEintrag& d) { return d.typ == t; });
        if (!da) fail(liste->line, "", "Typ " + t + " hat keinen Eintrag");
        else if (n != 1) fail(liste->line, "", "Typ " + t + ": " + std::to_string(n) + " Vorgaben, erwartet genau eine");
    }

    if (!fehler.empty()) {
        kat.eintraege_.clear();
        kat.fehler_ = "Drucker-ROM-Katalog fehlerhaft:";
        for (const auto& f : fehler) kat.fehler_ += "\n  " + f;
    }
    return kat;
}

const DruckerRomEintrag* DruckerRomKatalog::finde(const std::string& typ, const std::string& wahl) const
{
    for (const auto& e : eintraege_)
        if (e.typ == typ && (e.schluessel == wahl || std::find(e.aliases.begin(), e.aliases.end(), wahl) != e.aliases.end())) return &e;
    return nullptr;
}

const DruckerRomEintrag* DruckerRomKatalog::vorgabe(const std::string& typ) const
{
    for (const auto& e : eintraege_)
        if (e.typ == typ && e.vorgabe) return &e;
    return nullptr;
}

std::vector<const DruckerRomEintrag*> DruckerRomKatalog::fuerTyp(const std::string& typ) const
{
    std::vector<const DruckerRomEintrag*> r;
    for (const auto& e : eintraege_)
        if (e.typ == typ) r.push_back(&e);
    return r;
}
