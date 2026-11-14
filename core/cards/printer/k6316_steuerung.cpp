/**
 * @file k6316_steuerung.cpp
 * @brief Steuerung des K6316 — Speicher, E/A-Dekodierung, Diodenmatrix, Interruptkette, Mechanik-Pegel.
 * @see k6316_steuerung.h, doc/printer/k6316/hardware.md
 */

#include "core/cards/printer/k6316_steuerung.h"
#include "core/logger.h"
#include "core/serial/sio_format.h"

#include <stdexcept>
#include <string>

namespace {
// E/A-Adressen (Decoder D8, hardware.md §2)
constexpr uint8_t P_PIO12 = 0x80;   // 80H–83H
constexpr uint8_t P_CTC5  = 0x84;   // 84H–87H
constexpr uint8_t P_CTC10 = 0x88;   // 88H–8BH
constexpr uint8_t P_PIO14 = 0x8C;   // 8CH–8FH
constexpr uint8_t P_MODUL = 0xA0;   // A0H–A3H frei (Interfacemodul)
constexpr uint8_t P_SIO   = 0xA4;   // A4H–A7H
constexpr uint8_t P_PIO11 = 0xA8;   // A8H–ABH

constexpr std::size_t SIO_AUS_MAX = 1u << 16;

// Spalten der Diodenmatrix (Bit-Nummer in spalten())
constexpr int C_TSCH = 0, C_TBAW = 1, C_DIL = 2, C_TPVV = 3, C_TPVR = 4;

// DIL-Schalter je (Zeile AM1..AM4, Spalte): Schaltplan Blatt 10 [S:10]; 0 = kein Schalter
constexpr uint8_t DIL_NR[4][5] = {
    //  TSCH TBAW DIL TPVV TPVR
    {0,   0,   14,  0,   0},    // AM1
    {22,  24,  23,  25,  26},   // AM2
    {21,  19,  20,  18,  17},   // AM3
    {13,  11,  12,  15,  16},   // AM4
};
}  // namespace

// ─── Anschluss je SIO-Kanal (Entwurf 19 §5.1, Entwurf 49 §4.4) ───────────────
// Kanal B = IFSS (Stromschleife, keine Steuerleitungen), Kanal A = V.24.  Beide haengen am Baudtakt von CTC D10 Kanal 2.

class K6316Steuerung::Anschluss : public k1520::serial::SerialAnschluss {
public:
    Anschluss(K6316Steuerung& k, bool v24) : k_(k), v24_(v24) {}

    const char* name() const override { return v24_ ? "V.24" : "IFSS"; }
    const char* stecker() const override { return v24_ ? "V.24" : "IFSS"; }
    bool v24() const override { return v24_; }

    k1520::serial::SerialFormat format() const override
    {
        return k1520::serial::serialFormatAusSio(kanal(), k_.baudTakte());
    }
    bool senderHatZeichen() const override { return kanal().senderHatZeichen(); }
    uint8_t senderNimm() override
    {
        uint8_t b = kanal().txGet();
        const uint8_t bits = kanal().format().tx_bits;
        if (bits < 8) b &= static_cast<uint8_t>((1u << bits) - 1);
        std::string& m = k_.sio_aus_[v24_ ? 0 : 1];
        if (m.size() < SIO_AUS_MAX) m.push_back(static_cast<char>(b));
        k_.ketteNeu();
        k_.seriell_geaendert_ = true;
        if (!belegt_ && abnehmer_) abnehmer_(b);
        return b;
    }
    bool empfaengerFrei() const override { return kanal().empfaengerFrei(); }
    void empfange(uint8_t b) override
    {
        kanal().rxByte(b);
        k_.ketteNeu();
        k_.seriell_geaendert_ = true;
    }
    bool rts() const override { return kanal().rts(); }
    bool dtr() const override { return kanal().dtr(); }
    void setzeEingaenge(bool cts, bool /*dsr*/, bool dcd) override
    {
        if (!v24_) return;   // IFSS: keine Steuerleitungen
        kanal().setzeCTS(cts);
        kanal().setzeDCD(dcd);
        k_.ketteNeu();
        k_.seriell_geaendert_ = true;
    }
    bool breakGesendet() const override { return kanal().breakSenden(); }
    void breakEmpfang(bool aktiv) override
    {
        kanal().setzeBreakEmpfang(aktiv);
        k_.ketteNeu();
        k_.seriell_geaendert_ = true;
    }
    void leitungBelegt(bool belegt) override { belegt_ = belegt; }

