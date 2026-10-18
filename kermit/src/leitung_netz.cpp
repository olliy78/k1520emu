/**
 * @file leitung_netz.cpp
 * @brief Telnet-/RFC-2217-Leitungen ueber die Emulator-Codecs — siehe leitung_netz.h.
 */
#include "kermit/src/leitung_netz.h"

#include <algorithm>
#include <chrono>
#include <cstring>

#include "core/serial/net/socket.h"
#include "core/serial/rfc2217_codec.h"
#include "core/serial/telnet_codec.h"

namespace k1520::kermit {

namespace net = k1520::serial::net;
namespace r2 = ::serial::rfc2217;
using ::serial::Rfc2217Codec;
using ::serial::Rfc2217Ereignis;
using ::serial::TelnetCodec;
using ::serial::TelnetRolle;
namespace tn = ::serial::telnet;

namespace {
int64_t jetzt() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}
std::string fehlerText(const net::Fehler& f, const char* was) {
    return std::string(was) + ": " + (f.text.empty() ? "unbekannter Fehler" : f.text);
}
}  // namespace

NetzLeitung::NetzLeitung(Art art, const NetzOptionen& o) : art_(art), opt_(o) {}

NetzLeitung::~NetzLeitung() = default;

// ─── Codec-Weiche ────────────────────────────────────────────────────────────

void NetzLeitung::cEingabe(const uint8_t* d, std::size_t n) {
    if (r2_) r2_->eingabe(d, n); else tel_->eingabe(d, n);
}
std::vector<uint8_t> NetzLeitung::cNutz() { return r2_ ? r2_->nimmNutzdaten() : tel_->nimmNutzdaten(); }
void NetzLeitung::cSende(const uint8_t* d, std::size_t n) {
    if (r2_) r2_->sende(d, n); else tel_->sende(d, n);
}
std::vector<uint8_t> NetzLeitung::cAusgabe() { return r2_ ? r2_->nimmAusgabe() : tel_->nimmAusgabe(); }

bool NetzLeitung::binaerAktiv() const {
    const TelnetCodec* t = r2_ ? &r2_->telnet() : tel_.get();
    return t && t->lokalAktiv(tn::OPT_BINARY) && t->entferntAktiv(tn::OPT_BINARY);
}

// ─── Aufbau ─────────────────────────────────────────────────────────────────

void NetzLeitung::starteCodec(bool alsClient) {
    const TelnetRolle rolle = alsClient ? TelnetRolle::Client : TelnetRolle::Server;
    if (art_ == Art::Rfc2217) {
        r2_ = std::make_unique<Rfc2217Codec>(rolle);
        r2_->start();
        if (alsClient) {
            // Wunsch senden; der Gast (Gegenstelle) ist massgeblich — seine Antwort
            // landet als Ereignis in `ereignisse()`.
            r2_->sendeBaud(static_cast<uint32_t>(opt_.baud));
            r2_->sendeDatenbits(static_cast<uint8_t>(opt_.datenbits));
            const uint8_t sio = opt_.paritaet == 'O' ? 1 : opt_.paritaet == 'E' ? 2 : 0;
            r2_->sendeParitaet(r2::paritaetNetz(sio));
            r2_->sendeStoppbits(r2::stoppNetz(static_cast<uint8_t>(opt_.stoppbits == 2 ? 4 : 2)));
        } else {
            // Wie der Hub: Anfangszustand der Modemleitungen "bereit" (CTS|DSR|CD).
            r2_->sendeModemState(r2::MS_CTS | r2::MS_DSR | r2::MS_CD, true);
        }
    } else {
        tel_ = std::make_unique<TelnetCodec>(rolle);
        tel_->start();
        if (alsClient) {
            // Ein Client wartet sonst auf den Server; wer BINARY nicht von sich aus
            // anbietet, bekommt es von einem schlichten Server nie.
            tel_->bieteAn(tn::OPT_BINARY);
            tel_->verlange(tn::OPT_BINARY);
        }
    }
    flush();
}

