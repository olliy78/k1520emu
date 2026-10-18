/**
 * @file server.cpp
 * @brief Kermit-Serverschleife und Serverbefehle (Teil der `Sitzung`).
 *
 * @details
 * Doc: doc/design/36_kermit.md §4.2 (AP K2).  Der Server ist eine
 * ORDNERFREIGABE: er beantwortet I, S (Datei ablegen), R (Datei holen, Muster
 * mit * und ? im letzten Namensteil), G C/D/E/F/L/H.  Host-Befehle (C) und
 * Kermit-Befehle (K) werden mit einem E-Paket abgewiesen, ebenso alles
 * Unbekannte.  `G D` liefert ein selbst erzeugtes `ls -l` im System-III-Format
 * als Text (X-Paket, wie C-Kermit), `G C` quittiert mit dem neuen
 * Verzeichnis, `G E` mit einer Klartextmeldung.
 *
 * Im Leerlauf wartet der Server ohne NAK: ein N mitten in den Befehlsaustausch
 * eines C-Kermit-4.2-Clients (I → Y → G) wuerde dort als „anderes Paket"
 * mit einem NAK beantwortet und verwirrte den Austausch.
 */
#include "kermit/src/protokoll.h"

namespace k1520::kermit {

namespace {
const char* const HILFE =
    "k1520kermit-Server (Ordnerfreigabe)\n"
    "\n"
    "  GET datei            Datei holen (Muster * und ? erlaubt)\n"
    "  SEND datei           Datei in der Freigabe ablegen\n"
    "  REMOTE DIRECTORY [p] Verzeichnis (ls -l)\n"
    "  REMOTE CWD [p]       Verzeichnis wechseln (leer = Wurzel)\n"
    "  REMOTE DELETE datei  Datei loeschen\n"
    "  REMOTE HELP          diese Hilfe\n"
    "  FINISH, BYE          Server beenden\n"
    "\n"
    "Host-Befehle werden nicht ausgefuehrt.\n";

std::vector<std::string> felder(const std::string& cmd) {
    std::vector<std::string> v;
    std::size_t i = 1;
    while (i < cmd.size()) {
        int len = unchar(cmd[i]);
        if (len < 0 || i + 1 + static_cast<std::size_t>(len) > cmd.size())
            len = static_cast<int>(cmd.size() - i - 1);   // nachsichtig: Rest
        v.push_back(cmd.substr(i + 1, static_cast<std::size_t>(len)));
        i += 1 + static_cast<std::size_t>(len);
    }
    return v;
}
}  // namespace

Ergebnis Sitzung::serverSende(std::vector<Quelle>& quellen) { return sendeTransaktion(quellen); }

Ergebnis Sitzung::serverBefehl(const Paket& p, Freigabe& f, bool& ende) {
    ende = false;
    letzteSeq_ = p.seq;   // E-Pakete tragen die Nummer des Befehls
    auto abweisen = [&](Fehlerart a, const std::string& t) { return fehler(a, t, true); };

    switch (p.typ) {
    case 'I': {
        const InitParameter dein = zerlegeInit(p.daten);
        const InitParameter mein = antwortParameter(meineParameter(), dein);
        v_ = verhandle(mein, dein, e_.max_paket);
        sende('Y', p.seq, initDaten(mein, e_.capas_senden && dein.felder > 9));
        return {};
    }
    case 'S':
        if (f.nurLesen()) return abweisen(Fehlerart::PfadAbgewiesen, "Freigabe ist schreibgeschuetzt");
        return empfangeTransaktion(p, Ziel{&f, nullptr});
    case 'R': {
        const std::string name = dekodiereText(v_.kod, p.daten);
        std::string fe;
        const auto treffer = f.finde(name, fe);
        if (treffer.empty())
            return abweisen(fe.rfind("Datei nicht", 0) == 0 ? Fehlerart::DateiNichtGefunden : Fehlerart::PfadAbgewiesen,
                            fe);
        std::vector<Quelle> qs;
        for (const auto& t : treffer) {
            Quelle q;
            q.q = oeffneQuelle(t, fe);
            if (!q.q) return abweisen(Fehlerart::Lesefehler, fe);
            const auto u = t.filename().u8string();
            q.name = nameFuerSenden(std::string(u.begin(), u.end()), e_.namen_senden);
            q.text = e_.textmodus;
            q.anzeige = f.anzeigeName(t);
            qs.push_back(std::move(q));
        }
        return serverSende(qs);
    }
    case 'G': {
        const std::string cmd = dekodiereText(v_.kod, p.daten);
        if (cmd.empty()) return abweisen(Fehlerart::Protokoll, "Leerer Serverbefehl");
        const auto args = felder(cmd);
        const std::string a0 = args.empty() ? std::string() : args[0];
        std::string fe;
        switch (cmd[0]) {
        case 'C':
            if (!f.wechseln(a0, fe)) return abweisen(Fehlerart::PfadAbgewiesen, fe);
            sende('Y', p.seq, kodiere(v_.kod, "/" + f.aktuell()));
            if (r_.ereignis) r_.ereignis("Verzeichnis: /" + f.aktuell());
            return {};
        case 'D': {
            std::string text;
            if (!f.liste(a0, text, fe)) return abweisen(Fehlerart::DateiNichtGefunden, fe);
            std::vector<Quelle> qs(1);
            qs[0].q = textQuelle(text);
            qs[0].name = a0.empty() ? "ls -l" : "ls -l " + a0;
            qs[0].alsX = true;
            qs[0].text = true;
            qs[0].anzeige = qs[0].name;
            return serverSende(qs);
        }
        case 'E': {
            if (a0.empty()) return abweisen(Fehlerart::Protokoll, "Kein Dateiname");
            std::string meldung;
            if (!f.loeschen(a0, meldung)) return abweisen(Fehlerart::DateiNichtGefunden, meldung);
            sende('Y', p.seq, kodiere(v_.kod, meldung));
            if (r_.ereignis) r_.ereignis(meldung);
            return {};
        }
        case 'F':
        case 'L':
            sende('Y', p.seq, "");
            ende = true;
            if (r_.ereignis) r_.ereignis(cmd[0] == 'F' ? "Server beendet (FINISH)" : "Server beendet (BYE)");
            return {};
        case 'H': {
            std::vector<Quelle> qs(1);
            qs[0].q = textQuelle(HILFE);
            qs[0].name = "Hilfe";
            qs[0].alsX = true;
            qs[0].text = true;
            qs[0].anzeige = "Hilfe";
            return serverSende(qs);
        }
        default:
            return abweisen(Fehlerart::NichtUnterstuetzt, std::string("Nicht unterstuetzter Serverbefehl G ") + cmd[0]);
        }
    }
    case 'C':
        return abweisen(Fehlerart::NichtUnterstuetzt, "Host-Befehle werden nicht ausgefuehrt (Ordnerfreigabe)");
    case 'K':
        return abweisen(Fehlerart::NichtUnterstuetzt, "Kermit-Befehle werden nicht ausgefuehrt");
    case 'E':   // Client hat abgebrochen — weiter bedienen
        if (r_.ereignis) r_.ereignis("Fehlerpaket des Clients: " + dekodiereText(v_.kod, p.daten));
        return {};
    case 'N':
    case 'Y':   // verirrte Quittungen
        return {};
    default:
        return abweisen(Fehlerart::NichtUnterstuetzt, std::string("Nicht unterstuetzte Serverfunktion ") + p.typ);
    }
}

Ergebnis Sitzung::serve(Freigabe& f) {
    abbruch_ = false;
    Ergebnis gesamt;
    if (!f.gueltig()) return fehler(Fehlerart::PfadAbgewiesen, "Freigabe ungueltig", false);
    if (e_.max_namenslaenge > 0) f.setzeMaxNamenslaenge(e_.max_namenslaenge);
    if (r_.ereignis) r_.ereignis("Server bereit");
    for (;;) {
        chkt_ = 1;
        abbruchDatei_ = false;
        abbruchGruppe_ = false;
        Paket p;
        switch (lesePaket(p, e_.frist_ms)) {
        case Lesen::Abbruch: {
            Ergebnis r = fehler(Fehlerart::Abgebrochen, "Server angehalten", true);
            r.dateien = gesamt.dateien;
            return r;
        }
        case Lesen::Getrennt: {
            Ergebnis r = fehler(Fehlerart::LeitungGetrennt, "Leitung getrennt", false);
            r.dateien = gesamt.dateien;
            return r;
        }
        case Lesen::Frist: continue;   // Leerlauf: weiter warten
        case Lesen::Fehlerhaft:
            sendeRoh(baueRahmen(Paket{'N', 0, ""}, 1, v_.rahmen));
            continue;
        case Lesen::Ok: break;
        }
        bool ende = false;
        Ergebnis r = serverBefehl(p, f, ende);
        gesamt.dateien.insert(gesamt.dateien.end(), r.dateien.begin(), r.dateien.end());
        // Nur der EIGENE Abbruch beendet den Server; ein vom Client verworfener
        // Transfer meldet zwar Abgebrochen, der Server bedient weiter.
        if (r.art == Fehlerart::LeitungGetrennt || abbruch_) {
            r.dateien = gesamt.dateien;
            return r;
        }
        if (ende) return gesamt;
    }
}

}  // namespace k1520::kermit