    void setAbnehmer(std::function<void(uint8_t)> cb) { abnehmer_ = std::move(cb); }
    void einspeisen(uint8_t b)
    {
        if (belegt_) return;   // Transport/Loop am Stecker: ins Leere (Entwurf 19 §8)
        empfange(b);
    }

private:
    Z80SIO::Channel& kanal() const { return v24_ ? k_.sio_.channelA() : k_.sio_.channelB(); }

    K6316Steuerung& k_;
    bool            v24_;
    bool            belegt_ = false;
    std::function<void(uint8_t)> abnehmer_;
};

K6316Steuerung::~K6316Steuerung() = default;

k1520::serial::SerialAnschluss& K6316Steuerung::serialAnschluss(bool v24)
{
    return *anschluesse_[v24 ? 1 : 0];
}

void K6316Steuerung::serialAbnehmer(bool v24, std::function<void(uint8_t)> cb)
{
    anschluesse_[v24 ? 1 : 0]->setAbnehmer(std::move(cb));
}

void K6316Steuerung::serialEinspeisen(bool v24, uint8_t byte)
{
    anschluesse_[v24 ? 1 : 0]->einspeisen(byte);
}

uint64_t K6316Steuerung::baudTakte() const
{
    return ctc10_.teilerTakte(2);   // D10 Kanal 2 = Baudtakt (hardware.md §4)
}

K6316Steuerung::K6316Steuerung(K1520Bus& bus) : K6316Steuerung(bus, Config{}) {}

namespace {
NadeldruckParameter druckParameter(const K6316Steuerung::Config& c)
{
    NadeldruckParameter p = c.druck;
    p.bahnen = 1;
    return p;
}
}  // namespace

K6316Steuerung::K6316Steuerung(K1520Bus& bus, const Config& cfg)
    : bus_(bus), cfg_(cfg), betriebsart_(cfg.betriebsart), kopf_(cfg.kopf_angeschwenkt), haube_(cfg.haube_zu),
      pe_(!cfg.papier_da), dil_(cfg.dil),
      druck_(druckParameter(cfg), std::make_unique<SchrittWagen>(cfg.wagen), cfg.papier), vorsteck_(cfg.papier), beleg_(cfg.beleg)
{
    // Das ROM kommt vom Aufrufer (PrinterMachine aus dem Katalog data/drucker_roms.yaml, WP9): kein eingebettetes Abbild mehr.
    if (cfg_.rom_extern.size() != ROM_ENDE)
        throw std::invalid_argument("K6316: Config::rom_extern hat " + std::to_string(cfg_.rom_extern.size()) + " Bytes, erwartet " +
                                    std::to_string(ROM_ENDE) + " (ROM-Satz aus dem Katalog)");
    // Modus-3-Interrupts der PIOs sind Ereignisse (wie am SD 1157, merkposten/drucker.md Nr. 5)
    pio11_.setModus3Flanke(true);
    pio12_.setModus3Flanke(true);
    pio14_.setModus3Flanke(true);
    // Offene Eingangsbits lesen 0 (Spalten haengen an Pull-down-Widerstaenden, hardware.md §5)
    pio11_.setPullupsA(0);
    pio11_.setPullupsB(0);
    pio14_.setPullupsA(0);
    pio14_.setPullupsB(0);
    pio12_.setPullupsA(0);
    pio12_.setPullupsB(0);
    anschluesse_[0] = std::make_unique<Anschluss>(*this, false);
    anschluesse_[1] = std::make_unique<Anschluss>(*this, true);
    kette_ = {&ctc5_, &ctc10_, &pio12_, &sio_};

    cpu_.readByte     = [this](uint16_t a)            { return bus_.memRead(a); };
    cpu_.writeByte    = [this](uint16_t a, uint8_t d) { bus_.memWrite(a, d); };
    cpu_.readPort     = [this](uint16_t p)            { return bus_.ioRead(p); };
    cpu_.writePort    = [this](uint16_t p, uint8_t d) { bus_.ioWrite(p, d); };
    cpu_.retiCallback = [this]()                      { bus_.signalRETI(); };
}

