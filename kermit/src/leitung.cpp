/**
 * @file leitung.cpp
 * @brief Leitung (Zeitquelle) und SpeicherLeitungPaar mit virtueller Uhr.
 */
#include "kermit/src/leitung.h"

#include <algorithm>
#include <chrono>
#include <limits>

namespace k1520::kermit {

namespace {
int64_t monotonMs() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}
// Schutz gegen einen haengenden Test: wartet ein Ende laenger als das in
// ECHTZEIT (virtuelle Uhr steht, weil das andere Ende weder liest noch
// schliesst), gilt die Leitung als getrennt.
constexpr int64_t HAENGER_MS = 30000;
}  // namespace

int64_t Leitung::jetztMs() { return monotonMs(); }

SpeicherLeitungPaar::SpeicherLeitungPaar(bool virtuelleZeit) : virtuell_(virtuelleZeit), start_(monotonMs()) {
    enden_[0] = std::make_unique<SpeicherLeitung>(*this, 0);
    enden_[1] = std::make_unique<SpeicherLeitung>(*this, 1);
}

SpeicherLeitungPaar::~SpeicherLeitungPaar() = default;

Leitung& SpeicherLeitungPaar::ende(int i) { return *enden_[static_cast<std::size_t>(i & 1)]; }

int64_t SpeicherLeitungPaar::uhr() const { return virtuell_ ? jetzt_ : monotonMs() - start_; }

void SpeicherLeitungPaar::setzeVerzoegerung(int von, int64_t ms) {
    std::lock_guard<std::mutex> lk(m_);
    r_[static_cast<std::size_t>(von & 1)].verzoegerung = ms;
}

void SpeicherLeitungPaar::setzeSiebenBit(int von, bool an) {
    std::lock_guard<std::mutex> lk(m_);
    r_[static_cast<std::size_t>(von & 1)].siebenBit = an;
}

void SpeicherLeitungPaar::setzeStoerung(int von, Stoerung s) {
    std::lock_guard<std::mutex> lk(m_);
    r_[static_cast<std::size_t>(von & 1)].stoerung = std::move(s);
}

void SpeicherLeitungPaar::setzeBeteiligt(int i, bool an) {
    std::lock_guard<std::mutex> lk(m_);
    beteiligt_[static_cast<std::size_t>(i & 1)] = an;
    cv_.notify_all();
}

int SpeicherLeitungPaar::bloecke(int von) const {
    std::lock_guard<std::mutex> lk(m_);
    return r_[static_cast<std::size_t>(von & 1)].bloecke;
}

int64_t SpeicherLeitungPaar::jetzt() const {
    std::lock_guard<std::mutex> lk(m_);
    return uhr();
}

SpeicherLeitungPaar::Stoerung SpeicherLeitungPaar::verlustJedes(int n) {
    return [n](std::vector<uint8_t>&, int nr) { return n <= 0 || nr % n != 0; };
}

void SpeicherLeitungPaar::weckeFaellige() {
    for (std::size_t k = 0; k < 2; ++k)
        if (wartet_[k] && weck_[k] <= jetzt_) wartet_[k] = false;
}

// ─── Ende ──────────────────────────────────────────────────────────────────

