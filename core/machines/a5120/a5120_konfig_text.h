#pragma once
/**
 * @file a5120_konfig_text.h
 * @brief Konfigurationstext des A5120 (`schluessel=wert;…`) -> A5120Machine::Config.
 *
 * EINE Stelle fuer die C-ABI (`k1520_create_a5120_text`) und die Werkzeuge (`--ram`,
 * `--bildschirm`, `--schalter`, …), damit beide dieselben Namen und dieselben Fehlermeldungen
 * haben (Entwurf 33 §8 N4).
 *
 * Schluessel (Gross-/Kleinschreibung egal, Trenner `;` oder `,`):
 *   ausfuehrung = neu | alt | kmbg   Huelle: setzt die Einzelfelder einer Ausfuehrung; wird
 *                                    IMMER zuerst ausgewertet, die anderen Schluessel
 *                                    ueberschreiben danach einzeln
 *   ram         = k3526 | k3525_1 | k3525_2 | k3525_4
 *   bildschirm  = k7024 | k7023 | beide
 *   schalter    = k7024 | k7023      Anfangsstellung des Umschalters, nur mit bildschirm=beide
 *   tastatur    = k7637 | k7636
 *   laderom     = v09_2 | v09_1 | v06
 *   kassette    = 0 | 1
 *   em          = none | em064 | em256
 *   k3521       = aus | <Hex-Adresse im 4-KB-Raster, 0 .. f000>   akkugepuffertes RAM K3521.30
 *   k3521_memdi = bus | memdi1 | memdi2   (S1:02, nur mit k3521)
 *   aup         = 0 | 1           AUP 062-8531, Tastaturteil auf 40H-43H (Entwurf 33 N10c)
 *   aup_alpha   = led | ohne | mit   Annahme A3: wer die K7633-Tabelle waehlt (nur mit aup=1)
 *   aup_sonder  = 0 | 1           AUP-Sonderschnittstelle: CTC B0H-B3H + SIO B4H-B7H, Kanal B nach aussen
 *                                 (vierte Schnittstelle des SerialHub), nur mit aup=1 (Entwurf 33 N10d)
 *   (die Ablagedatei der K3521 steht NICHT im Text — Werte werden kleingeschrieben und
 *    Leerraum entfernt; Pfad ueber k1520_k3521_datei bzw. --k3521-datei)
 */
#include "core/machines/a5120/a5120.h"
#include <algorithm>
#include <cctype>
#include <string>
#include <vector>