void K6316Steuerung::attachToBus(K1520Bus& bus)
{
    bus.registerIO(this, P_PIO12, 16);   // 80H–8FH: PIO D12, CTC D5, CTC D10, PIO D14
    bus.registerIO(this, P_MODUL, 16);   // A0H–AFH: Modul, SIO, PIO D11
    bus.registerMem(this, 0x0000, RAM_BASIS + RAM_GROESSE);
}

void K6316Steuerung::powerOn()
{
    ram_.fill(cfg_.ram_fill);
    druck_.netzEin(cfg_.wagen_start);
    vorsteck_.netzEin();
    beleg_lage_ = 0;   // der Beleg (falls einer liegt) bleibt, wo er ist; nur der Zaehler des Motors beginnt neu
    messer_bereit_ = true;
    schnitte_ = 0;
    reset();
}

void K6316Steuerung::reset()
{
    ctc5_.reset();
    ctc10_.reset();
    pio11_.reset();
    pio12_.reset();
    pio14_.reset();
    sio_.reset();
    cpu_.reset();
    // IFSS (Kanal B) hat keine Steuerleitungen: /CTS und /DCD liegen fest aktiv.  Am V.24 (Kanal A) kommen sie vom Host (Wandler/
    // Gegenstelle): ohne Gegenstelle sind sie inaktiv, und die V.24-Firmware wartet mit dem ersten DC1 auf ihre Statusaenderung
    // (Interrupt, ROM 1995H) — wie am SD 1157.
    sio_.channelA().setzeDCD(false);
    sio_.channelA().setzeCTS(false);
    sio_.channelB().setzeDCD(true);
    sio_.channelB().setzeCTS(true);
    druck_.halt();
    nadeln_aktiv_ = 0;
    g1_ = druck_.wagen().rueckmeldung().g1;
    sio_aus_[0].clear();
    sio_aus_[1].clear();
    eingaengeSetzen();
    ketteNeu();
}

// ─── Eingangspegel ───────────────────────────────────────────────────────────

uint8_t K6316Steuerung::spalten() const
{
    // Zeilen AM1..AM4: D11 B Bit 4, 7, 5, 1 (Pegel an der Leitung); ein Eingang (AM2 bei der Einrichtung) treibt nicht
    const uint8_t b = pio11_.pinsB(0);
    const bool zeile[4] = {(b & 0x10) != 0, (b & 0x80) != 0, (b & 0x20) != 0, (b & 0x02) != 0};
    uint8_t s = 0;
    // Tasten legen ihre Spalte ueber eine Diode auf 5 V (Blatt 10: V5–V8), unabhaengig von den Zeilen
    if (taste_[4]) s |= 1 << C_TSCH;
    if (taste_[3]) s |= 1 << C_TBAW;
    if (taste_[1]) s |= 1 << C_TPVV;
    if (taste_[2]) s |= 1 << C_TPVR;
    for (int z = 0; z < 4; ++z) {
        if (!zeile[z]) continue;
        for (int c = 0; c < 5; ++c) {
            const int nr = DIL_NR[z][c];
            if (nr && ((dil_ >> (nr - 11)) & 1)) s |= static_cast<uint8_t>(1 << c);
        }
    }
    return s;
}

