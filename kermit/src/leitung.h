/**
 * @file leitung.h
 * @brief Abstrakte Kermit-Leitung und eine Speicher-Attrappe fuer Tests.
 *
 * @details
 * Doc: doc/design/36_kermit.md §4.3 (AP K2).  Die Protokollschicht kennt kein
 * Netz: sie bekommt eine `Leitung` (blockierend mit Frist).  Die Leitung ist
 * zugleich die ZEITQUELLE der Protokollschicht (`jetztMs`), damit Fristen in
 * Tests nicht in Echtzeit abgewartet werden muessen: die `SpeicherLeitung`
 * fuehrt auf Wunsch eine virtuelle Uhr, die erst dann vorrueckt, wenn ALLE
 * beteiligten Enden in `lesen` warten (diskrete Ereignissimulation ueber zwei
 * Faeden).  Netzleitungen (Telnet/RFC 2217) kommen in AP K3 dazu.
 *
 * Faeden: eine Leitung wird von genau einem Faden benutzt; `wecken()` darf
 * jeder andere Faden rufen (Abbruch durch die Oberflaeche).
 */
#pragma once

#include <array>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <vector>

namespace k1520::kermit {

class Leitung {
public:
    virtual ~Leitung() = default;
    /// Liest bis zu n Bytes, wartet hoechstens fristMs.  Rueckgabe: Anzahl (>0),
    /// 0 = Frist abgelaufen oder geweckt, -1 = Leitung getrennt.
    virtual int lesen(uint8_t* buf, std::size_t n, int64_t fristMs) = 0;
    /// Schreibt alle Bytes.  false = getrennt.
    virtual bool schreiben(const uint8_t* buf, std::size_t n) = 0;
    virtual void schliessen() = 0;
    /// Laesst ein wartendes (oder das naechste) `lesen` sofort mit 0 zurueckkehren.
    virtual void wecken() = 0;
    /// Zeitquelle der Protokollschicht in ms (Vorgabe: monotone Uhr).
    virtual int64_t jetztMs();
};

/**
 * Leitungspaar im Speicher.  `ende(0)` und `ende(1)` sind die beiden Enden;
 * was an einem geschrieben wird, liest das andere.  Je Richtung einstellbar:
 * Verzoegerung, Abschneiden des 8. Bits, eine Stoerfunktion (verwirft oder
 * veraendert einen Schreibblock — jeder `schreiben`-Aufruf der
 * Protokollschicht ist genau ein Paket).
 */
class SpeicherLeitung;

class SpeicherLeitungPaar {
public:
    /// Stoerfunktion: bekommt den Block und seine laufende Nummer (ab 1),
    /// darf ihn veraendern; Rueckgabe false = Block geht verloren.
    using Stoerung = std::function<bool(std::vector<uint8_t>& block, int nr)>;

    explicit SpeicherLeitungPaar(bool virtuelleZeit = true);
    ~SpeicherLeitungPaar();

    Leitung& ende(int i);

    /// Richtung `von` → andere Seite.
    void setzeVerzoegerung(int von, int64_t ms);
    void setzeSiebenBit(int von, bool an);
    void setzeStoerung(int von, Stoerung s);
    /// Ein nicht beteiligtes Ende haelt die virtuelle Uhr nicht an (stumme
    /// Gegenstelle).  Geschlossene Enden sind nie beteiligt.
    void setzeBeteiligt(int i, bool an);
    /// Anzahl geschriebener Bloecke in Richtung `von`.
    int bloecke(int von) const;
    int64_t jetzt() const;

    // Verlust jedes n-ten Blocks (Hilfe fuer Tests).
    static Stoerung verlustJedes(int n);

private:
    friend class SpeicherLeitung;
    struct Eintrag {
        int64_t ab;
        uint8_t byte;
    };
    struct Richtung {
        std::deque<Eintrag> q;
        int64_t verzoegerung = 0;
        bool siebenBit = false;
        Stoerung stoerung;
        int bloecke = 0;
    };

    int64_t uhr() const;   // ms, virtuell oder echt (unter Sperre)
    void weckeFaellige();  // nach einem Uhrsprung: faellige Enden gelten als lauffaehig

    bool virtuell_;
    mutable std::mutex m_;
    std::condition_variable cv_;
    int64_t jetzt_ = 0;
    std::array<Richtung, 2> r_;
    std::array<bool, 2> offen_{{true, true}};
    std::array<bool, 2> beteiligt_{{true, true}};
    std::array<bool, 2> wartet_{{false, false}};
    std::array<int64_t, 2> weck_{{0, 0}};
    std::array<bool, 2> geweckt_{{false, false}};
    std::array<std::unique_ptr<SpeicherLeitung>, 2> enden_;
    int64_t start_ = 0;
};

class SpeicherLeitung : public Leitung {
public:
    SpeicherLeitung(SpeicherLeitungPaar& p, int i) : p_(p), i_(i) {}
    int lesen(uint8_t* buf, std::size_t n, int64_t fristMs) override;
    bool schreiben(const uint8_t* buf, std::size_t n) override;
    void schliessen() override;
    void wecken() override;
    int64_t jetztMs() override;

private:
    SpeicherLeitungPaar& p_;
    int i_;
};

}  // namespace k1520::kermit
