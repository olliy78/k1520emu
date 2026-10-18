/**
 * @file k1520kermit.cpp
 * @brief `k1520kermit` — Kermit-Client und -Server fuer die Kommandozeile.
 *
 * Kommandozeilenseite von `k1520kermit_core` (Entwurf 36 §4.5, AP K5); dieselbe
 * Protokollschicht, die `libk1520kermit` fuer die Oberflaeche verpackt.  Wie das
 * DiskTool bindet das Werkzeug die statische Bibliothek direkt (keine DLL noetig).
 *
 * ```
 * k1520kermit ls     <leitung> [pfad] [-l]            Verzeichnis des Servers (G D)
 * k1520kermit get    <leitung> <entfernt…> [--to ordner]
 * k1520kermit put    <leitung> <lokal…> [--as name]
 * k1520kermit cd     <leitung> <pfad>
 * k1520kermit rm     <leitung> <name…>
 * k1520kermit finish|logout <leitung>
 * k1520kermit serve  <leitung> <ordner> [--nur-lesen]
 * Leitung: --telnet host:port | --rfc2217 host:port | --listen-telnet port | --listen-rfc2217 port
 *          [--login NAME[:KENNWORT]] [--baud 9600]
 * ```
 *
 * Ausgabe: Nutzlast (Namen, Verzeichniszeilen, JSON) auf stdout, alles andere
 * (Hinweise, Fortschritt, Port) auf stderr.
 * Rueckgabewerte: 0 ok · 1 Fehler der Uebertragung oder der Gegenstelle (auch Abbruch
 * mit Strg+C) · 2 Aufruf-, Verbindungs- oder Anmeldefehler.
 *
 * @see tools/k1520kermit.md, doc/design/36_kermit.md §4.5/§6.3
 * @author Olaf Krieger
 * @date 2026
 * @license MIT License
 */

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "core/util/os_compat.h"
#include "kermit/src/dateien.h"
#include "kermit/src/leitung_netz.h"
#include "kermit/src/loginskript.h"
#include "kermit/src/protokoll.h"

namespace fs = std::filesystem;
using namespace k1520::kermit;