bool NetzLeitung::aushandeln() {
    const int64_t ende = jetzt() + opt_.aushandeln_ms;
    while (!binaerAktiv()) {
        const int64_t rest = ende - jetzt();
        if (rest <= 0) break;
        const Pump p = pumpe(std::min<int64_t>(rest, 50));
        if (p == Pump::Getrennt || p == Pump::Geweckt) return !getrennt_;
    }
    if (!binaerAktiv()) {
        hinweis_ = "Telnet-BINARY nicht ausgehandelt (Gegenstelle antwortet nicht) - CR wird als CR NUL gesendet";
    }
    return true;
}

bool NetzLeitung::verbindeIntern(const std::string& host, int port, int fristMs, std::string* fehler) {
    net::Fehler f;
    if (!net::netzStarten(&f)) {
        if (fehler) *fehler = fehlerText(f, "Netz");
        return false;
    }
    if (!wecker_.gueltig()) {
        if (fehler) *fehler = "Weck-Socketpaar nicht anlegbar";
        return false;
    }
    auto ziele = net::aufloesen(host, port, &f);
    if (ziele.empty()) {
        if (fehler) *fehler = fehlerText(f, ("Aufloesen von " + host).c_str());
        return false;
    }
    sock_ = net::verbindenAlle(ziele, fristMs, &f);
    if (!sock_.gueltig()) {
        if (fehler) *fehler = fehlerText(f, ("Verbinden mit " + host + ":" + std::to_string(port)).c_str());
        return false;
    }
    gegenstelle_ = net::gegenstelle(sock_.fd());
    if (gegenstelle_.empty()) gegenstelle_ = host + ":" + std::to_string(port);
    verbunden_ = true;
    starteCodec(true);
    if (!aushandeln() && fehler) *fehler = "Leitung beim Aushandeln getrennt";
    return !getrennt_;
}

bool NetzLeitung::lauscheIntern(int port, std::string* fehler) {
    net::Fehler f;
    if (!net::netzStarten(&f)) {
        if (fehler) *fehler = fehlerText(f, "Netz");
        return false;
    }
    if (!wecker_.gueltig()) {
        if (fehler) *fehler = "Weck-Socketpaar nicht anlegbar";
        return false;
    }
    net::Lauscher l = net::lauschen(port);
    if (!l.sock.gueltig()) {
        if (fehler) *fehler = fehlerText(l.fehler, ("Lauschen auf Port " + std::to_string(port)).c_str());
        return false;
    }
    port_ = l.port;
    lauscher_ = std::move(l.sock);
    return true;
}

bool NetzLeitung::annehmen(int fristMs, std::string* fehler) {
    if (!lauscher_.gueltig()) {
        if (fehler) *fehler = verbunden_ ? "schon verbunden" : "kein Lauscher";
        return false;
    }
    const int64_t ende = jetzt() + std::max(0, fristMs);
    for (;;) {
        if (geweckt_.exchange(false)) {
            wecker_.leeren();
            if (fehler) *fehler = "abgebrochen";
            return false;
        }
        const int64_t rest = ende - jetzt();
        if (rest < 0) {
            if (fehler) *fehler = "keine Verbindung innerhalb der Frist";
            return false;
        }
        std::vector<net::PollEintrag> pe(2);
        pe[0].fd = lauscher_.fd();
        pe[0].lesen = true;
        pe[1].fd = wecker_.lesefd();
        pe[1].lesen = true;
        net::warten(pe, static_cast<int>(std::min<int64_t>(rest, 200)));
        if (pe[1].lesbar) wecker_.leeren();
        if (pe[0].lesbar) {
            net::Fehler f;
            net::Socket s = net::annehmen(lauscher_.fd(), &f);
            if (!s.gueltig()) continue;   // abgesprungene Verbindung
            sock_ = std::move(s);
            lauscher_.schliessen();   // genau EINE Verbindung
            gegenstelle_ = net::gegenstelle(sock_.fd());
            verbunden_ = true;
            starteCodec(false);
            if (!aushandeln() && fehler) *fehler = "Leitung beim Aushandeln getrennt";
            return !getrennt_;
        }
    }
}

