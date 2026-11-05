/**
 * @file sd1157_steuerung.cpp
 * @brief Steuerung des SD 1157 — Speicher, E/A-Dekodierung, Interruptkette, Eingangspegel.
 * @see sd1157_steuerung.h, doc/printer/sd1157/hardware.md
 */

#include "core/cards/printer/sd1157_steuerung.h"
#include "core/cards/printer/rom_sd1157.h"
#include "core/cards/printer/rom_sd1157_zg.h"
#include "core/logger.h"

namespace {
// E/A-Adressen, hardware.md §4
constexpr uint8_t P_SCHALTER3 = 0x6F;   // IN
constexpr uint8_t P_SIO       = 0x7C;   // 7CH–7FH
constexpr uint8_t P_BCH       = 0xBC;   // IN, 8212-System
constexpr uint8_t P_NAD18     = 0xBE;   // OUT, Nadel 1–8
constexpr uint8_t P_NAD9      = 0xBF;   // OUT, Nadel 9 (D7)
constexpr uint8_t P_CTC       = 0xDC;   // DCH–DFH
constexpr uint8_t P_PIO3      = 0xEC;   // ECH–EFH
constexpr uint8_t P_PIO1      = 0xF4;   // F4H–F7H
constexpr uint8_t P_PIO2      = 0xF8;   // F8H–FBH

constexpr std::size_t SIO_AUS_MAX = 1u << 16;   // Senke: hoechstens so viele Zeichen merken
}  // namespace

Sd1157Steuerung::Sd1157Steuerung(K1520Bus& bus) : Sd1157Steuerung(bus, Config{}) {}

namespace {
// Druckwerk nach Ausfuehrung: Bahnzahl (EBD) und Blattbreite (BDA) sind Schalter des Geraets.
NadeldruckParameter druckParameter(const Sd1157Steuerung::Config& c)
{
    NadeldruckParameter p = c.druck;
    p.bahnen = c.einbahn ? 1 : 2;
    p.blatt_breite = c.breit ? 2300 : 1500;   // Papier ueber den Randsensor hinaus [?]
    return p;
}
std::unique_ptr<Wagen> neuerWagen(const Sd1157Steuerung::Config& c)
{
    GeregelterWagen::Parameter w = c.wagen;
    w.rr_ab = c.breit ? 2200 : 1420;          // /RR ab Spalte 220 bzw. 142, hardware.md §10.1
    return std::make_unique<GeregelterWagen>(w);
}
}  // namespace

Sd1157Steuerung::Sd1157Steuerung(K1520Bus& bus, const Config& cfg)
    : bus_(bus), cfg_(cfg), online_(cfg.online), lpi6_(cfg.lpi6), kopf_dauernd_(cfg.kopf_dauernd),
      druck_(druckParameter(cfg), neuerWagen(cfg), cfg.papier)
{
    // Modus-3-Interrupts der PIOs sind EREIGNISSE (U855 §6.4: Uebergang Nichterfuellung -> Erfuellung), kein Pegel.
    // Ohne das loest jede Pegelauffrischung (eingaengeSetzen) bei /LR = 0 erneut den Randinterrupt aus; er setzt
    // die Spaltenzaehler (kpz = 7, h' = 6) mitten im Auslauf und nach jedem Papiervorschub zurueck, und der
    // Druck faehrt in die falsche Richtung (merkposten/drucker.md).
    pio1_.setModus3Flanke(true);
    pio2_.setModus3Flanke(true);
    pio3_.setModus3Flanke(true);
    // Kette in der Reihenfolge der Prioritaet (Annahme [?], hardware.md §8)
    kette_ = {&ctc_, &pio2_, &pio1_, &sio_, &pio3_};

    cpu_.readByte     = [this](uint16_t a)            { return bus_.memRead(a); };
    cpu_.writeByte    = [this](uint16_t a, uint8_t d) { bus_.memWrite(a, d); };
    cpu_.readPort     = [this](uint16_t p)            { return bus_.ioRead(p); };
    cpu_.writePort    = [this](uint16_t p, uint8_t d) { bus_.ioWrite(p, d); };
    cpu_.retiCallback = [this]()                      { bus_.signalRETI(); };
}