namespace {

constexpr int kOk = 0, kFehler = 1, kAufruf = 2;

// ─── Strg+C ──────────────────────────────────────────────────────────────────
// Der Signalbehandler setzt nur ein Flag; ein Beobachterfaden uebersetzt es in
// Sitzung::abbrechen() und Leitung::wecken() (beides ist im Handler nicht erlaubt).
std::atomic<bool> g_strgC{false};
extern "C" void bei_signal(int) { g_strgC = true; }

struct Optionen {
    std::string kommando;
    std::vector<std::string> rest;
    // Leitung
    std::string telnet, rfc2217;                 ///< host:port
    int listen_telnet = -1, listen_rfc2217 = -1; ///< -1 = nicht verlangt, 0 = freier Port
    std::string login;
    bool login_gesetzt = false;
    int baud = 9600;
    // Uebertragung
    int pruefsumme = 3;
    int paket = 1000;
    bool text = false;
    std::string namen;                           ///< "" = Vorgabe, sonst literal|konvertiert
    int max_namen = 0;
    int frist_s = 10;                            ///< je Paket und Verbindungsaufbau
    int warte_s = 300;                           ///< Annehmen einer Verbindung (Lauschen)
    // Kommandos
    std::string ziel = ".";                      ///< --to
    std::string als;                             ///< --as
    std::string cd;                              ///< --cd
    bool nur_lesen = false, json = false, lang = false, fortschritt = false, laut = false;
};

void gebrauch(std::ostream& os) {
    os <<
        "k1520kermit — Kermit-Client und -Server (Telnet / RFC 2217)\n\n"
        "  ls     <leitung> [pfad] [-l]            Verzeichnis des Servers (nur Namen; -l = Zeilen des Servers)\n"
        "  get    <leitung> <entfernt…> [--to ORDNER]   Dateien holen (Vorgabe: aktueller Ordner)\n"
        "  put    <leitung> <lokal…> [--as NAME]   Dateien senden (--as nur bei genau einer Datei)\n"
        "  cd     <leitung> <pfad>                 Verzeichnis des Servers wechseln\n"
        "  rm     <leitung> <name…>                Dateien auf dem Server loeschen\n"
        "  finish <leitung>                        Server beenden, Programm bleibt\n"
        "  logout <leitung>                        Server beenden und abmelden\n"
        "  serve  <leitung> <ordner> [--nur-lesen] Ordner freigeben; blockiert bis finish/logout/Trennung/Strg+C\n\n"
        "Leitung (genau eine):\n"
        "  --telnet HOST:PORT      --rfc2217 HOST:PORT        verbinden\n"
        "  --listen-telnet PORT    --listen-rfc2217 PORT      lauschen, EINE Verbindung annehmen\n"
        "                          (Port 0 = freier Port; er steht auf stderr als `Port: N`)\n"
        "  --login NAME[:KENNWORT] Anmeldung an einer getty vor dem Protokollstart\n"
        "  --baud N                RFC 2217: Wunsch (Vorgabe 9600; die Gegenstelle ist massgeblich)\n"
        "  --warte S               Lauschen: so lange auf die Verbindung warten (Vorgabe 300)\n\n"
        "Uebertragung:\n"
        "  --pruefsumme 1|2|3  --paket N  --binaer|--text  --namen literal|konvertiert\n"
        "  --max-namen N  --frist S (je Paket, Vorgabe 10)  --cd PFAD (vor dem Kommando wechseln)\n\n"
        "Ausgabe: --json  --fortschritt (sonst nur an einem Terminal)  -v (Ereignisse)  -l\n"
        "Rueckgabe: 0 ok · 1 Fehler der Uebertragung/Gegenstelle/Abbruch · 2 Aufruf-, Verbindungs-, Anmeldefehler\n";
}

// ─── JSON ────────────────────────────────────────────────────────────────────
// Gueltiges UTF-8 bleibt, alles andere wird als \u00XX (Latin-1) geschrieben.
std::size_t utf8Laenge(const std::string& s, std::size_t i) {
    const unsigned char c = static_cast<unsigned char>(s[i]);
    const std::size_t n = c >= 0xF0 && c <= 0xF4 ? 4 : c >= 0xE0 ? 3 : c >= 0xC2 && c < 0xE0 ? 2 : 0;
    if (c >= 0xF5 || n == 0 || i + n > s.size()) return 0;
    for (std::size_t k = 1; k < n; ++k)
        if ((static_cast<unsigned char>(s[i + k]) & 0xC0) != 0x80) return 0;
    return n;
}

std::string jt(const std::string& s) {
    std::string out = "\"";
    char b[8];
    for (std::size_t i = 0; i < s.size();) {
        const unsigned char c = static_cast<unsigned char>(s[i]);
        if (c == '"' || c == '\\') { out += '\\'; out += static_cast<char>(c); ++i; }
        else if (c < 0x20 || c == 0x7F) { std::snprintf(b, sizeof b, "\\u%04x", c); out += b; ++i; }
        else if (c < 0x80) { out += static_cast<char>(c); ++i; }
        else if (const std::size_t n = utf8Laenge(s, i)) { out.append(s, i, n); i += n; }
        else { std::snprintf(b, sizeof b, "\\u%04x", c); out += b; ++i; }
    }
    return out + '"';
}

std::string jsonListe(const std::vector<std::string>& v) {
    std::string o = "[";
    for (std::size_t i = 0; i < v.size(); ++i) o += (i ? "," : "") + jt(v[i]);
    return o + "]";
}

std::string jsonEintraege(const std::vector<VerzeichnisEintrag>& es) {
    std::string o = "[";
    bool erster = true;
    for (const auto& e : es) {
        if (!e.gueltig) continue;
        if (!erster) o += ',';
        erster = false;
        o += "{\"name\":" + jt(e.name) + ",\"art\":" + jt(std::string(1, e.art)) + ",\"rechte\":" + jt(e.rechte) +
             ",\"links\":" + std::to_string(e.links) + ",\"besitzer\":" + jt(e.besitzer) +
             ",\"gruppe\":" + jt(e.gruppe) + ",\"groesse\":" + std::to_string(e.groesse) +
             ",\"datum\":" + jt(e.datum) + ",\"ziel\":" + jt(e.ziel) + ",\"jahr\":" + std::to_string(e.jahr) +
             ",\"monat\":" + std::to_string(e.monat) + ",\"tag\":" + std::to_string(e.tag) +
             ",\"stunde\":" + std::to_string(e.stunde) + ",\"minute\":" + std::to_string(e.minute) + "}";
    }
    return o + "]";
}

// ─── Aufrufzerlegung ─────────────────────────────────────────────────────────
bool zahl(const std::string& s, int& out, int min, int max) {
    if (s.empty()) return false;
    char* ende = nullptr;
    const long v = std::strtol(s.c_str(), &ende, 10);
    if (*ende != '\0' || v < min || v > max) return false;
    out = static_cast<int>(v);
    return true;
}

/// Liest die Argumente.  Rueckgabe: 0 weiter, sonst Exit-Code (Hilfe = 0 mit `fertig`).
int lies(int argc, char** argv, Optionen& o, bool& fertig) {
    auto fehl = [](const std::string& t) {
        std::cerr << "Fehler: " << t << "\n";
        return kAufruf;
    };
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        std::string wert;
        bool hatWert = false;
        if (a.rfind("--", 0) == 0) {
            const auto g = a.find('=');
            if (g != std::string::npos) {
                wert = a.substr(g + 1);
                a = a.substr(0, g);
                hatWert = true;
            }
        }
        // Wert holen (aus --opt=wert oder dem naechsten Argument)
        auto hole = [&](std::string& dst) {
            if (hatWert) { dst = wert; return true; }
            if (i + 1 >= argc) return false;
            dst = argv[++i];
            return true;
        };
        std::string v;
        if (a == "-h" || a == "--help") { gebrauch(std::cout); fertig = true; return kOk; }
        else if (a == "-l") o.lang = true;
        else if (a == "-v" || a == "--verbose") o.laut = true;
        else if (a == "--json") o.json = true;
        else if (a == "--fortschritt") o.fortschritt = true;
        else if (a == "--nur-lesen") o.nur_lesen = true;
        else if (a == "--binaer" || a == "--binary") o.text = false;
        else if (a == "--text") o.text = true;
        else if (a == "--telnet") { if (!hole(o.telnet)) return fehl("--telnet braucht HOST:PORT"); }
        else if (a == "--rfc2217") { if (!hole(o.rfc2217)) return fehl("--rfc2217 braucht HOST:PORT"); }
        else if (a == "--listen-telnet") {
            if (!hole(v) || !zahl(v, o.listen_telnet, 0, 65535)) return fehl("--listen-telnet braucht einen Port (0-65535)");
        } else if (a == "--listen-rfc2217") {
            if (!hole(v) || !zahl(v, o.listen_rfc2217, 0, 65535)) return fehl("--listen-rfc2217 braucht einen Port (0-65535)");
        } else if (a == "--login") {
            if (!hole(o.login)) return fehl("--login braucht NAME[:KENNWORT]");
            o.login_gesetzt = true;
        } else if (a == "--baud") {
            if (!hole(v) || !zahl(v, o.baud, 50, 4000000)) return fehl("--baud braucht eine Zahl");
        } else if (a == "--pruefsumme") {
            if (!hole(v) || !zahl(v, o.pruefsumme, 1, 3)) return fehl("--pruefsumme muss 1, 2 oder 3 sein");
        } else if (a == "--paket") {
            if (!hole(v) || !zahl(v, o.paket, 20, 9024)) return fehl("--paket muss zwischen 20 und 9024 liegen");
        } else if (a == "--namen") {
            if (!hole(o.namen) || (o.namen != "literal" && o.namen != "konvertiert"))
                return fehl("--namen muss literal oder konvertiert sein");
        } else if (a == "--max-namen") {
            if (!hole(v) || !zahl(v, o.max_namen, 0, 255)) return fehl("--max-namen braucht eine Zahl (0 = keine Grenze)");
        } else if (a == "--frist") {
            if (!hole(v) || !zahl(v, o.frist_s, 1, 3600)) return fehl("--frist braucht Sekunden (1-3600)");
        } else if (a == "--warte") {
            if (!hole(v) || !zahl(v, o.warte_s, 1, 86400)) return fehl("--warte braucht Sekunden");
        } else if (a == "--to") { if (!hole(o.ziel)) return fehl("--to braucht einen Ordner"); }
        else if (a == "--as") { if (!hole(o.als)) return fehl("--as braucht einen Namen"); }
        else if (a == "--cd") { if (!hole(o.cd)) return fehl("--cd braucht einen Pfad"); }
        else if (a.size() > 1 && a[0] == '-' && a != "-") return fehl("unbekannter Schalter " + a);
        else if (o.kommando.empty()) o.kommando = a;
        else o.rest.push_back(a);
    }
    return kOk;
}

