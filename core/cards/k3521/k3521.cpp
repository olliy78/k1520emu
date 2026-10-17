/**
 * @file k3521.cpp
 * @brief K3521.30 – akkugepuffertes 4-KB-RAM (s. k3521.h).
 */

#include "core/cards/k3521/k3521.h"
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <vector>

K3521::K3521() : K3521(Config{}) {}

K3521::K3521(const Config& cfg) : cfg_(cfg) {
    if (cfg.base_addr % kRaster != 0)
        throw std::invalid_argument("K3521: Basisadresse muss im 4-KB-Raster liegen");
    mem_.fill(0xFF);   // [Annahme] ungeladener Einschaltzustand
}

K3521::~K3521() { flush(); }

uint8_t K3521::memRead(uint16_t addr) {
    if (!inRange(addr) || memdi_) return 0xFF;
    return mem_[addr - cfg_.base_addr];
}

void K3521::memWrite(uint16_t addr, uint8_t data) {
    if (!inRange(addr) || memdi_) return;
    // Auf Bus-/MEMDI gebrueckt: ein Vorrangspeicher (z. B. EM) hat den Zyklus uebernommen.
    if (cfg_.memdi == Memdi::Bus && bus_ && bus_->memdiActive()) return;
    uint8_t& z = mem_[addr - cfg_.base_addr];
    if (z != data) {
        z = data;
        revision_.fetch_add(1, std::memory_order_relaxed);
    }
}

void K3521::attachToBus(K1520Bus& bus) {
    bus_ = &bus;
    bus.registerMem(this, cfg_.base_addr, kGroesse);
}

void K3521::fill(uint8_t value) {
    mem_.fill(value);
    revision_.fetch_add(1, std::memory_order_relaxed);
}

void K3521::poke(uint16_t offset, uint8_t v) {
    if (offset >= kGroesse || mem_[offset] == v) return;
    mem_[offset] = v;
    revision_.fetch_add(1, std::memory_order_relaxed);
}

bool K3521::dateiBinden(const std::string& pfad, std::string& fehler) {
    std::error_code ec;
    std::array<uint8_t, kGroesse> neu{};
    bool geladen = false;
    if (std::filesystem::exists(std::filesystem::u8path(pfad), ec)) {
        std::ifstream f(std::filesystem::u8path(pfad), std::ios::binary);
        std::vector<char> b(kGroesse + 1);
        f.read(b.data(), static_cast<std::streamsize>(b.size()));
        if (!f.eof() || f.gcount() != static_cast<std::streamsize>(kGroesse)) {
            fehler = "K3521-Datei " + pfad + ": Groesse muss genau 4096 Byte sein";
            return false;
        }
        for (uint32_t i = 0; i < kGroesse; ++i) neu[i] = static_cast<uint8_t>(b[i]);
        geladen = true;
    }
    if (!datei_.empty() && datei_ != pfad && !flush()) {
        fehler = datei_fehler_;
        return false;
    }
    datei_ = pfad;
    if (geladen) mem_ = neu;
    // geladen: Datei und Speicher sind gleich; neu: der Stand gehoert beim ersten Mal hinein.
    revision_.fetch_add(1, std::memory_order_relaxed);
    gesichert_ = geladen ? revision_.load() : revision_.load() - 1;
    gesehen_ = revision_.load();
    return true;
}

bool K3521::schreiben() {
    std::ofstream f(std::filesystem::u8path(datei_), std::ios::binary | std::ios::trunc);
    f.write(reinterpret_cast<const char*>(mem_.data()), static_cast<std::streamsize>(kGroesse));
    f.flush();
    if (!f) {
        datei_fehler_ = "K3521-Datei " + datei_ + " nicht beschreibbar";
        return false;
    }
    datei_fehler_.clear();
    gesichert_ = revision_.load(std::memory_order_relaxed);
    return true;
}

bool K3521::flush() {
    if (datei_.empty() || gesichert_ == revision_.load(std::memory_order_relaxed)) return true;
    return schreiben();
}

void K3521::autoFlush(uint64_t jetzt) {
    if (datei_.empty()) return;
    const uint32_t r = revision_.load(std::memory_order_relaxed);
    if (r == gesichert_) { gesehen_ = r; return; }
    if (r != gesehen_) {            // es wurde (noch) geschrieben: Pause beginnt neu
        gesehen_ = r;
        ruhe_ab_ = jetzt;
        return;
    }
    if (jetzt >= ruhe_ab_ + kSchreibpause) schreiben();
}
