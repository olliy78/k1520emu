/**
 * @file osm_urlader.h
 * @brief Synthetisches OS/M-Bootabbild (P8000) fuer die DiskTool-Tests.
 *
 * Die Erkennung (`boot_header: osm`, doc/design/34_osm.md §3.2) haengt an drei
 * Merkmalen von c0h0 Sektor 1, belegt an allen acht OS/M-Systemdisketten der
 * pofo-Sammlung: die ersten zehn Codebytes des Urladers, FDCONF bei 0x7E und die
 * Kennung `P8000SYS` bei 0x80.  Mehr braucht ein Test nicht — der Urlader selbst
 * (und das System dahinter) wird nicht eingecheckt.  Der Rest des Abbilds ist 0xE5.
 *
 * Header-only und ohne Abhaengigkeiten (wie temp_path.h).
 *
 * ```
 * const auto img = k1520test::osmBootabbild(0x77, 20480);   // 800K, Typ 7
 * ```
 */
#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

namespace k1520test {

/// @brief Die ersten zehn Bytes des OS/M-Urladers (LD IY,VEKTOR / LD SP,4000H / LD A,(FDCONF)).
inline const uint8_t kOsmUrladerAnfang[10] = {0xFD, 0x21, 0x48, 0x0C, 0x31,
                                              0x00, 0x40, 0x3A, 0x7E, 0x0C};

/// @brief Bootabbild (Systemzylinder 0-1) mit OS/M-Urlader-Merkmalen in Sektor 1.
/// @param fdconf   Byte 0x7E — 0x55 (640K, Typ 5) bzw. 0x77 (800K, Typ 7)
/// @param groesse  16384 (640K) bzw. 20480 (800K)
inline std::vector<uint8_t> osmBootabbild(uint8_t fdconf, size_t groesse) {
    std::vector<uint8_t> img(groesse, 0xE5);
    std::memcpy(img.data(), kOsmUrladerAnfang, sizeof kOsmUrladerAnfang);
    img[0x7E] = fdconf;
    img[0x7F] = 0x00;
    std::memcpy(img.data() + 0x80, "P8000SYS", 8);
    return img;
}

/// @brief Schreibt @p img nach @p pfad (binaer).
inline void schreibeDatei(const std::string& pfad, const std::vector<uint8_t>& img) {
    std::ofstream(pfad, std::ios::binary)
        .write(reinterpret_cast<const char*>(img.data()), static_cast<std::streamsize>(img.size()));
}

}  // namespace k1520test