void K6316Steuerung::eingaengeSetzen()
{
    eingang_neu_ = true;
    const uint8_t sp = spalten();
    const bool mha = !kopf_ || !haube_;                 // 1 = Haube offen oder Kopf abgeschwenkt
    const bool melo = betriebsart_ != Betriebsart::Endlos;       // /MELO: 0 = Stellung Endlos
    const bool mauf = betriebsart_ != Betriebsart::Aufwickeln;   // /MAUF: 0 = Stellung Aufwickeln
    // D12 A: Bit 0 FSO, Bit 1 /ITON (Taste S1: gedrueckt = 0), Bit 2 FSU
    pio12_.setExternA(static_cast<uint8_t>((fso_ ? 0x01 : 0) | (taste_[0] ? 0 : 0x02) | (fsu_ ? 0x04 : 0)), 0x07);
    // D12 B: Bit 0 /G1 (Taktscheibe), Bit 2 MHA, Bit 6 PE
    pio12_.setExternB(static_cast<uint8_t>((g1_ ? 0x01 : 0) | (mha ? 0x04 : 0) | (pe_ ? 0x40 : 0)), 0x45);
    // D14 A: Bit 2 TPVV, Bit 3 TPVR, Bit 7 /MELO
    pio14_.setExternA(static_cast<uint8_t>((((sp >> C_TPVV) & 1) ? 0x04 : 0) | (((sp >> C_TPVR) & 1) ? 0x08 : 0) |
                                           (melo ? 0x80 : 0)), 0x8C);
    // D11 A: Bit 0 TSCH, Bit 1 TBAW, Bit 2 DIL
    pio11_.setExternA(static_cast<uint8_t>(sp & 0x07), 0x07);
    // D11 B: Bit 2 /MAUF, Bit 6 /MPV (Bit 7 = AM2 bleibt unbelegt)
    pio11_.setExternB(static_cast<uint8_t>((mauf ? 0x04 : 0) | (mpv_ ? 0x40 : 0)), 0x44);
}

