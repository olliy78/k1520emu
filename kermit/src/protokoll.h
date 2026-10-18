/**
 * @file protokoll.h
 * @brief Kermit-Zustandsautomaten: Senden (S,F,A,D,Z,B), Empfangen,
 *        Client-Befehle (R, G C/D/E/F/L) und die Serverschleife.
 *
 * @details
 * Doc: doc/design/36_kermit.md §4.2/§4.4 (AP K2).  Eine `Sitzung` fuehrt das
 * Protokoll ueber eine `Leitung`; gefahren wird Fenster 1 (Stop-and-Wait).
 *
 * Ablauf einer Uebertragung (Standardsemantik, Kermit Protocol Manual):
 *   Sender:    S → (Y) → F → [A] → D… → Z → … → B
 *   Empfaenger quittiert jedes Paket mit Y gleicher Nummer; NAK (N) bei
 *   Frist oder beschaedigtem Paket; ein N fuer das NAECHSTE Paket gilt als
 *   Quittung des laufenden.  Wiederholungen sind nach `max_wiederholungen`
 *   zu Ende (E-Paket „Zu viele Wiederholungen").
 *   Abbruch: Empfaenger quittiert ein D mit "X" (Datei) oder "Z" (Gruppe),
 *   der Sender antwortet mit Z "D" (verwerfen) und ggf. B.
 *
 * Blockpruefung: S/I und ihre Quittungen mit Typ 1; die ausgehandelte gilt
 * bis zum Ende der Transaktion (B), danach wieder 1 — so liest jede
 * Gegenstelle auch Serverbefehle und Kurzantworten (C-Kermit 4.2 tut dasselbe).
 *
 * Eigenheiten der Gegenstelle C-Kermit 4.2 (beruecksichtigt):
 *   - Die Quittung eines F-Pakets bleibt LEER: C-Kermit 4.2 prueft das erste
 *     Zeichen jeder Quittung auf 'X'/'Z' (canned()) — ein Dateiname dort
 *     („Zeta") brach sonst die Uebertragung ab.
 *   - Vor I/R/G wird der Eingang geleert (C-Kermit: ttflui); der C-Kermit-
 *     Server schickt bei jedem Wiedereintritt ein N(0).
 *   - `G C` traegt ggf. ein zweites Feld (Kennwort); gelesen wird nur das erste.
 *
 * Faeden: eine Sitzung = ein Faden.  `abbrechen*()` duerfen aus jedem Faden
 * gerufen werden (einzige gleichzeitige Operation).
 */
#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "kermit/src/dateien.h"
#include "kermit/src/leitung.h"
#include "kermit/src/paket.h"

namespace k1520::kermit {

enum class Fehlerart {
    Keiner = 0,
    Zeitueberschreitung,   ///< zu viele Fristen/Wiederholungen
    Gegenstelle,           ///< E-Paket empfangen
    Abgebrochen,           ///< Abbruch durch den Anwender
    LeitungGetrennt,
    DateiNichtGefunden,
    Lesefehler,
    Schreibfehler,         ///< u. a. Datentraeger voll
    PfadAbgewiesen,        ///< Freigabe hat den Namen abgewiesen
    NameZuLang,            ///< Ziel laenger als max_namenslaenge
    Abgelehnt,             ///< Datei von der Gegenstelle abgelehnt (A-Paket / X / Z)
    Protokoll,             ///< unerwartetes Paket
    NichtUnterstuetzt,
};

const char* fehlerText(Fehlerart a);

struct Einstellungen {
    int pruefsumme = 3;          ///< gewuenschte Blockpruefung 1..3
    int max_paket = 1000;        ///< Obergrenze fuer Langpakete (MAXLX)
    bool langpakete = true;      ///< CAPAS-Bit Langpakete anbieten
    bool attribute = true;       ///< CAPAS-Bit Attribute anbieten
    bool capas_senden = true;    ///< false: genau 9 Init-Felder (wie C-Kermit 4.2)
    int fenster = 1;             ///< angebotenes WINDO (0/1); gefahren wird immer 1
    char qbin = 'Y';             ///< 'Y' bereit, 'N' nie, oder Praefix ('&')
    char rept = '~';             ///< Wiederholungspraefix, 0 = aus
    char qctl = '#';
    int maxl = 94;               ///< angebotene Grenze normaler Pakete
    int frist_ms = 10000;        ///< eigene Frist je Paket
    bool frist_fest = false;     ///< true: TIME der Gegenstelle ignorieren
    int max_wiederholungen = 10;
    uint8_t eol = '\r';
    int npad = 0;
    uint8_t padc = 0;
    bool textmodus = false;      ///< Text: LF lokal ↔ CR LF auf der Leitung
    Namen namen_senden = Namen::Literal;
    Namen namen_empfang = Namen::Konvertiert;
    int max_namenslaenge = 0;    ///< 14 fuer WEGA-Ziele; 0 = keine Grenze
    bool i_paket = true;         ///< vor R/G ein I-Paket schicken
    bool ueberschreiben = true;  ///< vorhandene Dateien beim Empfang ersetzen
};

struct Rueckrufe {
    std::function<void(const std::string& datei, int64_t bytes, int64_t gesamt)> fortschritt;
    std::function<void(const std::string& text)> ereignis;
};

struct Ergebnis {
    Fehlerart art = Fehlerart::Keiner;
    std::string text;                 ///< Fehlertext (bzw. E-Paket der Gegenstelle)
    std::string ausgabe;              ///< Antworttext (G D, G E, Hilfe, X-Pakete)
    std::vector<std::string> dateien; ///< uebertragene Dateien (Ablagenamen)
    bool ok() const { return art == Fehlerart::Keiner; }
};

/// Eintrag fuer eine Sendegruppe.
struct SendeAuftrag {
    std::string lokal;     ///< lokaler Pfad
    std::string entfernt;  ///< Name beim Empfaenger (leer = Dateiname)
};

class Sitzung {
public:
    Sitzung(Leitung& l, Einstellungen e = {}, Rueckrufe r = {});

