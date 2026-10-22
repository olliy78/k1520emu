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
    // Die Frist laeuft ab dem letzten EMPFANGENEN Byte: solange Zeichen kommen, ist die
    // Gegenstelle am Werk (eine langsame Gegenstelle braucht fuer ein langes Paket laenger).
    int64_t ende = l_.jetztMs() + fristMs;
    zerleger_.setzeChkt(chkt_);
    for (;;) {
        if (abbruch_) return Lesen::Abbruch;
        while (pufPos_ < puffer_.size()) {
            const uint8_t b = puffer_[pufPos_++];
            tempoMessung(b);
            const auto r = zerleger_.zufuehren(b);
            if (r == PaketZerleger::Ergebnis::Paket) {
                p = zerleger_.paket();
                if (r_.paket) r_.paket(false, p.typ, p.seq, p.daten);
                if (messAktiv_) {
                    messAktiv_ = false;
                    const int64_t dauer = messEnde_ - messStart_;
                    // Rate nur aus Bytes, die NACH dem Puffer mit dem Paketanfang kamen:
                    // so kostet ein gebuendelt geliefertes Paket keine falsche (zu hohe) Rate.
                    if (messBytes_ >= 16 && dauer >= 5 && e_.baud > 0) {
                        messSummeBytes_ = 0.7 * messSummeBytes_ + messBytes_;
                        messSummeMs_ = 0.7 * messSummeMs_ + static_cast<double>(dauer);
                        const double rate = messSummeBytes_ * 1000.0 / messSummeMs_;   // Bytes/s Wirtszeit
                        tempoSchaetzung_ = std::clamp(rate / (e_.baud / 10.0), 0.01, 1000.0);
                    }
                }
                return Lesen::Ok;
            }
            if (r == PaketZerleger::Ergebnis::Fehlerhaft) {
                if (r_.paket) r_.paket(false, '?', -1, "");
                messAktiv_ = false;
                ++st_.fehlerhaft;
                return Lesen::Fehlerhaft;
            }
        }
        puffer_.clear();
        pufPos_ = 0;
        const int64_t rest = ende - l_.jetztMs();
        if (rest <= 0) {
            zerleger_.zuruecksetzen();
            messAktiv_ = false;
            ++st_.fristen;
            return Lesen::Frist;
        }
        uint8_t b[512];
        const int n = l_.lesen(b, sizeof b, rest);
        if (abbruch_) return Lesen::Abbruch;
        if (n < 0) return Lesen::Getrennt;
        if (n > 0) {
            pufZeit_ = l_.jetztMs();
            ++pufNr_;
            st_.bytes_empfangen += n;
            ende = std::max(ende, pufZeit_ + fristMs);
        }
        puffer_.assign(b, b + n);
    }
}

void Sitzung::tempoMessung(uint8_t b) {
    if (b == SOH) {
        messAktiv_ = true;
        messNr_ = pufNr_;
        messStart_ = messEnde_ = pufZeit_;
        messBytes_ = 0;
        return;
    }
    if (messAktiv_ && pufNr_ != messNr_) {
        ++messBytes_;
        messEnde_ = pufZeit_;
    }
}

void Sitzung::umlaufMessung(int64_t gesendetMs) {
    const double r = static_cast<double>(std::max<int64_t>(0, l_.jetztMs() - gesendetMs));
    srtt_ = srtt_ <= 0 ? r : 0.875 * srtt_ + 0.125 * r;
}

double Sitzung::tempo() const {
    if (e_.tempo > 0) return e_.tempo;
    if (tempoSchaetzung_ > 0) return tempoSchaetzung_;
    return 1.0;
}

void Sitzung::leereEingang() {
    // Nachlauf (Befund K7): C-Kermit 4.2 leert nach JEDEM guten Paket seinen Eingang (rpack:
    // ttflui).  Haben wir eben das B quittiert, liest es unser Y(B), leert den Eingang und
    // meldet sich dann mit seinem Leerlauf-N(0) — ein sofort hinterhergeschicktes I geriet in
    // dieses Leeren und ging verloren.  Deshalb auf das N warten (hoechstens nachlauf_ms in der
    // Zeit der Gegenstelle).
    if (nachlauf_ && e_.nachlauf_ms > 0) {
        nachlauf_ = false;
        const int64_t ende = l_.jetztMs() + static_cast<int64_t>(e_.nachlauf_ms / tempo());
        for (;;) {
            const int64_t rest = ende - l_.jetztMs();
            if (rest <= 0) break;
            Paket p;
            const Lesen r = lesePaket(p, rest);
            if (r == Lesen::Getrennt || r == Lesen::Abbruch || r == Lesen::Frist) break;
            if (r == Lesen::Ok && p.typ == 'N') break;
        }
    }
    nachlauf_ = false;
    puffer_.clear();
    pufPos_ = 0;
    zerleger_.zuruecksetzen();
    uint8_t b[512];
    while (l_.lesen(b, sizeof b, 0) > 0) {
    }
}