void Sd1157Steuerung::attachToBus(K1520Bus& bus)
{
    bus.registerIO(this, P_SCHALTER3, 1);
    bus.registerIO(this, P_SIO, 4);
    bus.registerIO(this, P_BCH, 1);
    bus.registerIO(this, P_NAD18, 2);
    bus.registerIO(this, P_CTC, 4);
    bus.registerIO(this, P_PIO3, 4);
    bus.registerIO(this, P_PIO1, 4);
    bus.registerIO(this, P_PIO2, 4);
    // 0000H–3BFFH in einem Stueck (256-B-ausgerichtet): ROM, Leerbereiche, RAM, ZG.
    // Die Aufteilung nimmt memRead/memWrite vor (hardware.md §2).
    bus.registerMem(this, 0x0000, RAM2_BASIS + RAM_GROESSE);
}

void Sd1157Steuerung::powerOn()
{
    ram1_.fill(cfg_.ram_fill);
    ram2_.fill(cfg_.ram_fill);
    druck_.netzEin(cfg_.wagen_start);   // Netz ein: Wagen auf der Startlage, Papierlage 0, leeres Blatt
    reset();
}

void Sd1157Steuerung::reset()
{
    ctc_.reset();
    pio1_.reset();
    pio2_.reset();
    pio3_.reset();
    sio_.reset();
    cpu_.reset();
    // /CTS und /DCD liegen fest aktiv: das V.24-Interface braucht beide, sonst bleiben Tx/Rx wegen
    // WR3 D5 (Auto-Enables) gesperrt (hardware.md §7); der IFSS hat keine Steuerleitungen.
    sio_.channelA().setzeDCD(true);
    sio_.channelA().setzeCTS(true);
    sio_.channelB().setzeDCD(true);
    sio_.channelB().setzeCTS(true);
    p1a_aus_ = p1b_aus_ = 0x08;   // Einschaltwerte nach §5.1 [?]
    p2a_aus_ = 0x10;
    p2b_aus_ = 0x18;
    nad18_ = nad9_ = 0xFF;
    druck_.halt();   // /RESET versetzt weder Wagen noch Papier: sie halten an, wo sie sind
    pegel_ = pegelNeu();
    sio_aus_[0].clear();
    sio_aus_[1].clear();
    eingaengeSetzen();
    ketteNeu();
}

// ─── Eingangspegel ───────────────────────────────────────────────────────────

uint8_t Sd1157Steuerung::bch() const
{
    // hardware.md §4.1
    return static_cast<uint8_t>((druck_.papier(0).papierDa() ? 0x01 : 0) | (lpi6_ ? 0x02 : 0) | (cfg_.einbahn ? 0x04 : 0) |
                                (cfg_.breit ? 0x08 : 0) | ((cfg_.formular_bahn1 & 3) << 4) |
                                ((cfg_.formular_bahn2 & 3) << 6));
}

uint8_t Sd1157Steuerung::pio3a() const
{
    // hardware.md §5.4: 0 = IFSS/1 = V.24, Bit 2/1 Stoppbits, 3 gerade, 4 Paritaet ein, 7..5 Baud
    return static_cast<uint8_t>((cfg_.schnittstelle == Config::Schnittstelle::V24 ? 0x01 : 0) |
                                ((cfg_.stopbits_code & 3) << 1) | (cfg_.paritaet_gerade ? 0x08 : 0) |
                                (cfg_.paritaet_ein ? 0x10 : 0) | ((cfg_.baud_code & 7) << 5));
}

