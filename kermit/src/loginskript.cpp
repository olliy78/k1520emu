/**
 * @file loginskript.cpp
 * @brief Anmeldung vor dem Kermit-Protokollstart — siehe loginskript.h.
 */
#include "kermit/src/loginskript.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <vector>

namespace k1520::kermit {

namespace {

std::string klein(std::string s) {
    for (auto& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

/// Gelesenes, Bit 7 geloescht, fuer Vergleiche (Kleinschrift).
struct Lauf {
    Leitung& l;
    const LoginOptionen& o;
    std::string alles;     ///< alles Gelesene (7 Bit), fuer die Fehlermeldung
    std::string fenster;   ///< noch nicht ausgewertet (Kleinschrift)
    bool getrennt = false;
    bool abgebrochen = false;
    // Ratenmessung: Bytes, die in einem zusammenhaengenden Strom NACH dem ersten Puffer kamen.
    int64_t letzter = -1;
    double messBytes = 0, messMs = 0;

    /// Zeit in ms, geteilt durch ein ausdrueckliches Tempo.
    int64_t z(int64_t ms) const {
        return o.tempo > 0 ? std::max<int64_t>(1, static_cast<int64_t>(static_cast<double>(ms) / o.tempo)) : ms;
    }

    bool stop() {
        if (o.abbruch && o.abbruch->load()) abgebrochen = true;
        return abgebrochen;
    }

    /// Liest bis `ms` lang; true, wenn etwas kam.
    bool lies(int64_t ms) {
        uint8_t buf[512];
        const int n = l.lesen(buf, sizeof buf, ms);
        if (n < 0) {
            getrennt = true;
            return false;
        }
        if (n > 0) {
            const int64_t t = l.jetztMs();
            // Luecke unter 50 ms (5 Zeichenzeiten bei 9600 Bd und Tempo 1): derselbe Strom.
            // Rechenpausen des Gastes darin senken die Rate — die Schaetzung bleibt vorsichtig.
            if (letzter >= 0 && t - letzter < z(50) && t > letzter) {
                messBytes += n;
                messMs += static_cast<double>(t - letzter);
            }
            letzter = t;
        }
        for (int i = 0; i < n; ++i) {
            const char c = static_cast<char>(buf[i] & 0x7F);   // Bit 7 VOR dem Vergleich
            alles.push_back(c);
            fenster.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
        }
        return n > 0;
    }

    /// Sucht eines der Muster im Fenster; bei Treffer wird das Fenster bis hinter den Treffer verworfen.
    int suche(const std::vector<std::string>& muster) {
        std::size_t best = std::string::npos;
        int welches = -1;
        for (std::size_t i = 0; i < muster.size(); ++i) {
            const std::size_t p = fenster.find(klein(muster[i]));
            if (p != std::string::npos && (best == std::string::npos || p < best)) {
                best = p;
                welches = static_cast<int>(i);
            }
        }
        if (welches >= 0) fenster.erase(0, best + muster[static_cast<std::size_t>(welches)].size());
        return welches;
    }

    bool sende(const std::string& s) {
        return l.schreiben(reinterpret_cast<const uint8_t*>(s.data()), s.size());
    }

    /// Liest, bis eines der Muster kommt; die Frist ist eine Frist der STILLE (jedes Zeichen
    /// setzt sie neu).  -1 = Frist, -2 = Abbruch/getrennt.
    int warte(const std::vector<std::string>& m, int64_t stilleMs) {
        int64_t ende = l.jetztMs() + stilleMs;
        for (;;) {
            if (stop() || getrennt) return -2;
            const int w = suche(m);
            if (w >= 0) return w;
            const int64_t rest = ende - l.jetztMs();
            if (rest <= 0) return -1;
            if (lies(rest)) ende = l.jetztMs() + stilleMs;
        }
    }

    /// Liest, bis die Leitung `stilleMs` still war (hoechstens `maxMs`).  false = Abbruch/getrennt.
    bool ruhe(int64_t stilleMs, int64_t maxMs) {
        const int64_t ende = l.jetztMs() + maxMs;
        while (l.jetztMs() < ende) {
            if (stop()) return false;
            if (!lies(std::min<int64_t>(stilleMs, std::max<int64_t>(1, ende - l.jetztMs())))) break;
        }
        return !getrennt && !abgebrochen;
    }

    void messung(LoginErgebnis& e) const {
        if (messBytes >= 64 && messMs > 0) {
            e.zeichen_pro_s = messBytes * 1000.0 / messMs;
            if (o.baud > 0) e.tempo = std::clamp(e.zeichen_pro_s / (o.baud / 10.0), 0.01, 1000.0);
        }
    }
};

LoginErgebnis fehler(Lauf& r, const std::string& grund, LoginArt art = LoginArt::Frist) {
    LoginErgebnis e;
    e.art = art;
    r.messung(e);
    e.gelesen = sichtbarMachen(r.alles, std::string::npos);
    e.text = "Anmeldung fehlgeschlagen: " + grund + "; zuletzt gelesen: \"" + sichtbarMachen(r.alles) + "\"";
    return e;
}

}  // namespace

std::string sichtbarMachen(const std::string& roh, std::size_t max) {
    std::string s;
    for (char ch : roh) {
        const unsigned char c = static_cast<unsigned char>(ch) & 0x7F;
        if (c == '\r') s += "\\r";
        else if (c == '\n') s += "\\n";
        else if (c < 0x20 || c == 0x7F) {
            char b[8];
            std::snprintf(b, sizeof b, "<%02X>", c);
            s += b;
        } else {
            s.push_back(static_cast<char>(c));
        }
    }
    if (max != std::string::npos && s.size() > max) s = "..." + s.substr(s.size() - max);
    return s;
}

LoginErgebnis loginSkript(Leitung& l, const LoginOptionen& o) {
    Lauf r{l, o, {}, {}, false, false};
    int versuche = 0;
    auto grundAbbruch = [&](const char* was) -> LoginErgebnis {
        if (r.getrennt) return fehler(r, std::string("Leitung getrennt (") + was + ")", LoginArt::Leitung);
        return fehler(r, std::string("abgebrochen (") + was + ")", LoginArt::Abbruch);
    };
    auto fertig = [&]() {
        LoginErgebnis e;
        e.ok = true;
        e.art = LoginArt::Ok;
        e.versuche = versuche;
        r.messung(e);
        e.gelesen = sichtbarMachen(r.alles, std::string::npos);
        return e;
    };
    auto abgelehnt = [&](const std::string& grund) {
        LoginErgebnis e = fehler(r, grund, LoginArt::Abgelehnt);
        e.versuche = versuche;
        return e;
    };

    // 1. RETURN, bis „login:" erscheint.  Wiederholt wird nur, wenn die Leitung
    //    `return_abstand_ms` still war; die Gesamtfrist zaehlt nur stille Zeit.
    {
        int64_t still = 0;
        bool da = false;
        while (!da) {
            if (r.stop() || r.getrennt) return grundAbbruch("warte auf login:");
            if (still >= r.z(o.frist_login_ms)) return fehler(r, "kein \"login:\" innerhalb der Frist");
            if (!r.sende("\r")) {
                r.getrennt = true;
                return grundAbbruch("warte auf login:");
            }
            const int64_t t0 = l.jetztMs();
            const int w = r.warte({"login:"}, r.z(o.return_abstand_ms));
            if (w == -2) return grundAbbruch("warte auf login:");
            if (w >= 0) da = true;
            else still += std::max<int64_t>(1, l.jetztMs() - t0);
        }
        // Weitere Antworten auf gestaute RETURN abwarten (ein zweites „login:" nach dem
        // Namen saehe sonst wie eine Ablehnung aus).
        if (!r.ruhe(r.z(o.ruhe_ms), r.z(o.ruhe_max_ms))) return grundAbbruch("warte auf login:");
    }

    // 2./3. Name, Kennwort, Serverbeginn.  Ohne Rueckruf genau einmal; mit Rueckruf so oft, wie der
    //    Anwender es nach einer Ablehnung noch einmal versucht (dieselbe Leitung, kein neues RETURN).
    std::string name = o.name, kennwort = o.kennwort;
    for (int versuch = 0;; ++versuch) {
        if (o.abfrage) {
            // Die Eingabezeit des Anwenders ist keine Frist: hier wird nicht gelesen, keine Uhr laeuft.
            AnmeldeAnfrage a;
            a.versuch = versuch;
            a.gelesen = sichtbarMachen(r.alles);
            if (!o.abfrage(a, name, kennwort) || r.stop()) {
                r.abgebrochen = true;
                return grundAbbruch("Anwender hat die Anmeldung abgebrochen");
            }
            // Was die Gegenstelle waehrend der Eingabe noch schrieb (Zeitablauf-Meldung, zweites
            // „login:"), gehoert nicht in die Auswertung der Antwort.
            if (!r.ruhe(r.z(o.ruhe_ms), r.z(o.ruhe_max_ms))) return grundAbbruch("warte nach der Eingabe");
        }

        r.fenster.clear();
        ++versuche;
        if (!r.sende(name + "\r")) {
            r.getrennt = true;
            return grundAbbruch("sende Namen");
        }
        const std::vector<std::string> nachName = {"password:", o.server_text, "login incorrect", "login:"};
        // 0: weiter zum Serverbeginn, 1: abgelehnt (Rueckruf erneut)
        bool nochmal = false;
        bool loginDa = false;   // „login:" der naechsten Runde steht schon im Fenster (verbraucht)
        if (o.mit_kennwort) {
            const int w = r.warte(nachName, r.z(o.frist_kennwort_ms));
            if (w == -2) return grundAbbruch("warte auf Password:");
            if (w == -1) return fehler(r, "kein \"Password:\" nach dem Namen innerhalb der Frist");
            if (w == 2 || w == 3) {
                if (!o.abfrage) return abgelehnt("Name \"" + name + "\" abgelehnt (login: erscheint erneut)");
                nochmal = true;
                loginDa = (w == 3);
            } else if (w == 0) {
                r.fenster.clear();
                if (!r.sende(kennwort + "\r")) {
                    r.getrennt = true;
                    return grundAbbruch("sende Kennwort");
                }
            } else {
                // Serverbeginn ohne Kennwort: fertig mit Schritt 3, Ruhe unten.
                r.fenster = o.server_text + " ";
            }
        }

        if (!nochmal) {
            if (!o.auf_server_warten) return fertig();

            // Auf den Serverbeginn warten.  „Choose one"/„New password" = Benutzer ohne Kennwort.
            const std::vector<std::string> m = {o.server_text, "login incorrect", "choose one", "new password", "login:"};
            const int w = r.warte(m, r.z(o.frist_server_ms));
            if (w == -2) return grundAbbruch("warte auf den Serverbeginn");
            if (w == -1) return fehler(r, "kein Serverbeginn (\"" + o.server_text + "\") innerhalb der Frist");
            if (w == 2 || w == 3) return fehler(r, "Benutzer hat kein Kennwort (System fordert ein neues)", LoginArt::Abgelehnt);
            if (w == 1 || w == 4) {
                if (!o.abfrage) return abgelehnt("Kennwort abgelehnt (Login incorrect)");
                nochmal = true;
                loginDa = (w == 4);
            }
        }

        if (nochmal) {
            // Die Gegenstelle fragt von vorn: erst das neue „login:" abwarten (nach „Login incorrect"
            // kommt es gleich danach, bei einer getty nach einer Zeitsperre), dann die Stille.
            if (!loginDa) {
                const int w = r.warte({"login:"}, r.z(o.frist_kennwort_ms));
                if (w == -2) return grundAbbruch("warte auf erneutes login:");
                if (w == -1) return fehler(r, "nach der Ablehnung kein erneutes \"login:\" innerhalb der Frist");
            }
            if (!r.ruhe(r.z(o.ruhe_ms), r.z(o.ruhe_max_ms))) return grundAbbruch("warte auf erneutes login:");
            continue;
        }
        break;
    }

    // 4. Ruhe: den Rest des Banners lesen, bis die Leitung `ruhe_ms` still war.
    if (!r.ruhe(r.z(o.ruhe_ms), r.z(o.ruhe_max_ms))) return grundAbbruch("warte auf Ruhe");
    return fertig();
}

}  // namespace k1520::kermit