bool Sitzung::sendeRoh(const std::string& rahmen) {
    ++st_.pakete_gesendet;
    if (r_.paket) {
        const std::size_t i = rahmen.find(static_cast<char>(SOH));
        if (i != std::string::npos && i + 3 < rahmen.size()) {
            const bool lang = unchar(rahmen[i + 1]) == 0;
            const std::size_t d0 = i + (lang ? 7 : 4);
            const int len = unchar(rahmen[i + 1]);
            const std::size_t chkl = static_cast<std::size_t>(chktFuerTyp(rahmen[i + 3], chkt_));
            std::string daten;
            if (!lang && len >= 2 && d0 <= rahmen.size()) daten = rahmen.substr(d0, std::max(0, len - 2 - static_cast<int>(chkl)));
            r_.paket(true, rahmen[i + 3], unchar(rahmen[i + 2]), daten);
        }
    }
    st_.bytes_gesendet += static_cast<int64_t>(rahmen.size());
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
    if (e_.frist_fest) return e_.frist_ms;
    const double t = tempo();
    // Das Tempo VERLAENGERT die Frist bei einer langsamen Gegenstelle (frist_ms gilt in ihrer
    // Zeit), verkuerzt sie aber nicht bei einer schnellen: die heilt mit ihrer eigenen Frist
    // (TIME in ihrer Zeit) ohnehin schneller, und gleich lange Fristen auf beiden Seiten
    // kreuzen ihre Wiederholungen (gemessen: 122 statt 0 Wiederholungen, test_kermit_tempo).
    const double basis = e_.frist_ms / std::min(t, 1.0);
    double f = basis;
    if (srtt_ > 0) f = std::max(f, 4.0 * srtt_);
    // TIME der Gegenstelle (in IHRER Zeit) ist nur eine Obergrenze — fuer den Zuschlag aus der
    // Umlaufzeit; laenger als sie selbst wartet, bringt das Warten nichts.
    if (v_.time > 0) f = std::min(f, std::max(v_.time * 1000.0 / t, basis));
    const double lo = std::max(1, e_.frist_min_ms), hi = std::max(e_.frist_min_ms, e_.frist_max_ms);
    return static_cast<int64_t>(std::clamp(f, lo, hi));
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
    const int64_t gesendet = l_.jetztMs();
    if (!sende(typ, seq, daten)) {
        a.art = Fehlerart::LeitungGetrennt;
        a.text = "Leitung getrennt";
        return a;
    }
    int versuche = 0, naks0 = 0;
    auto nochmal = [&](bool wiederholen) {
        if (++versuche > e_.max_wiederholungen) {
            a.art = Fehlerart::Zeitueberschreitung;
            a.text = "Zu viele Wiederholungen";
            return false;
        }
        if (wiederholen) {
            ++st_.wiederholungen;
            sendeRoh(letztes_);
        }
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
            if (versuche == 0) umlaufMessung(gesendet);   // nur ohne Wiederholung (Karn)
            a.paket = p;
            return a;
        }
        if (p.typ == 'N') ++st_.naks_empfangen;
        // NAK fuer das naechste = Quittung — nur nicht fuer S/I: deren Quittung traegt die
        // Parameter der Gegenstelle; ein leeres Ersatz-Y stellte die Blockpruefung auf 1, waehrend
        // die Gegenstelle schon die ausgehandelte faehrt (Befund K7: endlos „beschaedigt").
        if (p.typ == 'N' && p.seq == ((seq + 1) & 63) && typ != 'S' && typ != 'I') {
            a.paket = Paket{'Y', seq & 63, ""};
            return a;
        }
        if (p.typ == typ && p.seq == (seq & 63)) continue;   // Echo der Leitung
        // NAK oder ein wiederholtes Paket der Gegenstelle (z. B. ihr R/G, auf das unser S die
        // Antwort war): unseres kam nicht an — wiederholen.  Eine alte Quittung: weiter warten.
        // Ohne das wartete ein Sender mit der laengeren Frist ewig auf die Quittung, waehrend die
        // Gegenstelle (kuerzere Frist) ihr Paket wiederholt (K7: KermitVerlust).
        // Ebenso eine Quittung fuer das VORIGE Paket: C-Kermit 4.2 beantwortet ein beschaedigtes
        // Paket mit der Wiederholung seiner letzten Quittung statt mit einem NAK (Befund K7: elf Y1
        // auf ein verstuemmeltes D2, der Sender wartete bis „Zu viele Wiederholungen").
        const bool vorige = p.typ == 'Y' && p.seq == ((seq + 63) & 63);
        // Ausnahme N(0) auf unser S(0)/I(0): das ist meist der NAK, mit dem ein C-Kermit-Server bei
        // jedem Wiedereintritt in die Serverschleife von sich aus wartet (tinit → nack), nicht die
        // Antwort auf unser Paket.  Darauf zu wiederholen, ergibt ein doppeltes Y(0), das wir
        // spaeter als Antwort auf das NAECHSTE Paket missdeuten (Befund K7: G D lieferte die
        // Init-Parameter).  Ist unser Paket wirklich verloren, wiederholt die eigene Frist.
        const bool leerlaufNak =
            p.typ == 'N' && p.seq == 0 && (seq & 63) == 0 && (typ == 'S' || typ == 'I') && ++naks0 < 2;
        if (!nochmal((p.typ != 'Y' || vorige) && !leerlaufNak)) return a;
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
        ++st_.naks_gesendet;
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
            ++st_.wiederholungen;
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
    leereEingang();   // Nachlauf abwarten wie vor I/R/G (sonst verschluckt C-Kermit unser S)
    return sendeTransaktion(qs);
}