bool hostPort(const std::string& s, std::string& host, int& port) {
    const auto k = s.rfind(':');
    if (k == std::string::npos || k == 0) return false;
    host = s.substr(0, k);
    if (host.size() >= 2 && host.front() == '[' && host.back() == ']') host = host.substr(1, host.size() - 2);
    return zahl(s.substr(k + 1), port, 1, 65535);
}

// ─── Fortschritt ─────────────────────────────────────────────────────────────
struct Anzeige {
    bool an = false, tty = false;
    bool offen = false;   ///< eine Fortschrittszeile steht ohne Zeilenende da
    std::chrono::steady_clock::time_point zuletzt{};
    void zeige(const std::string& datei, int64_t bytes, int64_t gesamt) {
        if (!an) return;
        const auto jetzt = std::chrono::steady_clock::now();
        if (jetzt - zuletzt < std::chrono::milliseconds(200) && !(gesamt >= 0 && bytes >= gesamt)) return;
        zuletzt = jetzt;
        std::string z = datei + ": " + std::to_string(bytes);
        if (gesamt >= 0) z += " / " + std::to_string(gesamt);
        z += " Bytes";
        if (tty) {
            std::cerr << "\r" << z << "   " << std::flush;
            offen = true;
        } else {
            std::cerr << z << "\n";
        }
    }
    void schliesse() {
        if (offen) std::cerr << "\n";
        offen = false;
    }
};