void K6316Steuerung::ausgaengeAuswerten()
{
    const uint8_t a12 = pio12_.pinsA(0), b12 = pio12_.pinsB(0), a14 = pio14_.pinsA(0), b14 = pio14_.pinsB(0);
    const uint8_t a11 = pio11_.pinsA(0);
    // Motorphasen, low-aktiv: Wagen /SW1–/SW4 = D12 B Bit 1,3,5,7; Papier /SF1–/SF4 = D12 A Bit 4–7;
    // Vorsteck /SV1–/SV4 = D11 A Bit 4–7
    WagenEingang w;
    for (int k = 0; k < 4; ++k)
        if (!((b12 >> (1 + 2 * k)) & 1)) w.phasen |= static_cast<uint8_t>(1 << k);
    druck_.wagen().eingang(w);
    uint8_t pp = 0, vp = 0;
    for (int k = 0; k < 4; ++k) {
        if (!((a12 >> (4 + k)) & 1)) pp |= static_cast<uint8_t>(1 << k);
        if (!((a11 >> (4 + k)) & 1)) vp |= static_cast<uint8_t>(1 << k);
    }
    druck_.papier(0).phasen(pp);
    vorsteck_.phasen(vp);
    belegAbgleichen();
    // Nadeln: /DNx = 0 bei freigegebenem Treiber (/DTFG = 0); Nadel 1..8 = D14 B Bit 7..0, Nadel 9 = D14 A Bit 0
    uint16_t aktiv = 0;
    if (!(a14 & 0x02)) {
        for (int n = 1; n <= 8; ++n)
            if (!((b14 >> (8 - n)) & 1)) aktiv |= static_cast<uint16_t>(1u << (n - 1));
        if (!(a14 & 0x01)) aktiv |= 0x100;
    }
    const uint16_t neu = static_cast<uint16_t>(aktiv & ~nadeln_aktiv_);
    nadeln_aktiv_ = aktiv;
    if (neu) {
        // Liegt ein Beleg unter der Druckzeile und der Wagen ueber ihm, treffen die Nadeln ihn (er liegt zwischen Kopf und
        // Endlospapier, hardware.md §11.1/§11.4 [?]); sonst das Endlospapier.
        const int xpix = druck_.wagen().positionPixel(druck_.parameter().x_je_takt);
        if (beleg_.unterDruckzeile() && beleg_.trifftX(xpix))
            druck_.kopf().feuerePixel(neu, xpix - beleg_.xLinks(), beleg_.druckzeileY(), druck_.wagen().richtung(), kopf_ && haube_,
                                      beleg_.blatt());
        else
            druck_.feuere(neu, kopf_ && haube_);
    }
    // Schneideinrichtung: mechanisch ueber die Wagenlage
    const int pos = druck_.wagen().position();
    if (messer_bereit_ && pos >= cfg_.schnitt_pos) {
        messer_bereit_ = false;
        ++schnitte_;
        druck_.blatt(0).schnitt(druck_.papierY(0) + 6 * druck_.parameter().nadel_teilung_y + cfg_.schnitt_abstand_y);
    } else if (!messer_bereit_ && pos <= cfg_.schnitt_ruecksetz) {
        messer_bereit_ = true;
    }
    // Taktscheibe folgt der Wagenlage
    const bool g1 = druck_.wagen().rueckmeldung().g1;
    if (g1 != g1_) g1_ = g1;
}

// ─── Vorsteckbeleg (hardware.md §11) ─────────────────────────────────────────

namespace {
/// Einzug (Beleg in den Drucker) = Lage des Vorsteckmotors steigt (+1) oder faellt (-1): am Lauf der IFSS-Firmware gemessen
/// (BEZ: die Lage laeuft in diese Richtung), hardware.md §11.8.
constexpr int EINZUG_VORZEICHEN = -1;
constexpr double PIXEL_JE_MM = 300.0 / 25.4;    // 300 dpi in x
constexpr double SCHRITTE_JE_MM = 108.0 / 25.4; // 1 Vorsteckschritt = 1/108 Zoll
}  // namespace

void K6316Steuerung::belegAbgleichen()
{
    const int l = vorsteck_.lage();
    if (l == beleg_lage_) return;
    const int d = (l - beleg_lage_) * EINZUG_VORZEICHEN;
    beleg_lage_ = l;
    if (!beleg_.eingelegt()) return;
    beleg_.bewege(d);
    if (beleg_.fso() != fso_ || beleg_.fsu() != fsu_) setBeleg(beleg_.fso(), beleg_.fsu());
}

bool K6316Steuerung::belegEinlegen(double laenge_mm, double breite_mm)
{
    if (laenge_mm <= 0 || breite_mm <= 0) return false;
    if (!beleg_.einlegen(static_cast<int>(laenge_mm * SCHRITTE_JE_MM + 0.5), static_cast<int>(breite_mm * PIXEL_JE_MM + 0.5)))
        return false;
    beleg_lage_ = vorsteck_.lage();
    setBeleg(beleg_.fso(), beleg_.fsu());
    return true;
}

void K6316Steuerung::belegEntnehmen()
{
    if (!beleg_.eingelegt()) return;
    beleg_.entnehmen();
    setBeleg(false, false);
}

// ─── Bedienfeld ──────────────────────────────────────────────────────────────

