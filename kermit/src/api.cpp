/**
 * @file api.cpp
 * @brief C-ABI von libk1520kermit (kermit/include/k1520kermit.h) — duenne Huelle ueber
 *        `Sitzung`, `NetzLeitung`, `loginSkript` und `zerlegeLsListe`.
 *
 * Doc: doc/design/36_kermit.md §4.4 (AP K4).  Keine Ausnahme verlaesst die Grenze: jede
 * Funktion faengt ab und meldet K1520K_E_USAGE mit dem Text der Ausnahme.
 */
#include "kermit/include/k1520kermit.h"

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <exception>
#include <memory>
#include <string>

#include "core/version.h"
#include "kermit/src/dateien.h"
#include "kermit/src/leitung_netz.h"
#include "kermit/src/loginskript.h"
#include "kermit/src/protokoll.h"

using namespace k1520::kermit;

namespace {

struct Handle {
    std::unique_ptr<NetzLeitung> leitung;
    std::unique_ptr<Sitzung> sitzung;
    std::string fehler, ergebnis, peer, hinweis;
    std::atomic<bool> abbruch{false};
    // Rueckrufe: gesetzt zwischen den Operationen, gelesen im Faden der Operation.
    K1520KProgress cbFortschritt = nullptr;
    K1520KEvent cbEreignis = nullptr;
    void* user = nullptr;
    // Anmeldung (K10): Art des letzten Ausgangs und die Antwort, die der Abfrage-Rueckruf gibt.
    int loginArt = 0;
    struct LoginAntwort {
        bool gesetzt = false;
        std::string name, kennwort;
    };
    LoginAntwort* loginAntwort = nullptr;   ///< nur WAEHREND des Abfrage-Rueckrufs (im Faden von login_ask)
};

thread_local std::string tl_openFehler;
thread_local std::string tl_json;

Handle* H(K1520Kermit h) { return static_cast<Handle*>(h); }

K1520Kermit neu(std::unique_ptr<NetzLeitung> l) {
    auto h = std::make_unique<Handle>();
    h->leitung = std::move(l);
    Rueckrufe r;
    Handle* p = h.get();
    r.fortschritt = [p](const std::string& datei, int64_t b, int64_t g) {
        if (p->cbFortschritt) p->cbFortschritt(datei.c_str(), b, g, p->user);
    };
    r.ereignis = [p](const std::string& t) {
        if (p->cbEreignis) p->cbEreignis(t.c_str(), p->user);
    };
    h->sitzung = std::make_unique<Sitzung>(*h->leitung, Einstellungen{}, std::move(r));
    return h.release();
}

char parity(int p) {
    switch (p) {
        case 'O': case 'o': return 'O';
        case 'E': case 'e': return 'E';
        default: return 'N';
    }
}

NetzOptionen optionen(int baud, int daten, int par, int stopp) {
    NetzOptionen o;
    if (baud > 0) o.baud = baud;
    if (daten >= 5 && daten <= 8) o.datenbits = daten;
    o.paritaet = parity(par);
    if (stopp == 2) o.stoppbits = 2;
    return o;
}

/// Ergebnis der Sitzung in Handle-Zustand und Rueckgabecode uebersetzen.
int uebernimm(Handle* h, const Ergebnis& e, bool dateienAlsText = false) {
    h->fehler = e.ok() ? std::string() : e.text;
    if (e.ok() && h->fehler.empty()) h->fehler.clear();
    if (dateienAlsText) {
        std::string s;
        for (const auto& d : e.dateien) s += d + "\n";
        h->ergebnis = s;
    } else {
        h->ergebnis = e.ausgabe;
    }
    return static_cast<int>(e.art);   // Fehlerart und K1520K_E_* sind deckungsgleich (siehe Header)
}

int usage(Handle* h, const std::string& text) {
    if (h) h->fehler = text;
    return K1520K_E_USAGE;
}

#define K_BEGIN(h)                                                   \
    Handle* H_ = H(h);                                               \
    if (!H_) return K1520K_E_USAGE;                                  \
    try {
#define K_END                                                        \
    } catch (const std::exception& ex_) {                            \
        return usage(H_, std::string("Ausnahme: ") + ex_.what());    \
    } catch (...) {                                                  \
        return usage(H_, "unbekannte Ausnahme");                     \
    }

bool lokalGueltig(const char* s) { return s != nullptr && *s != '\0'; }

// UTF-8-Pruefung einer Sequenz ab s[i]; Laenge oder 0.
std::size_t utf8Laenge(const std::string& s, std::size_t i) {
    const unsigned char c = static_cast<unsigned char>(s[i]);
    std::size_t n = c >= 0xF0 && c <= 0xF4 ? 4 : c >= 0xE0 ? 3 : c >= 0xC2 && c < 0xE0 ? 2 : 0;
    if (c >= 0xF5 || n == 0 || i + n > s.size()) return 0;
    for (std::size_t k = 1; k < n; ++k)
        if ((static_cast<unsigned char>(s[i + k]) & 0xC0) != 0x80) return 0;
    return n;
}

void jsonText(std::string& out, const std::string& s) {
    out += '"';
    for (std::size_t i = 0; i < s.size();) {
        const unsigned char c = static_cast<unsigned char>(s[i]);
        if (c == '"' || c == '\\') {
            out += '\\';
            out += static_cast<char>(c);
            ++i;
        } else if (c < 0x20 || c == 0x7F) {
            char b[8];
            std::snprintf(b, sizeof b, "\\u%04x", c);
            out += b;
            ++i;
        } else if (c < 0x80) {
            out += static_cast<char>(c);
            ++i;
        } else if (const std::size_t n = utf8Laenge(s, i)) {
            out.append(s, i, n);   // gueltiges UTF-8 bleibt
            i += n;
        } else {
            char b[8];
            std::snprintf(b, sizeof b, "\\u%04x", c);   // Rest als Latin-1
            out += b;
            ++i;
        }
    }
    out += '"';
}

}  // namespace