uint8_t Sd1157Steuerung::pio3b() const
{
    // hardware.md §5.4: 0 ser/par (0 = seriell); 1 ONLINE (0 = online); 2/3 PV1/PV2, 6 TEST (0 = gedrueckt);
    // 4 Kopf dauernd (1); 5 ZFV = 1 lassen; 7 nur im ZFV=0-Zweig gelesen -> 1 [?]
    return static_cast<uint8_t>((online_ ? 0 : 0x02) | (taste_pv1_ ? 0 : 0x04) | (taste_pv2_ ? 0 : 0x08) |
                                (kopf_dauernd_ ? 0x10 : 0) | 0x20 | (taste_test_ ? 0 : 0x40) | 0x80);
}

void Sd1157Steuerung::eingaengeSetzen()
{
    // PIO 2 A: Eingaenge D0 (Takt vor), D3 (/VK), D5 (LAUF); PIO 2 B: D0 (Takt rueck)   [§5.2]
    const auto& p = pegel_;
    pio2_.setExternA(static_cast<uint8_t>((p.takt_vor ? 0x01 : 0) | (p.vk ? 0x08 : 0) | (p.lauf ? 0x20 : 0)), 0x29);
    pio2_.setExternB(static_cast<uint8_t>(p.takt_rueck ? 0x01 : 0), 0x01);
    // PIO 1 A: D4 (PV-Schritt 1, aktiv low), D5 (/LR); PIO 1 B: D0 (INA = 0), D4 (PV-Schritt 2), D5 (/RR)   [§5.3]
    pio1_.setExternA(static_cast<uint8_t>((p.pv1 ? 0x10 : 0) | (p.lr ? 0x20 : 0)), 0x30);
    pio1_.setExternB(static_cast<uint8_t>((p.pv2 ? 0x10 : 0) | (p.rr ? 0x20 : 0)), 0x31);
    // PIO 3: alles Eingaenge (nach der Umprogrammierung von A auf Modus 3), §5.4
    pio3_.setExternA(pio3a(), 0xFF);
    pio3_.setExternB(pio3b(), 0xFF);
}

void Sd1157Steuerung::mechanikAusgaenge()
{
    WagenEingang w;
    w.lfg       = (p2a_aus_ & 0x40) != 0;   // 2A.6 LFG
    w.vorwaerts = (p2a_aus_ & 0x10) != 0;   // 2A.4 VW
    w.schnell   = (p2a_aus_ & 0x04) != 0;   // 2A.2 VTAB
    druck_.wagen().eingang(w);
    // Papier: 1A.2 XPFG1, 1A.3 XPWV1, 1A.1 FRH1; Bahn 2 ebenso an PIO 1 B (nur beim Zweibahnendrucker)
    druck_.papier(0).ausgaenge((p1a_aus_ & 0x04) != 0, (p1a_aus_ & 0x08) != 0, (p1a_aus_ & 0x02) != 0);
    if (druck_.bahnen() > 1)
        druck_.papier(1).ausgaenge((p1b_aus_ & 0x04) != 0, (p1b_aus_ & 0x08) != 0, (p1b_aus_ & 0x02) != 0);
    mechanikPegel();
}

Sd1157Steuerung::Pegel Sd1157Steuerung::pegelNeu() const
{
    const WagenRueckmeldung r = druck_.wagen().rueckmeldung();
    Pegel p;
    p.lauf = r.lauf;
    p.takt_vor = r.takt_vor;
    p.takt_rueck = r.takt_rueck;
    p.vk = r.vk;
    p.lr = r.lr;
    p.rr = r.rr;
    p.pv1 = !druck_.papier(0).impuls();   // aktiv low
    p.pv2 = druck_.bahnen() > 1 ? !druck_.papier(1).impuls() : true;
    return p;
}

void Sd1157Steuerung::mechanikPegel()
{
    const Pegel p = pegelNeu();
    if (p == pegel_) return;
    pegel_ = p;
    eingaengeSetzen();
}

// ─── Bedienfeld ──────────────────────────────────────────────────────────────

