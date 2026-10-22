/**
 * @file loginskript.h
 * @brief Anmeldung an einer getty-Leitung vor dem Kermit-Protokollstart.
 *
 * @details
 * Doc: doc/design/36_kermit.md §4.3 (AP K3), Messwerte `doc/p8000/kermit_wega.md` §3/§4.
 * Ablauf gegen die WEGA-3.1-Anmeldung (`login kermit`, Shell = `kermit -x`-Ersatz ueber `.kermrc`):
 *   1. RETURN senden, bis „login:" erscheint (eine getty, die ins Leere gesendet hat,
 *      meldet sich erst auf Eingabe), dann den Namen senden;
 *   2. bei „Password:" das Kennwort senden;
 *   3. auf den Serverbeginn warten („server starting"): vorher kommen Kennwortzeile,
 *      Tagesmeldung (~1,9 KB, ESC-Folgen) und das Kermit-Banner, dazu ca. 10 s
 *      MASCHINENZEIT Stille (Anmeldung, `.kermrc`) — alles wird gelesen und verworfen;
 *   4. danach eine kurze Ruhezeit abwarten, damit nichts davon als Paketanfang gilt.
 *
 * **Bit 7 wird VOR dem Vergleich geloescht**: getty gibt nach einer Sitzung mit gerader
 * Paritaet aus (`WEGA login:` kommt als `<D7><C5>GA<A0>lo<E7>i<EE>:`).
 *
 * Fehlschlag = Text mit dem zuletzt Gelesenen (sichtbar gemacht, ohne Steuerzeichen).
 * Fristen sind einstellbar; sie laufen auf der Zeit der Leitung (`Leitung::jetztMs`) und sind
 * FRISTEN DER STILLE (K7, Entwurf 36 §6.5): jedes empfangene Zeichen setzt sie neu — eine
 * langsame Gegenstelle (Emulator unter Last), die gerade die Tagesmeldung schreibt, laeuft so
 * nicht in eine feste Sekundenzahl.  Ein ausdrueckliches `tempo` teilt alle Zeiten (Ruhe,
 * RETURN-Abstand, Fristen).  RETURN wird nur wiederholt, wenn die Leitung still ist, und nach
 * „login:" wird erst die Stille abgewartet (sonst laesen gestaute RETURN ein zweites
 * „login:" als Ablehnung des Namens).  Nebenbei wird die Zeichenrate gemessen
 * (`LoginErgebnis::tempo`, Grundlage fuer `Sitzung::setzeTempoSchaetzung`).
 * Abbruch: `*abbruch` (optional) wird zwischen den Leseschritten geprueft; ein
 * `Leitung::wecken()` beendet den laufenden Lesevorgang sofort.
 */
#pragma once

#include <atomic>
#include <functional>
#include <string>

#include "kermit/src/leitung.h"

namespace k1520::kermit {

/// Art des Ausgangs (K10): die Oberflaeche fragt nur bei `Abgelehnt` erneut, bei `Frist`/`Leitung` nicht.
enum class LoginArt {
    Ok,         ///< angemeldet
    Abgelehnt,  ///< nach dem Kennwort (oder dem Namen) erscheint wieder `login:` / `Login incorrect`
    Frist,      ///< Stille ohne Fortschritt: kein `login:` / `Password:` / Serverbeginn
    Leitung,    ///< Leitung getrennt
    Abbruch     ///< Abbruch-Flag oder der Anwender hat die Abfrage abgebrochen
};

/// Was die Anmeldung dem Abruf-Rueckruf (`LoginOptionen::abfrage`) mitgibt.
struct AnmeldeAnfrage {
    int versuch = 0;        ///< 0 = erste Frage; ab 1: der vorige Versuch wurde abgelehnt
    std::string gelesen;    ///< zuletzt von der Gegenstelle Gelesenes (sichtbar gemacht), z. B. "Login incorrect"
};

struct LoginOptionen {
    std::string name = "kermit";
    std::string kennwort = "kermit";
    bool mit_kennwort = true;       ///< false: nach dem Namen kein Kennwort erwarten
    int frist_login_ms = 60000;     ///< bis „login:" erscheint (RETURN-Wiederholung eingerechnet)
    int return_abstand_ms = 2000;   ///< so lange auf „login:" warten, dann erneut RETURN
    int frist_kennwort_ms = 60000;  ///< bis „Password:" (bzw. dem Serverbeginn) nach dem Namen
    int frist_server_ms = 120000;   ///< bis „server starting" nach dem Kennwort
    int ruhe_ms = 500;              ///< Stille nach dem Serverbeginn, bevor das Protokoll startet
    int ruhe_max_ms = 5000;         ///< laenger als so lange wird die Ruhe nicht abgewartet
    bool auf_server_warten = true;  ///< false: nach dem Kennwort fertig (kein Kermit-Server dahinter)
    std::string server_text = "server starting";
    double tempo = 0.0;             ///< Tempo der Gegenstelle (0 = unbekannt: Zeiten wie angegeben)
    int baud = 9600;                ///< Nenn-Baudrate (fuer die Tempo-Messung)
    std::atomic<bool>* abbruch = nullptr;
    /// Anmeldung AUF ABRUF (K10): Ist der Rueckruf gesetzt, wird er erst gerufen, wenn die Gegenstelle
    /// `login:` zeigt (und die Leitung dazu still ist); `name`/`kennwort` darf er ausfuellen.  `false` =
    /// der Anwender bricht ab (`LoginArt::Abbruch`).  Bei `Abgelehnt` wird er mit `versuch + 1` erneut
    /// gerufen, auf derselben Leitung.  Gerufen im Faden von `loginSkript`; die Zeit darin ist KEINE Frist
    /// (Fristen laufen nur in den Lesewartezeiten).  Er darf beliebig lange blockieren; ein Abbruch von
    /// aussen (`abbruch`-Flag) wirkt, sobald er zurueckkehrt.  `name`/`kennwort` der Optionen sind dann
    /// nur Vorbelegung.
    std::function<bool(const AnmeldeAnfrage&, std::string& name, std::string& kennwort)> abfrage;
};

struct LoginErgebnis {
    bool ok = false;
    LoginArt art = LoginArt::Frist;   ///< Ausgang (bei `ok` immer `Ok`)
    int versuche = 0;        ///< wie oft Name/Kennwort gesendet wurden
    std::string text;        ///< bei Fehlschlag: Grund + zuletzt Gelesenes
    std::string gelesen;     ///< gesamter gelesener Text (7 Bit, Steuerzeichen sichtbar)
    double tempo = 0.0;      ///< aus der Zeichenrate geschaetztes Tempo der Gegenstelle (0 = zu wenig Text)
    double zeichen_pro_s = 0.0;   ///< gemessene Zeichenrate in Wirtszeit
};

/// Fuehrt die Anmeldung aus.  Die Leitung muss verbunden sein.
LoginErgebnis loginSkript(Leitung& l, const LoginOptionen& o = {});

/// Text mit geloeschtem Bit 7 und sichtbaren Steuerzeichen (`\r`, `\n`, `<1B>`), gekuerzt auf die letzten `max` Zeichen.
std::string sichtbarMachen(const std::string& roh, std::size_t max = 240);

}  // namespace k1520::kermit
