/**
 * @file sound_opts.h
 * @brief Kommandozeilenoptionen der SOUND-Karte K1520 fuer boot_trace und k1520dbg (Entwurf 51, AP S6).
 *
 *   --sound[=basis]            Karte stecken (Basis hex/dez, Vorgabe 38H; PC 1715/1715W E0H)
 *   --sound-takt=quarz|halb|system   Chiptakt (Vorgabe halb)
 *   --sound-kanal=abc|acb|mono       Mischer (Vorgabe acb)
 *   --sound-chip=8910|8912
 *   --wav=DATEI                WAV-Mitschrift ab dem Stecken bis zum Programmende (nur mit --sound)
 *
 * Header ohne Abhaengigkeiten, damit auch @ref K8915TraceOpts ihn tragen kann.
 *
 * @license MIT
 */
#pragma once
#include <cstdlib>
#include <cstring>
#include <string>

struct SoundOpts {
    bool an = false;            ///< --sound (oder eine der Unteroptionen)
    int basis = -1;             ///< -1 = Vorgabe der Maschine
    std::string takt = "halb";  ///< quarz | halb | system
    std::string kanal = "acb";  ///< acb | abc | mono
    std::string chip = "8910";  ///< 8910 | 8912
    std::string wav;            ///< "" = keine Mitschrift
    std::string fehler;         ///< erster Optionsfehler (Wert unbekannt)

    /// true, wenn @p arg eine Soundoption ist (und uebernommen wurde; Wertefehler landen in @ref fehler).
    bool parse(const char* arg) {
        auto wert = [&](const char* pre, std::string& ziel, std::initializer_list<const char*> ok) {
            const size_t n = strlen(pre);
            if (strncmp(arg, pre, n) != 0) return false;
            an = true;
            const std::string v = arg + n;
            bool gut = false;
            for (const char* o : ok) if (v == o) gut = true;
            if (gut) ziel = v;
            else if (fehler.empty()) {
                fehler = std::string(pre, n - 1) + ": '" + v + "' unbekannt (";
                bool erst = true;
                for (const char* o : ok) { fehler += (erst ? "" : " | "); fehler += o; erst = false; }
                fehler += ")";
            }
            return true;
        };
        if (!strcmp(arg, "--sound")) { an = true; basis = -1; return true; }
        if (!strncmp(arg, "--sound=", 8)) { an = true; basis = static_cast<int>(strtol(arg + 8, nullptr, 0)); return true; }
        if (wert("--sound-takt=", takt, {"quarz", "halb", "system"})) return true;
        if (wert("--sound-kanal=", kanal, {"abc", "acb", "mono"})) return true;
        if (wert("--sound-chip=", chip, {"8910", "8912"})) return true;
        if (!strncmp(arg, "--wav=", 6)) { wav = arg + 6; return true; }
        return false;
    }
};