// ─── Version / Leitung ──────────────────────────────────────────────────────

extern "C" K1520K_API const char* k1520k_version(void) { return K1520_VERSION_TEXT; }

extern "C" K1520K_API K1520Kermit k1520k_open_telnet_client(const char* host, int port, int frist_ms) {
    try {
        if (!lokalGueltig(host) || port <= 0 || port > 65535) {
            tl_openFehler = "Host oder Port fehlt";
            return nullptr;
        }
        auto l = TelnetLeitung::verbinden(host, port, frist_ms > 0 ? frist_ms : 5000, {}, &tl_openFehler);
        if (!l) return nullptr;
        return neu(std::move(l));
    } catch (const std::exception& e) {
        tl_openFehler = e.what();
        return nullptr;
    }
}

extern "C" K1520K_API K1520Kermit k1520k_open_telnet_server(int port) {
    try {
        if (port < 0 || port > 65535) {
            tl_openFehler = "Port ausserhalb 0..65535";
            return nullptr;
        }
        auto l = TelnetLeitung::lauschen(port, {}, &tl_openFehler);
        if (!l) return nullptr;
        return neu(std::move(l));
    } catch (const std::exception& e) {
        tl_openFehler = e.what();
        return nullptr;
    }
}

extern "C" K1520K_API K1520Kermit k1520k_open_rfc2217_client(const char* host, int port, int frist_ms, int baud,
                                                             int datenbits, int paritaet, int stoppbits) {
    try {
        if (!lokalGueltig(host) || port <= 0 || port > 65535) {
            tl_openFehler = "Host oder Port fehlt";
            return nullptr;
        }
        auto l = Rfc2217Leitung::verbinden(host, port, frist_ms > 0 ? frist_ms : 5000,
                                           optionen(baud, datenbits, paritaet, stoppbits), &tl_openFehler);
        if (!l) return nullptr;
        return neu(std::move(l));
    } catch (const std::exception& e) {
        tl_openFehler = e.what();
        return nullptr;
    }
}

