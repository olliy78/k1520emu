/**
 * @file atp590068.cpp
 * @brief ATP 590068 (8279 + EPROMmer) — siehe atp590068.h.
 */

#include "core/cards/atp590068/atp590068.h"
#include "core/logger.h"

Atp590068::Atp590068(const Config& cfg)
    : cfg_(cfg), eprommer_(Eprommer590068::Config{cfg.eprommer_base, 2'457'600}) {}

void Atp590068::attachTastatur(K1520Bus& bus)
{
    bus.registerIO(&tastatur_, cfg_.kbc_base, 2);
}

// ─── ATP-Zusatz ──────────────────────────────────────────────────────────────

void Atp590068::attachZusatz(K1520Bus& bus)
{
    bus.registerIO(&sio_ports_, cfg_.sio_base, 4);
    bus.registerIO(&anzeige_ports_, cfg_.anzeige_base, 2);
    zusatz_ = true;
}

void Atp590068::resetZusatz()
{
    sio_.reset();
    faellig_[0] = faellig_[1] = 0;
    anzeige_ = Anzeige{};
}

void Atp590068::anzeigeSchreiben(uint8_t port, uint8_t wert)
{
    const uint8_t p = static_cast<uint8_t>(cfg_.anzeige_base + (port & 1));
    (p == cfg_.anzeige_base ? anzeige_.f4 : anzeige_.f5) = wert;
    ++anzeige_.anzahl;
    if (anzeige_.protokoll.size() < 256) anzeige_.protokoll.push_back({p, wert});
    LOG_DEBUG("ATP", "Anzeige %02XH := %02X", p, wert);
}

uint64_t Atp590068::zeichenTakte(const Z80SIO::Channel& c) const
{
    const Z80SIO::Channel::Format f = c.format();
    // Startbit + Datenbits + Paritaet + Stopp; synchron (stopp_halbe 0) wie 1 Stoppbit
    const int bits = 1 + f.tx_bits + (f.paritaet ? 1 : 0) + (f.stopp_halbe ? (f.stopp_halbe + 1) / 2 : 1);
    return static_cast<uint64_t>(bits) * f.teiler * static_cast<uint64_t>(cfg_.bittakt_teiler);
}

bool Atp590068::clockTick(int takte)
{
    if (!zusatz_) return false;
    zeit_ += static_cast<uint64_t>(takte);
    bool geaendert = false;
    Z80SIO::Channel* kanal[2] = {&sio_.channelA(), &sio_.channelB()};
    const bool schleife[2] = {cfg_.schleife_a, cfg_.schleife_b};
    for (int i = 0; i < 2; ++i) {
        Z80SIO::Channel& c = *kanal[i];
        if (faellig_[i] == 0) {
            // Neues Zeichen im Sender: ins Schieberegister nehmen (Puffer frei -> TxEmpty)
            if (!c.senderHatZeichen()) continue;
            unterwegs_[i] = c.txGet();
            faellig_[i] = zeit_ + zeichenTakte(c);
            geaendert = true;
        } else if (zeit_ >= faellig_[i]) {
            faellig_[i] = 0;
            if (schleife[i]) {
                static constexpr uint8_t MASKE[4] = {0x1F, 0x7F, 0x3F, 0xFF};
                const uint8_t maske = MASKE[(c.wr[3] >> 6) & 3];
                if (c.empfaengerFrei()) c.rxByte(static_cast<uint8_t>(unterwegs_[i] & maske));
            }
            geaendert = true;
        }
    }
    return geaendert;
}
