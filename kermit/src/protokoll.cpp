/**
 * @file protokoll.cpp
 * @brief Kermit-Zustandsautomaten Senden/Empfangen und Client-Befehle
 *        (siehe protokoll.h).  Die Serverschleife steht in server.cpp.
 */
#include "kermit/src/protokoll.h"

#include <algorithm>

namespace k1520::kermit {

const char* fehlerText(Fehlerart a) {
    switch (a) {
    case Fehlerart::Keiner: return "kein Fehler";
    case Fehlerart::Zeitueberschreitung: return "Zeitueberschreitung (zu viele Wiederholungen)";
    case Fehlerart::Gegenstelle: return "Fehlermeldung der Gegenstelle";
    case Fehlerart::Abgebrochen: return "abgebrochen";
    case Fehlerart::LeitungGetrennt: return "Leitung getrennt";
    case Fehlerart::DateiNichtGefunden: return "Datei nicht gefunden";
    case Fehlerart::Lesefehler: return "Lesefehler";
    case Fehlerart::Schreibfehler: return "Schreibfehler";
    case Fehlerart::PfadAbgewiesen: return "Pfad abgewiesen";
    case Fehlerart::NameZuLang: return "Dateiname zu lang";
    case Fehlerart::Abgelehnt: return "von der Gegenstelle abgelehnt";
    case Fehlerart::Protokoll: return "Protokollfehler";
    case Fehlerart::NichtUnterstuetzt: return "nicht unterstuetzt";
    }
    return "?";
}

Sitzung::Sitzung(Leitung& l, Einstellungen e, Rueckrufe r) : l_(l), e_(std::move(e)), r_(std::move(r)) {
    v_.kod.qctlMein = e_.qctl;
}

// ─── Paket-E/A ─────────────────────────────────────────────────────────────

Sitzung::Lesen Sitzung::lesePaket(Paket& p, int64_t fristMs) {
    const int64_t ende = l_.jetztMs() + fristMs;
    zerleger_.setzeChkt(chkt_);
    for (;;) {
        if (abbruch_) return Lesen::Abbruch;
        while (pufPos_ < puffer_.size()) {
            const auto r = zerleger_.zufuehren(puffer_[pufPos_++]);
            if (r == PaketZerleger::Ergebnis::Paket) {
                p = zerleger_.paket();
                return Lesen::Ok;
            }
            if (r == PaketZerleger::Ergebnis::Fehlerhaft) return Lesen::Fehlerhaft;
        }
        puffer_.clear();
        pufPos_ = 0;
        const int64_t rest = ende - l_.jetztMs();
        if (rest <= 0) {
            zerleger_.zuruecksetzen();
            return Lesen::Frist;
        }
        uint8_t b[512];
        const int n = l_.lesen(b, sizeof b, rest);
        if (abbruch_) return Lesen::Abbruch;
        if (n < 0) return Lesen::Getrennt;
        puffer_.assign(b, b + n);
    }
}

void Sitzung::leereEingang() {
    puffer_.clear();
    pufPos_ = 0;
    zerleger_.zuruecksetzen();
    uint8_t b[512];
    while (l_.lesen(b, sizeof b, 0) > 0) {
    }
}

bool Sitzung::sendeRoh(const std::string& rahmen) {
    return l_.schreiben(reinterpret_cast<const uint8_t*>(rahmen.data()), rahmen.size());
}

bool Sitzung::sende(char typ, int seq, const std::string& daten) {
    letztes_ = baueRahmen(Paket{typ, seq & 63, daten}, chkt_, v_.rahmen);
    letzterTyp_ = typ;
    letzteSeq_ = seq & 63;
    return sendeRoh(letztes_);
}

void Sitzung::sendeFehler(const std::string& text) {
    // Das zuletzt Gesendete bleibt fuer eine Wiederholung erhalten.
    const int seq = letzteSeq_ < 0 ? 0 : letzteSeq_;
    sendeRoh(baueRahmen(Paket{'E', seq, kodiere(v_.kod, text)}, chkt_, v_.rahmen));
    if (r_.ereignis) r_.ereignis("Fehlerpaket gesendet: " + text);
}

int64_t Sitzung::wartefrist() const {
    if (!e_.frist_fest && v_.time > 0) return static_cast<int64_t>(v_.time) * 1000;
    return e_.frist_ms;
}

InitParameter Sitzung::meineParameter() const {
    InitParameter ip;
    ip.maxl = std::clamp(e_.maxl, 10, MAX_NORMAL);
    ip.time = std::clamp((e_.frist_ms + 999) / 1000, 1, 94);
    ip.npad = e_.npad;
    ip.padc = e_.padc;
    ip.eol = e_.eol;
    ip.qctl = e_.qctl;
    ip.qbin = e_.qbin;
    ip.chkt = std::clamp(e_.pruefsumme, 1, 3);
    ip.rept = e_.rept ? e_.rept : ' ';
    ip.capas = (e_.langpakete ? CAPAS_LANGPAKETE : 0) | (e_.attribute ? CAPAS_ATTRIBUTE : 0);
    ip.windo = std::clamp(e_.fenster, 0, 1);
    ip.maxlx = e_.langpakete ? std::clamp(e_.max_paket, MAX_NORMAL, MAX_LANG) : 0;
    return ip;
}

Ergebnis Sitzung::fehler(Fehlerart a, const std::string& text, bool epaket) {
    if (epaket) sendeFehler(text);
    chkt_ = 1;
    Ergebnis r;
    r.art = a;
    r.text = text;
    if (r_.ereignis) r_.ereignis(std::string(fehlerText(a)) + ": " + text);
    return r;
}

Ergebnis Sitzung::ausAntwort(const Antwort& a) {
    switch (a.art) {
    case Fehlerart::Zeitueberschreitung: return fehler(a.art, a.text, true);
    case Fehlerart::Abgebrochen: return fehler(a.art, "Abbruch durch den Anwender", true);
    default: return fehler(a.art, a.text, false);
    }
}

// ─── Stop-and-Wait ─────────────────────────────────────────────────────────

Sitzung::Antwort Sitzung::sendeUndWarte(char typ, int seq, const std::string& daten) {
    Antwort a;
    if (!sende(typ, seq, daten)) {
        a.art = Fehlerart::LeitungGetrennt;
        a.text = "Leitung getrennt";
        return a;
    }
    int versuche = 0;
    auto nochmal = [&](bool wiederholen) {
        if (++versuche > e_.max_wiederholungen) {
            a.art = Fehlerart::Zeitueberschreitung;
            a.text = "Zu viele Wiederholungen";
            return false;
        }
        if (wiederholen) sendeRoh(letztes_);
        return true;
    };
    for (;;) {
        Paket p;
        switch (lesePaket(p, wartefrist())) {
        case Lesen::Abbruch: a.art = Fehlerart::Abgebrochen; return a;
        case Lesen::Getrennt: a.art = Fehlerart::LeitungGetrennt; a.text = "Leitung getrennt"; return a;
        case Lesen::Frist:
        case Lesen::Fehlerhaft:
            if (!nochmal(true)) return a;
            continue;
        case Lesen::Ok: break;
        }
        if (p.typ == 'E') {
            a.art = Fehlerart::Gegenstelle;
            a.text = dekodiereText(v_.kod, p.daten);
            return a;
        }
        if (p.typ == 'Y' && p.seq == (seq & 63)) {
            a.paket = p;
            return a;
        }
        if (p.typ == 'N' && p.seq == ((seq + 1) & 63)) {   // NAK fuer das naechste = Quittung
            a.paket = Paket{'Y', seq & 63, ""};
            return a;
        }
        if (p.typ == typ && p.seq == (seq & 63)) continue;   // Echo der Leitung
        if (!nochmal(p.typ == 'N')) return a;                 // NAK: wiederholen; alte Quittung: weiter warten
    }
}

Sitzung::Antwort Sitzung::warteAuf(int erw) {
    Antwort a;
    erw &= 63;
    int versuche = 0;
    auto nak = [&]() {
        if (++versuche > e_.max_wiederholungen) {
            a.art = Fehlerart::Zeitueberschreitung;
            a.text = "Zu viele Wiederholungen";
            return false;
        }
        // Das NAK ersetzt NICHT das zuletzt Gesendete: kommt das vorige Paket
        // noch einmal, muss die alte Quittung wiederholt werden.
        sendeRoh(baueRahmen(Paket{'N', erw, ""}, chkt_, v_.rahmen));
        return true;
    };
    for (;;) {
        Paket p;
        switch (lesePaket(p, wartefrist())) {
        case Lesen::Abbruch: a.art = Fehlerart::Abgebrochen; return a;
        case Lesen::Getrennt: a.art = Fehlerart::LeitungGetrennt; a.text = "Leitung getrennt"; return a;
        case Lesen::Frist:
        case Lesen::Fehlerhaft:
            if (!nak()) return a;
            continue;
        case Lesen::Ok: break;
        }
        if (p.typ == 'E') {
            a.art = Fehlerart::Gegenstelle;
            a.text = dekodiereText(v_.kod, p.daten);
            return a;
        }
        if (p.typ == 'Y' || p.typ == 'N') {   // Echo eigener Quittungen / verirrte NAKs
            if (++versuche > e_.max_wiederholungen) {
                a.art = Fehlerart::Zeitueberschreitung;
                a.text = "Zu viele Wiederholungen";
                return a;
            }
            continue;
        }
        if (p.seq == erw) {
            a.paket = p;
            return a;
        }
        if (p.seq == ((erw + 63) & 63)) {   // Duplikat: Quittung ging verloren
            if (++versuche > e_.max_wiederholungen) {
                a.art = Fehlerart::Zeitueberschreitung;
                a.text = "Zu viele Wiederholungen";
                return a;
            }
            sendeRoh(letztes_);
            continue;
        }
        if (!nak()) return a;
    }
}

// ─── Senden ────────────────────────────────────────────────────────────────

static std::string grundname(const std::string& s) {
    const std::size_t p = s.find_last_of('/');
    return p == std::string::npos ? s : s.substr(p + 1);
}

Ergebnis Sitzung::sendeTransaktion(std::vector<Quelle>& quellen) {
    chkt_ = 1;
    int seq = 0;
    const InitParameter mein = meineParameter();
    Antwort a = sendeUndWarte('S', seq, initDaten(mein, e_.capas_senden));
    if (a.art != Fehlerart::Keiner) return ausAntwort(a);
    v_ = verhandle(mein, zerlegeInit(a.paket.daten), e_.max_paket);
    chkt_ = v_.chkt;
    if (r_.ereignis)
        r_.ereignis("Ausgehandelt: Pruefung " + std::to_string(v_.chkt) + ", Paket " +
                    std::to_string(v_.datenBudget()) + (v_.kod.qbin ? ", 8-Bit-Praefix" : "") +
                    (v_.kod.rept ? ", Wiederholung" : "") + (v_.attribute ? ", Attribute" : ""));
    const int budget = v_.datenBudget();
    Ergebnis erg;
    bool gruppeAbbruch = false;

    for (auto& q : quellen) {
        if (gruppeAbbruch) break;
        abbruchDatei_ = false;
        if (!q.alsX && e_.max_namenslaenge > 0 && static_cast<int>(grundname(q.name).size()) > e_.max_namenslaenge)
            return fehler(Fehlerart::NameZuLang,
                          "Zielname laenger als " + std::to_string(e_.max_namenslaenge) + " Zeichen: " + q.name, true);
        seq = (seq + 1) & 63;
        a = sendeUndWarte(q.alsX ? 'X' : 'F', seq, kodiere(v_.kod, q.name));
        if (a.art != Fehlerart::Keiner) return ausAntwort(a);
        bool verwerfen = false;
        const int64_t gesamt = q.q->groesse();
        if (v_.attribute && !q.alsX) {
            Attribute at;
            at.laenge = gesamt;
            if (gesamt >= 0) at.laengeK = (gesamt + 1023) / 1024;
            at.datum = q.q->datum();
            at.typ = q.text ? 'A' : 'B';
            seq = (seq + 1) & 63;
            a = sendeUndWarte('A', seq, attributDaten(at));
            if (a.art != Fehlerart::Keiner) return ausAntwort(a);
            if (!a.paket.daten.empty() && a.paket.daten[0] == 'N') {
                verwerfen = true;
                erg.art = Fehlerart::Abgelehnt;
                erg.text = "Datei von der Gegenstelle abgelehnt: " + q.name;
                if (r_.ereignis) r_.ereignis(erg.text);
            }
        }
        std::vector<uint8_t> vorrat;
        std::size_t vp = 0;
        bool dateiEnde = false;
        int64_t gelesen = 0;
        while (!verwerfen) {
            if (abbruch_) return fehler(Fehlerart::Abgebrochen, "Abbruch durch den Anwender", true);
            if (abbruchDatei_ || abbruchGruppe_) {
                verwerfen = true;
                gruppeAbbruch = abbruchGruppe_;
                erg.art = Fehlerart::Abgebrochen;
                erg.text = "Uebertragung abgebrochen: " + q.name;
                break;
            }
            while (!dateiEnde && vorrat.size() - vp < static_cast<std::size_t>(budget)) {
                if (vp > 0) {
                    vorrat.erase(vorrat.begin(), vorrat.begin() + static_cast<std::ptrdiff_t>(vp));
                    vp = 0;
                }
                uint8_t b[4096];
                const long n = q.q->lesen(b, sizeof b);
                if (n < 0) return fehler(Fehlerart::Lesefehler, "Lesefehler: " + q.anzeige, true);
                if (n == 0) {
                    dateiEnde = true;
                    break;
                }
                gelesen += n;
                for (long i = 0; i < n; ++i) {
                    if (q.text && b[i] == '\n') vorrat.push_back('\r');   // LF lokal → CR LF
                    vorrat.push_back(b[i]);
                }
            }
            if (vp >= vorrat.size()) break;   // Dateiende
            std::string d;
            vp += kodiereBlock(v_.kod, vorrat.data() + vp, vorrat.size() - vp, static_cast<std::size_t>(budget), d);
            seq = (seq + 1) & 63;
            a = sendeUndWarte('D', seq, d);
            if (a.art != Fehlerart::Keiner) return ausAntwort(a);
            if (!a.paket.daten.empty() && (a.paket.daten[0] == 'X' || a.paket.daten[0] == 'Z')) {
                verwerfen = true;
                gruppeAbbruch = a.paket.daten[0] == 'Z';
                erg.art = Fehlerart::Abgelehnt;
                erg.text = std::string("Empfaenger bricht ab (") + a.paket.daten[0] + "): " + q.name;
                if (r_.ereignis) r_.ereignis(erg.text);
            }
            if (r_.fortschritt) r_.fortschritt(q.anzeige, gelesen, gesamt);
        }
        seq = (seq + 1) & 63;
        a = sendeUndWarte('Z', seq, verwerfen ? "D" : "");
        if (a.art != Fehlerart::Keiner) return ausAntwort(a);
        if (!verwerfen) {
            erg.dateien.push_back(q.anzeige);
            if (r_.fortschritt) r_.fortschritt(q.anzeige, gelesen, gesamt);
            if (r_.ereignis && !q.alsX) r_.ereignis("Gesendet: " + q.anzeige + " als " + q.name);
        }
    }
    seq = (seq + 1) & 63;
    a = sendeUndWarte('B', seq, "");
    if (a.art != Fehlerart::Keiner) return ausAntwort(a);
    chkt_ = 1;
    abbruchDatei_ = false;
    abbruchGruppe_ = false;
    return erg;
}

Ergebnis Sitzung::senden(const std::string& lokalerPfad, const std::string& entfernterName) {
    return sendenGruppe({SendeAuftrag{lokalerPfad, entfernterName}});
}

Ergebnis Sitzung::sendenGruppe(const std::vector<SendeAuftrag>& auftraege) {
    abbruch_ = false;
    abbruchDatei_ = false;
    abbruchGruppe_ = false;
    std::vector<Quelle> qs;
    for (const auto& au : auftraege) {
        Quelle q;
        std::string f;
        const auto p = std::filesystem::u8path(au.lokal);
        q.q = oeffneQuelle(p, f);
        if (!q.q) return fehler(Fehlerart::DateiNichtGefunden, f, false);
        const auto u = p.filename().u8string();
        const std::string datei(u.begin(), u.end());
        q.name = nameFuerSenden(au.entfernt.empty() ? datei : au.entfernt, e_.namen_senden);
        q.text = e_.textmodus;
        q.anzeige = au.lokal;
        qs.push_back(std::move(q));
    }
    return sendeTransaktion(qs);
}

// ─── Empfangen ─────────────────────────────────────────────────────────────

Ergebnis Sitzung::empfangeTransaktion(const Paket& s, Ziel z) {
    const InitParameter dein = zerlegeInit(s.daten);
    const InitParameter mein = antwortParameter(meineParameter(), dein);
    chkt_ = 1;
    v_ = verhandle(mein, dein, e_.max_paket);   // EOL/Fuellung schon fuer das Y
    if (!sende('Y', s.seq, initDaten(mein, e_.capas_senden && dein.felder > 9)))
        return fehler(Fehlerart::LeitungGetrennt, "Leitung getrennt", false);
    chkt_ = v_.chkt;
    if (z.freigabe && e_.max_namenslaenge > 0) z.freigabe->setzeMaxNamenslaenge(e_.max_namenslaenge);

    Ergebnis erg;
    int erw = (s.seq + 1) & 63;
    std::unique_ptr<DateiSenke> senke;
    bool inAusgabe = false, text = e_.textmodus, crOffen = false, verworfen = false;
    std::string name;
    Attribute attr;
    int64_t bytes = 0;
    std::vector<uint8_t> roh;
    std::string block;

    for (;;) {
        Antwort a = warteAuf(erw);
        if (a.art != Fehlerart::Keiner) {
            if (senke) senke->verwerfen();
            return ausAntwort(a);
        }
        const Paket& p = a.paket;
        switch (p.typ) {
        case 'F': {
            const std::string fname = dekodiereText(v_.kod, p.daten);
            text = e_.textmodus;
            attr = Attribute{};
            bytes = 0;
            crOffen = false;
            verworfen = false;
            abbruchDatei_ = false;
            if (!z.freigabe) {
                inAusgabe = true;
                name = fname;
            } else {
                std::string gesp, f;
                senke = z.freigabe->anlegen(nameFuerEmpfang(fname, e_.namen_empfang), e_.ueberschreiben, gesp, f);
                if (!senke) {
                    const Fehlerart art = f.rfind("Dateiname laenger", 0) == 0 ? Fehlerart::NameZuLang
                                          : f.rfind("Freigabe", 0) == 0 || f.rfind("Kann", 0) == 0 ||
                                                  f.rfind("Datei existiert", 0) == 0 || f.rfind("Ist ein", 0) == 0 ||
                                                  f.rfind("Verzeichnis fehlt", 0) == 0
                                              ? Fehlerart::Schreibfehler
                                              : Fehlerart::PfadAbgewiesen;
                    return fehler(art, f, true);
                }
                name = gesp;
                inAusgabe = false;
                if (r_.ereignis) r_.ereignis("Empfange: " + fname + " als " + gesp);
            }
            sende('Y', erw, "");   // leer: C-Kermit 4.2 deutet 'X'/'Z' am Anfang als Abbruch
            break;
        }
        case 'X':
            inAusgabe = true;
            text = true;
            crOffen = false;
            sende('Y', erw, "");
            break;
        case 'A': {
            attr = zerlegeAttribute(p.daten);
            if (attr.typ) text = (attr.typ == 'A');
            const bool zuGross = z.freigabe && z.freigabe->platzFrei() >= 0 && attr.laenge > z.freigabe->platzFrei();
            if (zuGross) {
                verworfen = true;
                sende('Y', erw, "N1");
                if (r_.ereignis) r_.ereignis("Abgelehnt (kein Platz): " + name);
            } else {
                sende('Y', erw, "");
            }
            break;
        }
        case 'D': {
            roh.clear();
            if (!dekodiere(v_.kod, p.daten, roh)) {   // Formfehler trotz guter Pruefung
                sendeRoh(baueRahmen(Paket{'N', erw, ""}, chkt_, v_.rahmen));
                continue;
            }
            block.clear();
            for (uint8_t b : roh) {
                if (!text) {
                    block += static_cast<char>(b);
                    continue;
                }
                if (crOffen) {
                    crOffen = false;
                    if (b == '\n') {
                        block += '\n';
                        continue;
                    }
                    block += '\r';
                }
                if (b == '\r') crOffen = true;
                else block += static_cast<char>(b);
            }
            bytes += static_cast<int64_t>(roh.size());
            if (inAusgabe) {
                if (z.ausgabe) *z.ausgabe += block;
            } else if (senke && !verworfen) {
                if (!senke->schreiben(reinterpret_cast<const uint8_t*>(block.data()), block.size())) {
                    senke->verwerfen();
                    senke.reset();
                    return fehler(Fehlerart::Schreibfehler, "Schreibfehler (Datentraeger voll?): " + name, true);
                }
            }
            if (abbruch_) {
                if (senke) senke->verwerfen();
                return fehler(Fehlerart::Abgebrochen, "Abbruch durch den Anwender", true);
            }
            sende('Y', erw, abbruchGruppe_ ? "Z" : abbruchDatei_ ? "X" : "");
            if (r_.fortschritt && !inAusgabe) r_.fortschritt(name, bytes, attr.laenge);
            break;
        }
        case 'Z': {
            if (crOffen) {
                crOffen = false;
                if (inAusgabe && z.ausgabe) *z.ausgabe += '\r';
                else if (senke && !verworfen) senke->schreiben(reinterpret_cast<const uint8_t*>("\r"), 1);
            }
            const bool nutzer = abbruchDatei_ || abbruchGruppe_;
            const bool weg = p.daten == "D" || verworfen || nutzer;
            if (senke) {
                if (weg) {
                    senke->verwerfen();
                    if (r_.ereignis) r_.ereignis("Verworfen: " + name);
                } else if (!senke->schliessen(attr.datum)) {
                    senke.reset();
                    return fehler(Fehlerart::Schreibfehler, "Schreibfehler beim Schliessen: " + name, true);
                } else {
                    erg.dateien.push_back(name);
                    if (r_.ereignis) r_.ereignis("Empfangen: " + name);
                }
                senke.reset();
            }
            if (nutzer) {
                erg.art = Fehlerart::Abgebrochen;
                erg.text = "Uebertragung abgebrochen: " + name;
            } else if (p.daten == "D" && !verworfen && !inAusgabe) {
                erg.art = Fehlerart::Abgebrochen;
                erg.text = "Sender hat die Datei verworfen: " + name;
            }
            inAusgabe = false;
            abbruchDatei_ = false;
            sende('Y', erw, "");
            break;
        }
        case 'B':
            sende('Y', erw, "");
            chkt_ = 1;
            abbruchGruppe_ = false;
            return erg;
        default:
            if (senke) senke->verwerfen();
            return fehler(Fehlerart::Protokoll, std::string("Unerwartetes Paket '") + p.typ + "'", true);
        }
        erw = (erw + 1) & 63;
    }
}

Ergebnis Sitzung::empfangen(const std::string& zielOrdner) {
    abbruch_ = false;
    Freigabe ziel(std::filesystem::u8path(zielOrdner));
    if (!ziel.gueltig()) return fehler(Fehlerart::PfadAbgewiesen, "Zielordner fehlt: " + zielOrdner, false);
    chkt_ = 1;
    int versuche = 0;
    for (;;) {
        Paket p;
        switch (lesePaket(p, e_.frist_ms)) {
        case Lesen::Abbruch: return fehler(Fehlerart::Abgebrochen, "Abbruch durch den Anwender", false);
        case Lesen::Getrennt: return fehler(Fehlerart::LeitungGetrennt, "Leitung getrennt", false);
        case Lesen::Frist:
        case Lesen::Fehlerhaft:
            if (++versuche > e_.max_wiederholungen)
                return fehler(Fehlerart::Zeitueberschreitung, "Kein Send-Init empfangen", false);
            sendeRoh(baueRahmen(Paket{'N', 0, ""}, 1, v_.rahmen));
            continue;
        case Lesen::Ok: break;
        }
        if (p.typ == 'S') {
            std::string aus;
            Ergebnis r = empfangeTransaktion(p, Ziel{&ziel, &aus});
            r.ausgabe = aus;
            return r;
        }
        if (p.typ == 'E') return fehler(Fehlerart::Gegenstelle, dekodiereText(v_.kod, p.daten), false);
    }
}

// ─── Client-Befehle ────────────────────────────────────────────────────────

Ergebnis Sitzung::iAustausch() {
    leereEingang();
    chkt_ = 1;
    const InitParameter mein = meineParameter();
    if (!sende('I', 0, initDaten(mein, e_.capas_senden)))
        return fehler(Fehlerart::LeitungGetrennt, "Leitung getrennt", false);
    int versuche = 0;
    for (;;) {
        Paket p;
        switch (lesePaket(p, e_.frist_ms)) {
        case Lesen::Abbruch: return fehler(Fehlerart::Abgebrochen, "Abbruch durch den Anwender", false);
        case Lesen::Getrennt: return fehler(Fehlerart::LeitungGetrennt, "Leitung getrennt", false);
        case Lesen::Frist:
        case Lesen::Fehlerhaft:
            if (++versuche > e_.max_wiederholungen)
                return fehler(Fehlerart::Zeitueberschreitung, "Server antwortet nicht", true);
            sendeRoh(letztes_);
            continue;
        case Lesen::Ok: break;
        }
        if (p.typ == 'Y' && p.seq == 0) {
            v_ = verhandle(mein, zerlegeInit(p.daten), e_.max_paket);
            return {};
        }
        if (p.typ == 'E') return {};   // Server kennt kein I: Vorgaben behalten
        if (p.typ == 'N') {
            if (++versuche > e_.max_wiederholungen)
                return fehler(Fehlerart::Zeitueberschreitung, "Server antwortet nicht", true);
            sendeRoh(letztes_);
        }
    }
}

Ergebnis Sitzung::befehl(char typ, const std::string& klartext, Ziel z) {
    abbruch_ = false;
    abbruchDatei_ = false;
    abbruchGruppe_ = false;
    if (e_.i_paket) {
        Ergebnis r = iAustausch();
        if (!r.ok()) return r;
    } else {
        leereEingang();
    }
    chkt_ = 1;
    if (!sende(typ, 0, kodiere(v_.kod, klartext))) return fehler(Fehlerart::LeitungGetrennt, "Leitung getrennt", false);
    int versuche = 0;
    for (;;) {
        Paket p;
        switch (lesePaket(p, wartefrist())) {
        case Lesen::Abbruch: return fehler(Fehlerart::Abgebrochen, "Abbruch durch den Anwender", true);
        case Lesen::Getrennt: return fehler(Fehlerart::LeitungGetrennt, "Leitung getrennt", false);
        case Lesen::Frist:
        case Lesen::Fehlerhaft:
            if (++versuche > e_.max_wiederholungen)
                return fehler(Fehlerart::Zeitueberschreitung, "Zu viele Wiederholungen", true);
            sendeRoh(letztes_);
            continue;
        case Lesen::Ok: break;
        }
        if (p.typ == 'E') return fehler(Fehlerart::Gegenstelle, dekodiereText(v_.kod, p.daten), false);
        if (p.typ == 'Y' && p.seq == 0) {
            Ergebnis r;
            r.ausgabe = dekodiereText(v_.kod, p.daten);
            return r;
        }
        if (p.typ == 'S') return empfangeTransaktion(p, z);
        if (p.typ == 'N' && p.seq == 0) {
            if (++versuche > e_.max_wiederholungen)
                return fehler(Fehlerart::Zeitueberschreitung, "Zu viele Wiederholungen", true);
            sendeRoh(letztes_);
        }
    }
}

Ergebnis Sitzung::generisch(char unter, const std::vector<std::string>& args) {
    // Wie C-Kermit setgen(): Felder nur, solange sie nicht leer sind.
    std::string k(1, unter);
    for (const auto& a : args) {
        if (a.empty()) break;
        k += tochar(static_cast<int>(std::min<std::size_t>(a.size(), 94)));
        k += a.substr(0, 94);
    }
    std::string aus;
    Ergebnis r = befehl('G', k, Ziel{nullptr, &aus});
    if (!aus.empty()) r.ausgabe = aus + r.ausgabe;
    return r;
}

Ergebnis Sitzung::holen(const std::string& entfernterName, const std::string& zielOrdner) {
    Freigabe ziel(std::filesystem::u8path(zielOrdner));
    if (!ziel.gueltig()) return fehler(Fehlerart::PfadAbgewiesen, "Zielordner fehlt: " + zielOrdner, false);
    std::string aus;
    Ergebnis r = befehl('R', entfernterName, Ziel{&ziel, &aus});
    if (!aus.empty()) r.ausgabe = aus + r.ausgabe;
    if (r.ok() && r.dateien.empty() && r.ausgabe.empty()) {
        r.art = Fehlerart::DateiNichtGefunden;
        r.text = "Keine Datei empfangen: " + entfernterName;
    }
    return r;
}

Ergebnis Sitzung::entferntVerzeichnis(const std::string& pfad) { return generisch('D', {pfad}); }
Ergebnis Sitzung::entferntWechseln(const std::string& pfad) { return generisch('C', {pfad}); }
Ergebnis Sitzung::entferntLoeschen(const std::string& name) { return generisch('E', {name}); }
Ergebnis Sitzung::entferntHilfe() { return generisch('H', {}); }
Ergebnis Sitzung::entferntBeenden() { return generisch('F', {}); }
Ergebnis Sitzung::entferntAbmelden() { return generisch('L', {}); }

void Sitzung::abbrechen() {
    abbruch_ = true;
    l_.wecken();
}

}  // namespace k1520::kermit