// ─── Schreiben ──────────────────────────────────────────────────────────────

bool NetzLeitung::schreibeRoh(const std::vector<uint8_t>& v) {
    std::size_t pos = 0;
    const int64_t ende = jetzt() + opt_.schreibfrist_ms;
    while (pos < v.size()) {
        const net::IoErgebnis r = net::senden(sock_.fd(), v.data() + pos, v.size() - pos);
        if (r.status == net::IoStatus::Ok) {
            pos += r.n;
            continue;
        }
        if (r.status != net::IoStatus::Warten || jetzt() > ende) {
            getrennt_ = true;
            return false;
        }
        std::vector<net::PollEintrag> pe(1);
        pe[0].fd = sock_.fd();
        pe[0].schreiben = true;
        net::warten(pe, 100);
    }
    return true;
}

bool NetzLeitung::flush() {
    if (!sock_.gueltig()) return false;
    const std::vector<uint8_t> aus = cAusgabe();
    if (aus.empty()) return true;
    return schreibeRoh(aus);
}

bool NetzLeitung::schreiben(const uint8_t* buf, std::size_t n) {
    if (getrennt_ || !sock_.gueltig()) return false;
    cSende(buf, n);
    return flush();
}

// ─── Lesen ──────────────────────────────────────────────────────────────────

void NetzLeitung::ereignisse() {
    if (!r2_) return;
    for (const Rfc2217Ereignis& ev : r2_->nimmEreignisse()) {
        using A = Rfc2217Ereignis::Art;
        if (r2_->rolle() == TelnetRolle::Client) {
            if (ev.art == A::Baud && ev.antwort && ev.wert) gegenBaud_ = ev.wert;
            continue;
        }
        // Server: antworten, nichts umstellen (die Leitung hat feste Werte).
        if (ev.antwort) continue;
        const uint8_t sio = opt_.paritaet == 'O' ? 1 : opt_.paritaet == 'E' ? 2 : 0;
        switch (ev.art) {
            case A::Signatur: r2_->sendeSignatur(opt_.signatur, true); break;
            case A::Baud: r2_->sendeBaud(static_cast<uint32_t>(opt_.baud), true); break;
            case A::Datenbits: r2_->sendeDatenbits(static_cast<uint8_t>(opt_.datenbits), true); break;
            case A::Paritaet: r2_->sendeParitaet(r2::paritaetNetz(sio), true); break;
            case A::Stoppbits:
                r2_->sendeStoppbits(r2::stoppNetz(static_cast<uint8_t>(opt_.stoppbits == 2 ? 4 : 2)), true);
                break;
            case A::Steuerung: r2_->sendeSteuerung(static_cast<uint8_t>(ev.wert), true); break;
            case A::Leeren: r2_->sendeLeeren(static_cast<uint8_t>(ev.wert), true); break;
            case A::LineStateMaske: r2_->sendeLineStateMaske(static_cast<uint8_t>(ev.wert), true); break;
            case A::ModemStateMaske: r2_->sendeModemStateMaske(static_cast<uint8_t>(ev.wert), true); break;
            default: break;   // FLOWCONTROL, NOTIFY: nichts zu tun
        }
    }
}