extern "C" K1520K_API K1520Kermit k1520k_open_rfc2217_server(int port, int baud, int datenbits, int paritaet,
                                                             int stoppbits) {
    try {
        if (port < 0 || port > 65535) {
            tl_openFehler = "Port ausserhalb 0..65535";
            return nullptr;
        }
        auto l = Rfc2217Leitung::lauschen(port, optionen(baud, datenbits, paritaet, stoppbits), &tl_openFehler);
        if (!l) return nullptr;
        return neu(std::move(l));
    } catch (const std::exception& e) {
        tl_openFehler = e.what();
        return nullptr;
    }
}

extern "C" K1520K_API const char* k1520k_last_open_error(void) { return tl_openFehler.c_str(); }

extern "C" K1520K_API int k1520k_listen_port(K1520Kermit h) {
    Handle* p = H(h);
    return p && p->leitung ? p->leitung->port() : 0;
}

extern "C" K1520K_API int k1520k_accept(K1520Kermit h, int frist_ms) {
    K_BEGIN(h)
    std::string f;
    if (!H_->leitung->annehmen(frist_ms > 0 ? frist_ms : 60000, &f)) {
        H_->fehler = f.empty() ? "Annehmen fehlgeschlagen" : f;
        return K1520K_E_LINE;
    }
    H_->fehler.clear();
    return K1520K_OK;
    K_END
}

extern "C" K1520K_API int k1520k_connected(K1520Kermit h) {
    Handle* p = H(h);
    return p && p->leitung && p->leitung->verbunden() ? 1 : 0;
}

extern "C" K1520K_API const char* k1520k_peer(K1520Kermit h) {
    Handle* p = H(h);
    if (!p || !p->leitung) return "";
    p->peer = p->leitung->gegenstelle();
    return p->peer.c_str();
}

extern "C" K1520K_API const char* k1520k_line_notice(K1520Kermit h) {
    Handle* p = H(h);
    if (!p || !p->leitung) return "";
    p->hinweis = p->leitung->hinweis();
    return p->hinweis.c_str();
}

// ─── Anmeldung ──────────────────────────────────────────────────────────────

namespace {

int loginIntern(Handle* H_, LoginOptionen o, int frist_ms) {
    if (frist_ms > 0) {
        o.frist_login_ms = frist_ms;
        o.frist_kennwort_ms = frist_ms;
        o.frist_server_ms = frist_ms * 2;
    }
    H_->abbruch = false;
    o.abbruch = &H_->abbruch;
    o.tempo = H_->sitzung->einstellungen().tempo;
    o.baud = H_->sitzung->einstellungen().baud;
    const LoginErgebnis r = loginSkript(*H_->leitung, o);
    if (r.ok && r.tempo > 0) H_->sitzung->setzeTempoSchaetzung(r.tempo);
    H_->ergebnis = r.gelesen;
    H_->fehler = r.ok ? std::string() : r.text;
    H_->loginArt = static_cast<int>(r.art);   // LoginArt und K1520K_LOGIN_* sind deckungsgleich (Header)
    return r.ok ? K1520K_OK : K1520K_E_LOGIN;
}

}  // namespace

extern "C" K1520K_API int k1520k_login(K1520Kermit h, const char* name, const char* kennwort, int frist_ms) {
    K_BEGIN(h)
    if (!lokalGueltig(name)) return usage(H_, "Anmeldename fehlt");
    LoginOptionen o;
    o.name = name;
    o.mit_kennwort = kennwort != nullptr;
    if (kennwort) o.kennwort = kennwort;
    return loginIntern(H_, std::move(o), frist_ms);
    K_END
}

