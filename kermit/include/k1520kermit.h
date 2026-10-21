/**
 * @file k1520kermit.h
 * @brief C-ABI von `libk1520kermit` — Kermit (Client UND Server) ueber Telnet/RFC 2217.
 *
 * Doc: doc/design/36_kermit.md §4.4 (AP K4).  Stabil und additiv: Funktionen kommen
 * dazu, keine aendert ihre Signatur.  Stil wie `k1520_disk_api.h`: opakes Handle,
 * `int`-Ergebnis (0 = in Ordnung) + Fehlertext in Klartext-Deutsch, keine Ausnahmen,
 * Zeichenketten UTF-8.
 *
 * **Ein Handle = eine Leitung samt Kermit-Sitzung.**  Ein Handle wird von EINEM Faden
 * benutzt; allein `k1520k_abort*` darf ein anderer Faden rufen.  Rueckrufe laufen im
 * Faden, der die Operation ruft (Arbeitsfaden der Oberflaeche — die stellt sie ueber
 * Qt-Signale in den GUI-Faden).
 *
 * Zeichenketten-Rueckgaben (`k1520k_last_error`, `k1520k_result_string`) zeigen auf
 * Puffer IM Handle und gelten bis zum naechsten Aufruf auf diesem Handle; wer sie
 * behalten will, kopiert sie.  `k1520k_last_open_error` und `k1520k_parse_ls` benutzen
 * einen Puffer je Faden.
 *
 * Ablauf Client (PC fragt WEGA):
 * @code
 *   h = k1520k_open_telnet_client("127.0.0.1", 5000, 5000);      // oder ..._server + k1520k_accept
 *   k1520k_login(h, "kermit", "kermit", 60000);                  // nur an einer getty
 *   k1520k_send(h, "C:/daten/a.bin", "a.bin");
 *   k1520k_remote_dir(h, "", buf, sizeof buf);
 *   k1520k_remote_finish(h);  k1520k_close(h);
 * @endcode
 * Ablauf Server (PC gibt einen Ordner frei): Leitung oeffnen/annehmen, dann
 * `k1520k_serve(h, wurzel, nur_lesen)` (blockiert bis G F/G L, Abbruch, Trennung; mit Anmeldezwang nur bis Abbruch/Trennung).
 *
 * @see doc/design/36_kermit.md
 */
#pragma once
#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#include "k1520kermit_export.h"

/// Opakes Handle (Leitung + Sitzung).
typedef void* K1520Kermit;

/// Ergebniscodes.  0 = in Ordnung; 1..12 = Fehlerart der Sitzung (`Fehlerart` in
/// `kermit/src/protokoll.h`), ab 100 Fehler der Hilfsschicht.
enum {
    K1520K_OK = 0,
    K1520K_E_TIMEOUT = 1,        ///< zu viele Fristen/Wiederholungen
    K1520K_E_REMOTE = 2,         ///< Fehlerpaket (E) der Gegenstelle
    K1520K_E_ABORTED = 3,        ///< Abbruch durch den Anwender
    K1520K_E_DISCONNECTED = 4,   ///< Leitung getrennt
    K1520K_E_NOT_FOUND = 5,      ///< lokale Datei nicht gefunden
    K1520K_E_READ = 6,           ///< Lesefehler
    K1520K_E_WRITE = 7,          ///< Schreibfehler (u. a. Datentraeger voll)
    K1520K_E_PATH = 8,           ///< Pfad/Name abgewiesen
    K1520K_E_NAME_TOO_LONG = 9,  ///< Zielname laenger als `k1520k_set_max_namenslaenge`
    K1520K_E_REFUSED = 10,       ///< Datei von der Gegenstelle abgelehnt
    K1520K_E_PROTOCOL = 11,      ///< unerwartetes Paket
    K1520K_E_UNSUPPORTED = 12,   ///< Gegenstelle kann es nicht
    K1520K_E_USAGE = 100,        ///< falsches Argument oder falscher Zustand (kein Handle, nicht verbunden, ...)
    K1520K_E_LINE = 101,         ///< Leitung: Verbinden/Lauschen/Annehmen fehlgeschlagen
    K1520K_E_LOGIN = 102         ///< Anmeldung fehlgeschlagen (Text: `k1520k_last_error`)
};

/// Fortschritt einer Datei: `gesamt` = -1, wenn die Gegenstelle die Groesse nicht nennt.
typedef void (*K1520KProgress)(const char* datei, int64_t bytes, int64_t gesamt, void* user);
/// Ereignistext (Protokollfenster: Aushandlung, Wiederholungen, Dateibeginn/-ende ...).
typedef void (*K1520KEvent)(const char* text, void* user);

