/**
 * @file programmdatei.h
 * @brief Programmdateiformate von JKCEMU (Speicherabbild mit/ohne Kopf) lesen und schreiben —
 *        Headersave, KCC/KCB/KCM/JTC, KC-TAP, Intel-HEX, roh.
 *
 * **Wozu.**  Das Band (`.k52`) traegt rohe Bloecke, deren Inhalt die Gastsoftware bestimmt; ein
 * JKCEMU-Format ist deshalb kein Bandformat, sondern eine Huelle um ein *Speicherabbild*
 * (Anfangsadresse, optional Startadresse, Name, Bytes).  Diese Huelle wird hier abgestreift bzw.
 * angelegt, damit `k1520tape` (put/get/bootband) dieselben Dateien versteht wie JKCEMU
 * (doc/kassette/jkcemu_formate.md).  Nicht uebernommen: Audio-/Tape-Dateien (WAV, CSW, TZX, CDT,
 * ZX-TAP) und KC-BASIC-Dateien — sie beschreiben ein Tonsignal bzw. Rechner, die wir nicht haben.
 *
 * Alles little-endian.  Reine Funktionen ohne Zustand.
 */
#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace kassette::programmdatei {

enum class Format { Auto, Roh, Headersave, Kcc, KcTap, IntelHex };

/// Ein Speicherabbild: `daten` liegen ab `anfang` im Speicher.
struct Abbild {
    bool mit_adresse = true;        ///< false bei Roh: `anfang` ist dann nur 0, keine Angabe der Datei
    uint16_t anfang = 0;
    std::optional<uint16_t> start;  ///< Einsprung (nur wenn die Datei ihn angibt)
    std::string name;               ///< Kopfname (Headersave 16, KCC 8+3 → „NAME.TYP“), leer bei Roh/HEX
    char typ = 0;                   ///< Headersave-Dateityp (`C`, `M`, `B` …), sonst 0
    std::vector<uint8_t> daten;     ///< nie leer bei Erfolg
};

/// Kennt den Namen: "auto|roh|headersave|kcc|kctap|hex" (auch ".z80", ".kcc", ".kcb", ".kcm",
/// ".jtc", ".tap", ".hex"); nullopt bei Unbekanntem.
std::optional<Format> formatVonName(const std::string& s);
const char* formatName(Format f);

/// Format am Inhalt erkennen (Headersave: `D3 D3 D3` bei 13; KC-TAP: Kennung; Intel-HEX: `:` +
/// Hexziffern; KCC: nur ueber die Endung ableitbar, deshalb nie Ergebnis von Auto — `Roh`, wenn
/// nichts erkannt wird).
Format erkenne(const std::vector<uint8_t>& d);

/**
 * @brief Datei → Abbild.  @p f = Auto erkennt (`.kcc` & Co. muss der Aufrufer ausdruecklich waehlen).
 *
 * Roh liefert `anfang = 0`.  Intel-HEX fuellt Luecken mit 00H und braucht zusammenhaengende
 * Daten innerhalb 16 Bit; mehr als 64 KiB oder Adresse > FFFFH ist ein Fehler.  KC-TAP: nur die
 * ERSTE Teildatei (Multi-KC-TAP: @p weitere meldet, ob dahinter noch eine Kennung folgt).
 * KCC-Endadresse: JKCEMU laedt bis einschliesslich der eingetragenen Adresse, hoechstens soviel
 * wie die Datei hergibt — nie ein Byte zu wenig (Endadresse Z9001-inklusiv wie KC85-exklusiv).
 * @return nullopt und @p fehler (deutscher Satz) bei kaputtem/abgeschnittenem Inhalt.
 */
std::optional<Abbild> lesen(const std::vector<uint8_t>& d, Format f, std::string& fehler, bool* weitere = nullptr);

struct SchreibOptionen {
    bool kc_ende_exklusiv = true;  ///< KCC/KC-TAP: Endadresse + 1 (KC85/HC900, JKCEMU-Vorgabe); false = Z9001/KC87/JTC
};

/// Abbild → Dateiinhalt.  Roh = nur die Daten.  nullopt/@p fehler bei Format Auto, Datenlaenge 0
/// oder Ende > FFFFH.
std::optional<std::vector<uint8_t>> schreiben(const Abbild& a, Format f, std::string& fehler,
                                              const SchreibOptionen& o = {});

}  // namespace kassette::programmdatei