void Sd1157Steuerung::setOnline(bool v)       { online_ = v;      eingaengeSetzen(); }
void Sd1157Steuerung::setTastePV1(bool g)     { taste_pv1_ = g;   eingaengeSetzen(); }
void Sd1157Steuerung::setTastePV2(bool g)     { taste_pv2_ = g;   eingaengeSetzen(); }
void Sd1157Steuerung::setTasteTest(bool g)    { taste_test_ = g;  eingaengeSetzen(); }
void Sd1157Steuerung::setPapierDa(bool da)    { druck_.papier(0).setzePapierEnde(!da); }
void Sd1157Steuerung::setLpi6(bool v)         { lpi6_ = v; }
void Sd1157Steuerung::setKopf(bool dauernd)   { kopf_dauernd_ = dauernd; eingaengeSetzen(); }

uint8_t Sd1157Steuerung::portAusgabe(int pio, int kanal) const
{
    if (pio == 1) return kanal ? p1b_aus_ : p1a_aus_;
    if (pio == 2) return kanal ? p2b_aus_ : p2a_aus_;
    return 0xFF;
}

// ─── E/A ─────────────────────────────────────────────────────────────────────

uint8_t Sd1157Steuerung::ioRead(uint8_t port)
{
    uint8_t r = 0xFF;
    if (port == P_SCHALTER3) return cfg_.schalter_6f;
    if (port == P_BCH) return bch();
    if (port == P_NAD18 || port == P_NAD9) return 0xFF;   // nur Ausgabe
    if (uint8_t(port - P_SIO) < 4) {
        r = sio_.ioRead(chipPort(port - P_SIO));
    } else if (uint8_t(port - P_CTC) < 4) {
        r = ctc_.ioRead(port - P_CTC);
    } else if (uint8_t(port - P_PIO3) < 4) {
        r = pio3_.ioRead(chipPort(port - P_PIO3));
    } else if (uint8_t(port - P_PIO1) < 4) {
        r = pio1_.ioRead(chipPort(port - P_PIO1));
    } else if (uint8_t(port - P_PIO2) < 4) {
        r = pio2_.ioRead(chipPort(port - P_PIO2));
    }
    ketteNeu();
    return r;
}

void Sd1157Steuerung::ioWrite(uint8_t port, uint8_t data)
{
    if (port == P_NAD18) {   // Nadelmuster nur speichern, kein Ausloesen beim Schreiben (§15 Nr. 7)
        nad18_ = data;
        return;
    }
    if (port == P_NAD9) {
        nad9_ = data;        // nur D7 wirkt (§4); der Rest bleibt stehen, nadeln() wertet D7 aus
        return;
    }
    if (port == P_SCHALTER3 || port == P_BCH) return;   // nur Eingabe
    if (uint8_t(port - P_SIO) < 4) {
        sio_.ioWrite(chipPort(port - P_SIO), data);
    } else if (uint8_t(port - P_CTC) < 4) {
        ctc_.ioWrite(port - P_CTC, data);
    } else if (uint8_t(port - P_PIO3) < 4) {
        const uint8_t rel = port - P_PIO3;
        pio3_.ioWrite(chipPort(rel), data);
        if (rel & 2) eingaengeSetzen();
    } else if (uint8_t(port - P_PIO1) < 4) {
        const uint8_t rel = port - P_PIO1;
        pio1_.ioWrite(chipPort(rel), data);
        if (rel & 2) { eingaengeSetzen(); }
        else {
            (rel & 1 ? p1b_aus_ : p1a_aus_) = data;
            mechanikAusgaenge();
        }
    } else if (uint8_t(port - P_PIO2) < 4) {
        const uint8_t rel = port - P_PIO2;
        pio2_.ioWrite(chipPort(rel), data);
        if (rel & 2) { eingaengeSetzen(); }
        else {
            uint8_t& alt = (rel & 1 ? p2b_aus_ : p2a_aus_);
            const uint8_t vorher = alt;
            alt = data;
            // FGDI (2B.5) 0 -> 1: Nadelanschlag mit dem Latch BEH/BFH (hardware.md §10.4, §15 Nr. 14)
            if ((rel & 1) && (data & 0x20) && !(vorher & 0x20)) druck_.feuere(nadeln(), kopfAngeschwenkt());
            mechanikAusgaenge();
        }
    }
    ketteNeu();
}