namespace a5120konfig {

inline std::string klein(std::string s) {
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

/// Ein Schluessel/Wert-Paar auf @p cfg anwenden; false + @p err bei unbekanntem Schluessel/Wert.
inline bool wert(A5120Machine::Config& cfg, const std::string& schluessel_in,
                 const std::string& wert_in, std::string& err) {
    using C = A5120Machine::Config;
    const std::string k = klein(schluessel_in), v = klein(wert_in);
    if (v.empty()) { err = "A5120-Konfiguration: '" + k + "' ohne Wert"; return false; }
    if (k == "ausfuehrung") {
        C::Ausfuehrung a;
        if      (v == "neu")  a = C::Ausfuehrung::Neu;
        else if (v == "alt")  a = C::Ausfuehrung::Alt;
        else if (v == "kmbg") a = C::Ausfuehrung::Kmbg;
        else { err = "ausfuehrung: unbekannt '" + v + "' (neu|alt|kmbg)"; return false; }
        const auto em = cfg.em;
        cfg = C::fuerAusfuehrung(a);
        cfg.em = em;
    } else if (k == "ram") {
        if      (v == "k3526")    cfg.ram = C::Ram::K3526;
        else if (v == "k3525_1")  cfg.ram = C::Ram::K3525_1x;
        else if (v == "k3525_2")  cfg.ram = C::Ram::K3525_2x;
        else if (v == "k3525_4")  cfg.ram = C::Ram::K3525_4x;
        else { err = "ram: unbekannt '" + v + "' (k3526|k3525_1|k3525_2|k3525_4)"; return false; }
    } else if (k == "bildschirm") {
        if      (v == "k7024") cfg.bildschirm = C::Bildschirm::K7024;
        else if (v == "k7023") cfg.bildschirm = C::Bildschirm::K7023;
        else if (v == "beide") cfg.bildschirm = C::Bildschirm::Beide;
        else { err = "bildschirm: unbekannt '" + v + "' (k7024|k7023|beide)"; return false; }
    } else if (k == "schalter") {
        if      (v == "k7024") cfg.bildschirm_schalter = C::Bildschirm::K7024;
        else if (v == "k7023") cfg.bildschirm_schalter = C::Bildschirm::K7023;
        else { err = "schalter: unbekannt '" + v + "' (k7024|k7023)"; return false; }
    } else if (k == "tastatur") {
        if      (v == "k7637") cfg.tastatur = C::Tastatur::K7637;
        else if (v == "k7636") cfg.tastatur = C::Tastatur::K7636;
        else { err = "tastatur: unbekannt '" + v + "' (k7637|k7636)"; return false; }
    } else if (k == "laderom") {
        if      (v == "v09_2") cfg.laderom = K2526::Laderom::V09_2;
        else if (v == "v09_1") cfg.laderom = K2526::Laderom::V09_1;
        else if (v == "v06")   cfg.laderom = K2526::Laderom::V06;
        else { err = "laderom: unbekannt '" + v + "' (v09_2|v09_1|v06)"; return false; }
    } else if (k == "kassette") {
        if      (v == "1" || v == "ja")   cfg.kassette = true;
        else if (v == "0" || v == "nein") cfg.kassette = false;
        else { err = "kassette: unbekannt '" + v + "' (0|1)"; return false; }
    } else if (k == "em") {
        if      (v == "none")  cfg.em = C::Em::none;
        else if (v == "em064") cfg.em = C::Em::em064;
        else if (v == "em256") cfg.em = C::Em::em256;
        else { err = "em: unbekannt '" + v + "' (none|em064|em256)"; return false; }
    } else if (k == "k3521") {
        if (v == "aus" || v == "none") { cfg.k3521_basis = -1; return true; }
        if (v.size() > 4 || v.find_first_not_of("0123456789abcdef") != std::string::npos) {
            err = "k3521: unbekannt '" + v + "' (aus|Hex-Adresse 0..f000 im 4-KB-Raster)";
            return false;
        }
        const long a = std::stol(v, nullptr, 16);
        if (a % 0x1000 != 0) {
            err = "k3521: '" + v + "' liegt nicht im 4-KB-Raster (0, 1000, 2000 ... f000)";
            return false;
        }
        cfg.k3521_basis = static_cast<int>(a);
    } else if (k == "k3521_memdi") {
        if      (v == "bus")    cfg.k3521_memdi = K3521::Memdi::Bus;
        else if (v == "memdi1") cfg.k3521_memdi = K3521::Memdi::Memdi1;
        else if (v == "memdi2") cfg.k3521_memdi = K3521::Memdi::Memdi2;
        else { err = "k3521_memdi: unbekannt '" + v + "' (bus|memdi1|memdi2)"; return false; }
    } else if (k == "aup") {
        if      (v == "1" || v == "ja")   cfg.aup = true;
        else if (v == "0" || v == "nein") cfg.aup = false;
        else { err = "aup: unbekannt '" + v + "' (0|1)"; return false; }
    } else if (k == "aup_alpha") {
        if      (v == "led")  cfg.aup_alpha = AUP::Alpha::NachLed;
        else if (v == "ohne") cfg.aup_alpha = AUP::Alpha::Ohne;
        else if (v == "mit")  cfg.aup_alpha = AUP::Alpha::Mit;
        else { err = "aup_alpha: unbekannt '" + v + "' (led|ohne|mit)"; return false; }
    } else if (k == "aup_sonder") {
        if      (v == "1" || v == "ja")   cfg.aup_sonder = true;
        else if (v == "0" || v == "nein") cfg.aup_sonder = false;
        else { err = "aup_sonder: unbekannt '" + v + "' (0|1)"; return false; }
    } else {
        err = "A5120-Konfiguration: unbekannter Schluessel '" + k +
              "' (ausfuehrung|ram|bildschirm|schalter|tastatur|laderom|kassette|em|k3521|k3521_memdi|aup|aup_alpha|aup_sonder)";
        return false;
    }
    return true;
}

/// Folgefehler, die erst die fertige Konfiguration zeigt (Schalter ohne zwei Karten).
inline bool pruefe(const A5120Machine::Config& cfg, bool schalter_gesetzt, std::string& err) {
    if (schalter_gesetzt && cfg.bildschirm != A5120Machine::Config::Bildschirm::Beide) {
        err = "schalter: nur mit bildschirm=beide";
        return false;
    }
    if (cfg.k3521_basis < 0 && cfg.k3521_memdi != K3521::Memdi::Bus) {
        err = "k3521_memdi: nur mit k3521";
        return false;
    }
    if (!cfg.aup && cfg.aup_alpha != AUP::Alpha::NachLed) {
        err = "aup_alpha: nur mit aup=1";
        return false;
    }
    if (!cfg.aup && cfg.aup_sonder) {
        err = "aup_sonder: nur mit aup=1";
        return false;
    }
    return true;
}

/// Ganzen Text auswerten.  `ausfuehrung` zuerst (es setzt die Einzelfelder neu auf), der
/// Rest in Textreihenfolge.  Leerer Text = nichts zu tun.
inline bool text(A5120Machine::Config& cfg, const std::string& t, std::string& err) {
    std::vector<std::pair<std::string, std::string>> paare;
    size_t pos = 0;
    while (pos <= t.size()) {
        size_t ende = t.find_first_of(";,", pos);
        if (ende == std::string::npos) ende = t.size();
        std::string teil = t.substr(pos, ende - pos);
        pos = ende + 1;
        teil.erase(std::remove_if(teil.begin(), teil.end(),
                                  [](unsigned char c) { return std::isspace(c); }), teil.end());
        if (teil.empty()) continue;
        const size_t gl = teil.find('=');
        if (gl == std::string::npos) {
            err = "A5120-Konfiguration: '" + teil + "' ohne '=' (schluessel=wert)";
            return false;
        }
        paare.emplace_back(teil.substr(0, gl), teil.substr(gl + 1));
    }
    bool schalter = false;
    for (const auto& p : paare)
        if (klein(p.first) == "ausfuehrung" && !wert(cfg, p.first, p.second, err)) return false;
    for (const auto& p : paare) {
        const std::string k = klein(p.first);
        if (k == "ausfuehrung") continue;
        if (k == "schalter") schalter = true;
        if (!wert(cfg, p.first, p.second, err)) return false;
    }
    return pruefe(cfg, schalter, err);
}

}  // namespace a5120konfig