    Ergebnis senden(const std::string& lokalerPfad, const std::string& entfernterName);
    Ergebnis sendenGruppe(const std::vector<SendeAuftrag>& auftraege);
    /// Holt eine Datei (Muster erlaubt, wenn der Server es kann) vom Server.
    Ergebnis holen(const std::string& entfernterName, const std::string& zielOrdner);
    /// Wartet auf eine Uebertragung (S) der Gegenstelle (Kermit „receive").
    Ergebnis empfangen(const std::string& zielOrdner);
    Ergebnis entferntVerzeichnis(const std::string& pfad);
    Ergebnis entferntWechseln(const std::string& pfad);
    Ergebnis entferntLoeschen(const std::string& name);
    Ergebnis entferntHilfe();
    Ergebnis entferntBeenden();   ///< G F
    Ergebnis entferntAbmelden();  ///< G L
    /// Serverschleife bis G F/G L, Abbruch oder getrennte Leitung.
    Ergebnis serve(Freigabe& f);

    /// Sofortiger Abbruch (E-Paket an die Gegenstelle).
    void abbrechen();
    /// Laufende Datei abbrechen (X-Semantik), Gruppe abbrechen (Z-Semantik).
    void abbrechenDatei() { abbruchDatei_ = true; }
    void abbrechenGruppe() { abbruchGruppe_ = true; }

    Einstellungen& einstellungen() { return e_; }

private:
    // Paket-E/A
    enum class Lesen { Ok, Frist, Fehlerhaft, Getrennt, Abbruch };
    Lesen lesePaket(Paket& p, int64_t fristMs);
    bool sende(char typ, int seq, const std::string& daten);
    bool sendeRoh(const std::string& rahmen);
    void sendeFehler(const std::string& text);
    void leereEingang();
    int64_t wartefrist() const;

    // Bausteine des Stop-and-Wait
    struct Antwort {
        Fehlerart art = Fehlerart::Keiner;
        std::string text;
        Paket paket;
    };
    /// Sendet ein Paket und wartet auf seine Quittung (Y gleicher Nummer).
    Antwort sendeUndWarte(char typ, int seq, const std::string& daten);
    /// Wartet auf Paket `erw` (quittiert Duplikate erneut, NAK bei Frist).
    Antwort warteAuf(int erw);

    InitParameter meineParameter() const;
    Ergebnis fehler(Fehlerart a, const std::string& text, bool epaket);
    Ergebnis ausAntwort(const Antwort& a);

    // Transaktionen
    struct Quelle {
        std::unique_ptr<DateiQuelle> q;
        std::string name;        ///< Name im F/X-Paket
        bool alsX = false;       ///< Text-Paket (Bildschirm) statt Datei
        bool text = false;
        std::string anzeige;
    };
    Ergebnis sendeTransaktion(std::vector<Quelle>& quellen);
    struct Ziel {
        Freigabe* freigabe = nullptr;   ///< nullptr: alles in `ausgabe`
        std::string* ausgabe = nullptr;
    };
    Ergebnis empfangeTransaktion(const Paket& s, Ziel z);
    Ergebnis iAustausch();
    Ergebnis befehl(char typ, const std::string& klartext, Ziel z);
    Ergebnis generisch(char unter, const std::vector<std::string>& args);

    // Server
    Ergebnis serverBefehl(const Paket& p, Freigabe& f, bool& ende);
    Ergebnis serverSende(std::vector<Quelle>& quellen);

    Leitung& l_;
    Einstellungen e_;
    Rueckrufe r_;
    PaketZerleger zerleger_;
    std::vector<uint8_t> puffer_;
    std::size_t pufPos_ = 0;
    int chkt_ = 1;                  ///< laufende Blockpruefung
    Vereinbarung v_;                ///< Parameter der laufenden Transaktion
    std::string letztes_;           ///< zuletzt gesendeter Rahmen (fuer Wiederholung)
    char letzterTyp_ = 0;
    int letzteSeq_ = -1;
    std::atomic<bool> abbruch_{false};
    std::atomic<bool> abbruchDatei_{false};
    std::atomic<bool> abbruchGruppe_{false};
};

}  // namespace k1520::kermit
