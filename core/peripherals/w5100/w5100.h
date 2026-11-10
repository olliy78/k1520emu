/**
 * @file w5100.h
 * @brief Registermodell des WIZnet W5100 (Ethernet-Baustein auf dem WIZ812MJ der GUN,
 *        Entwurf 50 §6, AP N1) mit Anbindung an den Wirtsrechner.
 *
 * Reine Klasse ohne Bus, PIO oder ATmega: die Karte (AP N2) ruft nur `lese`/`schreibe` auf
 * der 15-Bit-Adresse des Chips (Common-Register 0000h, Socket-Register 0400h+100h*n,
 * TX-Speicher 4000h-5FFFh, RX-Speicher 6000h-7FFFh).
 *
 * Netz und Emulationsfaden (Merkposten serielle_schnittstellen.md): der Gast-Faden ruft
 * `lese`/`schreibe` und beruehrt dabei NIE ein Socket; alle Netzzugriffe (bind, listen,
 * accept, connect, send, recv, UDP) macht ein eigener Arbeitsfaden dieser Klasse ueber die
 * Socket-Schicht `core/serial/net/`.  Zustand liegt hinter EINER Sperre.  Rueckstau:
 * der Arbeitsfaden liest nur so viel aus dem Socket, wie der RX-Ring frei hat (TCP) —
 * nichts geht verloren; UDP-Datagramme ohne Platz verwirft er wie der Chip.
 *
 * Wirts-IP-Abbildung: die vom Gast programmierten GAR/SUBR/SHAR/SIPR werden nur als
 * Register gespeichert und ansonsten IGNORIERT.  Verbindungen gehen vom Wirtsrechner aus
 * (Gast-IP wird nicht verwendet); Ziel ist die in Sn_DIPR/Sn_DPORT stehende Adresse.
 * Es gibt keine ARP-/ICMP-/Routing-Nachbildung.
 *
 * Richtlinie: ausgehend NUR Loopback (127.0.0.0/8), ausser `aussenErlaubt` oder der Rueckruf
 * `frage` erlaubt das Ziel.  Eingehende Listen-Sockets sind immer auf 127.0.0.1 gebunden.
 * Ein abgewiesenes Ziel oeffnet gar keinen Socket (`verbindungsversuche()` zaehlt nur
 * wirkliche Versuche) und meldet wie der Chip bei nicht erreichbarem Ziel `TIMEOUT`.
 *
 * Mit [?] gekennzeichnete Stellen sind aus dem Gedaechtnis des Datenblatts gesetzt, nicht
 * an einem Chip gemessen.
 */
#pragma once

#include "core/serial/net/socket.h"

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

namespace k1520::w5100 {

/// Registeradressen und Werte (fuer Karte und Tests).
namespace reg {
constexpr uint16_t MR = 0x0000, GAR = 0x0001, SUBR = 0x0005, SHAR = 0x0009, SIPR = 0x000F,
                   IR = 0x0015, IMR = 0x0016, RTR = 0x0017, RCR = 0x0019, RMSR = 0x001A,
                   TMSR = 0x001B, PATR = 0x001C, PTIMER = 0x0028, PMAGIC = 0x0029;
constexpr uint16_t SOCK_BASIS = 0x0400, SOCK_SCHRITT = 0x0100;
constexpr uint16_t Sn_MR = 0x00, Sn_CR = 0x01, Sn_IR = 0x02, Sn_SR = 0x03, Sn_PORT = 0x04,
                   Sn_DHAR = 0x06, Sn_DIPR = 0x0C, Sn_DPORT = 0x10, Sn_MSSR = 0x12,
                   Sn_PROTO = 0x14, Sn_TOS = 0x15, Sn_TTL = 0x16, Sn_TX_FSR = 0x20,
                   Sn_TX_RD = 0x22, Sn_TX_WR = 0x24, Sn_RX_RSR = 0x26, Sn_RX_RD = 0x28;
constexpr uint16_t TX_SPEICHER = 0x4000, RX_SPEICHER = 0x6000;
constexpr uint16_t sock(int n, uint16_t off) { return static_cast<uint16_t>(SOCK_BASIS + SOCK_SCHRITT * n + off); }

constexpr uint8_t MR_RESET = 0x80;
// Sn_MR
constexpr uint8_t PROTO_TCP = 1, PROTO_UDP = 2, PROTO_IPRAW = 3, PROTO_MACRAW = 4;
// Sn_CR
constexpr uint8_t CMD_OPEN = 0x01, CMD_LISTEN = 0x02, CMD_CONNECT = 0x04, CMD_DISCON = 0x08,
                  CMD_CLOSE = 0x10, CMD_SEND = 0x20, CMD_SEND_MAC = 0x21, CMD_SEND_KEEP = 0x22,
                  CMD_RECV = 0x40;
// Sn_IR
constexpr uint8_t IR_CON = 0x01, IR_DISCON = 0x02, IR_RECV = 0x04, IR_TIMEOUT = 0x08, IR_SEND_OK = 0x10;
// Sn_SR
constexpr uint8_t SR_CLOSED = 0x00, SR_INIT = 0x13, SR_LISTEN = 0x14, SR_SYNSENT = 0x15,
                  SR_SYNRECV = 0x16, SR_ESTABLISHED = 0x17, SR_FIN_WAIT = 0x18, SR_CLOSING = 0x1A,
                  SR_TIME_WAIT = 0x1B, SR_CLOSE_WAIT = 0x1C, SR_LAST_ACK = 0x1D, SR_UDP = 0x22,
                  SR_IPRAW = 0x32, SR_MACRAW = 0x42, SR_ARP = 0x01;
} // namespace reg

class W5100 {
public:
    /// Ausgehende Ziele.  `frage` wird im ARBEITSFADEN ohne Sperre gerufen (darf also die
    /// Oberflaeche fragen; muss dann selbst threadsicher sein).
    struct Richtlinie {
        bool aussenErlaubt = false;
        std::function<bool(uint32_t ip, uint16_t port)> frage;   ///< ip: a<<24|b<<16|c<<8|d
    };

