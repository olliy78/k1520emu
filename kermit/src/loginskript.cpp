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
};

LoginErgebnis fehler(Lauf& r, const std::string& grund) {
    LoginErgebnis e;
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
    const int64_t t0 = l.jetztMs();

    auto grundAbbruch = [&](const char* was) -> LoginErgebnis {
        if (r.getrennt) return fehler(r, std::string("Leitung getrennt (") + was + ")");
        return fehler(r, std::string("abgebrochen (") + was + ")");
    };

    // 1. RETURN, bis „login:" erscheint.
    {
        const int64_t ende = t0 + o.frist_login_ms;
        bool da = false;
        while (!da) {
            if (r.stop() || r.getrennt) return grundAbbruch("warte auf login:");
            if (l.jetztMs() >= ende) return fehler(r, "kein \"login:\" innerhalb der Frist");
            if (!r.sende("\r")) {
                r.getrennt = true;
                return grundAbbruch("warte auf login:");
            }
            const int64_t bis = std::min<int64_t>(ende, l.jetztMs() + o.return_abstand_ms);
            while (l.jetztMs() < bis) {
                r.lies(std::max<int64_t>(1, bis - l.jetztMs()));
                if (r.stop() || r.getrennt) return grundAbbruch("warte auf login:");
                if (r.suche({"login:"}) >= 0) {
                    da = true;
                    break;
                }
            }
        }
    }

    // 2. Name senden, „Password:" abwarten.
    r.fenster.clear();
    if (!r.sende(o.name + "\r")) {
        r.getrennt = true;
        return grundAbbruch("sende Namen");
    }
    const std::vector<std::string> nachName = {"password:", o.server_text, "login incorrect", "login:"};
    if (o.mit_kennwort) {
        const int64_t ende = l.jetztMs() + o.frist_kennwort_ms;
        int w = -1;
        while (w < 0) {
            if (r.stop() || r.getrennt) return grundAbbruch("warte auf Password:");
            const int64_t rest = ende - l.jetztMs();
            if (rest <= 0) return fehler(r, "kein \"Password:\" nach dem Namen innerhalb der Frist");
            r.lies(rest);
            w = r.suche(nachName);
        }
        if (w == 2 || w == 3) return fehler(r, "Name \"" + o.name + "\" abgelehnt (login: erscheint erneut)");
        if (w == 0) {
            r.fenster.clear();
            if (!r.sende(o.kennwort + "\r")) {
                r.getrennt = true;
                return grundAbbruch("sende Kennwort");
            }
        } else {
            // Serverbeginn ohne Kennwort: fertig mit Schritt 3, Ruhe unten.
            r.fenster = o.server_text + " ";
        }
    }

    if (!o.auf_server_warten) {
        LoginErgebnis e;
        e.ok = true;
        e.gelesen = sichtbarMachen(r.alles, std::string::npos);
        return e;
    }

    // 3. Auf den Serverbeginn warten.  „Choose one"/„New password" = Benutzer ohne Kennwort.
    {
        const int64_t ende = l.jetztMs() + o.frist_server_ms;
        const std::vector<std::string> m = {o.server_text, "login incorrect", "choose one", "new password", "login:"};
        int w = -1;
        while (w < 0) {
            if (r.stop() || r.getrennt) return grundAbbruch("warte auf den Serverbeginn");
            const int64_t rest = ende - l.jetztMs();
            if (rest <= 0) return fehler(r, "kein Serverbeginn (\"" + o.server_text + "\") innerhalb der Frist");
            r.lies(rest);
            w = r.suche(m);
        }
        if (w == 1 || w == 4) return fehler(r, "Kennwort abgelehnt (Login incorrect)");
        if (w == 2 || w == 3) return fehler(r, "Benutzer hat kein Kennwort (System fordert ein neues)");
    }

    // 4. Ruhe: den Rest des Banners lesen, bis die Leitung `ruhe_ms` still war.
    {
        const int64_t ende = l.jetztMs() + o.ruhe_max_ms;
        while (l.jetztMs() < ende) {
            if (r.stop()) return grundAbbruch("warte auf Ruhe");
            if (!r.lies(o.ruhe_ms)) break;
            if (r.getrennt) return grundAbbruch("warte auf Ruhe");
        }
        if (r.getrennt) return grundAbbruch("warte auf Ruhe");
    }

    LoginErgebnis e;
    e.ok = true;
    e.gelesen = sichtbarMachen(r.alles, std::string::npos);
    return e;
}

}  // namespace k1520::kermit