int SpeicherLeitung::lesen(uint8_t* buf, std::size_t n, int64_t fristMs) {
    auto& p = p_;
    const std::size_t ich = static_cast<std::size_t>(i_), du = 1 - ich;
    std::unique_lock<std::mutex> lk(p.m_);
    auto& ein = p.r_[du];
    const int64_t frist = p.uhr() + std::max<int64_t>(0, fristMs);
    const int64_t echtBeginn = monotonMs();
    for (;;) {
        if (p.geweckt_[ich]) {
            p.geweckt_[ich] = false;
            return 0;
        }
        if (!p.offen_[ich]) return -1;
        const int64_t t = p.uhr();
        if (!ein.q.empty() && ein.q.front().ab <= t) {
            std::size_t k = 0;
            while (k < n && !ein.q.empty() && ein.q.front().ab <= t) {
                buf[k++] = ein.q.front().byte;
                ein.q.pop_front();
            }
            return static_cast<int>(k);
        }
        if (ein.q.empty() && !p.offen_[du]) return -1;
        if (t >= frist) return 0;
        int64_t weck = frist;
        if (!ein.q.empty()) weck = std::min(weck, ein.q.front().ab);
        if (p.virtuell_) {
            p.wartet_[ich] = true;
            p.weck_[ich] = weck;
            bool alle = true;
            int64_t mn = std::numeric_limits<int64_t>::max();
            for (std::size_t k = 0; k < 2; ++k) {
                if (!p.offen_[k] || !p.beteiligt_[k]) continue;
                if (!p.wartet_[k]) alle = false;
                else mn = std::min(mn, p.weck_[k]);
            }
            if (alle && mn != std::numeric_limits<int64_t>::max() && mn > p.jetzt_) {
                p.jetzt_ = mn;   // alle warten: Uhr springt zum fruehesten Weckpunkt
                p.weckeFaellige();
                p.wartet_[ich] = false;
                p.cv_.notify_all();
                continue;
            }
            // Auch ein nicht beteiligtes Ende darf lesen; wartet es allein, springt die Uhr.
            if (!p.beteiligt_[ich] && alle && mn == std::numeric_limits<int64_t>::max() && weck > p.jetzt_) {
                p.jetzt_ = weck;
                p.wartet_[ich] = false;
                p.cv_.notify_all();
                continue;
            }
            p.cv_.wait_for(lk, std::chrono::milliseconds(200));
            p.wartet_[ich] = false;
            if (monotonMs() - echtBeginn > HAENGER_MS) return -1;
        } else {
            p.cv_.wait_for(lk, std::chrono::milliseconds(std::max<int64_t>(1, weck - t)));
        }
    }
}

bool SpeicherLeitung::schreiben(const uint8_t* buf, std::size_t n) {
    auto& p = p_;
    const std::size_t ich = static_cast<std::size_t>(i_), du = 1 - ich;
    std::vector<uint8_t> blk(buf, buf + n);
    std::lock_guard<std::mutex> lk(p.m_);
    if (!p.offen_[ich] || !p.offen_[du]) return false;
    auto& aus = p.r_[ich];
    ++aus.bloecke;
    if (aus.stoerung && !aus.stoerung(blk, aus.bloecke)) {
        p.cv_.notify_all();
        return true;   // verloren — fuer den Sender nicht erkennbar
    }
    const int64_t ab = p.uhr() + aus.verzoegerung;
    for (uint8_t b : blk) aus.q.push_back({ab, aus.siebenBit ? static_cast<uint8_t>(b & 0x7F) : b});
    // Die Gegenseite ist ab jetzt lauffaehig (sofern die Daten schon faellig
    // sind) — auch wenn ihr Faden noch nicht aufgewacht ist.  Sonst saehe ein
    // gleich danach lesendes Ende „alle warten" und liesse die Uhr springen.
    if (p.wartet_[du]) {
        if (ab <= p.uhr()) p.wartet_[du] = false;
        else p.weck_[du] = std::min(p.weck_[du], ab);
    }
    p.cv_.notify_all();
    return true;
}

void SpeicherLeitung::schliessen() {
    std::lock_guard<std::mutex> lk(p_.m_);
    p_.offen_[static_cast<std::size_t>(i_)] = false;
    p_.wartet_[0] = p_.wartet_[1] = false;
    p_.cv_.notify_all();
}

void SpeicherLeitung::wecken() {
    std::lock_guard<std::mutex> lk(p_.m_);
    p_.geweckt_[static_cast<std::size_t>(i_)] = true;
    p_.wartet_[static_cast<std::size_t>(i_)] = false;
    p_.cv_.notify_all();
}

int64_t SpeicherLeitung::jetztMs() {
    std::lock_guard<std::mutex> lk(p_.m_);
    return p_.uhr();
}

}  // namespace k1520::kermit