extern "C" K1520K_API int k1520k_login_ask(K1520Kermit h, K1520KLoginAsk ask, void* user, int frist_ms) {
    K_BEGIN(h)
    if (!ask) return usage(H_, "Abfrage-Rueckruf fehlt");
    LoginOptionen o;
    o.mit_kennwort = true;
    o.abfrage = [H_, ask, user](const AnmeldeAnfrage& a, std::string& name, std::string& kennwort) {
        Handle::LoginAntwort antwort;
        H_->loginAntwort = &antwort;
        try {
            ask(a.versuch, a.gelesen.c_str(), user);
        } catch (...) {
            H_->loginAntwort = nullptr;
            throw;
        }
        H_->loginAntwort = nullptr;
        if (!antwort.gesetzt) return false;   // keine Antwort = der Anwender bricht ab
        name = antwort.name;
        kennwort = antwort.kennwort;
        return true;
    };
    return loginIntern(H_, std::move(o), frist_ms);
    K_END
}

extern "C" K1520K_API int k1520k_login_answer(K1520Kermit h, const char* name, const char* kennwort) {
    K_BEGIN(h)
    if (!H_->loginAntwort) return usage(H_, "k1520k_login_answer ausserhalb des Abfrage-Rueckrufs");
    if (!lokalGueltig(name)) return usage(H_, "Anmeldename fehlt");
    H_->loginAntwort->name = name;
    H_->loginAntwort->kennwort = kennwort ? kennwort : "";
    H_->loginAntwort->gesetzt = true;
    return K1520K_OK;
    K_END
}

extern "C" K1520K_API int k1520k_login_kind(K1520Kermit h) {
    Handle* H_ = H(h);
    return H_ ? H_->loginArt : -1;
}

// ─── Einstellungen ──────────────────────────────────────────────────────────

extern "C" K1520K_API int k1520k_set_pruefsumme(K1520Kermit h, int typ) {
    K_BEGIN(h)
    if (typ < 1 || typ > 3) return usage(H_, "Pruefsumme muss 1, 2 oder 3 sein");
    H_->sitzung->einstellungen().pruefsumme = typ;
    return K1520K_OK;
    K_END
}

extern "C" K1520K_API int k1520k_set_paketlaenge(K1520Kermit h, int laenge) {
    K_BEGIN(h)
    if (laenge < 20 || laenge > 9024) return usage(H_, "Paketlaenge muss zwischen 20 und 9024 liegen");
    Einstellungen& e = H_->sitzung->einstellungen();
    if (laenge <= 94) {
        e.langpakete = false;
        e.maxl = laenge;
    } else {
        e.langpakete = true;
        e.max_paket = laenge;
        e.maxl = 94;
    }
    return K1520K_OK;
    K_END
}

extern "C" K1520K_API int k1520k_set_namen(K1520Kermit h, int senden_konvertiert, int empfang_konvertiert) {
    K_BEGIN(h)
    Einstellungen& e = H_->sitzung->einstellungen();
    e.namen_senden = senden_konvertiert ? Namen::Konvertiert : Namen::Literal;
    e.namen_empfang = empfang_konvertiert ? Namen::Konvertiert : Namen::Literal;
    return K1520K_OK;
    K_END
}

extern "C" K1520K_API int k1520k_set_textmodus(K1520Kermit h, int an) {
    K_BEGIN(h)
    H_->sitzung->einstellungen().textmodus = an != 0;
    return K1520K_OK;
    K_END
}

extern "C" K1520K_API int k1520k_set_fristen(K1520Kermit h, int paket_ms, int max_wiederholungen) {
    K_BEGIN(h)
    if (paket_ms < 100 || max_wiederholungen < 1) return usage(H_, "Frist >= 100 ms und Wiederholungen >= 1 verlangt");
    Einstellungen& e = H_->sitzung->einstellungen();
    e.frist_ms = paket_ms;
    e.max_wiederholungen = max_wiederholungen;
    return K1520K_OK;
    K_END
}

