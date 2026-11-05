/**
 * @file blattmodell.cpp
 * @brief Blattmodell — zeilenblockweises Punktraster.
 * @see blattmodell.h
 */

#include "core/peripherals/druckwerk/blattmodell.h"
#include <algorithm>
#include <cstring>

Blattmodell::Blattmodell(int breite) : breite_(breite < 1 ? 1 : breite) {}

const uint8_t* Blattmodell::zeile(int y) const
{
    if (y < 0) return nullptr;
    const size_t blk = static_cast<size_t>(y / BLOCK_ZEILEN);
    if (blk >= bloecke_.size() || !bloecke_[blk]) return nullptr;
    return bloecke_[blk].get() + static_cast<size_t>(y % BLOCK_ZEILEN) * zeilenBytes();
}

uint8_t* Blattmodell::zeileAnlegen(int y)
{
    const size_t blk = static_cast<size_t>(y / BLOCK_ZEILEN);
    if (blk >= bloecke_.size()) bloecke_.resize(blk + 1);
    if (!bloecke_[blk]) {
        const size_t n = static_cast<size_t>(BLOCK_ZEILEN) * zeilenBytes();
        bloecke_[blk].reset(new uint8_t[n]());   // () = mit Nullen
    }
    return bloecke_[blk].get() + static_cast<size_t>(y % BLOCK_ZEILEN) * zeilenBytes();
}

bool Blattmodell::setze(int x, int y)
{
    if (x < 0 || x >= breite_ || y < 0) {
        ++ausserhalb_;
        return false;
    }
    uint8_t* z = zeileAnlegen(y);
    const uint8_t m = static_cast<uint8_t>(0x80u >> (x & 7));
    if (!(z[x >> 3] & m)) {
        z[x >> 3] |= m;
        ++punkte_;
        ++version_;
    }
    if (y + 1 > hoehe_) {
        hoehe_ = y + 1;
        ++version_;
    }
    return true;
}

bool Blattmodell::punkt(int x, int y) const
{
    if (x < 0 || x >= breite_) return false;
    const uint8_t* z = zeile(y);
    return z && (z[x >> 3] & (0x80u >> (x & 7)));
}

void Blattmodell::streckeBis(int zeilen)
{
    if (zeilen > hoehe_) {
        hoehe_ = zeilen;
        ++version_;
    }
}

void Blattmodell::leeren()
{
    bloecke_.clear();
    hoehe_ = 0;
    punkte_ = 0;
    ++version_;
}

int Blattmodell::lies(int y0, int n, uint8_t* aus, size_t cap) const
{
    if (!aus || n <= 0 || y0 < 0 || y0 >= hoehe_) return 0;
    const size_t zb = static_cast<size_t>(zeilenBytes());
    n = std::min(n, hoehe_ - y0);
    n = static_cast<int>(std::min(static_cast<size_t>(n), cap / zb));
    for (int i = 0; i < n; ++i) {
        const uint8_t* z = zeile(y0 + i);
        if (z) std::memcpy(aus + static_cast<size_t>(i) * zb, z, zb);
        else   std::memset(aus + static_cast<size_t>(i) * zb, 0, zb);
    }
    return n;
}

std::string blattAscii(const Blattmodell& b, int x0, int y0, int breite, int hoehe, int sx, int sy)
{
    if (sx < 1) sx = 1;
    if (sy < 1) sy = 1;
    std::string s;
    for (int y = y0; y < y0 + hoehe; y += sy) {
        for (int x = x0; x < x0 + breite; x += sx) {
            bool an = false;
            for (int dy = 0; dy < sy && !an; ++dy)
                for (int dx = 0; dx < sx && !an; ++dx) an = b.punkt(x + dx, y + dy);
            s.push_back(an ? '#' : '.');
        }
        s.push_back('\n');
    }
    return s;
}