void K6316Steuerung::setTaste(int nr, bool g)
{
    if (nr < 1 || nr > 5) return;
    taste_[nr - 1] = g;
    eingaengeSetzen();
}
void K6316Steuerung::setBetriebsart(Betriebsart b) { betriebsart_ = b; eingaengeSetzen(); }
void K6316Steuerung::setKopf(bool a)               { kopf_ = a; eingaengeSetzen(); }
void K6316Steuerung::setHaube(bool z)              { haube_ = z; eingaengeSetzen(); }
void K6316Steuerung::setPapierDa(bool da)          { pe_ = !da; eingaengeSetzen(); }
void K6316Steuerung::setBeleg(bool o, bool u)      { fso_ = o; fsu_ = u; eingaengeSetzen(); }
void K6316Steuerung::setMpv(bool beleg_vorn)       { mpv_ = !beleg_vorn; eingaengeSetzen(); }
void K6316Steuerung::setDil(int nr, bool ein)
{
    if (nr < 11 || nr > 26) return;
    if (ein) dil_ = static_cast<uint16_t>(dil_ | (1u << (nr - 11)));
    else     dil_ = static_cast<uint16_t>(dil_ & ~(1u << (nr - 11)));
    eingaengeSetzen();
}

bool K6316Steuerung::lampe(int h) const
{
    switch (h) {
        case 1: return (pio14_.pinsA(0) & 0x20) != 0;   // H1 Test: LTEST (D14 A Bit 5)
        case 2: return (pio14_.pinsA(0) & 0x40) != 0;   // H2 Fehler: LTF (Bit 6)
        case 3: return pe_;                             // H3 Papierende: PE treibt die Lampe /LPE direkt
        case 4: return true;                            // H4 BEREIT: LSPAUS = invertiertes /SPAUS der Spannungsueberwachung [S:03]
        case 5: return (pio14_.pinsA(0) & 0x10) != 0;   // H5 ON-LINE: LON (Bit 4)
    }
    return false;
}

uint16_t K6316Steuerung::nadeln() const { return nadeln_aktiv_; }

uint8_t K6316Steuerung::portAusgabe(int pio, int kanal) const
{
    const Z80PIO& p = pio == 11 ? pio11_ : pio == 12 ? pio12_ : pio14_;
    return kanal ? p.pinsB(0) : p.pinsA(0);
}

// ─── E/A ─────────────────────────────────────────────────────────────────────

uint8_t K6316Steuerung::ioRead(uint8_t port)
{
    uint8_t r = 0xFF;
    if (port >= P_PIO12 && port < P_PIO12 + 4)       r = pio12_.ioRead(chipPort(port - P_PIO12));
    else if (port >= P_CTC5 && port < P_CTC5 + 4)    r = ctc5_.ioRead(port - P_CTC5);
    else if (port >= P_CTC10 && port < P_CTC10 + 4)  r = ctc10_.ioRead(port - P_CTC10);
    else if (port >= P_PIO14 && port < P_PIO14 + 4)  r = pio14_.ioRead(chipPort(port - P_PIO14));
    else if (port >= P_SIO && port < P_SIO + 4)      r = sio_.ioRead(chipPort(port - P_SIO));
    else if (port >= P_PIO11 && port < P_PIO11 + 4)  r = pio11_.ioRead(chipPort(port - P_PIO11));
    ketteNeu();
    return r;
}

void K6316Steuerung::ioWrite(uint8_t port, uint8_t data)
{
    bool pio_daten = false;
    if (port >= P_PIO12 && port < P_PIO12 + 4) {
        pio12_.ioWrite(chipPort(port - P_PIO12), data);
        pio_daten = !((port - P_PIO12) & 2);
    } else if (port >= P_CTC5 && port < P_CTC5 + 4) {
        ctc5_.ioWrite(port - P_CTC5, data);
    } else if (port >= P_CTC10 && port < P_CTC10 + 4) {
        ctc10_.ioWrite(port - P_CTC10, data);
    } else if (port >= P_PIO14 && port < P_PIO14 + 4) {
        pio14_.ioWrite(chipPort(port - P_PIO14), data);
        pio_daten = !((port - P_PIO14) & 2);
    } else if (port >= P_SIO && port < P_SIO + 4) {
        sio_.ioWrite(chipPort(port - P_SIO), data);
    } else if (port >= P_PIO11 && port < P_PIO11 + 4) {
        pio11_.ioWrite(chipPort(port - P_PIO11), data);
        pio_daten = !((port - P_PIO11) & 2);
    }
    if (pio_daten) ausgaengeAuswerten();
    if (port >= P_PIO12 && port < P_CTC5) eingaengeSetzen();
    else if (port >= P_PIO14 && port < P_PIO14 + 4) eingaengeSetzen();
    else if (port >= P_PIO11 && port < P_PIO11 + 4) eingaengeSetzen();
    ketteNeu();
}

