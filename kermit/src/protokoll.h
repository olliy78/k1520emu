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
    /// 'Y' bereit, 'N' nie, oder Praefix ('&').  Vorgabe '&' (Befund K7): C-Kermit 4.2 schaltet das
    /// 8-Bit-Praefix nur ein, wenn WIR ein Zeichen nennen, und die WEGA-Leitung tty0 loescht Bit 7
    /// auf dem Weg PC → WEGA (getty-Einstellung) — mit 'Y' scheitert jedes Senden binaerer Daten.
    char qbin = '&';
    char rept = '~';             ///< Wiederholungspraefix, 0 = aus
    char qctl = '#';
    int maxl = 94;               ///< angebotene Grenze normaler Pakete
    /// Paketfrist in der ZEIT DER GEGENSTELLE (bei Tempo 1 = Wirtszeit).  Die tatsaechliche
    /// Wartezeit ist adaptiv (siehe `Sitzung::aktuelleFrist`): frist_ms / min(Tempo, 1) (eine
    /// langsame Gegenstelle bekommt mehr Zeit, eine schnelle nicht weniger), mindestens
    /// 4 x geglaettete Umlaufzeit, dieser Zuschlag hoechstens bis TIME der Gegenstelle (in ihrer
    /// Zeit); begrenzt auf [frist_min_ms, frist_max_ms] Wirtszeit.  Jedes empfangene Byte
    /// verlaengert die laufende Frist.
    int frist_ms = 10000;
    bool frist_fest = false;     ///< true: genau frist_ms Wirtszeit, keine Anpassung (alter Weg)
    /// Geschwindigkeit der Gegenstelle gegenueber Echtzeit (Emulator mit 10x Takt = 10, unter
    /// Last 0.5).  0 = Automatik: aus der Zeichenrate der Leitung geschaetzt (der Hub des
    /// Emulators taktet die Gast-Baudrate in MASCHINENZEIT, Zeichen kommen also tempo-mal so
    /// schnell wie die Baudrate) und aus der Umlaufzeit.
    double tempo = 0.0;
    int baud = 9600;             ///< Nenn-Baudrate der Gegenstelle (Grundlage der Tempo-Schaetzung)
    int frist_min_ms = 1000;     ///< untere Grenze der adaptiven Frist (Wirtszeit)
    int frist_max_ms = 600000;   ///< obere Grenze der adaptiven Frist (Wirtszeit)
    /// Nach einer Transaktion, deren B WIR quittiert haben, vor dem naechsten Befehl so lange (Zeit der
    /// Gegenstelle) auf das Leerlauf-N(0) eines C-Kermit-Servers warten; 0 = nicht warten.
    int nachlauf_ms = 2000;
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
    /// Jedes Paket (Protokollfenster, Fehlersuche): gesendet/empfangen, Typ, Folgenummer, Datenfeld
    /// (kodiert, wie auf der Leitung).  Wiederholungen erscheinen erneut.
    std::function<void(bool gesendet, char typ, int seq, const std::string& daten)> paket;
};

/// Zaehler einer Sitzung (Messung K7: Wiederholungen, Verluste, Durchsatz).
struct Statistik {
    int pakete_gesendet = 0;     ///< alle Rahmen (auch Wiederholungen, NAKs)
    int wiederholungen = 0;      ///< erneut gesendete Rahmen (Frist, NAK, Duplikat)
    int naks_gesendet = 0;
    int naks_empfangen = 0;
    int fristen = 0;             ///< abgelaufene Paketfristen
    int fehlerhaft = 0;          ///< verworfene Pakete (Pruefung, Form)
    int64_t bytes_gesendet = 0;  ///< Leitungsbytes
    int64_t bytes_empfangen = 0;
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

    // --- Zeitverhalten (adaptive Fristen, Entwurf 36 §6.5) -------------------
    /// Wirksames Tempo der Gegenstelle: Einstellung, sonst Schaetzung, sonst 1.
    double tempo() const;
    /// Geschaetztes Tempo aus der Zeichenrate (0 = noch keine Messung).
    double tempoSchaetzung() const { return tempoSchaetzung_; }
    /// Schaetzung vorbelegen (z. B. aus der Zeichenrate des Login-Textes).
    void setzeTempoSchaetzung(double t) { if (t > 0) tempoSchaetzung_ = t; }
    /// Geglaettete Umlaufzeit Paket → Quittung in ms Wirtszeit (0 = noch keine Messung).
    double umlaufzeit() const { return srtt_; }
    /// Aktuelle Paketfrist in ms Wirtszeit.
    int64_t aktuelleFrist() const { return wartefrist(); }
    /// Ausgehandelte Parameter der laufenden/letzten Transaktion.
    const Vereinbarung& vereinbarung() const { return v_; }
    const Statistik& statistik() const { return st_; }
    void statistikZuruecksetzen() { st_ = {}; }

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
    Ergebnis befehlG(const std::string& klartext);

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
    // Zeitmessung
    void tempoMessung(uint8_t b);   ///< je zugefuehrtem Byte (Rate innerhalb eines Pakets)
    void umlaufMessung(int64_t gesendetMs);
    double tempoSchaetzung_ = 0;
    double srtt_ = 0;
    int64_t pufZeit_ = 0;           ///< Ankunftszeit des aktuellen Lesepuffers
    uint64_t pufNr_ = 0;            ///< laufende Nummer des Lesepuffers
    bool messAktiv_ = false;
    uint64_t messNr_ = 0;
    int64_t messStart_ = 0, messEnde_ = 0;
    int messBytes_ = 0;
    double messSummeBytes_ = 0, messSummeMs_ = 0;   ///< gleitende Summen der Ratenmessung
    Statistik st_;
    bool nachlauf_ = false;         ///< zuletzt ein B quittiert (siehe leereEingang)
    bool nachlaufNoetig_ = true;    ///< die Gegenstelle sendet wie C-Kermit 4.2 (9 Init-Felder, kein CAPAS)
    std::atomic<bool> abbruch_{false};
    std::atomic<bool> abbruchDatei_{false};
    std::atomic<bool> abbruchGruppe_{false};
};

}  // namespace k1520::kermit