    W5100();
    explicit W5100(Richtlinie r);
    ~W5100();
    W5100(const W5100&) = delete;
    W5100& operator=(const W5100&) = delete;

    // ── Gast-Seite (nur Registerzugriff, nie Netz) ─────────────────────────
    uint8_t  lese(uint16_t adr);
    void     schreibe(uint16_t adr, uint8_t d);
    uint16_t lese16(uint16_t adr);                       ///< Hoeherwertiges Byte zuerst
    void     schreibe16(uint16_t adr, uint16_t w);
    void     leseBlock(uint16_t adr, uint8_t* ziel, std::size_t n);
    void     schreibeBlock(uint16_t adr, const uint8_t* quelle, std::size_t n);

    /// (IR & IMR) != 0 — Pegel der /INT-Leitung (aktiv, wenn true).
    bool interrupt();

    // ── Wirts-Seite / Test ──────────────────────────────────────────────────
    void setzeRichtlinie(const Richtlinie& r);
    /// Obergrenze der Wirts-Verbindungsfrist in ms (Vorgabe 3000; Chip: RTR*(RCR+1)*100 us).
    void setzeVerbindenFristMax(int ms) { fristMax_ = ms; }
    /// Tatsaechlicher Wirtsport des Sockets `n` (Listen/UDP), 0 solange nicht gebunden.
    int  wirtsPort(int n);
    /// Zahl der wirklich begonnenen ausgehenden Verbindungen (nicht der abgewiesenen).
    uint32_t verbindungsversuche() const { return versuche_; }
    uint32_t abgewieseneZiele() const { return abgewiesen_; }

private:
    struct Sock {
        k1520::serial::net::Socket fd;       ///< verbundener TCP- oder UDP-Socket
        k1520::serial::net::Socket lausch;   ///< Listen-Socket
        uint32_t gen = 0;                    ///< zaehlt Oeffnen/Schliessen (veraltete Connect-Ergebnisse verwerfen)
        bool     brauchtBind = false;
        bool     brauchtVerbinden = false;
        bool     sendeAuftrag = false;
        uint16_t sendeZiel = 0;              ///< TX_WR zum Zeitpunkt von SEND
        uint8_t  udpPolitik = 0;             ///< 0 offen, 1 erlaubt, 2 abgewiesen (fuer den laufenden SEND)
        uint16_t rxWr = 0;                   ///< interner RX-Schreibzeiger (volle 16 Bit)
        int      wirtsPort = 0;
    };

    void     zuruecksetzen();                 // Sperre gehalten
    void     befehl(int n, uint8_t cmd);      // Sperre gehalten
    void     schliesse(int n);                // Sperre gehalten
    uint8_t& r(int n, uint16_t off) { return reg_[reg::sock(n, off)]; }
    uint16_t r16(int n, uint16_t off) { return static_cast<uint16_t>((r(n, off) << 8) | r(n, off + 1)); }
    void     setR16(int n, uint16_t off, uint16_t w) { r(n, off) = w >> 8; r(n, off + 1) = w & 0xFF; }
    std::size_t txGroesse(int n) const { return puffer(n, reg_[reg::TMSR]).second; }
    std::size_t rxGroesse(int n) const { return puffer(n, reg_[reg::RMSR]).second; }
    std::size_t txBasis(int n) const { return puffer(n, reg_[reg::TMSR]).first; }
    std::size_t rxBasis(int n) const { return puffer(n, reg_[reg::RMSR]).first; }
    static std::pair<std::size_t, std::size_t> puffer(int n, uint8_t msr);
    uint16_t txFsr(int n);
    uint16_t rxRsr(int n);
    void     meldeIr(int n, uint8_t bits) { r(n, reg::Sn_IR) |= bits; }

    void faden();
    void schritt(std::unique_lock<std::mutex>& sp);
    void sockSchritt(int n);
    void rxSchreibe(int n, const uint8_t* d, std::size_t len);

    std::mutex              m_;
    std::condition_variable cv_;
    std::vector<uint8_t>    reg_;     // 0x0800 Bytes: Common + Socket-Register
    std::vector<uint8_t>    tx_, rx_; // je 8 KB
    Sock                    s_[4];
    Richtlinie              richtlinie_;
    std::atomic<int>        fristMax_{3000};
    std::atomic<uint32_t>   versuche_{0}, abgewiesen_{0};
    std::atomic<bool>       ende_{false};
    std::thread             faden_;
};

} // namespace k1520::w5100