extern "C" K1520K_API int k1520k_set_tempo(K1520Kermit h, double tempo) {
    K_BEGIN(h)
    if (!(tempo == 0.0 || (tempo >= 0.01 && tempo <= 1000.0)))
        return usage(H_, "Tempo muss 0 (Automatik) oder 0.01 bis 1000 sein");
    H_->sitzung->einstellungen().tempo = tempo;
    return K1520K_OK;
    K_END
}

extern "C" K1520K_API double k1520k_tempo(K1520Kermit h) {
    if (!h) return 0.0;
    try {
        return static_cast<Handle*>(h)->sitzung->tempo();
    } catch (...) {
        return 0.0;
    }
}

extern "C" K1520K_API int k1520k_set_max_namenslaenge(K1520Kermit h, int n) {
    K_BEGIN(h)
    if (n < 0) return usage(H_, "Namenslaenge darf nicht negativ sein");
    H_->sitzung->einstellungen().max_namenslaenge = n;
    return K1520K_OK;
    K_END
}

extern "C" K1520K_API int k1520k_set_ueberschreiben(K1520Kermit h, int an) {
    K_BEGIN(h)
    H_->sitzung->einstellungen().ueberschreiben = an != 0;
    return K1520K_OK;
    K_END
}

extern "C" K1520K_API int k1520k_set_server_login(K1520Kermit h, int pflicht, const char* name,
                                                  const char* kennwort) {
    K_BEGIN(h)
    auto& e = H_->sitzung->einstellungen();
    e.login_pflicht = pflicht != 0;
    if (name) e.login_name = name;
    if (kennwort) e.login_kennwort = kennwort;
    return K1520K_OK;
    K_END
}

extern "C" K1520K_API int k1520k_set_callbacks(K1520Kermit h, K1520KProgress fortschritt, K1520KEvent ereignis,
                                               void* user) {
    K_BEGIN(h)
    H_->cbFortschritt = fortschritt;
    H_->cbEreignis = ereignis;
    H_->user = user;
    return K1520K_OK;
    K_END
}

// ─── Client ─────────────────────────────────────────────────────────────────

extern "C" K1520K_API int k1520k_send(K1520Kermit h, const char* lokal, const char* entfernt) {
    K_BEGIN(h)
    if (!lokalGueltig(lokal)) return usage(H_, "lokaler Pfad fehlt");
    return uebernimm(H_, H_->sitzung->senden(lokal, entfernt ? entfernt : ""), true);
    K_END
}

extern "C" K1520K_API int k1520k_get(K1520Kermit h, const char* entfernt, const char* zielordner) {
    K_BEGIN(h)
    if (!lokalGueltig(entfernt) || !lokalGueltig(zielordner)) return usage(H_, "Name oder Zielordner fehlt");
    return uebernimm(H_, H_->sitzung->holen(entfernt, zielordner), true);
    K_END
}

extern "C" K1520K_API int k1520k_remote_dir(K1520Kermit h, const char* pfad, char* puffer, int groesse) {
    K_BEGIN(h)
    const int rc = uebernimm(H_, H_->sitzung->entferntVerzeichnis(pfad ? pfad : ""));
    if (puffer && groesse > 0) {
        const std::size_t n = std::min<std::size_t>(H_->ergebnis.size(), static_cast<std::size_t>(groesse) - 1);
        std::memcpy(puffer, H_->ergebnis.data(), n);
        puffer[n] = '\0';
    }
    return rc;
    K_END
}

extern "C" K1520K_API int k1520k_remote_cd(K1520Kermit h, const char* pfad) {
    K_BEGIN(h)
    return uebernimm(H_, H_->sitzung->entferntWechseln(pfad ? pfad : ""));
    K_END
}

extern "C" K1520K_API int k1520k_remote_delete(K1520Kermit h, const char* name) {
    K_BEGIN(h)
    if (!lokalGueltig(name)) return usage(H_, "Name fehlt");
    return uebernimm(H_, H_->sitzung->entferntLoeschen(name));
    K_END
}