// ─── Abschluss eines Client-Kommandos ────────────────────────────────────────
int meldeFehler(const Optionen& o, const std::string& text, int code) {
    std::cerr << "Fehler: " << text << "\n";
    if (o.json) std::cout << "{\"ok\":false,\"fehler\":" << jt(text) << "}\n";
    return code;
}

int ergebnis(const Optionen& o, const Ergebnis& r, const std::vector<std::string>& namen, const char* meldung = nullptr) {
    if (!r.ok()) return meldeFehler(o, std::string(fehlerText(r.art)) + (r.text.empty() ? "" : ": " + r.text), kFehler);
    if (o.json) {
        std::cout << "{\"ok\":true,\"dateien\":" << jsonListe(namen) << ",\"meldung\":" << jt(r.ausgabe) << "}\n";
    } else {
        for (const auto& n : namen) std::cout << n << "\n";
    }
    if (meldung && !r.ausgabe.empty()) std::cerr << r.ausgabe << (r.ausgabe.back() == '\n' ? "" : "\n");
    return kOk;
}

}  // namespace

int main(int argc, char** argv) {
    Optionen o;
    bool fertig = false;
    if (const int rc = lies(argc, argv, o, fertig); rc != kOk || fertig) return rc;
    if (o.kommando.empty()) {
        gebrauch(std::cerr);
        return kAufruf;
    }

    // ── Kommando und Argumente pruefen, BEVOR eine Leitung aufgebaut wird ──
    const std::string& k = o.kommando;
    const auto& r = o.rest;
    auto brauche = [&](std::size_t min, std::size_t max, const char* form) {
        if (r.size() < min || r.size() > max) {
            std::cerr << "Fehler: Aufruf: k1520kermit " << form << "\n";
            return false;
        }
        return true;
    };
    if (k == "ls") { if (!brauche(0, 1, "ls <leitung> [pfad]")) return kAufruf; }
    else if (k == "get") { if (!brauche(1, 1000, "get <leitung> <entfernt…> [--to ordner]")) return kAufruf; }
    else if (k == "put") { if (!brauche(1, 1000, "put <leitung> <lokal…> [--as name]")) return kAufruf; }
    else if (k == "cd") { if (!brauche(1, 1, "cd <leitung> <pfad>")) return kAufruf; }
    else if (k == "rm") { if (!brauche(1, 1000, "rm <leitung> <name…>")) return kAufruf; }
    else if (k == "finish" || k == "logout") { if (!brauche(0, 0, "finish|logout <leitung>")) return kAufruf; }
    else if (k == "serve") { if (!brauche(1, 1, "serve <leitung> <ordner>")) return kAufruf; }
    else {
        std::cerr << "Fehler: unbekanntes Kommando '" << k << "'\n";
        gebrauch(std::cerr);
        return kAufruf;
    }
    if (!o.als.empty() && (k != "put" || r.size() != 1)) {
        std::cerr << "Fehler: --as gilt nur fuer `put` mit genau einer Datei\n";
        return kAufruf;
    }
    if (o.nur_lesen && k != "serve") std::cerr << "Hinweis: --nur-lesen wirkt nur bei `serve`\n";

    fs::path serveOrdner;
    std::vector<std::string> putDateien;
    if (k == "serve") {
        std::error_code ec;
        serveOrdner = fs::u8path(r[0]);
        if (!fs::is_directory(serveOrdner, ec)) {
            std::cerr << "Fehler: kein Ordner: " << r[0] << "\n";
            return kAufruf;
        }
    } else if (k == "put") {
        for (const auto& p : r) {
            std::error_code ec;
            if (!fs::is_regular_file(fs::u8path(p), ec)) {
                std::cerr << "Fehler: keine Datei: " << p << "\n";
                return kAufruf;
            }
            putDateien.push_back(p);
        }
    }

    // ── Leitung waehlen ──
    const int wahl = (!o.telnet.empty()) + (!o.rfc2217.empty()) + (o.listen_telnet >= 0) + (o.listen_rfc2217 >= 0);
    if (wahl != 1) {
        std::cerr << "Fehler: genau eine Leitung angeben (--telnet, --rfc2217, --listen-telnet oder --listen-rfc2217)\n";
        return kAufruf;
    }
    std::string host;
    int port = 0;
    const bool lauschen = o.listen_telnet >= 0 || o.listen_rfc2217 >= 0;
    if (!lauschen && !hostPort(o.telnet.empty() ? o.rfc2217 : o.telnet, host, port)) {
        std::cerr << "Fehler: HOST:PORT erwartet\n";
        return kAufruf;
    }

    NetzOptionen no;
    no.baud = o.baud;
    std::string fehler;
    std::unique_ptr<NetzLeitung> leitung;
    if (!o.telnet.empty())
        leitung = TelnetLeitung::verbinden(host, port, o.frist_s * 1000, no, &fehler);
    else if (!o.rfc2217.empty())
        leitung = Rfc2217Leitung::verbinden(host, port, o.frist_s * 1000, no, &fehler);
    else if (o.listen_telnet >= 0)
        leitung = TelnetLeitung::lauschen(o.listen_telnet, no, &fehler);
    else
        leitung = Rfc2217Leitung::lauschen(o.listen_rfc2217, no, &fehler);
    if (!leitung) {
        return meldeFehler(o, "Leitung: " + (fehler.empty() ? std::string("nicht aufgebaut") : fehler), kAufruf);
    }

    // ── Sitzung ──
    Einstellungen e;
    e.pruefsumme = o.pruefsumme;
    if (o.paket <= 94) {
        e.langpakete = false;
        e.maxl = o.paket;
    } else {
        e.langpakete = true;
        e.max_paket = o.paket;
        e.maxl = 94;
    }
    e.textmodus = o.text;
    e.frist_ms = o.frist_s * 1000;
    e.max_namenslaenge = o.max_namen;
    if (!o.namen.empty()) {
        e.namen_senden = e.namen_empfang = o.namen == "literal" ? Namen::Literal : Namen::Konvertiert;
    }
    Anzeige anzeige;
    anzeige.tty = k1520::os::isTerminal(2);
    anzeige.an = o.fortschritt || anzeige.tty;
    Rueckrufe rr;
    rr.fortschritt = [&](const std::string& d, int64_t b, int64_t g) { anzeige.zeige(d, b, g); };
    // Ein Server meldet jede Uebertragung; ein Client nur mit -v.
    const bool ereignisse = o.laut || k == "serve";
    rr.ereignis = [&](const std::string& t) {
        if (!ereignisse) return;
        anzeige.schliesse();
        std::cerr << t << "\n";
    };
    Sitzung sitzung(*leitung, e, rr);

    // ── Strg+C ──
    std::signal(SIGINT, bei_signal);
    std::signal(SIGTERM, bei_signal);
    std::atomic<bool> loginAbbruch{false};
    std::atomic<int> phase{0};        // 0 Aufbau/Anmeldung, 1 Protokoll
    std::atomic<bool> beendet{false};
    std::thread waechter([&] {
        int runde = 0;
        while (!beendet) {
            if (g_strgC) {
                loginAbbruch = true;
                if (phase == 1 && runde % 20 == 0) sitzung.abbrechen();   // E-Paket, alle 0,5 s bis zum Ende
                leitung->wecken();
                ++runde;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(25));
        }
    });
    struct Aufraeumen {
        std::atomic<bool>& b;
        std::thread& t;
        ~Aufraeumen() {
            b = true;
            if (t.joinable()) t.join();
        }
    } aufraeumen{beendet, waechter};

    auto abbruchMeldung = [&]() {
        anzeige.schliesse();
        return meldeFehler(o, "Abbruch durch den Anwender", kFehler);
    };

    // ── Lauschen: Port melden, Verbindung annehmen ──
    if (lauschen) {
        std::cerr << "Port: " << leitung->port() << std::endl;
        std::string f;
        if (!leitung->annehmen(o.warte_s * 1000, &f)) {
            if (g_strgC) return abbruchMeldung();
            return meldeFehler(o, "Annehmen: " + (f.empty() ? std::string("Frist abgelaufen") : f), kAufruf);
        }
        if (o.laut) std::cerr << "Verbunden: " << leitung->gegenstelle() << "\n";
    }
    if (!leitung->hinweis().empty()) std::cerr << "Hinweis: " << leitung->hinweis() << "\n";

    // ── Anmeldung ──
    if (o.login_gesetzt) {
        LoginOptionen lo;
        const auto dp = o.login.find(':');
        lo.name = o.login.substr(0, dp);
        lo.mit_kennwort = dp != std::string::npos;
        if (lo.mit_kennwort) lo.kennwort = o.login.substr(dp + 1);
        if (lo.name.empty()) return meldeFehler(o, "--login: Name fehlt", kAufruf);
        lo.abbruch = &loginAbbruch;
        const LoginErgebnis lr = loginSkript(*leitung, lo);
        if (!lr.ok) {
            if (g_strgC) return abbruchMeldung();
            return meldeFehler(o, "Anmeldung: " + lr.text, kAufruf);
        }
        if (o.laut) std::cerr << "Angemeldet als " << lo.name << "\n";
    }

    // ── Kommando ──
    phase = 1;
    int rc = kOk;

    if (k == "serve") {
        Freigabe fr(serveOrdner, o.nur_lesen);
        if (!fr.gueltig()) return meldeFehler(o, "Ordner nicht freigebbar: " + r[0], kAufruf);
        fr.setzeMaxNamenslaenge(o.max_namen);
        std::cerr << "Freigabe: " << serveOrdner.u8string() << (o.nur_lesen ? " (nur lesen)" : "") << "\n";
        const Ergebnis er = sitzung.serve(fr);
        anzeige.schliesse();
        if (er.ok()) {
            std::cerr << "Server beendet\n";
            rc = kOk;
        } else if (er.art == Fehlerart::LeitungGetrennt && !g_strgC) {
            std::cerr << "Gegenstelle hat die Leitung getrennt\n";
            rc = kOk;
        } else if (g_strgC || er.art == Fehlerart::Abgebrochen) {
            rc = abbruchMeldung();
        } else {
            rc = meldeFehler(o, std::string(fehlerText(er.art)) + (er.text.empty() ? "" : ": " + er.text), kFehler);
        }
        if (o.json && rc == kOk) std::cout << "{\"ok\":true}\n";
        return rc;
    }

    if (!o.cd.empty()) {
        const Ergebnis c = sitzung.entferntWechseln(o.cd);
        if (!c.ok()) return meldeFehler(o, "cd " + o.cd + ": " + fehlerText(c.art) + (c.text.empty() ? "" : ": " + c.text), kFehler);
    }

    if (k == "ls") {
        const Ergebnis er = sitzung.entferntVerzeichnis(r.empty() ? std::string() : r[0]);
        if (!er.ok()) rc = g_strgC ? abbruchMeldung() : ergebnis(o, er, {});
        else {
            const auto eintraege = zerlegeLsListe(er.ausgabe);
            if (o.json) {
                std::cout << jsonEintraege(eintraege) << "\n";
            } else if (o.lang) {
                std::cout << er.ausgabe << (er.ausgabe.empty() || er.ausgabe.back() == '\n' ? "" : "\n");
            } else {
                for (const auto& x : eintraege)
                    if (x.gueltig) std::cout << x.name << "\n";
                bool leer = true;
                for (const auto& x : eintraege) leer = leer && !x.gueltig;
                if (leer && er.ausgabe.find_first_not_of(" \r\n\t") != std::string::npos)
                    std::cerr << "Hinweis: Antwort des Servers nicht als Verzeichnis lesbar (-l zeigt sie)\n";
            }
        }
    } else if (k == "get") {
        std::error_code ec;
        const fs::path ziel = fs::u8path(o.ziel);
        fs::create_directories(ziel, ec);
        std::vector<std::string> alle;
        for (const auto& n : r) {
            const Ergebnis er = sitzung.holen(n, ziel.u8string());
            anzeige.schliesse();
            for (const auto& d : er.dateien) alle.push_back((ziel / fs::u8path(d)).u8string());
            if (!er.ok()) {
                rc = g_strgC ? abbruchMeldung() : meldeFehler(o, n + ": " + fehlerText(er.art) + (er.text.empty() ? "" : ": " + er.text), kFehler);
                break;
            }
        }
        if (rc == kOk) {
            if (o.json) std::cout << "{\"ok\":true,\"dateien\":" << jsonListe(alle) << ",\"meldung\":\"\"}\n";
            else for (const auto& n : alle) std::cout << n << "\n";
        }
    } else if (k == "put") {
        std::vector<SendeAuftrag> auftraege;
        for (const auto& p : putDateien) auftraege.push_back({p, o.als});
        const Ergebnis er = auftraege.size() == 1 ? sitzung.senden(auftraege[0].lokal, auftraege[0].entfernt)
                                                  : sitzung.sendenGruppe(auftraege);
        anzeige.schliesse();
        rc = g_strgC ? abbruchMeldung() : ergebnis(o, er, er.dateien);
    } else if (k == "cd") {
        const Ergebnis er = sitzung.entferntWechseln(r[0]);
        rc = g_strgC ? abbruchMeldung() : ergebnis(o, er, {}, "");
    } else if (k == "rm") {
        std::vector<std::string> meldungen;
        for (const auto& n : r) {
            const Ergebnis er = sitzung.entferntLoeschen(n);
            if (!er.ok()) {
                rc = g_strgC ? abbruchMeldung() : meldeFehler(o, n + ": " + fehlerText(er.art) + (er.text.empty() ? "" : ": " + er.text), kFehler);
                break;
            }
            if (!er.ausgabe.empty()) std::cerr << er.ausgabe << (er.ausgabe.back() == '\n' ? "" : "\n");
            meldungen.push_back(n);
        }
        if (rc == kOk) {
            if (o.json) std::cout << "{\"ok\":true,\"dateien\":" << jsonListe(meldungen) << ",\"meldung\":\"\"}\n";
            else for (const auto& n : meldungen) std::cout << n << "\n";
        }
    } else {   // finish | logout
        const Ergebnis er = k == "finish" ? sitzung.entferntBeenden() : sitzung.entferntAbmelden();
        rc = g_strgC ? abbruchMeldung() : ergebnis(o, er, {}, "");
    }
    return rc;
}
