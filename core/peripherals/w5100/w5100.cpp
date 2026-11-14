#include "core/peripherals/w5100/w5100.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>

namespace k1520::w5100 {

namespace net = k1520::serial::net;

namespace {
constexpr std::size_t kRegGroesse = 0x0800;
constexpr std::size_t kSpeicher   = 0x2000;   // TX und RX je 8 KB

bool istLoopback(uint32_t ip) { return (ip >> 24) == 127; }
} // namespace

W5100::W5100() : W5100(Richtlinie()) {}

W5100::W5100(Richtlinie r)
    : reg_(kRegGroesse, 0), tx_(kSpeicher, 0), rx_(kSpeicher, 0), richtlinie_(std::move(r)) {
    zuruecksetzen();
    faden_ = std::thread([this] { faden(); });
}

W5100::~W5100() {
    {
        std::lock_guard<std::mutex> l(m_);
        ende_ = true;
    }
    cv_.notify_all();
    if (faden_.joinable()) faden_.join();
}

// Puffergroessen: je Socket 2 Bit in RMSR/TMSR → 1/2/4/8 KB; die Summe darf 8 KB nicht
// uebersteigen — was darueber hinausragt, bekommt Groesse 0 [?] (Datenblatt: unzulaessig).
std::pair<std::size_t, std::size_t> W5100::puffer(int n, uint8_t msr) {
    std::size_t basis = 0;
    for (int i = 0; i < n; ++i) basis += (std::size_t(1) << ((msr >> (2 * i)) & 3)) * 1024;
    std::size_t gr = (std::size_t(1) << ((msr >> (2 * n)) & 3)) * 1024;
    if (basis + gr > kSpeicher) gr = 0;
    return {basis, gr};
}

uint16_t W5100::txFsr(int n) {
    const uint16_t belegt = static_cast<uint16_t>(r16(n, reg::Sn_TX_WR) - r16(n, reg::Sn_TX_RD));
    const std::size_t g = txGroesse(n);
    return belegt >= g ? 0 : static_cast<uint16_t>(g - belegt);
}

uint16_t W5100::rxRsr(int n) {
    return static_cast<uint16_t>(s_[n].rxWr - r16(n, reg::Sn_RX_RD));
}

void W5100::zuruecksetzen() {
    for (int n = 0; n < 4; ++n) schliesse(n);
    std::fill(reg_.begin(), reg_.end(), 0);
    reg_[reg::RTR] = 0x07; reg_[reg::RTR + 1] = 0xD0;   // 200 ms in 100-us-Einheiten
    reg_[reg::RCR] = 0x08;
    reg_[reg::RMSR] = 0x55;                              // 2 KB je Socket
    reg_[reg::TMSR] = 0x55;
    for (int n = 0; n < 4; ++n) {
        r(n, reg::Sn_TTL) = 0x80;
        s_[n].rxWr = 0;
        s_[n].wirtsPort = 0;
    }
}

void W5100::schliesse(int n) {
    Sock& s = s_[n];
    s.fd.schliessen();
    s.lausch.schliessen();
    s.brauchtBind = s.brauchtVerbinden = s.sendeAuftrag = false;
    s.udpPolitik = 0;
    s.wirtsPort = 0;
    ++s.gen;
}

// ── Gast-Seite ───────────────────────────────────────────────────────────────

uint8_t W5100::lese(uint16_t adr) {
    std::lock_guard<std::mutex> l(m_);
    adr &= 0x7FFF;
    if (adr == reg::IR) {
        uint8_t v = reg_[reg::IR] & 0xF0;
        for (int n = 0; n < 4; ++n) if (r(n, reg::Sn_IR)) v |= (1 << n);
        return v;
    }
    if (adr < kRegGroesse) {
        if (adr >= reg::SOCK_BASIS) {
            const int n = (adr - reg::SOCK_BASIS) >> 8;
            const uint16_t off = adr & 0xFF;
            if (n > 3) return 0;
            switch (off) {
            case reg::Sn_TX_FSR:     return txFsr(n) >> 8;
            case reg::Sn_TX_FSR + 1: return txFsr(n) & 0xFF;
            case reg::Sn_RX_RSR:     return rxRsr(n) >> 8;
            case reg::Sn_RX_RSR + 1: return rxRsr(n) & 0xFF;
            default: break;
            }
        }
        return reg_[adr];
    }
    if (adr >= reg::TX_SPEICHER && adr < reg::RX_SPEICHER) return tx_[adr - reg::TX_SPEICHER];
    if (adr >= reg::RX_SPEICHER) return rx_[adr - reg::RX_SPEICHER];
    return 0;   // 0800h-3FFFh nicht belegt
}

void W5100::schreibe(uint16_t adr, uint8_t d) {
    std::lock_guard<std::mutex> l(m_);
    adr &= 0x7FFF;
    if (adr < kRegGroesse) {
        if (adr == reg::MR) {
            if (d & reg::MR_RESET) { zuruecksetzen(); return; }   // Reset-Bit loescht sich selbst
            reg_[adr] = d;
            return;
        }
        if (adr == reg::IR) { reg_[reg::IR] &= ~d; return; }      // 1 loescht (obere Bits; untere abgeleitet)
        if (adr < reg::SOCK_BASIS) {
            // UIPR/UPORT (2Ah-2Fh) sind schreibgeschuetzt
            if (adr >= 0x2A && adr <= 0x2F) return;
            reg_[adr] = d;
            return;
        }
        const int n = (adr - reg::SOCK_BASIS) >> 8;
        const uint16_t off = adr & 0xFF;
        if (n > 3) return;
        switch (off) {
        case reg::Sn_CR: befehl(n, d); return;                    // Befehl wird sofort angenommen, CR liest 0
        case reg::Sn_IR: r(n, reg::Sn_IR) &= ~d; return;
        case reg::Sn_SR: return;
        case reg::Sn_TX_FSR: case reg::Sn_TX_FSR + 1:
        case reg::Sn_TX_RD:  case reg::Sn_TX_RD + 1:
        case reg::Sn_RX_RSR: case reg::Sn_RX_RSR + 1:
            return;   // nur lesbar
        default: reg_[adr] = d; return;
        }
    }
    if (adr >= reg::TX_SPEICHER && adr < reg::RX_SPEICHER) tx_[adr - reg::TX_SPEICHER] = d;
    // RX-Speicher ist fuer den Gast nur lesbar (am Chip schreibbar, aber ohne Sinn)
}

uint16_t W5100::lese16(uint16_t adr) { return static_cast<uint16_t>((lese(adr) << 8) | lese(adr + 1)); }
void W5100::schreibe16(uint16_t adr, uint16_t w) { schreibe(adr, w >> 8); schreibe(adr + 1, w & 0xFF); }

void W5100::leseBlock(uint16_t adr, uint8_t* ziel, std::size_t n) {
    for (std::size_t i = 0; i < n; ++i) ziel[i] = lese(static_cast<uint16_t>(adr + i));
}
void W5100::schreibeBlock(uint16_t adr, const uint8_t* q, std::size_t n) {
    for (std::size_t i = 0; i < n; ++i) schreibe(static_cast<uint16_t>(adr + i), q[i]);
}

bool W5100::interrupt() {
    std::lock_guard<std::mutex> l(m_);
    uint8_t v = reg_[reg::IR] & 0xF0;
    for (int n = 0; n < 4; ++n) if (r(n, reg::Sn_IR)) v |= (1 << n);
    return (v & reg_[reg::IMR]) != 0;
}

bool W5100::zielErlaubt(uint32_t ip, uint16_t port) {
    Richtlinie rl;
    { std::lock_guard<std::mutex> l(m_); rl = richtlinie_; }
    if (rl.frage) return rl.frage(ip, port);
    return rl.aussenErlaubt || istLoopback(ip);
}

void W5100::setzeRichtlinie(const Richtlinie& rl) {
    std::lock_guard<std::mutex> l(m_);
    richtlinie_ = rl;
}

int W5100::wirtsPort(int n) {
    std::lock_guard<std::mutex> l(m_);
    return (n >= 0 && n < 4) ? s_[n].wirtsPort : 0;
}

// ── Befehle (Gast-Faden, Sperre gehalten; kein Netz) ────────────────────────────

void W5100::befehl(int n, uint8_t cmd) {
    Sock& s = s_[n];
    uint8_t& sr = r(n, reg::Sn_SR);
    switch (cmd) {
    case reg::CMD_OPEN: {
        schliesse(n);
        const uint8_t proto = r(n, reg::Sn_MR) & 0x0F;
        // Zeiger beginnen bei OPEN neu [?]; Treiber lesen sie ohnehin nach dem OPEN.
        setR16(n, reg::Sn_TX_RD, 0); setR16(n, reg::Sn_TX_WR, 0); setR16(n, reg::Sn_RX_RD, 0);
        s.rxWr = 0;
        if (proto == reg::PROTO_TCP) {
            sr = reg::SR_INIT;
            setR16(n, reg::Sn_MSSR, 1460);
        } else if (proto == reg::PROTO_UDP) {
            sr = reg::SR_UDP;
            s.brauchtBind = true;
        } else if (proto == reg::PROTO_IPRAW) {
            sr = reg::SR_IPRAW;           // ohne Netzfunktion [?]: Roh-Pakete braeuchten Rechte am Wirt
        } else if (proto == reg::PROTO_MACRAW && n == 0) {
            sr = reg::SR_MACRAW;          // ebenso ohne Netzfunktion
        } else {
            sr = reg::SR_CLOSED;
        }
        break;
    }
    case reg::CMD_LISTEN:
        if (sr == reg::SR_INIT) { sr = reg::SR_LISTEN; s.brauchtBind = true; }
        break;
    case reg::CMD_CONNECT:
        if (sr == reg::SR_INIT) { sr = reg::SR_SYNSENT; s.brauchtVerbinden = true; ++s.gen; }
        break;
    case reg::CMD_DISCON:
        if (sr == reg::SR_ESTABLISHED || sr == reg::SR_CLOSE_WAIT) {
            schliesse(n);                 // Wirt schliesst sofort; der Chip ginge ueber FIN_WAIT/LAST_ACK
            sr = reg::SR_CLOSED;
            meldeIr(n, reg::IR_DISCON);
        } else if (sr != reg::SR_CLOSED) {
            schliesse(n);
            sr = reg::SR_CLOSED;
        }
        break;
    case reg::CMD_CLOSE:
        schliesse(n);
        sr = reg::SR_CLOSED;
        break;
    case reg::CMD_SEND: case reg::CMD_SEND_MAC:
        if (sr == reg::SR_ESTABLISHED || sr == reg::SR_CLOSE_WAIT || sr == reg::SR_UDP ||
            sr == reg::SR_IPRAW || sr == reg::SR_MACRAW) {
            s.sendeAuftrag = true;
            s.sendeZiel = r16(n, reg::Sn_TX_WR);
            s.udpPolitik = 0;
        }
        break;
    case reg::CMD_SEND_KEEP:
        break;   // Keep-Alive: am Wirtssocket ohne Wirkung [?]
    case reg::CMD_RECV:
        if (rxRsr(n) != 0) meldeIr(n, reg::IR_RECV);
        break;
    default: break;
    }
}

// ── Arbeitsfaden (Netz) ───────────────────────────────────────────────────────

void W5100::rxSchreibe(int n, const uint8_t* d, std::size_t len) {
    const std::size_t g = rxGroesse(n), b = rxBasis(n);
    if (g == 0) return;
    for (std::size_t i = 0; i < len; ++i)
        rx_[b + ((s_[n].rxWr + i) & (g - 1))] = d[i];
    s_[n].rxWr = static_cast<uint16_t>(s_[n].rxWr + len);
}

void W5100::faden() {
    std::unique_lock<std::mutex> sp(m_);
    while (!ende_) schritt(sp);
}

void W5100::schritt(std::unique_lock<std::mutex>& sp) {
    // 1) Auftraege, die ohne Sperre laufen muessen (Rueckfrage / blockierendes Connect).
    struct Verb { int n; uint32_t gen; uint32_t ip; uint16_t port; int frist; };
    std::vector<Verb> verb;
    struct Pol { int n; uint32_t ip; uint16_t port; };
    std::vector<Pol> pol;
    for (int n = 0; n < 4; ++n) {
        Sock& s = s_[n];
        const uint32_t ip = (uint32_t(r(n, reg::Sn_DIPR)) << 24) | (uint32_t(r(n, reg::Sn_DIPR + 1)) << 16) |
                            (uint32_t(r(n, reg::Sn_DIPR + 2)) << 8) | r(n, reg::Sn_DIPR + 3);
        const uint16_t port = r16(n, reg::Sn_DPORT);
        if (s.brauchtVerbinden) {
            s.brauchtVerbinden = false;
            const long rtr = (long(reg_[reg::RTR]) << 8) | reg_[reg::RTR + 1];
            const long frist = std::max(1L, rtr / 10 * (reg_[reg::RCR] + 1L));
            verb.push_back({n, s.gen, ip, port, int(std::min<long>(frist, fristMax_))});
        }
        if (s.sendeAuftrag && s.udpPolitik == 0 && r(n, reg::Sn_SR) == reg::SR_UDP)
            pol.push_back({n, ip, port});
    }
    if (!verb.empty() || !pol.empty()) {
        const Richtlinie rl = richtlinie_;
        sp.unlock();
        auto erlaubt = [&](uint32_t ip, uint16_t port) {
            if (rl.frage) return rl.frage(ip, port);
            return rl.aussenErlaubt || istLoopback(ip);
        };
        struct VErg { Verb v; bool erlaubt; net::Socket sock; };
        std::vector<VErg> verg;
        for (const Verb& v : verb) {
            VErg e{v, erlaubt(v.ip, v.port), net::Socket()};
            if (e.erlaubt) {
                ++versuche_;
                char text[32];
                std::snprintf(text, sizeof text, "%u.%u.%u.%u", v.ip >> 24, (v.ip >> 16) & 255, (v.ip >> 8) & 255, v.ip & 255);
                const auto ziele = net::aufloesen(text, v.port);
                if (!ziele.empty()) e.sock = net::verbindenAlle(ziele, v.frist);
            } else {
                ++abgewiesen_;
            }
            verg.push_back(std::move(e));
        }
        std::vector<std::pair<int, bool>> perg;
        for (const Pol& p : pol) {
            const bool ok = erlaubt(p.ip, p.port);
            if (!ok) ++abgewiesen_;
            perg.push_back({p.n, ok});
        }
        sp.lock();
        for (VErg& e : verg) {
            Sock& s = s_[e.v.n];
            if (s.gen != e.v.gen || r(e.v.n, reg::Sn_SR) != reg::SR_SYNSENT) continue;   // inzwischen geschlossen
            if (e.sock) {
                s.fd = std::move(e.sock);
                r(e.v.n, reg::Sn_SR) = reg::SR_ESTABLISHED;
                meldeIr(e.v.n, reg::IR_CON);
            } else {
                // Abgewiesen, abgelehnt (RST) oder Zeitueberschreitung: der Chip meldet TIMEOUT [?]
                r(e.v.n, reg::Sn_SR) = reg::SR_CLOSED;
                meldeIr(e.v.n, reg::IR_TIMEOUT);
            }
        }
        for (auto& p : perg) if (s_[p.first].sendeAuftrag) s_[p.first].udpPolitik = p.second ? 1 : 2;
    }

    // 2) Nicht blockierende Ein-/Ausgabe aller Sockets.
    for (int n = 0; n < 4; ++n) sockSchritt(n);

    // 3) Warten: auf Socket-Bereitschaft, sonst kurz schlafen (Befehle des Gastes sind
    //    nur Registerwerte; ihre Latenz von hoechstens ~5 ms ist fuer die Netzseite ohne Belang).
    std::vector<net::PollEintrag> pe;
    for (int n = 0; n < 4; ++n) {
        for (const net::Socket* so : {&s_[n].fd, &s_[n].lausch}) {
            if (!*so) continue;
            net::PollEintrag e;
            e.fd = so->fd();
            e.lesen = true;
            e.schreiben = s_[n].sendeAuftrag;
            pe.push_back(e);
        }
    }
    if (pe.empty()) {
        cv_.wait_for(sp, std::chrono::milliseconds(3), [this] { return ende_.load(); });
    } else {
        sp.unlock();
        net::warten(pe, 3);
        sp.lock();
    }
}

void W5100::sockSchritt(int n) {
    Sock& s = s_[n];
    uint8_t& sr = r(n, reg::Sn_SR);

    // Binden (Listen/UDP) — der Wirtsport kann erst jetzt bekannt sein.
    if (s.brauchtBind) {
        s.brauchtBind = false;
        const int port = r16(n, reg::Sn_PORT);
        net::Fehler f;
        if (sr == reg::SR_LISTEN) {
            net::Lauscher l = net::lauschen(port);   // alle Schnittstellen
            if (l.sock) { s.lausch = std::move(l.sock); s.wirtsPort = l.port; }
        } else if (sr == reg::SR_UDP) {
            net::Socket u = net::udpOeffnen(port, false, &f);
            if (u) { s.wirtsPort = net::lokalerPort(u.fd()); s.fd = std::move(u); }
        }
        if (s.wirtsPort == 0) {
            sr = reg::SR_CLOSED;               // Bindung gescheitert (Port belegt o. ae.) [?]
            meldeIr(n, reg::IR_TIMEOUT);
            return;
        }
        if (port == 0) setR16(n, reg::Sn_PORT, static_cast<uint16_t>(s.wirtsPort));   // Port 0: gewaehlten zeigen [?]
    }

    // Listen: eingehende Verbindung annehmen; der Chip wird zur verbundenen Stelle.
    if (sr == reg::SR_LISTEN && s.lausch) {
        net::Socket neu = net::annehmen(s.lausch.fd());
        if (neu) {
            unsigned a = 0, b = 0, c = 0, d = 0, p = 0;
            if (std::sscanf(net::gegenstelle(neu.fd()).c_str(), "%u.%u.%u.%u:%u", &a, &b, &c, &d, &p) == 5) {
                r(n, reg::Sn_DIPR) = a; r(n, reg::Sn_DIPR + 1) = b; r(n, reg::Sn_DIPR + 2) = c; r(n, reg::Sn_DIPR + 3) = d;
                setR16(n, reg::Sn_DPORT, static_cast<uint16_t>(p));
            }
            s.lausch.schliessen();
            s.fd = std::move(neu);
            sr = reg::SR_ESTABLISHED;
            meldeIr(n, reg::IR_CON);
        }
    }

    const std::size_t txG = txGroesse(n), txB = txBasis(n), rxG = rxGroesse(n);

    if ((sr == reg::SR_ESTABLISHED || sr == reg::SR_CLOSE_WAIT) && s.fd) {
        // Senden: so viel, wie der Wirtssocket annimmt (TX_RD rueckt nach; Rest bleibt im Ring).
        if (s.sendeAuftrag && txG) {
            for (;;) {
                const uint16_t rd = r16(n, reg::Sn_TX_RD);
                const uint16_t rest = static_cast<uint16_t>(s.sendeZiel - rd);
                if (rest == 0) { s.sendeAuftrag = false; meldeIr(n, reg::IR_SEND_OK); break; }
                const std::size_t ab = rd & (txG - 1);
                const std::size_t stueck = std::min<std::size_t>({rest, txG - ab});
                const net::IoErgebnis e = net::senden(s.fd.fd(), &tx_[txB + ab], stueck);
                if (e.status == net::IoStatus::Ok && e.n > 0) {
                    setR16(n, reg::Sn_TX_RD, static_cast<uint16_t>(rd + e.n));
                    continue;
                }
                if (e.status == net::IoStatus::Warten) break;
                schliesse(n);                         // harter Fehler: Verbindung weg
                sr = reg::SR_CLOSED;
                meldeIr(n, reg::IR_TIMEOUT);
                return;
            }
        }
        // Empfangen (nur solange noch Platz ist: Rueckstau statt Verlust).
        while (sr == reg::SR_ESTABLISHED && s.fd && rxG) {
            const std::size_t belegt = rxRsr(n);
            if (belegt >= rxG) break;
            const std::size_t ab = s.rxWr & (rxG - 1);
            const std::size_t max = std::min(rxG - belegt, rxG - ab);
            uint8_t tmp[2048];
            const net::IoErgebnis e = net::empfangen(s.fd.fd(), tmp, std::min<std::size_t>(max, sizeof tmp));
            if (e.status == net::IoStatus::Ok && e.n > 0) {
                rxSchreibe(n, tmp, e.n);
                meldeIr(n, reg::IR_RECV);
                continue;
            }
            if (e.status == net::IoStatus::Warten) break;
            if (e.status == net::IoStatus::Geschlossen) {
                // FIN der Gegenstelle: der Socket bleibt (Senden noch moeglich), bis der Gast DISCON/CLOSE gibt.
                sr = reg::SR_CLOSE_WAIT;
                meldeIr(n, reg::IR_DISCON);
                break;
            }
            schliesse(n);                              // RST / harter Fehler
            sr = reg::SR_CLOSED;
            meldeIr(n, reg::IR_DISCON);
            return;
        }
    } else if (sr == reg::SR_UDP && s.fd) {
        if (s.sendeAuftrag && s.udpPolitik != 0 && txG) {
            const uint16_t rd = r16(n, reg::Sn_TX_RD);
            const std::size_t len = static_cast<uint16_t>(s.sendeZiel - rd);
            std::vector<uint8_t> dg(len);
            for (std::size_t i = 0; i < len; ++i) dg[i] = tx_[txB + ((rd + i) & (txG - 1))];
            bool ok = false;
            if (s.udpPolitik == 1) {
                unsigned char ip[4] = {r(n, reg::Sn_DIPR), r(n, reg::Sn_DIPR + 1), r(n, reg::Sn_DIPR + 2), r(n, reg::Sn_DIPR + 3)};
                ok = net::udpSenden(s.fd.fd(), ip, r16(n, reg::Sn_DPORT), dg.data(), dg.size()).status == net::IoStatus::Ok;
            }
            setR16(n, reg::Sn_TX_RD, s.sendeZiel);
            s.sendeAuftrag = false;
            meldeIr(n, ok ? reg::IR_SEND_OK : reg::IR_TIMEOUT);
        }
        // Empfang: 8-Byte-Kopf (IP, Port, Laenge) + Daten; ohne Platz verwirft der Chip das Datagramm.
        for (int i = 0; i < 16 && rxG; ++i) {
            uint8_t buf[2048];
            unsigned char ip[4] = {};
            int port = 0;
            const net::IoErgebnis e = net::udpEmpfangen(s.fd.fd(), buf, sizeof buf, ip, &port);
            if (e.status != net::IoStatus::Ok) break;
            if (rxG - rxRsr(n) < 8 + e.n) continue;
            const uint8_t kopf[8] = {ip[0], ip[1], ip[2], ip[3], uint8_t(port >> 8), uint8_t(port & 255),
                                     uint8_t(e.n >> 8), uint8_t(e.n & 255)};
            rxSchreibe(n, kopf, 8);
            rxSchreibe(n, buf, e.n);
            meldeIr(n, reg::IR_RECV);
        }
    } else if ((sr == reg::SR_IPRAW || sr == reg::SR_MACRAW) && s.sendeAuftrag) {
        // Keine Netzfunktion: der Auftrag laeuft ins Leere (TIMEOUT wie bei unerreichbarem Ziel).
        setR16(n, reg::Sn_TX_RD, s.sendeZiel);
        s.sendeAuftrag = false;
        meldeIr(n, reg::IR_TIMEOUT);
    }
}

} // namespace k1520::w5100