// ─── Empfangen ─────────────────────────────────────────────────────────────

Ergebnis Sitzung::empfangeTransaktion(const Paket& s, Ziel z) {
    const InitParameter dein = zerlegeInit(s.daten);
    const InitParameter mein = antwortParameter(meineParameter(), dein);
    // Das Leerlauf-N(0) nach dem B kommt vom ttflui/rpack des C-Kermit 4.2, der GENAU 9 Init-Felder
    // schickt.  Meldet die Gegenstelle CAPAS (> 9 Felder) — etwa unser eigener Server —, ist sie keine
    // solche und schickt kein N: dann kostete das Warten nur 2 s je Befehl.
    nachlaufNoetig_ = dein.felder <= 9;
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
            nachlauf_ = nachlaufNoetig_;
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
        switch (lesePaket(p, wartefrist())) {
        case Lesen::Abbruch: return fehler(Fehlerart::Abgebrochen, "Abbruch durch den Anwender", false);
        case Lesen::Getrennt: return fehler(Fehlerart::LeitungGetrennt, "Leitung getrennt", false);
        case Lesen::Frist:
        case Lesen::Fehlerhaft:
            if (++versuche > e_.max_wiederholungen)
                return fehler(Fehlerart::Zeitueberschreitung, "Kein Send-Init empfangen", false);
            ++st_.naks_gesendet;
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
    v_.time = 0;   // TIME der vorigen Transaktion gilt nicht mehr
    const int64_t gesendet = l_.jetztMs();
    if (!sende('I', 0, initDaten(mein, e_.capas_senden)))
        return fehler(Fehlerart::LeitungGetrennt, "Leitung getrennt", false);
    int versuche = 0, naks0 = 0;
    bool altAbgebrochen = false;
    for (;;) {
        Paket p;
        switch (lesePaket(p, wartefrist())) {
        case Lesen::Abbruch: return fehler(Fehlerart::Abgebrochen, "Abbruch durch den Anwender", false);
        case Lesen::Getrennt: return fehler(Fehlerart::LeitungGetrennt, "Leitung getrennt", false);
        case Lesen::Frist:
        case Lesen::Fehlerhaft:
            if (++versuche > e_.max_wiederholungen)
                return fehler(Fehlerart::Zeitueberschreitung, "Server antwortet nicht", true);
            ++st_.wiederholungen;
            sendeRoh(letztes_);
            continue;
        case Lesen::Ok: break;
        }
        if (p.typ == 'Y' && p.seq == 0) {
            if (versuche == 0) umlaufMessung(gesendet);
            v_ = verhandle(mein, zerlegeInit(p.daten), e_.max_paket);
            return {};
        }
        if (p.typ == 'E') {
            if (altAbgebrochen) continue;   // Quittung unseres E, falls der Server sie schickt
            return {};                      // Server kennt kein I: Vorgaben behalten
        }
        if (p.seq != 0) {
            // Paket einer ALTEN Transaktion (Befund K7): nach einer Trennung mitten in der Datei
            // steckt der C-Kermit-Server noch in ihr und NAKt das naechste Datenpaket (N25) —
            // ohne Eingriff bis zu MAXTRY × (TIME + Leerpause), ueber 120 s Maschinenzeit.  Ein
            // E-Paket bricht sie ab; danach meldet er sich mit N(0), und das I wird wiederholt.
            if (++versuche > e_.max_wiederholungen)
                return fehler(Fehlerart::Zeitueberschreitung, "Server antwortet nicht", true);
            // Mit der Blockpruefung der alten Transaktion — die steht in der Laenge ihres NAK; ein E
            // mit Pruefung 1 verwirft der Server dort als beschaedigt (gemessen).
            sendeRoh(baueRahmen(Paket{'E', p.seq, kodiere(v_.kod, "Neue Sitzung")}, p.chkl > 0 ? p.chkl : 1, v_.rahmen));
            if (r_.ereignis) r_.ereignis("Alte Transaktion der Gegenstelle abgebrochen (Paket " + std::to_string(p.seq) + ")");
            altAbgebrochen = true;
            continue;
        }
        if (p.typ == 'N') {   // Leerlauf-NAK: das erste nicht beantworten, ab dem zweiten wiederholen
            ++st_.naks_empfangen;
            if (++versuche > e_.max_wiederholungen)
                return fehler(Fehlerart::Zeitueberschreitung, "Server antwortet nicht", true);
            if (++naks0 >= 2 || altAbgebrochen) {
                altAbgebrochen = false;
                ++st_.wiederholungen;
                sendeRoh(letztes_);
            }
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
    const int64_t gesendet = l_.jetztMs();
    if (!sende(typ, 0, kodiere(v_.kod, klartext))) return fehler(Fehlerart::LeitungGetrennt, "Leitung getrennt", false);
    int versuche = 0, naks0 = 0;
    for (;;) {
        Paket p;
        switch (lesePaket(p, wartefrist())) {
        case Lesen::Abbruch: return fehler(Fehlerart::Abgebrochen, "Abbruch durch den Anwender", true);
        case Lesen::Getrennt: return fehler(Fehlerart::LeitungGetrennt, "Leitung getrennt", false);
        case Lesen::Frist:
        case Lesen::Fehlerhaft:
            if (++versuche > e_.max_wiederholungen)
                return fehler(Fehlerart::Zeitueberschreitung, "Zu viele Wiederholungen", true);
            ++st_.wiederholungen;
            sendeRoh(letztes_);
            continue;
        case Lesen::Ok: break;
        }
        if (versuche == 0 && p.typ != 'N') umlaufMessung(gesendet);
        if (p.typ == 'E') return fehler(Fehlerart::Gegenstelle, dekodiereText(v_.kod, p.daten), false);
        if (p.typ == 'Y' && p.seq == 0) {
            Ergebnis r;
            r.ausgabe = dekodiereText(v_.kod, p.daten);
            return r;
        }
        if (p.typ == 'S') return empfangeTransaktion(p, z);
        if (p.typ == 'N' && p.seq == 0) {   // Leerlauf-NAK: das erste nicht beantworten, ab dem zweiten wiederholen
            ++st_.naks_empfangen;
            if (++versuche > e_.max_wiederholungen)
                return fehler(Fehlerart::Zeitueberschreitung, "Zu viele Wiederholungen", true);
            if (++naks0 >= 2) {
                ++st_.wiederholungen;
                sendeRoh(letztes_);
            }
        }
    }
}

Ergebnis Sitzung::generisch(char unter, const std::vector<std::string>& args) {
    // Wie C-Kermit setgen(): Felder nur, solange sie nicht leer sind.
    std::string k(1, unter);
    // `G C` ohne Pfad: ein Feld der Laenge 0 statt gar keines.  C-Kermit 4.2 (ckfns.c cwd()) liest
    // die Feldlaenge ungeprueft (`vdir[unchar(*vdir)+1] = 0`) — ohne Feld ist das unchar('\0') = -32,
    // ein Schreibzugriff VOR den Befehlspuffer.  Mit Laenge 0 wechselt es nach $HOME (zchdir("")).
    if (unter == 'C' && (args.empty() || args[0].empty())) return befehlG(k + tochar(0));
    for (const auto& a : args) {
        if (a.empty()) break;
        k += tochar(static_cast<int>(std::min<std::size_t>(a.size(), 94)));
        k += a.substr(0, 94);
    }
    return befehlG(k);
}

Ergebnis Sitzung::befehlG(const std::string& k) {
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
