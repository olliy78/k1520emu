/**
 * @file paket.h
 * @brief Kermit-Paketschicht: Blockpruefung 1/2/3, Paketrahmen (normal und
 *        Langpaket), Paketzerleger, Init-Parameter (S/I/Y), Aushandlung,
 *        Praefix-Kodierung (Steuerzeichen, 8. Bit, Wiederholung) und
 *        Attribute-Pakete.
 *
 * @details
 * Doc: doc/design/36_kermit.md §4.1/§4.2 (AP K2).  Grundlage ist das Kermit
 * Protocol Manual (6. Auflage); die Gegenstelle im Emulator ist C-Kermit 4.2
 * (ckfns.c/ckfns2.c), deren Eigenheiten hier ausdruecklich beruecksichtigt
 * sind:
 *   - C-Kermit 4.2 sendet GENAU 9 Init-Felder (kein CAPAS ⇒ weder Langpakete
 *     noch Attribute) und uebernimmt beim Senden die Blockpruefung, die der
 *     Empfaenger in seinem Y nennt; als Empfaenger spiegelt es die des Senders.
 *     Wir spiegeln als Empfaenger ebenfalls (siehe `antwortParameter`).
 *   - QBIN: C-Kermit 4.2 bietet ohne Paritaet nur 'Y' an und schaltet die
 *     8-Bit-Praefixe nur ein, wenn WIR ein Zeichen ('&') nennen.
 *   - S-, I-Pakete (und die Y darauf, weil die Blockpruefung vor dem Ende der
 *     Aushandlung 1 ist) tragen Blockpruefung 1; ein N wird ueber seine Laenge
 *     erkannt (NAKs haben keine Daten).
 *
 * Die Schicht ist rein: kein Faden, keine Leitung, keine Datei.
 */
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace k1520::kermit {

constexpr uint8_t SOH = 0x01;           ///< Paketbeginn (MARK), Vorgabe
constexpr int MAX_NORMAL = 94;          ///< groesster LEN-Wert eines normalen Pakets
constexpr int MAX_LANG = 9024;          ///< groesste Langpaketlaenge (95*95-1, gerundet)

inline char tochar(int x) { return static_cast<char>(x + 32); }
inline int unchar(char c) { return static_cast<int>(static_cast<uint8_t>(c)) - 32; }
inline uint8_t ctl(uint8_t c) { return static_cast<uint8_t>(c ^ 64); }

/// Gueltiges Praefixzeichen fuer QBIN/REPT (33..62, 96..126), wie C-Kermit 4.2.
inline bool gueltigesPraefix(char c) {
    const int v = static_cast<uint8_t>(c);
    return (v > 32 && v < 64) || (v > 96 && v < 127);
}

// ─── Blockpruefung ─────────────────────────────────────────────────────────

/// Typ 1: 6-Bit-Pruefsumme (Summe s; (s + ((s & 0xC0) >> 6)) & 0x3F).
int pruefsumme1(const uint8_t* p, std::size_t n);
/// Typ 2: 12-Bit-Summe aller Bytes.
int pruefsumme2(const uint8_t* p, std::size_t n);
/// Typ 3: CRC-16/KERMIT (Polynom 0x1021 gespiegelt = 0x8408, Start 0, kein
/// Endexor; Pruefwert "123456789" = 0x2189).  NICHT CRC-16/CCITT-FALSE.
uint16_t crc16Kermit(const uint8_t* p, std::size_t n);
/// Blockpruefung als druckbare Zeichen (1, 2 oder 3 Zeichen).
std::string blockpruefung(int typ, const uint8_t* p, std::size_t n);

// ─── Pakete ────────────────────────────────────────────────────────────────

struct Paket {
    char typ = 0;
    int seq = 0;
    std::string daten;   ///< Datenfeld, wie es auf der Leitung steht (kodiert)
    int chkl = 0;        ///< Laenge der Blockpruefung, mit der es gelesen wurde (0 = unbekannt)
};

struct RahmenParameter {
    uint8_t mark = SOH;
    uint8_t eol = '\r';
    int npad = 0;
    uint8_t padc = 0;
};

/// Baut ein Paket fuer die Leitung (Fuellzeichen, MARK … Pruefung, EOL).
/// Langpaket genau dann, wenn das Datenfeld nicht in ein normales passt.
std::string baueRahmen(const Paket& p, int chkt, const RahmenParameter& rp);

/// Blockpruefungslaenge, mit der ein Paket des Typs gelesen bzw. gesendet
/// wird: S und I immer 1, sonst die laufende.
inline int chktFuerTyp(char typ, int laufend) { return (typ == 'S' || typ == 'I') ? 1 : laufend; }

/**
 * Byteweiser Paketzerleger.  Sucht MARK, liest Kopf, Daten und Pruefung,
 * synchronisiert bei einem MARK mitten im Paket neu (wie C-Kermit) und
 * meldet ein Steuerzeichen im Datenteil sofort als beschaedigt (abgeschnittenes
 * Paket — im Datenfeld stehen nie rohe Steuerzeichen).
 */
class PaketZerleger {
public:
    enum class Ergebnis { Weiter, Paket, Fehlerhaft };

    void setzeChkt(int c) { chkt_ = c; }
    void setzeMark(uint8_t m) { mark_ = m; }
    void zuruecksetzen() { z_ = Z::Suche; }
    /// Die Gegenstelle sendet mit Paritaetsbit (erkannt oder vorgegeben): Bit 7 wird abgestreift.
    bool paritaetErkannt() const { return par_; }

    Ergebnis zufuehren(uint8_t b);
    const Paket& paket() const { return paket_; }

private:
    enum class Z { Suche, Len, Seq, Typ, Lx1, Lx2, Hchk, Daten, Pruef };
    Ergebnis fehler() { z_ = Z::Suche; return Ergebnis::Fehlerhaft; }

    Z z_ = Z::Suche;
    bool par_ = false;
    int chkt_ = 1;
    uint8_t mark_ = SOH;
    int len_ = 0, chkl_ = 1, datenLen_ = 0;
    std::string roh_;      ///< ab LEN bis vor die Pruefung
    std::string pruef_;
    Paket paket_;
};

// ─── Init-Parameter und Aushandlung ────────────────────────────────────────

enum : int {
    CAPAS_LANGPAKETE = 0x02,
    CAPAS_FENSTER = 0x04,
    CAPAS_ATTRIBUTE = 0x08,
};

struct InitParameter {
    int maxl = 80;
    int time = 5;
    int npad = 0;
    uint8_t padc = 0;
    uint8_t eol = '\r';
    char qctl = '#';
    char qbin = 'N';      ///< 'Y', 'N' oder Praefixzeichen
    int chkt = 1;
    char rept = ' ';      ///< ' ' = keine Wiederholung
    int capas = 0;
    int windo = 0;
    int maxlx = 0;        ///< 0 = nicht angegeben
    int felder = 0;       ///< Anzahl gelesener Felder (Diagnose)
};

/// Datenfeld eines S/I/Y-Pakets.  mitCapas=false ⇒ genau 9 Felder (C-Kermit 4.2).
std::string initDaten(const InitParameter& ip, bool mitCapas);
InitParameter zerlegeInit(const std::string& daten);

/// Parameter, mit denen ein Empfaenger auf das S/I des Senders antwortet:
/// Blockpruefung gespiegelt (sofern 1..3, sonst 1), sonst die eigenen.
InitParameter antwortParameter(const InitParameter& mein, const InitParameter& dein);

struct Kodierung {
    char qctlMein = '#';   ///< Praefix, mit dem ICH Steuerzeichen markiere
    char qctlDein = '#';   ///< Praefix der Gegenstelle (zum Dekodieren)
    char qbin = 0;         ///< 0 = kein 8-Bit-Praefix
    char rept = 0;         ///< 0 = keine Wiederholungszaehler
};

struct Vereinbarung {
    int chkt = 1;
    Kodierung kod;
    bool langpakete = false;
    bool attribute = false;
    int maxl = 80;          ///< LEN-Grenze normaler Pakete an die Gegenstelle
    int maxlx = 0;          ///< Langpaketgrenze an die Gegenstelle (0 = keine)
    RahmenParameter rahmen; ///< EOL/Fuellung, wie die Gegenstelle sie will
    int time = 0;           ///< TIME der Gegenstelle (s, 0 = keine Angabe)

    /// Groesstes Datenfeld, das an die Gegenstelle gehen darf.
    int datenBudget() const;
};

/// Aushandlung aus den gesendeten (mein) und empfangenen (dein) Parametern.
/// `eigenesMaxPaket`: eigene Obergrenze fuer Langpakete.
Vereinbarung verhandle(const InitParameter& mein, const InitParameter& dein, int eigenesMaxPaket);

// ─── Praefix-Kodierung ─────────────────────────────────────────────────────

/// Kodiert ab src[0] so viele Bytes, wie in `maxLen` Zeichen passen (eine
/// Praefixfolge wird nie zerteilt), haengt an `aus` an und liefert die Anzahl
/// verbrauchter Bytes.  Wiederholung ab drei gleichen Bytes, hoechstens 94.
std::size_t kodiereBlock(const Kodierung& k, const uint8_t* src, std::size_t n,
                         std::size_t maxLen, std::string& aus);
/// Kodiert eine ganze Zeichenkette ohne Laengengrenze (Namen, Befehle, Text).
std::string kodiere(const Kodierung& k, const std::string& s);
/// Dekodiert ein Datenfeld.  false bei Formfehler (abgeschnittene Praefixfolge).
bool dekodiere(const Kodierung& k, const std::string& ein, std::vector<uint8_t>& aus);
/// Dekodiert, bei Formfehler das Rohfeld (fuer E-Pakete und Meldungen).
std::string dekodiereText(const Kodierung& k, const std::string& ein);

// ─── Attribute ─────────────────────────────────────────────────────────────

struct Attribute {
    int64_t laenge = -1;      ///< '1' genaue Laenge in Bytes
    int64_t laengeK = -1;     ///< '!' ungefaehre Laenge in KB
    std::string datum;        ///< '#' "yyyymmdd hh:mm:ss" (Ortszeit)
    char typ = 0;             ///< '"' 'A' Text, 'B' binaer, 0 unbekannt
};

/// Attribute-Datenfeld (unkodiert: alle Werte druckbar, wie C-Kermit 5A).
std::string attributDaten(const Attribute& a);
Attribute zerlegeAttribute(const std::string& daten);

}  // namespace k1520::kermit
