/**
 * @file tar_archiv.cpp
 * @brief Implementierung des editierbaren tar-Archivs im Speicher.
 *
 * @see core/filesystem/tar/tar_archiv.h
 * @author Olaf Krieger
 * @date 2026
 * @license MIT License
 */

#include "core/filesystem/tar/tar_archiv.h"

#include <cstring>
#include <ctime>

namespace tar {

namespace {

/// Oktalfeld: n-1 Ziffern (mit fuehrenden Nullen) + NUL.  false, wenn der Wert nicht passt.
bool oktalSchreiben(uint8_t* f, size_t n, uint64_t v) {
    std::string s(n - 1, '0');
    for (size_t i = n - 1; i-- > 0;) { s[i] = static_cast<char>('0' + v % 8); v /= 8; }
    if (v) return false;
    std::memcpy(f, s.data(), n - 1);
    f[n - 1] = 0;
    return true;
}

bool beginntMit(const std::string& s, const std::string& p) {
    return s.size() > p.size() && s.compare(0, p.size(), p) == 0 && s[p.size()] == '/';
}

}  // namespace

std::string bereinigePfad(const std::string& roh, bool& geaendert) {
    geaendert = !roh.empty() && roh[0] == '/';
    std::string out, t;
    for (char c : roh + "/") {
        if (c != '/') { t += c; continue; }
        if (t == "..") { t = "__"; geaendert = true; }
        if (!t.empty() && t != ".") out += (out.empty() ? "" : "/") + t;
        t.clear();
    }
    return out;
}

std::string ArchivEintrag::schluessel() const {
    bool g = false;
    return bereinigePfad(k.name, g);
}

bool kopfSchreiben(const Kopf& k, uint64_t groesse, uint8_t out[kBlock], std::string& err) {
    if (k.name.size() > kNameMax) {
        err = "Der Name '" + k.name + "' ist laenger als " + std::to_string(kNameMax)
              + " Zeichen — das V7-tar des P8000 kennt keine laengeren Namen.";
        return false;
    }
    if (k.linkname.size() > kNameMax) {
        err = "Das Linkziel von '" + k.name + "' ist laenger als " + std::to_string(kNameMax) + " Zeichen.";
        return false;
    }
    std::memset(out, 0, kBlock);
    std::memcpy(out, k.name.data(), k.name.size());
    if (!oktalSchreiben(out + 100, 8, k.mode & 07777) || !oktalSchreiben(out + 108, 8, k.uid)
        || !oktalSchreiben(out + 116, 8, k.gid)) {
        err = "Besitzer (uid/gid) von '" + k.name + "' passt nicht in den tar-Kopf (hoechstens 2097151).";
        return false;
    }
    if (!oktalSchreiben(out + 124, 12, groesse)) { err = "Datei '" + k.name + "' ist zu gross fuer tar."; return false; }
    if (!oktalSchreiben(out + 136, 12, k.mtime)) { err = "Zeitstempel von '" + k.name + "' passt nicht."; return false; }
    out[156] = static_cast<uint8_t>(k.typ);
    std::memcpy(out + 157, k.linkname.data(), k.linkname.size());
    // Pruefsumme: Feld zaehlt als 8 Leerzeichen; geschrieben wird "ooooo\0 " (6 Ziffern, NUL, Leerzeichen).
    std::memset(out + 148, ' ', 8);
    uint32_t s = 0;
    for (uint32_t i = 0; i < kBlock; ++i) s += out[i];
    oktalSchreiben(out + 148, 7, s);
    out[155] = ' ';
    return true;
}

int TarArchiv::finde(const std::string& name) const {
    bool g = false;
    const std::string w = bereinigePfad(name, g);
    for (size_t i = e_.size(); i-- > 0;)
        if (e_[i].schluessel() == w) return static_cast<int>(i);
    return -1;
}

bool TarArchiv::hinzufuegen(const std::string& name, const std::vector<uint8_t>& daten,
                            const NeuAngaben& a, bool ersetzen, std::string& err) {
    bool g = false;
    const std::string w = bereinigePfad(name, g);
    if (w.empty()) { err = "Leerer Dateiname"; return false; }
    if (w.size() > kNameMax) {
        err = "Der Name '" + w + "' ist laenger als " + std::to_string(kNameMax)
              + " Zeichen — das V7-tar des P8000 kennt keine laengeren Namen.";
        return false;
    }
    const int alt = finde(w);
    if (alt >= 0) {
        if (e_[static_cast<size_t>(alt)].k.typ == '5') { err = "'" + w + "' ist ein Verzeichnis"; return false; }
        if (!ersetzen) { err = "Datei existiert bereits: " + w; return false; }
    }
    ArchivEintrag n;
    n.k.name  = w;
    n.k.typ   = '0';
    n.k.mode  = a.mode & 07777;
    n.k.uid   = a.uid;
    n.k.gid   = a.gid;
    n.k.mtime = a.mtime ? a.mtime : static_cast<uint32_t>(std::time(nullptr));
    n.daten   = daten;
    if (alt >= 0) {
        // Ersetzen: alle fruehen Fassungen desselben Namens fallen weg, die letzte behaelt ihren Platz.
        for (size_t i = e_.size(); i-- > 0;)
            if (static_cast<int>(i) != alt && e_[i].schluessel() == w) {
                e_.erase(e_.begin() + static_cast<std::ptrdiff_t>(i));
            }
        e_[static_cast<size_t>(finde(w))] = std::move(n);
    } else {
        e_.push_back(std::move(n));
    }
    return true;
}

bool TarArchiv::verzeichnis(const std::string& name, const NeuAngaben& a, std::string& err) {
    bool g = false;
    const std::string w = bereinigePfad(name, g);
    if (w.empty()) { err = "Leerer Verzeichnisname"; return false; }
    // Jede Ebene des Pfades: fehlende anlegen, vorhandene Dateien melden.
    std::string teil;
    size_t p = 0;
    while (p <= w.size()) {
        size_t q = w.find('/', p);
        if (q == std::string::npos) q = w.size();
        teil = w.substr(0, q);
        p = q + 1;
        const int i = finde(teil);
        if (i >= 0) {
            if (e_[static_cast<size_t>(i)].k.typ != '5') { err = "'" + teil + "' ist schon eine Datei"; return false; }
            continue;
        }
        if (teil.size() + 1 > kNameMax) {
            err = "Der Name '" + teil + "/' ist laenger als " + std::to_string(kNameMax) + " Zeichen.";
            return false;
        }
        ArchivEintrag n;
        n.k.name  = teil + "/";
        n.k.typ   = '5';
        n.k.mode  = a.mode & 07777;
        n.k.uid   = a.uid;
        n.k.gid   = a.gid;
        n.k.mtime = a.mtime ? a.mtime : static_cast<uint32_t>(std::time(nullptr));
        e_.push_back(std::move(n));
    }
    return true;
}

bool TarArchiv::loeschen(const std::string& name, std::string& err) {
    bool g = false;
    const std::string w = bereinigePfad(name, g);
    const int i = finde(w);
    if (i < 0) { err = "Datei nicht gefunden: " + name; return false; }
    if (e_[static_cast<size_t>(i)].k.typ == '5')
        for (const ArchivEintrag& e : e_)
            if (beginntMit(e.schluessel(), w)) { err = "Verzeichnis '" + w + "' ist nicht leer"; return false; }
    for (size_t k = e_.size(); k-- > 0;)
        if (e_[k].schluessel() == w) e_.erase(e_.begin() + static_cast<std::ptrdiff_t>(k));
    return true;
}

bool TarArchiv::attribute(const std::string& name, const uint32_t* mode, const uint32_t* uid,
                          const uint32_t* gid, const uint32_t* mtime, std::string& err) {
    const int i = finde(name);
    if (i < 0) { err = "Datei nicht gefunden: " + name; return false; }
    Kopf& k = e_[static_cast<size_t>(i)].k;
    if (mode)  k.mode  = *mode & 07777;
    if (uid)   k.uid   = *uid;
    if (gid)   k.gid   = *gid;
    if (mtime) k.mtime = *mtime;
    return true;
}

uint64_t TarArchiv::stromLaenge() const {
    uint64_t n = 2 * kBlock;
    for (const ArchivEintrag& e : e_) n += e.belegt();
    return n;
}

bool TarArchiv::strom(std::vector<uint8_t>& out, std::string& err) const {
    out.clear();
    out.reserve(static_cast<size_t>(stromLaenge()));
    uint8_t h[kBlock];
    for (const ArchivEintrag& e : e_) {
        Kopf k = e.k;
        // Verzeichnisse tragen das '/' am Namensende (so erkennt es auch ein reines V7-tar).
        if (k.typ == '5' && (k.name.empty() || k.name.back() != '/')) k.name += '/';
        if (k.typ != '1' && k.typ != '2') k.linkname.clear();
        if (!kopfSchreiben(k, e.mitDaten() ? e.daten.size() : 0, h, err)) { out.clear(); return false; }
        out.insert(out.end(), h, h + kBlock);
        if (e.mitDaten()) {
            out.insert(out.end(), e.daten.begin(), e.daten.end());
            out.resize((out.size() + kBlock - 1) / kBlock * kBlock, 0);
        }
    }
    out.resize(out.size() + 2 * kBlock, 0);     // Endmarke
    return true;
}

}  // namespace tar