// ─── Speicher ────────────────────────────────────────────────────────────────

uint8_t Sd1157Steuerung::memRead(uint16_t a)
{
    if (a < ROM_ENDE) return SD1157_ROM[a];
    if (a < RAM1_BASIS) return 0xFF;                       // 2980H–2BFFH unbelegt [?] §14 Nr. 14
    if (a < ZG1_BASIS) return ram1_[a - RAM1_BASIS];
    if (a < ZG2_BASIS) return SD1157_ZG1[a - ZG1_BASIS];
    if (a < RAM2_BASIS) return 0xFF;                       // ZG2 nicht bestueckt: FFH, nicht 00H (§2)
    if (a < RAM2_BASIS + RAM_GROESSE) return ram2_[a - RAM2_BASIS];
    return 0xFF;
}

void Sd1157Steuerung::memWrite(uint16_t a, uint8_t d)
{
    if (a >= RAM1_BASIS && a < ZG1_BASIS) ram1_[a - RAM1_BASIS] = d;
    else if (a >= RAM2_BASIS && a < RAM2_BASIS + RAM_GROESSE) ram2_[a - RAM2_BASIS] = d;
    // ROM, ZG und Leerbereiche schlucken den Schreibzyklus
}

// ─── Takt ────────────────────────────────────────────────────────────────────

bool Sd1157Steuerung::clockTick(int ticks)
{
    bool dirty = ctc_.clockTick(ticks);
    if (druck_.takt(ticks)) {
        const Pegel p = pegelNeu();
        if (!(p == pegel_)) { pegel_ = p; eingaengeSetzen(); }
        dirty = true;
    }
    dirty |= sioSenke();
    if (dirty) ketteNeu();
    return dirty;
}

bool Sd1157Steuerung::sioSenke()
{
    // Kein Anschluss nach aussen (WP4): was der Sender abgibt, wird sofort abgenommen und gemerkt.
    // Ohne das bliebe das erste DC1 im Sendepuffer und die Firmware wartete auf den Sender.
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

// ─── Interruptkette: CTC -> PIO 2 -> PIO 1 -> SIO -> PIO 3 [?] ───────────────

void Sd1157Steuerung::ketteNeu()
{
    // Z80SIO::getIEO liefert auch bei IEI = low true (Verdacht, Datenblatt: IEO folgt IEI);
    // darum wird der Durchlass hier mit dem eigenen IEI verundet.
    bool prev = iei_in_;
    for (auto* c : kette_) {
        c->setIEI(prev);
        prev = prev && c->getIEO();
    }
    ieo_ = prev;
}

void Sd1157Steuerung::setIEI(bool iei)
{
    iei_in_ = iei;
    ketteNeu();
}

bool Sd1157Steuerung::hasInterrupt() const
{
    for (auto* c : kette_)
        if (c->hasInterrupt()) return true;
    return false;
}

uint8_t Sd1157Steuerung::getVector() const
{
    uint8_t v = 0xFF;
    for (auto* c : kette_)
        if (c->hasInterrupt()) { v = c->getVector(); break; }
    const_cast<Sd1157Steuerung*>(this)->ketteNeu();
    return v;
}

const char* Sd1157Steuerung::intDeviceName() const
{
    for (std::size_t i = 0; i < kette_.size(); ++i)
        if (kette_[i]->hasInterrupt()) return KETTE_NAMEN[i];
    return "SD1157";
}

bool Sd1157Steuerung::inService() const
{
    for (auto* c : kette_)
        if (c->inService()) return true;
    return false;
}

void Sd1157Steuerung::onRETI()
{
    // Das RETI nimmt der erste Baustein in Bedienung an (Daisy-Chain); wie Mc80Asp::onRETI.
    for (auto* c : kette_)
        if (c->inService()) {
            c->setIEI(true);
            c->onRETI();
            break;
        }
    ketteNeu();
}