/* ─── Version ─────────────────────────────────────────────────────────────── */

/// Bauversion, dieselbe Quelle wie `k1520_version()`/`k1520d_version()`.
K1520K_API const char* k1520k_version(void);

/* ─── Leitung oeffnen ─────────────────────────────────────────────────────── */

/// Telnet-Client: verbindet zu host:port (blockierend, hoechstens `frist_ms`) und
/// handelt 8-Bit-BINARY aus.  NULL bei Fehler (Grund: @ref k1520k_last_open_error).
K1520K_API K1520Kermit k1520k_open_telnet_client(const char* host, int port, int frist_ms);

/// Telnet-Server: lauscht auf `port` (0 = freier Port, abfragbar mit
/// @ref k1520k_listen_port); die Verbindung nimmt @ref k1520k_accept an.
K1520K_API K1520Kermit k1520k_open_telnet_server(int port);

/// RFC-2217-Client.  Baud/Format gehen als WUNSCH hinaus (`baud` <= 0: 9600, `datenbits`
/// <= 0: 8, `paritaet` 0 oder 'N'/'O'/'E', `stoppbits` <= 0: 1); der Gast ist massgeblich.
K1520K_API K1520Kermit k1520k_open_rfc2217_client(const char* host, int port, int frist_ms, int baud,
                                                  int datenbits, int paritaet, int stoppbits);

/// RFC-2217-Server (Parameter wie beim Client: die Werte, mit denen er Anfragen beantwortet).
K1520K_API K1520Kermit k1520k_open_rfc2217_server(int port, int baud, int datenbits, int paritaet,
                                                  int stoppbits);

/// Grund des letzten fehlgeschlagenen `k1520k_open_*` dieses Fadens.
K1520K_API const char* k1520k_last_open_error(void);

/// Gebundener Port eines lauschenden Servers (0 bei Clients oder nach `accept`).
K1520K_API int k1520k_listen_port(K1520Kermit h);

/// Nimmt EINE Verbindung an (blockierend, hoechstens `frist_ms`; Abbruch mit @ref k1520k_abort).
/// @return 0 oder K1520K_E_LINE (Frist, Abbruch, Fehler — Text in `k1520k_last_error`).
K1520K_API int k1520k_accept(K1520Kermit h, int frist_ms);

/// 1, wenn die Leitung verbunden ist (Client nach dem Oeffnen, Server nach `accept`), sonst 0.
K1520K_API int k1520k_connected(K1520Kermit h);

/// Gegenstelle als Text ("127.0.0.1:40122"), leer ohne Verbindung.
K1520K_API const char* k1520k_peer(K1520Kermit h);

/// Hinweis zur Leitung (z. B. "Telnet-BINARY nicht ausgehandelt ..."), meist leer.
K1520K_API const char* k1520k_line_notice(K1520Kermit h);

/* ─── Anmeldung ───────────────────────────────────────────────────────────── */

/// Login-Skript an einer getty-Leitung (RETURN bis "login:", Name, Kennwort, Serverbeginn
/// abwarten).  `frist_ms` gilt je Schritt (auf den Serverbeginn 2x).  `kennwort` NULL = keine
/// Kennwortabfrage erwarten.  Mit @ref k1520k_abort abbrechbar.
/// @return 0 oder K1520K_E_LOGIN (Text mit dem zuletzt Gelesenen); der volle Text liegt danach
///         in @ref k1520k_result_string.
K1520K_API int k1520k_login(K1520Kermit h, const char* name, const char* kennwort, int frist_ms);

/// Ausgang der letzten Anmeldung (@ref k1520k_login, @ref k1520k_login_ask) — nur diese Art sagt der
/// Oberflaeche, ob sie erneut fragen soll: nur bei ABGELEHNT.  Werte deckungsgleich mit `LoginArt`.
enum {
    K1520K_LOGIN_OK = 0,
    K1520K_LOGIN_ABGELEHNT = 1,   ///< wieder `login:` / `Login incorrect`: erneut fragen
    K1520K_LOGIN_FRIST = 2,       ///< Stille ohne Fortschritt
    K1520K_LOGIN_LEITUNG = 3,     ///< Leitung getrennt
    K1520K_LOGIN_ABBRUCH = 4      ///< @ref k1520k_abort oder keine Antwort im Rueckruf
};