NetzLeitung::Pump NetzLeitung::pumpe(int64_t ms) {
    if (getrennt_ || !sock_.gueltig()) return Pump::Getrennt;
    std::vector<net::PollEintrag> pe(2);
    pe[0].fd = sock_.fd();
    pe[0].lesen = true;
    pe[1].fd = wecker_.lesefd();
    pe[1].lesen = true;
    net::warten(pe, static_cast<int>(std::max<int64_t>(0, std::min<int64_t>(ms, 1000000))));
    Pump ergebnis = Pump::Nichts;
    if (pe[1].lesbar) {
        wecker_.leeren();
        ergebnis = Pump::Geweckt;
    }
    if (pe[0].lesbar || pe[0].fehler) {
        uint8_t buf[4096];
        bool weiter = true;
        // Alles abholen, was da ist (nicht blockierend) — so bleibt kein Rest im Kernel liegen,
        // wenn gleich danach die Gegenseite schliesst.
        while (weiter) {
            const net::IoErgebnis r = net::empfangen(sock_.fd(), buf, sizeof buf);
            switch (r.status) {
                case net::IoStatus::Ok:
                    cEingabe(buf, r.n);
                    ergebnis = Pump::Daten;
                    break;
                case net::IoStatus::Warten: weiter = false; break;
                case net::IoStatus::Geschlossen:
                case net::IoStatus::Fehler:
                    getrennt_ = true;
                    weiter = false;
                    break;
            }
        }
        ereignisse();
        const std::vector<uint8_t> nutz = cNutz();
        if (!nutz.empty()) {
            if (eingangPos_ >= eingang_.size()) {
                eingang_.clear();
                eingangPos_ = 0;
            }
            eingang_.insert(eingang_.end(), nutz.begin(), nutz.end());
        }
        if (!getrennt_) flush();   // Verhandlungsantworten
    }
    if (getrennt_ && ergebnis != Pump::Daten) return Pump::Getrennt;
    return ergebnis;
}

int NetzLeitung::lesen(uint8_t* buf, std::size_t n, int64_t fristMs) {
    if (n == 0) return 0;
    const int64_t ende = jetzt() + std::max<int64_t>(0, fristMs);
    for (;;) {
        if (eingangPos_ < eingang_.size()) {
            const std::size_t k = std::min(n, eingang_.size() - eingangPos_);
            std::memcpy(buf, eingang_.data() + eingangPos_, k);
            eingangPos_ += k;
            return static_cast<int>(k);
        }
        if (geweckt_.exchange(false)) return 0;
        if (getrennt_) return -1;
        const int64_t rest = ende - jetzt();
        const Pump p = pumpe(std::max<int64_t>(0, rest));
        if (eingangPos_ < eingang_.size()) continue;
        if (p == Pump::Getrennt) return -1;
        if (p == Pump::Geweckt) {
            if (geweckt_.exchange(false)) return 0;
            continue;
        }
        if (rest <= 0) return 0;
    }
}

void NetzLeitung::wecken() {
    geweckt_ = true;
    wecker_.wecken();
}

void NetzLeitung::schliessen() {
    sock_.schliessen();
    lauscher_.schliessen();
    getrennt_ = true;
}

// ─── Fabriken ───────────────────────────────────────────────────────────────

std::unique_ptr<TelnetLeitung> TelnetLeitung::verbinden(const std::string& host, int port, int fristMs,
                                                        const NetzOptionen& o, std::string* fehler) {
    std::unique_ptr<TelnetLeitung> l(new TelnetLeitung(o));
    if (!l->verbindeIntern(host, port, fristMs, fehler)) return nullptr;
    return l;
}
std::unique_ptr<TelnetLeitung> TelnetLeitung::lauschen(int port, const NetzOptionen& o, std::string* fehler) {
    std::unique_ptr<TelnetLeitung> l(new TelnetLeitung(o));
    if (!l->lauscheIntern(port, fehler)) return nullptr;
    return l;
}
std::unique_ptr<Rfc2217Leitung> Rfc2217Leitung::verbinden(const std::string& host, int port, int fristMs,
                                                          const NetzOptionen& o, std::string* fehler) {
    std::unique_ptr<Rfc2217Leitung> l(new Rfc2217Leitung(o));
    if (!l->verbindeIntern(host, port, fristMs, fehler)) return nullptr;
    return l;
}
std::unique_ptr<Rfc2217Leitung> Rfc2217Leitung::lauschen(int port, const NetzOptionen& o, std::string* fehler) {
    std::unique_ptr<Rfc2217Leitung> l(new Rfc2217Leitung(o));
    if (!l->lauscheIntern(port, fehler)) return nullptr;
    return l;
}

}  // namespace k1520::kermit
