/**
 * @file blattmodell.h
 * @brief Das Blatt: Punkte in Mechanikaufloesung, waechst nach unten (Entwurf 49 §4.3, §8 Speicher).
 *
 * Ein Bit je Punkt, **zeilenblockweise** (je 64 Zeilen ein Block, erst beim ersten Punkt angelegt) — ein
 * langes Endlospapier kostet nur, was beschrieben ist.  Kein Bezug auf Geraet, Nadeln oder Papierweg: wer
 * Punkte setzt, rechnet vorher in Blattkoordinaten (x in Takten, y in Rasterzeilen).
 *
 * Lesen je **Zeilenbereich** (`lies`), dazu ein @ref version "Versionszaehler": die Oberflaeche holt nur,
 * was sich geaendert hat, nie das ganze Blatt je Bild.  Zeilenformat: `(breite+7)/8` Bytes, **Bit 7 =
 * linkester Punkt**, 1 = Punkt.  Nicht thread-sicher; die Maschine laeuft unter der Kernsperre.
 */

#pragma once
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

class Blattmodell {
public:
    static constexpr int BLOCK_ZEILEN = 64;

    explicit Blattmodell(int breite);

    int breite() const { return breite_; }
    /// Hoehe in Zeilen: das Blatt reicht bis zur tiefsten beschriebenen/vom Papierweg erreichten Zeile.
    int hoehe() const { return hoehe_; }
    int zeilenBytes() const { return (breite_ + 7) / 8; }
    /// Wird bei jeder Aenderung (Punkt neu gesetzt, Blatt gewachsen, geleert) hochgezaehlt.
    uint64_t version() const { return version_; }

    /// Punkt setzen; false = ausserhalb des Blatts (x < 0, x >= breite, y < 0), dann nur gezaehlt.
    bool setze(int x, int y);
    bool punkt(int x, int y) const;
    /// Das Blatt reicht mindestens bis @p zeilen Zeilen (der Papierweg hat es so weit geschoben).
    void streckeBis(int zeilen);
    void leeren();

    /// Blattgrenze: das Papier wurde in Zeile @p y durchgeschnitten (K6316, Schneideinrichtung).  Die Punkte darunter gehoeren
    /// zum naechsten Beleg; die Oberflaeche zeichnet die Kante, das PDF kann dort umbrechen.  `leeren()` loescht sie.
    void schnitt(int y);
    const std::vector<int>& schnitte() const { return schnitte_; }

    /// Beschriebene Punkte (je Position nur einmal gezaehlt) und verworfene (ausserhalb).
    uint64_t punktzahl() const { return punkte_; }
    uint64_t ausserhalb() const { return ausserhalb_; }

    /// @p n Zeilen ab @p y0 ins Feld @p aus (je zeilenBytes() Bytes); Rueckgabe = Zeilen, die das Blatt hat
    /// (y0 + Rueckgabe <= hoehe(); weniger, wenn @p cap nicht reicht).
    int lies(int y0, int n, uint8_t* aus, size_t cap) const;

private:
    const uint8_t* zeile(int y) const;     ///< nullptr = nie beschrieben
    uint8_t*       zeileAnlegen(int y);

    int breite_;
    int hoehe_ = 0;
    uint64_t version_ = 0, punkte_ = 0, ausserhalb_ = 0;
    std::vector<std::unique_ptr<uint8_t[]>> bloecke_;
    std::vector<int> schnitte_;
};

/**
 * @brief ASCII-Abzug eines Ausschnitts: `#` = Punkt, `.` = leer.  Jedes Zeichen steht fuer @p sx mal @p sy
 *        Blattpunkte (ein Zeichen ist gesetzt, wenn irgendeiner davon gesetzt ist).  Zeilen enden mit '\n'.
 *        Die Diagnose der Druckertests und des Debuggers (`blatt`).
 */
std::string blattAscii(const Blattmodell& b, int x0, int y0, int breite, int hoehe, int sx = 1, int sy = 1);