/// Abfrage-Rueckruf der Anmeldung auf Abruf.  Wird erst gerufen, wenn die Gegenstelle `login:` zeigt,
/// IM FADEN von @ref k1520k_login_ask.  `versuch` 0 = erste Frage, ab 1 wurde der vorige Versuch
/// abgelehnt; `gelesen` = zuletzt Gelesenes (sichtbar gemacht).  Der Rueckruf antwortet mit
/// @ref k1520k_login_answer; ohne Antwort gilt die Anmeldung als abgebrochen.  Er darf beliebig lange
/// blockieren (die Eingabezeit ist keine Frist); @ref k1520k_abort wirkt, sobald er zurueckkehrt.
typedef void (*K1520KLoginAsk)(int versuch, const char* gelesen, void* user);

/// Anmeldung auf Abruf: wie @ref k1520k_login, aber Name und Kennwort kommen aus `ask`, und bei
/// ABGELEHNT wird `ask` erneut gerufen (dieselbe Leitung).  `frist_ms` wie bei k1520k_login.
/// @return 0 oder K1520K_E_LOGIN; die Art danach ueber @ref k1520k_login_kind.
K1520K_API int k1520k_login_ask(K1520Kermit h, K1520KLoginAsk ask, void* user, int frist_ms);

/// Nur INNERHALB des Abfrage-Rueckrufs (im selben Faden) rufen: setzt Name und Kennwort (`kennwort` NULL = leer).
K1520K_API int k1520k_login_answer(K1520Kermit h, const char* name, const char* kennwort);

/// Art des Ausgangs der letzten Anmeldung (K1520K_LOGIN_*); -1 bei ungueltigem Handle.
K1520K_API int k1520k_login_kind(K1520Kermit h);

/* ─── Einstellungen (jederzeit zwischen den Operationen) ──────────────────── */

/// Gewuenschte Blockpruefung 1..3 (Vorgabe 3; ausgehandelt, die Gegenstelle kann herunterstufen).
K1520K_API int k1520k_set_pruefsumme(K1520Kermit h, int typ);
/// Paketlaenge: <= 94 normale Pakete dieser Laenge (Langpakete aus), > 94 Langpakete bis zu dieser
/// Laenge (bis 9024).  Vorgabe 1000.
K1520K_API int k1520k_set_paketlaenge(K1520Kermit h, int laenge);
/// Namensumsetzung: 1 = konvertiert (Grossschrift/Punkt wie C-Kermit), 0 = literal.
/// Vorgabe: Senden literal, Empfangen konvertiert.
K1520K_API int k1520k_set_namen(K1520Kermit h, int senden_konvertiert, int empfang_konvertiert);
/// Textmodus (LF lokal <-> CR LF auf der Leitung): 1 an, 0 binaer (Vorgabe 0).
K1520K_API int k1520k_set_textmodus(K1520Kermit h, int an);
/// Frist je Paket in ms in der Zeit der Gegenstelle (Vorgabe 10000; wirksam adaptiv, siehe
/// @ref k1520k_set_tempo) und Wiederholungen je Paket (Vorgabe 10).
K1520K_API int k1520k_set_fristen(K1520Kermit h, int paket_ms, int max_wiederholungen);
/// Tempo der Gegenstelle gegenueber Echtzeit (Emulator mit 10x Takt = 10, unter Last 0.5);
/// 0 = Automatik (Vorgabe: aus Zeichenrate und Umlaufzeit geschaetzt).  Die Paketfrist von
/// @ref k1520k_set_fristen gilt in der Zeit der Gegenstelle und wird durch das Tempo geteilt;
/// gilt auch fuer die Anmeldung.  Bereich 0 oder 0.01 .. 1000.
K1520K_API int k1520k_set_tempo(K1520Kermit h, double tempo);
/// Wirksames Tempo (Einstellung, sonst Schaetzung, sonst 1.0); 0.0 bei ungueltigem Handle.
K1520K_API double k1520k_tempo(K1520Kermit h);
/// Hoechstlaenge eines Zielnamens (WEGA: 14); 0 = keine Grenze.  Laengere werden ABGELEHNT.
K1520K_API int k1520k_set_max_namenslaenge(K1520Kermit h, int n);
/// Vorhandene Dateien beim Empfang ersetzen (1, Vorgabe) oder umbenennen/ablehnen (0).
K1520K_API int k1520k_set_ueberschreiben(K1520Kermit h, int an);
/// Server mit Anmeldezwang (K12): `pflicht` = 1 laesst @ref k1520k_serve wie getty + login der WEGA
/// `login:` / `Password:` abfragen (nur mit passendem Namen/Kennwort beginnt die Serverschleife, sonst
/// `Login incorrect`); nach FINISH/BYE kehrt der Server zu `login:` zurueck, @ref k1520k_serve endet dann nur
/// noch durch Abbruch oder Trennung.  NULL fuer name/kennwort = unveraendert (Vorgabe kermit/kermit).
/// Ohne Pflicht (Vorgabe) endet der Server wie bisher nach FINISH/BYE.
K1520K_API int k1520k_set_server_login(K1520Kermit h, int pflicht, const char* name, const char* kennwort);
/// Rueckrufe; NULL schaltet einen ab.  Nicht waehrend einer laufenden Operation aufrufen.
K1520K_API int k1520k_set_callbacks(K1520Kermit h, K1520KProgress fortschritt, K1520KEvent ereignis,
                                    void* user);

