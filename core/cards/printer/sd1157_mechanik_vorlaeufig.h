/**
 * @file sd1157_mechanik_vorlaeufig.h
 * @brief VORLAEUFIGER Platzhalter fuer Wagen und Papiervorschub des SD 1157 (WP1, Entwurf 49).
 *
 * **Wird in WP3 durch `GeregelterWagen` + `Papierweg` (core/peripherals/druckwerk/) ersetzt**
 * und lebt nur, damit die Firmware im ersten Schritt nicht schon bei LFG -> LAUF haengt.
 * Kein Druckwerk, kein Blatt: nur die Rueckmeldungen, die die Firmware liest
 * (hardware.md §10.1, §10.2, §15 Nr. 9-13):
 *
 *  - LAUF = LFG ODER Wagen bewegt (sofort mit LFG, §10.1),
 *  - je Position ein Impuls auf Takt vorwaerts (2A.0) bzw. rueckwaerts (2B.0), aktiv high,
 *  - Raender /LR (1A.5), /RR (1B.5) und /VK (2A.3) als Pegel der Position,
 *  - Papier: bei XPFG = 1 Schrittimpulse (aktiv low) auf 1A.4 / 1B.4 (§10.2).
 *
 * Alle Zahlen sind aus hardware.md; `[?]` = dort als Annahme gefuehrt.  Maschinenzeit in
 * Takten des CPU-Takts (2,4576 MHz).  Ideal: keine Traegheit ausser dem Auslauf.
 */

#pragma once
#include <cstdint>

class Sd1157MechanikVorlaeufig {
public:
    // ─── Wagen (Position in Takten der Taktscheibe, §10.1 / §15 Nr. 11-12) ───
    static constexpr int START_POS         = 300;    ///< Startlage [?] §15 Nr. 11
    static constexpr int RAND_LINKS_MAX    = 76;     ///< /LR = 0 fuer Position <= 76 [Q:1239-1240] §10.1
    static constexpr int RAND_RECHTS_SCHMAL = 1420;  ///< /RR = 0 ab Position 1420 (schmal) §10.1/§15
    static constexpr int RAND_RECHTS_BREIT  = 2200;  ///< /RR = 0 ab Position 2200 (breit) §10.1/§15
    static constexpr int VK_MAX            = 10;     ///< /VK = 0 nur ganz links [?] §15 Nr. 11
    static constexpr int POS_MIN           = -200;   ///< weiche Anschlaege, kein NMI §15 Nr. 12
    static constexpr int POS_MAX           = 2400;
    /// Takt-Abstand bei Druckgeschwindigkeit: 180 Z/s -> 1800 Takte/s [?] -> 2457600/1800 §10.1
    static constexpr int TAKT_ABSTAND_DRUCK = 1365;
    /// Takt-Abstand bei VTAB = 1: >= 600 T verlangt, 700 gewaehlt [?] §15 Nr. 9
    static constexpr int TAKT_ABSTAND_TAB   = 700;
    /// Auslauf nach LFG -> 0: 8...70 Takte, Mitte [?] §15 Nr. 10
    static constexpr int AUSLAUF_TAKTE      = 20;
    /// Breite der Takt-Impulse: >= 1 us = 3 T verlangt (§15 Nr. 9); 25 T gewaehlt [?]
    static constexpr int TAKT_IMPULS_T      = 25;

    // ─── Papier (§10.2, §15 Nr. 13) ──────────────────────────────────────────
    /// Impulsabstand FRH = 1: 2...4 ms -> 3 ms = 7373 T; FRH = 0: 4...5 ms -> 4,5 ms = 11059 T.
    /// Beide < 7,08 ms (Waechter CTC 2/3, §6).
    static constexpr int PV_ABSTAND_SCHNELL = 7373;
    static constexpr int PV_ABSTAND_ANLAUF  = 11059;
    /// Impulsbreite aktiv low: >= 10 us = 25 T verlangt; 50 T gewaehlt [?] §15 Nr. 13
    static constexpr int PV_IMPULS_T        = 50;

    /// Ausgaenge der Firmware, die die Mechanik beeinflussen (PIO 2 A/1 A/1 B, hardware.md §5).
    struct Ausgaenge {
        bool lfg = false;    ///< 2A.6 Lauffreigabe
        bool vw = true;      ///< 2A.4 Richtung, 1 = vorwaerts
        bool vtab = false;   ///< 2A.2 Tabulatorgeschwindigkeit
        bool xpfg1 = false;  ///< 1A.2 PV-Freigabe Bahn 1
        bool frh1 = false;   ///< 1A.1 Frequenzhochlauf Bahn 1
        bool xpfg2 = false;  ///< 1B.2 PV-Freigabe Bahn 2
        bool frh2 = false;   ///< 1B.1
    };