extern "C" K1520K_API int k1520k_remote_finish(K1520Kermit h) {
    K_BEGIN(h)
    return uebernimm(H_, H_->sitzung->entferntBeenden());
    K_END
}

extern "C" K1520K_API int k1520k_remote_logout(K1520Kermit h) {
    K_BEGIN(h)
    return uebernimm(H_, H_->sitzung->entferntAbmelden());
    K_END
}

// ─── Server ─────────────────────────────────────────────────────────────────

extern "C" K1520K_API int k1520k_serve(K1520Kermit h, const char* wurzel, int nur_lesen) {
    K_BEGIN(h)
    if (!lokalGueltig(wurzel)) return usage(H_, "Freigabeordner fehlt");
    Freigabe f(std::filesystem::u8path(wurzel), nur_lesen != 0);
    if (!f.gueltig()) return usage(H_, std::string("Freigabeordner nicht benutzbar: ") + wurzel);
    f.setzeMaxNamenslaenge(H_->sitzung->einstellungen().max_namenslaenge);
    return uebernimm(H_, H_->sitzung->serve(f));
    K_END
}

// ─── Abbruch ────────────────────────────────────────────────────────────────

extern "C" K1520K_API void k1520k_abort(K1520Kermit h) {
    Handle* p = H(h);
    if (!p) return;
    p->abbruch = true;
    p->sitzung->abbrechen();   // setzt den Sitzungsschalter und weckt die Leitung
    p->leitung->wecken();
}

extern "C" K1520K_API void k1520k_abort_file(K1520Kermit h) {
    if (Handle* p = H(h)) p->sitzung->abbrechenDatei();
}

extern "C" K1520K_API void k1520k_abort_group(K1520Kermit h) {
    if (Handle* p = H(h)) p->sitzung->abbrechenGruppe();
}

// ─── Auskunft ───────────────────────────────────────────────────────────────

extern "C" K1520K_API const char* k1520k_last_error(K1520Kermit h) {
    Handle* p = H(h);
    return p ? p->fehler.c_str() : "";
}

extern "C" K1520K_API const char* k1520k_result_string(K1520Kermit h) {
    Handle* p = H(h);
    return p ? p->ergebnis.c_str() : "";
}

extern "C" K1520K_API const char* k1520k_parse_ls(const char* text) {
    try {
        tl_json = "[";
        bool erster = true;
        for (const VerzeichnisEintrag& e : zerlegeLsListe(text ? text : "")) {
            if (!e.gueltig) continue;
            if (!erster) tl_json += ',';
            erster = false;
            tl_json += "{\"name\":";
            jsonText(tl_json, e.name);
            tl_json += ",\"art\":";
            jsonText(tl_json, std::string(1, e.art));
            tl_json += ",\"rechte\":";
            jsonText(tl_json, e.rechte);
            tl_json += ",\"links\":" + std::to_string(e.links);
            tl_json += ",\"besitzer\":";
            jsonText(tl_json, e.besitzer);
            tl_json += ",\"gruppe\":";
            jsonText(tl_json, e.gruppe);
            tl_json += ",\"groesse\":" + std::to_string(e.groesse);
            tl_json += ",\"datum\":";
            jsonText(tl_json, e.datum);
            tl_json += ",\"ziel\":";
            jsonText(tl_json, e.ziel);
            tl_json += ",\"jahr\":" + std::to_string(e.jahr) + ",\"monat\":" + std::to_string(e.monat) +
                       ",\"tag\":" + std::to_string(e.tag) + ",\"stunde\":" + std::to_string(e.stunde) +
                       ",\"minute\":" + std::to_string(e.minute) + "}";
        }
        tl_json += "]";
    } catch (...) {
        tl_json = "[]";
    }
    return tl_json.c_str();
}

extern "C" K1520K_API void k1520k_close(K1520Kermit h) {
    Handle* p = H(h);
    if (!p) return;
    p->leitung->schliessen();
    delete p;
}