// ─── Speicher ────────────────────────────────────────────────────────────────

uint8_t K6316Steuerung::memRead(uint16_t a)
{
    if (a < ROM_ENDE) {
        return cfg_.rom_extern[a];
    }
    if (a >= RAM_BASIS && a < RAM_BASIS + RAM_GROESSE) return ram_[a - RAM_BASIS];
    return 0xFF;
}

void K6316Steuerung::memWrite(uint16_t a, uint8_t d)
{
    if (a >= RAM_BASIS && a < RAM_BASIS + RAM_GROESSE) ram_[a - RAM_BASIS] = d;
}

// ─── Takt ────────────────────────────────────────────────────────────────────

bool K6316Steuerung::clockTick(int ticks)
{
    bool dirty = eingang_neu_;   // Taste/Schalter von aussen: ein PIO-Interrupt kann anstehen
    eingang_neu_ = false;
    dirty |= ctc5_.clockTick(ticks);
    dirty |= ctc10_.clockTick(ticks);
    if (druck_.takt(ticks)) dirty = true;
    if (senke_) dirty |= sioSenke();
    if (dirty) ketteNeu();
    return dirty;
}

bool K6316Steuerung::sioSenke()
{
    bool any = false;
    Z80SIO::Channel* k[2] = {&sio_.channelA(), &sio_.channelB()};
    for (int i = 0; i < 2; ++i) {
        while (k[i]->senderHatZeichen()) {
            const uint8_t b = k[i]->txGet();
            if (sio_aus_[i].size() < SIO_AUS_MAX) sio_aus_[i].push_back(static_cast<char>(b));
            any = true;
        }
    }
    return any;
}

// ─── Interruptkette: CTC D5 -> CTC D10 -> PIO D12 -> SIO ─────────────────────

void K6316Steuerung::ketteNeu()
{
    bool prev = iei_in_;
    for (auto* c : kette_) {
        c->setIEI(prev);
        prev = prev && c->getIEO();
    }
    ieo_ = prev;
}

void K6316Steuerung::setIEI(bool iei)
{
    iei_in_ = iei;
    ketteNeu();
}

bool K6316Steuerung::hasInterrupt() const
{
    for (auto* c : kette_)
        if (c->hasInterrupt()) return true;
    return false;
}

uint8_t K6316Steuerung::getVector() const
{
    uint8_t v = 0xFF;
    for (auto* c : kette_)
        if (c->hasInterrupt()) { v = c->getVector(); break; }
    const_cast<K6316Steuerung*>(this)->ketteNeu();
    return v;
}

const char* K6316Steuerung::intDeviceName() const
{
    for (std::size_t i = 0; i < kette_.size(); ++i)
        if (kette_[i]->hasInterrupt()) return KETTE_NAMEN[i];
    return "K6316";
}

bool K6316Steuerung::inService() const
{
    for (auto* c : kette_)
        if (c->inService()) return true;
    return false;
}

void K6316Steuerung::onRETI()
{
    for (auto* c : kette_)
        if (c->inService()) {
            c->setIEI(true);
            c->onRETI();
            break;
        }
    ketteNeu();
}