    /// Pegel der Eingangspins, wie die Firmware sie liest.
    struct Pegel {
        bool lauf = false;
        bool takt_vor = false, takt_rueck = false;   ///< aktiv high
        bool vk = true, lr = true, rr = true;        ///< /VK, /LR, /RR (0 = aktiv)
        bool pv1 = true, pv2 = true;                 ///< PV-Schritt (0 = Impuls)
        bool operator==(const Pegel& o) const {
            return lauf == o.lauf && takt_vor == o.takt_vor && takt_rueck == o.takt_rueck &&
                   vk == o.vk && lr == o.lr && rr == o.rr && pv1 == o.pv1 && pv2 == o.pv2;
        }
    };

    void reset(int pos = START_POS) {
        *this = Sd1157MechanikVorlaeufig();
        pos_ = pos;
    }

    /// Neue Ausgaenge der Firmware, sofort wirksam (LAUF binnen Mikrosekunden, §10.1).
    void ausgaenge(const Ausgaenge& a) {
        if (a.lfg && !a_.lfg) {                // LFG 0 -> 1: Wagen laeuft sofort
            laeuft_ = true;
            auslauf_ = -1;
            akku_ = 0;
        } else if (!a.lfg && a_.lfg && laeuft_) {
            auslauf_ = AUSLAUF_TAKTE;          // LFG 1 -> 0: Auslauf, Richtung bleibt
        }
        if (a.lfg) richtung_ = a.vw ? +1 : -1; // nur unter LFG folgt die Richtung VW [?] §10.1
        if (a.xpfg1 && !a_.xpfg1) { pv_akku_[0] = 0; }
        if (a.xpfg2 && !a_.xpfg2) { pv_akku_[1] = 0; }
        a_ = a;
    }

    /// @p t Takte fortschreiben; true = ein Pegel (siehe @ref pegel) hat gewechselt.
    bool takt(int t, bool breit) {
        const Pegel vorher = pegel(breit);
        // Takt-Impulse laufen ab
        if (puls_vor_ > 0)   { puls_vor_   -= t; }
        if (puls_rueck_ > 0) { puls_rueck_ -= t; }
        for (int i = 0; i < 2; ++i)
            if (pv_puls_[i] > 0) pv_puls_[i] -= t;
        // Wagen
        if (laeuft_) {
            akku_ += t;
            const int abstand = a_.vtab ? TAKT_ABSTAND_TAB : TAKT_ABSTAND_DRUCK;
            while (laeuft_ && akku_ >= abstand) {
                akku_ -= abstand;
                const int neu = pos_ + richtung_;
                if (neu < POS_MIN || neu > POS_MAX) {   // weicher Anschlag: bleibt stehen
                    laeuft_ = false;
                    break;
                }
                pos_ = neu;
                if (richtung_ > 0) puls_vor_ = TAKT_IMPULS_T; else puls_rueck_ = TAKT_IMPULS_T;
                if (auslauf_ > 0 && --auslauf_ == 0) laeuft_ = false;
            }
        }
        // Papier
        pvTakt(0, a_.xpfg1, a_.frh1, t);
        pvTakt(1, a_.xpfg2, a_.frh2, t);
        return !(pegel(breit) == vorher);
    }

    Pegel pegel(bool breit) const {
        Pegel p;
        p.lauf = a_.lfg || laeuft_;
        p.takt_vor = puls_vor_ > 0;
        p.takt_rueck = puls_rueck_ > 0;
        p.vk = pos_ > VK_MAX;
        p.lr = pos_ > RAND_LINKS_MAX;
        p.rr = pos_ < (breit ? RAND_RECHTS_BREIT : RAND_RECHTS_SCHMAL);
        p.pv1 = pv_puls_[0] <= 0;
        p.pv2 = pv_puls_[1] <= 0;
        return p;
    }

    int  position() const { return pos_; }
    bool laeuft() const { return laeuft_; }
    /// Gezaehlte PV-Schritte je Bahn (Pruefhilfe, kein Hardwarewert).
    uint32_t pvSchritte(int bahn) const { return pv_n_[bahn & 1]; }

private:
    void pvTakt(int i, bool xpfg, bool frh, int t) {
        if (!xpfg) { pv_akku_[i] = 0; return; }
        pv_akku_[i] += t;
        const int abstand = frh ? PV_ABSTAND_SCHNELL : PV_ABSTAND_ANLAUF;
        if (pv_akku_[i] >= abstand) {
            pv_akku_[i] -= abstand;
            pv_puls_[i] = PV_IMPULS_T;
            ++pv_n_[i];
        }
    }

    Ausgaenge a_;
    int pos_ = START_POS;
    int richtung_ = +1;
    bool laeuft_ = false;
    int auslauf_ = -1;     ///< -1 = LFG aktiv, sonst verbleibende Takte (Positionen) bis Stillstand
    int akku_ = 0;
    int puls_vor_ = 0, puls_rueck_ = 0;
    int pv_akku_[2] = {0, 0};
    int pv_puls_[2] = {0, 0};
    uint32_t pv_n_[2] = {0, 0};
};