/* ─── Client-Operationen (blockierend) ────────────────────────────────────── */

/// Datei senden.  `entfernt` NULL/"" = Dateiname.  Nach Erfolg: `k1520k_result_string` = die gesendeten
/// lokalen Dateien, eine je Zeile.
K1520K_API int k1520k_send(K1520Kermit h, const char* lokal, const char* entfernt);
/// Datei(en) holen (Muster nur, wenn der Server es kann) in `zielordner`.  Nach Erfolg:
/// `k1520k_result_string` = die abgelegten Namen, einer je Zeile.
K1520K_API int k1520k_get(K1520Kermit h, const char* entfernt, const char* zielordner);
/// Verzeichnis des Servers (G D, `ls -l`-Text; "" = aktuelles Verzeichnis).  Der Text steht in
/// `puffer` (NUL-beendet, ggf. gekuerzt; `puffer` darf NULL sein) und vollstaendig in
/// `k1520k_result_string`.
K1520K_API int k1520k_remote_dir(K1520Kermit h, const char* pfad, char* puffer, int groesse);
/// Verzeichnis des Servers wechseln (G C).
K1520K_API int k1520k_remote_cd(K1520Kermit h, const char* pfad);
/// Datei(en) auf dem Server loeschen (G E); Meldung in `k1520k_result_string`.
K1520K_API int k1520k_remote_delete(K1520Kermit h, const char* name);
/// Server beenden, Programm bleibt (G F) / abmelden (G L).
K1520K_API int k1520k_remote_finish(K1520Kermit h);
K1520K_API int k1520k_remote_logout(K1520Kermit h);

/* ─── Server (blockierend) ────────────────────────────────────────────────── */

/// Serverschleife ueber den Ordner `wurzel` (Pfade werden auf die Wurzel begrenzt; `nur_lesen` = 1:
/// Schreiben/Loeschen abgewiesen).  Kehrt bei G F/G L zurueck (0), bei Abbruch (K1520K_E_ABORTED)
/// oder Trennung (K1520K_E_DISCONNECTED).
K1520K_API int k1520k_serve(K1520Kermit h, const char* wurzel, int nur_lesen);

/* ─── Abbruch (aus JEDEM Faden) ───────────────────────────────────────────── */

/// Sofortiger Abbruch der laufenden Operation (auch `accept`, `login`, `serve`).
K1520K_API void k1520k_abort(K1520Kermit h);
/// Nur die laufende Datei bzw. die ganze Gruppe abbrechen (Operation endet geordnet).
K1520K_API void k1520k_abort_file(K1520Kermit h);
K1520K_API void k1520k_abort_group(K1520Kermit h);

/* ─── Auskunft ────────────────────────────────────────────────────────────── */

/// Fehlertext der letzten Operation dieses Handles (leer nach Erfolg).
K1520K_API const char* k1520k_last_error(K1520Kermit h);
/// Ergebnistext der letzten Operation (Verzeichnis, Namen, Antworttext, Login-Verlauf).
K1520K_API const char* k1520k_result_string(K1520Kermit h);

/* ─── Hilfen fuer die Oberflaeche ─────────────────────────────────────────── */

/// Zerlegt `ls -l`-Text (System III/WEGA, V7, Linux) und liefert ein JSON-Array:
/// `[{"name","art","rechte","links","besitzer","gruppe","groesse","datum","ziel",
///    "jahr","monat","tag","stunde","minute"}, ...]` (fehlende Zahlen -1, fehlende Texte "").
/// "total N"-Zeilen fehlen.  Puffer je Faden, gueltig bis zum naechsten Aufruf.
K1520K_API const char* k1520k_parse_ls(const char* text);

/// Schliesst die Leitung und gibt das Handle frei.  NULL ist erlaubt.
K1520K_API void k1520k_close(K1520Kermit h);

#ifdef __cplusplus
}
#endif
