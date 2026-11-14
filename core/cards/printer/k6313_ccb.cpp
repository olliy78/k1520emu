/**
 * @file k6313_ccb.cpp
 * @brief CCB des K6313 — Speicher, E/A (eine Leitung tief), Tastenmatrix, Interruptkette, Mechanik-Pegel.
 * @see k6313_ccb.h, doc/printer/k6313/hardware.md
 */

#include "core/cards/printer/k6313_ccb.h"
#include "core/cards/printer/rom_k6311_12128062.h"
#include "core/cards/printer/rom_k6313_33538013.h"
#include "core/logger.h"
#include "core/serial/sio_format.h"

namespace {
// E/A-Adressen: jeder Baustein an einer Adressleitung A2...A7 (hardware.md §2.2)
constexpr uint8_t P_SIO    = 0x7C;   // A7 = 0: SIO der V.24-Kassette (Plan der Kassette fehlt: nur aus der Firmware [?])
constexpr uint8_t P_CENTRO = 0xBC;   // A6 = 0: PIO der Centronics-Kassette (auf der V.24-Kassette nicht bestueckt)
constexpr uint8_t P_PIO105 = 0xDC;   // A5 = 0
constexpr uint8_t P_CTC102 = 0xEC;   // A4 = 0
constexpr uint8_t P_CTC101 = 0xF4;   // A3 = 0
constexpr uint8_t P_PIO104 = 0xF8;   // A2 = 0

constexpr std::size_t SIO_AUS_MAX = 1u << 16;

constexpr int D(int paar, int n) { return K6313Ccb::dilIndex(paar, n); }

// Tastenmatrix: [Waehlcode][Zeile] -> Eintrag.  >= 0 DIL-Index, -1..-4 Taste 1..4, 0x7F = leer.
// Ablesung aus der Firmware 33538013 (Einlesen 0041H-005BH legt die Nibbles in 3849H-384CH ab: 3849H = Spalte 7:6, 384AH = 2:0,
// 384BH = 4:5, 384CH = 3; jede Zeile = Bit des Nibbles).  Gesichert (Befund = Auswertung im ROM):
//   6-1  Spalte 6/Zeile 1   (IX+37) Bit 1 -> Papierendekontakt, 0074H
//   14-2/15-2/15-1  Spalte 5/Zeile 0/2/3   Bit 0/2/3 -> Baud 200/1200/4800 (0613H-063CH), dazu 16-1/16-2 (Bit 5/4 -> Paritaet),
//   17-x (Bit 6/7 -> RTS/Protokoll), 18-2/18-1 (Spalte 3, Bit 0/1 -> Stoppbits/Datenbits, 0677H/0689H)
//   8-x/9-x   Spalte 7, Bit 4/5/6/7 -> Landestabelle (0469H-0494H: Bit 6 = Index 8, Bit 7 = +4, Bits 5/4 = Index)
//   12-x      Spalte 2 Bit 5/4 -> Formatlaenge 11 Zoll / halbiert (042AH); 13-1 = Bit 7 (1 Zoll Vorschub)
//   11-2      Spalte 0 Bit 2 -> Zeilenabstand 1BH (27/216) / 24H (36/216) (0459H); 11-1 = Bit 3 (Null 7FH, 0C66H)
// Aus dem Muster (je Spalte zwei Paare, zuerst -2, dann -1) und dem Ausschluss belegt, aber ohne eigenen Beleg [?]:
//   5-1/5-2 (Spalte 3, Zeile 3/2; die Normalstellung 018CH liest Zeile 2 = 1, Zeile 3 = 0), 6-2 (Spalte 6 Zeile 0, Summer 055CH),
//   7-1/7-2 (Spalte 6 Zeile 3/2), 10-1/10-2 (Spalte 0 Zeile 1/0), 13-2 (Spalte 2 Zeile 2).
// Spalte 1: Tasten.  Zeile 0..2 liest die Firmware beim Einschalten als Einschalttasten (00D5H-00E6H); Zeile 3 = ON/OFF LINE
// (02AFH-02B6H loescht LON).  Welche der Zeilen 0/1/2 LF zurueck / LF vor / FF ist, folgt aus dem Verhalten (0300H: Zeile 0 ruft
// 0B09H = Zeilenvorschub rueckwaerts, 02D0H: Zeile 1 = dauernder Vorschub, 0318H: Zeile 2 -> 09F5H = Formularvorschub) [?].
constexpr int8_t MX_LEER = 0x7F;
struct MatrixZeile { int8_t z[4]; };
constexpr MatrixZeile MATRIX[8] = {
    /* 0 */ {{(int8_t)D(10, 2), (int8_t)D(10, 1), (int8_t)D(11, 2), (int8_t)D(11, 1)}},
    /* 1 */ {{-3 /* LF zurueck */, -2 /* LF vor */, -4 /* FF */, -1 /* ON/OFF LINE */}},
    /* 2 */ {{(int8_t)D(12, 2), (int8_t)D(12, 1), (int8_t)D(13, 2), (int8_t)D(13, 1)}},
    /* 3 */ {{(int8_t)D(18, 2), (int8_t)D(18, 1), (int8_t)D(5, 2), (int8_t)D(5, 1)}},
    /* 4 */ {{(int8_t)D(16, 2), (int8_t)D(16, 1), (int8_t)D(17, 2), (int8_t)D(17, 1)}},
    /* 5 */ {{(int8_t)D(14, 2), (int8_t)D(14, 1), (int8_t)D(15, 2), (int8_t)D(15, 1)}},
    /* 6 */ {{(int8_t)D(6, 2), (int8_t)D(6, 1), (int8_t)D(7, 2), (int8_t)D(7, 1)}},
    /* 7 */ {{(int8_t)D(8, 2), (int8_t)D(8, 1), (int8_t)D(9, 2), (int8_t)D(9, 1)}},
};
}  // namespace

int K6313Ccb::matrixZelle(int spalte, int zeile)
{
    if (spalte < 0 || spalte > 7 || zeile < 0 || zeile > 3) return MX_LEER;
    return MATRIX[spalte].z[zeile];
}

// ─── Anschluss je SIO-Kanal ──────────────────────────────────────────────────
// V.24-Kassette: Daten, CTS und DCD an Kanal A (die Firmware richtet A als Datenkanal ein, WR3 = 61H mit Auto Enables); DTR/RTS (Flusssteuerung,
// WR5 = 88H/08H bzw. +02H) schreibt sie an Kanal B (0641H-0663H, 1EEEH-1EFDH) [F]: die V.24-Leitungen DTR/RTS kommen also vom Kanal B.
// IFSS-Kassette (WP8g, `Config::ifss`, Merkposten drucker.md Nr. 36): es ist immer genau EIN Anschluss wirksam, der andere ohne Funktion.
//   K6313: alle K6313-Abzuege fahren die Daten ueber Kanal A (7CH/7EH; Kanal-B-Daten 7DH nutzt keiner [F]) - die "CL"-Abzuege (Stromschleife) haben
//   keinen eigenen Firmwarezweig, die Stromschleife ist eine andere Kassette hinter demselben SIO-Kanal [?, Kassettenplan fehlt]; ohne Steuerleitungen.
//   K6311: der IFSS-Abzug 13325212 faehrt die Daten ueber Kanal B (7DH/7FH, WR5 = AAH), Kanal A ruht [F]; ebenfalls ohne Steuerleitungen.

class K6313Ccb::Anschluss : public k1520::serial::SerialAnschluss {
public:
    Anschluss(K6313Ccb& k, bool v24) : k_(k), v24_(v24) {}

    const char* name() const override { return v24_ ? "V.24" : "IFSS"; }
    const char* stecker() const override { return v24_ ? "V.24" : "IFSS"; }
    bool v24() const override { return v24_; }

    k1520::serial::SerialFormat format() const override
    {
        return k1520::serial::serialFormatAusSio(kanal(), k_.baudTakte());
    }
    bool senderHatZeichen() const override { return aktiv() && kanal().senderHatZeichen(); }
    uint8_t senderNimm() override
    {
        if (!aktiv()) return 0;
        Z80SIO::Channel& a = kanal();
        uint8_t b = a.txGet();
        const uint8_t bits = a.format().tx_bits;
        if (bits < 8) b &= static_cast<uint8_t>((1u << bits) - 1);
        std::string& m = k_.sio_aus_[kanalNr()];
        if (m.size() < SIO_AUS_MAX) m.push_back(static_cast<char>(b));
        k_.ketteNeu();
        k_.seriell_geaendert_ = true;
        if (!belegt_ && abnehmer_) abnehmer_(b);
        return b;
    }
    bool empfaengerFrei() const override { return aktiv() && kanal().empfaengerFrei(); }
    void empfange(uint8_t b) override
    {
        if (!aktiv()) return;
        kanal().rxByte(b);
        k_.ketteNeu();
        k_.seriell_geaendert_ = true;
    }
    bool rts() const override { return v24_ && aktiv() && k_.sio_.channelB().rts(); }
    bool dtr() const override { return v24_ && aktiv() && k_.sio_.channelB().dtr(); }
    void setzeEingaenge(bool cts, bool /*dsr*/, bool dcd) override
    {
        if (!v24_ || !aktiv()) return;   // IFSS: keine Steuerleitungen, /CTS und /DCD liegen fest aktiv (reset())
        k_.sio_.channelA().setzeCTS(cts);
        k_.sio_.channelA().setzeDCD(dcd);
        k_.ketteNeu();
        k_.seriell_geaendert_ = true;
    }
    bool breakGesendet() const override { return aktiv() && kanal().breakSenden(); }
    void breakEmpfang(bool an) override
    {
        if (!aktiv()) return;
        kanal().setzeBreakEmpfang(an);
        k_.ketteNeu();
        k_.seriell_geaendert_ = true;
    }
    void leitungBelegt(bool belegt) override { belegt_ = belegt; }

    void setAbnehmer(std::function<void(uint8_t)> cb) { abnehmer_ = std::move(cb); }
    void einspeisen(uint8_t b)
    {
        if (belegt_) return;
        empfange(b);
    }

private:
    bool aktiv() const { return v24_ != k_.cfg_.ifss; }   ///< gesteckt ist genau eine Kassette
    int  kanalNr() const { return (!v24_ && k_.cfg_.k6311) ? 1 : 0; }   ///< SIO-Datenkanal: 0 = A, 1 = B (nur K6311-IFSS)
    Z80SIO::Channel& kanal() const { return kanalNr() ? k_.sio_.channelB() : k_.sio_.channelA(); }

    K6313Ccb& k_;
    bool      v24_;
    bool      belegt_ = false;
    std::function<void(uint8_t)> abnehmer_;
};

K6313Ccb::~K6313Ccb() = default;

k1520::serial::SerialAnschluss& K6313Ccb::serialAnschluss(bool v24) { return *anschluesse_[v24 ? 1 : 0]; }
void K6313Ccb::serialAbnehmer(bool v24, std::function<void(uint8_t)> cb) { anschluesse_[v24 ? 1 : 0]->setAbnehmer(std::move(cb)); }
void K6313Ccb::serialEinspeisen(bool v24, uint8_t byte) { anschluesse_[v24 ? 1 : 0]->einspeisen(byte); }

uint64_t K6313Ccb::baudTakte() const
{
    return ctc102_.teilerTakte(2);   // D102 Kanal 2 = RTC, Sende-/Empfangstakt der SIO (hardware.md §4)
}

K6313Ccb::K6313Ccb(K1520Bus& bus) : K6313Ccb(bus, Config{}) {}

namespace {
NadeldruckParameter druckParameter(const K6313Ccb::Config& c)
{
    NadeldruckParameter p = c.druck;
    p.bahnen = 1;
    return p;
}
}  // namespace

K6313Ccb::K6313Ccb(K1520Bus& bus, const Config& cfg)
    : bus_(bus), cfg_(cfg), pe_(!cfg.papier_da), dil_(cfg.dil),
      druck_(druckParameter(cfg), std::make_unique<SchrittWagen>(cfg.wagen), cfg.papier)
{
    if (!cfg_.k6311 && cfg_.rom_extern.size() == ROM_ENDE_LANG) cfg_.lang = true;   // 16-KiB-Abzug: Speicherplan "lang"
    if (cfg_.lang && cfg_.rom_extern.size() != ROM_ENDE_LANG) cfg_.rom_extern.assign(ROM_ENDE_LANG, 0xFF);   // ohne Abzug nichts zu fahren (Aufrufer pruefen)
    if (cfg_.k6311 && cfg_.rom_extern.size() == ROM_ENDE_8K) cfg_.rom_extern.resize(ROM_ENDE, 0xFF);   // vier Chips: der fuenfte Sockel liest FFH [?]
    pio104_.setModus3Flanke(true);
    pio105_.setModus3Flanke(true);
    // offene Eingangsbits lesen 0 (Zeilen der Tastenmatrix: Taste/Schalter legen die gewaehlte Spalte darauf, Firmware 02D0H: gedrueckt = 1)
    pio104_.setPullupsA(0);
    pio104_.setPullupsB(0);
    pio105_.setPullupsA(0);
    pio105_.setPullupsB(0);
    anschluesse_[0] = std::make_unique<Anschluss>(*this, false);
    anschluesse_[1] = std::make_unique<Anschluss>(*this, true);
    kette_ = {&ctc101_, &ctc102_, &pio104_, &pio105_, &sio_};
    // HCP (D102 ZC/TO0, Draht 57) speist C/TRG1 und C/TRG2 von D101 [S:4]: die Firmware zaehlt damit Chopper-Takte (Spalten- und Schrittzeitbasis,
    // 1BF0H-1C0AH, ISR 1F10H/1C17H).  C7H = Zaehlbetrieb auf der fallenden Flanke; der Rueckruf reicht beide Pegel durch, clkTrg waehlt.
    ctc102_.setZCTOCallback([this](int ch, bool lvl) {
        if (ch == 0) { ctc101_.clkTrg(1, lvl); ctc101_.clkTrg(2, lvl); eingang_neu_ = true; }   // eingang_neu_: Interruptkette neu bewerten
    }, 0x01);

    cpu_.readByte     = [this](uint16_t a)            { return bus_.memRead(a); };
    cpu_.writeByte    = [this](uint16_t a, uint8_t d) { bus_.memWrite(a, d); };
    cpu_.readPort     = [this](uint16_t p)            { return bus_.ioRead(p); };
    cpu_.writePort    = [this](uint16_t p, uint8_t d) { bus_.ioWrite(p, d); };
    cpu_.retiCallback = [this]()                      { bus_.signalRETI(); };
}

void K6313Ccb::attachToBus(K1520Bus& bus)
{
    bus.registerIO(this, P_SIO, 4);
    bus.registerIO(this, P_CENTRO, 4);
    bus.registerIO(this, P_PIO105, 4);
    bus.registerIO(this, P_CTC102, 4);
    bus.registerIO(this, P_CTC101, 4);
    bus.registerIO(this, P_PIO104, 4);
    if (cfg_.lang) {
        bus.registerMem(this, 0x0000, ROM_ENDE_LANG);
        bus.registerMem(this, RAM_LANG_BASIS, RAM_LANG_GROESSE);
        return;
    }
    bus.registerMem(this, 0x0000, RAM_BASIS + RAM_GROESSE);   // ROM, 3000H-Fenster, Luecke und 3800H-Fenster in einem Bereich; memRead/-Write entscheiden
}

void K6313Ccb::powerOn()
{
    ram_.fill(cfg_.ram_fill);
    druck_.netzEin(cfg_.wagen_start);
    reset();
}

void K6313Ccb::reset()
{
    ctc101_.reset();
    ctc102_.reset();
    pio104_.reset();
    pio105_.reset();
    sio_.reset();
    cpu_.reset();
    // V.24: /CTS und /DCD kommen vom Host (Wandler/Gegenstelle), ohne Gegenstelle inaktiv (wie K6316 V.24).
    // IFSS (WP8g): keine Steuerleitungen, /CTS und /DCD fest aktiv (auch am Kanal A des K6313: dort liegen die Daten).
    sio_.channelA().setzeDCD(cfg_.ifss);
    sio_.channelA().setzeCTS(cfg_.ifss);
    sio_.channelB().setzeDCD(true);
    sio_.channelB().setzeCTS(true);
    druck_.halt();
    home_ = cfg_.k6311 && druck_.wagen().rueckmeldung().g1;
    tor_offen_ = false;
    cmt_beobachtet_ = false;
    cmtBeobachten();   // Kanal 2 steht nach dem Reset im Ruhezustand: D115 transparent
    nadeln_aktiv_ = 0;
    g1_ = druck_.wagen().rueckmeldung().g1;
    sio_aus_[0].clear();
    sio_aus_[1].clear();
    eingaengeSetzen();
    ketteNeu();
}

// ─── Eingangspegel ───────────────────────────────────────────────────────────

uint8_t K6313Ccb::zeilen() const
{
    const int sp = matrixSpalte();
    uint8_t r = 0;
    for (int z = 0; z < 4; ++z) {
        const int e = MATRIX[sp].z[z];
        bool zu = false;
        if (e >= 0) zu = (dil_ >> e) & 1;
        else if (e >= -4) zu = taste_[-e - 1];
        if (zu) r |= static_cast<uint8_t>(1u << z);
    }
    return r;
}

void K6313Ccb::eingaengeSetzen()
{
    eingang_neu_ = true;
    if (cfg_.k6311) { eingaengeSetzen6311(); return; }
    // D105 A: Bit 0...3 = Zeilen KB01...KB04 (Eingaenge, Maske 0FH, 0041H)
    pio105_.setExternA(zeilen(), 0x0F);
    // D104 A: Bit 0 = Eingang, unbenannt (Maske 05H) -> Taktscheibe [?: die Firmware wertet nur die Flanken "A0 high" aus (1ACFH, ISR 1B30H/1B3CH:
    // Zeitgeber D101 K3 bei jeder Flanke neu starten = "Wagen steht"); Zeichnung: Pin 15 mit unklarem Anschluss neben /CM1]; Bit 2 = PE (1 = Papierende)
    pio104_.setExternA(static_cast<uint8_t>((g1_ ? 0x01 : 0) | (pe_ ? 0x04 : 0)), 0x05);
    // D104 B: Bit 4 = PE1 (Kassettenkennung, V.24 = 0), Bit 6 = TDS (Taktscheibe)
    pio104_.setExternB(static_cast<uint8_t>(g1_ ? 0x40 : 0), 0x50);
}

void K6313Ccb::ausgaengeAuswerten()
{
    if (cfg_.k6311) { ausgaengeAuswerten6311(); return; }
    const uint8_t a104 = pio104_.pinsA(0), b104 = pio104_.pinsB(0), b105 = pio105_.pinsB(0);
    // Wagenmotor /CM1...4 = D104 A1,3,5,7, Hochpegel = Phase bestromt (die Firmware rotiert das Muster in 3824H um 2 Bit, 1B7BH)
    cm_wunsch_ = 0;
    for (int k = 0; k < 4; ++k)
        if ((a104 >> (1 + 2 * k)) & 1) cm_wunsch_ |= static_cast<uint8_t>(1 << k);
    if (!cmtZaehlt()) phasenAnlegen();   // sonst uebernimmt D115 sie erst mit der naechsten CMT-Flanke (cmtBeobachten)
    // Formularmotor PFA1...4 = D104 B1,3,5,7 [?]
    uint8_t pp = 0;
    for (int k = 0; k < 4; ++k)
        if ((b104 >> (1 + 2 * k)) & 1) pp |= static_cast<uint8_t>(1 << k);
    druck_.papier(0).phasen(pp);
    // Nadeln: PP1...8 = D105 B7...B0, PP9 = D104 B0; Tiefpegel = Nadel [?: /IPC-Freigabe (D104 B2) nicht modelliert]
    // Vor der Einrichtung der Tore (Modus 3, Ausgang) sind die Pins hochohmig, die Endstufen sehen H: keine Nadel (sonst schluege das
    // Rueckgesetzte Register 0 beim Netz-Ein einen Punkt)
    uint16_t aktiv = 0;
    const auto s105 = pio105_.debugState(), s104 = pio104_.debugState();
    if (s105.port[1].mode == 3 && s105.port[1].dir == 0x00)
        for (int n = 1; n <= 8; ++n)
            if (!((b105 >> (8 - n)) & 1)) aktiv |= static_cast<uint16_t>(1u << (n - 1));
    if (s104.port[1].mode == 3 && !(s104.port[1].dir & 0x01) && !(b104 & 0x01)) aktiv |= 0x100;
    const uint16_t neu = static_cast<uint16_t>(aktiv & ~nadeln_aktiv_);
    nadeln_aktiv_ = aktiv;
    if (neu) druck_.feuere(neu, true);
    const bool g1 = druck_.wagen().rueckmeldung().g1;
    if (g1 != g1_) g1_ = g1;
}

// Phasenregister D115 (DL175): /CM1...4 -> /CMA1...4 mit der steigenden Flanke von CMT = D101 ZC/TO2 [S:4].  Laeuft Kanal 2 als Zeitgeber
// (Synchronisation: 17H, TC 1 = alle 16 Takte), ist das Register fuer die Mechanik transparent und wird nicht Takt fuer Takt nachgebildet;
// im Zaehlbetrieb (C7H, 9 HCP-Impulse je Schritt) schaltet der Wagen genau mit der CMT-Flanke statt mit dem Schreibzugriff der Firmware -
// das haelt die Spalten des Druckbilds auf dem Raster der HCP-Impulse (Interruptlatenz geht nicht mehr in die Wagenlage ein) [?].
bool K6313Ccb::cmtZaehlt() const { return (ctc101_.debugState().ch[2].control & 0x40) != 0; }

void K6313Ccb::phasenAnlegen()
{
    WagenEingang w;
    w.phasen = cm_wunsch_;
    druck_.wagen().eingang(w);
}

void K6313Ccb::cmtBeobachten()
{
    ctc101_.setZCTOCallback([this](int ch, bool lvl) { if (ch == 2 && lvl) { phasenAnlegen(); eingang_neu_ = true; } }, cmtZaehlt() ? 0x04 : 0x00);
}

// ─── Bedienfeld ──────────────────────────────────────────────────────────────

void K6313Ccb::setTaste(int nr, bool g)
{
    if (nr < 1 || nr > 4) return;
    taste_[nr - 1] = g;
    eingaengeSetzen();
}
void K6313Ccb::setPapierDa(bool da) { pe_ = !da; eingaengeSetzen(); }
void K6313Ccb::setDil(int paar, int n, bool ein)
{
    if (paar < 5 || paar > 18 || n < 1 || n > 2) return;
    const uint32_t m = 1u << dilIndex(paar, n);
    dil_ = ein ? (dil_ | m) : (dil_ & ~m);
    eingaengeSetzen();
}

bool K6313Ccb::lampe(int nr) const
{
    // K6311: Belegung von LON/ERR/Papierende unbekannt [?] -> nur Netz wird gemeldet
    if (cfg_.k6311) {
        // DC.2 = ON LINE (gesetzt beim Eintritt in die Leerlaufschleife 0180H, geloescht beim Offline-Gehen 0226H), DC.3 = Fehler (gesetzt bei Wachhund-
        // Ablauf 1375H und Haltepunkt 04ABH, nie geloescht) [F: Ablauf; die Lampenrolle ist erschlossen, nicht belegt [?]]; PE-Lampe: das ROM schreibt keine.
        switch (nr) {
            case 1: return (pio105_.pinsA(0) & 0x04) != 0;
            case 2: return true;
            case 3: return (pio105_.pinsA(0) & 0x08) != 0;
        }
        return false;
    }
    switch (nr) {
        case 1: return (pio104_.pinsA(0) & 0x40) != 0;   // ON LINE: LON (D104 A6)
        case 2: return true;                              // POW: Netz
        case 3: return (pio105_.pinsA(0) & 0x10) != 0;   // ERR: D105 A4 [?] Polaritaet
        case 4: return pe_;                               // PE
    }
    return false;
}

uint8_t K6313Ccb::portAusgabe(int pio, int kanal) const
{
    const Z80PIO& p = pio == 104 ? pio104_ : pio105_;
    return kanal ? p.pinsB(0) : p.pinsA(0);
}

// ─── E/A ─────────────────────────────────────────────────────────────────────

static bool imBereich(uint8_t port, uint8_t basis) { return port >= basis && port < basis + 4; }

uint8_t K6313Ccb::ioRead(uint8_t port)
{
    uint8_t r = 0xFF;
    if (imBereich(port, P_SIO))         r = sio_.ioRead(chipPort(port - P_SIO));
    else if (imBereich(port, P_PIO105)) r = pio105_.ioRead(chipPort(port - P_PIO105));
    else if (imBereich(port, P_CTC102)) r = ctc102_.ioRead(port - P_CTC102);
    else if (imBereich(port, P_CTC101)) r = ctc101_.ioRead(port - P_CTC101);
    else if (imBereich(port, P_PIO104)) r = pio104_.ioRead(chipPort(port - P_PIO104));
    // BCH-BFH: Centronics-PIO nicht bestueckt -> FFH (offener Bus).  K6311 (WP8e): BCH/BDH sind Schaltereingaenge [E: ROM 00AFH, 00FAH]; die
    // Firmware schreibt vorher Modus 3 / Maske FFH (alle Eingang) nach BEH/BFH [E: 009BH-00A1H], die Ausgabe ist damit ohne Wirkung.
    else if (cfg_.k6311 && port == P_CENTRO)     r = cfg_.schalter_baud;
    else if (cfg_.k6311 && port == P_CENTRO + 1) r = cfg_.schalter_format;
    ketteNeu();
    return r;
}

void K6313Ccb::ioWrite(uint8_t port, uint8_t data)
{
    bool pio_daten = false;
    if (imBereich(port, P_SIO)) {
        sio_.ioWrite(chipPort(port - P_SIO), data);
    } else if (imBereich(port, P_PIO105)) {
        pio105_.ioWrite(chipPort(port - P_PIO105), data);
        pio_daten = !((port - P_PIO105) & 2);
    } else if (imBereich(port, P_CTC102)) {
        ctc102_.ioWrite(port - P_CTC102, data);
    } else if (imBereich(port, P_CTC101)) {
        ctc101_.ioWrite(port - P_CTC101, data);
        if (port == P_CTC101 + 2) {   // Kanal 2 (CMT): Betriebsart kann sich geaendert haben
            const bool z = cmtZaehlt();
            if (z != cmt_beobachtet_) { cmt_beobachtet_ = z; cmtBeobachten(); }
            if (!z) { phasenAnlegen(); }   // Rueckfall in den Zeitgeberbetrieb: Register transparent
        }
    } else if (imBereich(port, P_PIO104)) {
        pio104_.ioWrite(chipPort(port - P_PIO104), data);
        pio_daten = !((port - P_PIO104) & 2);
    }
    if (pio_daten) ausgaengeAuswerten();
    if (cfg_.k6311 && imBereich(port, P_PIO105) && !((port - P_PIO105) & 2) && (port & 1) == 0) nadelnAuswerten6311();
    if (imBereich(port, P_PIO105) || imBereich(port, P_PIO104)) eingaengeSetzen();
    ketteNeu();
}

// ─── Speicher ────────────────────────────────────────────────────────────────

uint8_t K6313Ccb::memRead(uint16_t a)
{
    if (cfg_.lang) {   // Speicherplan "lang": 27128 ab 0000H, 2 KiB RAM bei F000H (A15 fehlt am Modul, sonst Leerlauf FFH)
        if (a < ROM_ENDE_LANG) return cfg_.rom_extern[a];
        if (a >= RAM_LANG_BASIS && a < RAM_LANG_BASIS + RAM_LANG_GROESSE) return ram_[a - RAM_LANG_BASIS];
        return 0xFF;
    }
    if (a < ROM_ENDE) return !cfg_.rom_extern.empty() ? cfg_.rom_extern[a] : cfg_.k6311 ? K6311_ROM_12128062[a] : K6313_ROM_33538013[a];
    if (a >= RAM_BASIS && a < RAM_BASIS + RAM_GROESSE) return ram_[a - RAM_BASIS];
    if (a >= RAM2_BASIS && a < RAM2_BASIS + RAM_GROESSE) return ram_[RAM_GROESSE + (a - RAM2_BASIS)];
    return 0xFF;
}

void K6313Ccb::memWrite(uint16_t a, uint8_t d)
{
    if (cfg_.lang) {
        if (a >= RAM_LANG_BASIS && a < RAM_LANG_BASIS + RAM_LANG_GROESSE) ram_[a - RAM_LANG_BASIS] = d;
        return;
    }
    if (a >= RAM_BASIS && a < RAM_BASIS + RAM_GROESSE) ram_[a - RAM_BASIS] = d;
    else if (a >= RAM2_BASIS && a < RAM2_BASIS + RAM_GROESSE) ram_[RAM_GROESSE + (a - RAM2_BASIS)] = d;
}

// ─── Takt ────────────────────────────────────────────────────────────────────

bool K6313Ccb::clockTick(int ticks)
{
    bool dirty = eingang_neu_;
    eingang_neu_ = false;
    dirty |= ctc101_.clockTick(ticks);
    dirty |= ctc102_.clockTick(ticks);
    if (druck_.takt(ticks)) dirty = true;
    // Taktscheibe folgt der Wagenlage: Pegel an D104 nachfuehren (Flanke -> Interrupt im Modus 3)
    const bool g1 = druck_.wagen().rueckmeldung().g1;
    if (g1 != g1_) { g1_ = g1; eingaengeSetzen(); dirty = true; }
    if (senke_) dirty |= sioSenke();
    if (dirty) ketteNeu();
    return dirty;
}

bool K6313Ccb::sioSenke()
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

// ─── Interruptkette ──────────────────────────────────────────────────────────

void K6313Ccb::ketteNeu()
{
    bool prev = iei_in_;
    for (auto* c : kette_) {
        c->setIEI(prev);
        prev = prev && c->getIEO();
    }
    ieo_ = prev;
}

void K6313Ccb::setIEI(bool iei)
{
    iei_in_ = iei;
    ketteNeu();
}

bool K6313Ccb::hasInterrupt() const
{
    for (auto* c : kette_)
        if (c->hasInterrupt()) return true;
    return false;
}

uint8_t K6313Ccb::getVector() const
{
    uint8_t v = 0xFF;
    for (auto* c : kette_)
        if (c->hasInterrupt()) { v = c->getVector(); break; }
    const_cast<K6313Ccb*>(this)->ketteNeu();
    return v;
}

const char* K6313Ccb::intDeviceName() const
{
    for (std::size_t i = 0; i < kette_.size(); ++i)
        if (kette_[i]->hasInterrupt()) return KETTE_NAMEN[i];
    return "K6313";
}

bool K6313Ccb::inService() const
{
    for (auto* c : kette_)
        if (c->inService()) return true;
    return false;
}

void K6313Ccb::onRETI()
{
    for (auto* c : kette_)
        if (c->inService()) {
            c->setIEI(true);
            c->onRETI();
            break;
        }
    ketteNeu();
}

// ─── K6311 (WP8f): Belegung nur aus dem ROM 12128062 erschlossen [?] ─────────────────────────────────────────────────
// Die Belegung folgt NICHT der des K6313; jede Zeile steht unter [?], soweit kein Trace-Beleg genannt ist.

void K6313Ccb::eingaengeSetzen6311()
{
    // D104 A (F8H), Maske 45H = A0, A2, A6 Eingang [F: 0F72H-0F7AH].
    //  A2 = Taste ON LINE (aktiv low): das ROM richtet den Interrupt "A2 low" ein (D7H/FBH an FAH, 018AH-0190H); bei 0 meldet es (IX+2).1 und geht in die
    //       Offline-Folge 01DBH-0350H, die A2 = 1 (Taste losgelassen) abwartet, im Offline-Zustand A2 = 0 als "wieder online" auswertet (0339H-034AH) [F]; Ruhe = 1.
    //  A6 = Papierende (1 = kein Papier) [?]: das ROM tastet ihn je 4 Papierschritte ab (1178H) und beim Anlauf (0192H), setzt dann (IX+78).1/(IX+79).5 und geht
    //       offline (01B0H), im Offline-Zustand sperrt A6 = 1 die Tastenabfrage (0236H-023AH) - die Rolle eines Papierende-Sensors ist erschlossen, nicht belegt.
    pio104_.setExternA(static_cast<uint8_t>((taste_[0] ? 0x00 : 0x04) | (pe_ ? 0x40 : 0x00)), 0x45);
    // D104 B (F9H), Maske 45H = B0, B2, B6 Eingang.  B0: das ROM richtet den Interrupt auf B0 ein (F7H bzw. D7H/FEH an FBH, 1284H/128DH) und setzt in ISR 138FH
    // den Wachhund F6H neu: B0 = Taktscheibe des Wagens, ein Wechsel je Vollschritt (wie /G1 des K6316/K6313) [?; ohne Impulse meldet der Wachhund nach
    // 26 ms Wagenstillstand (ISR 1356H: Motoren aus, DC.3 = Fehler)].  B2 wertet das ROM nur bei CR aus (0761H, "CALL NZ,0BF2": Zeile ausgeben); Bedeutung
    // unbekannt [?], Ruhepegel 0.  B6 liest das ROM nie.
    pio104_.setExternB(static_cast<uint8_t>(home_ ? 0x01 : 0x00), 0x45);
    // D105 A (DCH), Maske 70H = A4...A6 Eingang: Tasten, aktiv low (041CH: OR E / CP FFH, E = BFH bzw. DFH; 023FH BIT 4) [F]; Ruhe = 1.
    // Taste 2 = DC.4, 3 = DC.5, 4 = DC.6.
    uint8_t t = 0x70;
    for (int i = 1; i < 4; ++i) if (taste_[i]) t = static_cast<uint8_t>(t & ~(0x10 << (i - 1)));
    pio105_.setExternA(t, 0x70);
}

void K6313Ccb::ausgaengeAuswerten6311()
{
    const uint8_t a104 = pio104_.pinsA(0), b104 = pio104_.pinsB(0);
    // Wagenmotor = F9H B1, B3, B5, B7 (Phasen 1...4, hoch = bestromt), B4 = Leistung (Halten ohne B4 [F: AND AAH, 1109H]) [?].  Phasenfolge laut ROM:
    // Synchronisation (ISR 1061H, RLC) 93H-1EH-38H-F0H = Phasenpaare {4,1},{1,2},{2,3},{3,4}, aufsteigende Phasennummer; Heimatfahrt -> Wagen nach links.
    uint8_t cm = 0;
    for (int k = 0; k < 4; ++k)
        if ((b104 >> (1 + 2 * k)) & 1) cm |= static_cast<uint8_t>(1 << k);
    WagenEingang w;
    w.phasen = cm;
    druck_.wagen().eingang(w);
    const bool home = druck_.wagen().rueckmeldung().g1;
    if (home != home_) { home_ = home; eingaengeSetzen6311(); }
    // Papiermotor = F8H A4/A5 als Zwei-Bit-Code (00H-20H-30H-10H), A3 = Haltestrom/Freigabe [?]: wie eine Vollschrittfolge zweier Wicklungen
    // gedeutet; die Zuordnung Code -> Phasenpaar ist eine Annahme, die Richtung folgt aus dem Zeilenvorschub (Bild nach unten).
    static const uint8_t PAAR[4] = {0x03, 0x06, 0x0C, 0x09};   // Code (A5,A4) 00, 01, 11, 10
    const uint8_t code = static_cast<uint8_t>((a104 >> 4) & 3);
    static const int FOLGE[4] = {0, 1, 3, 2};                    // Bitmuster -> Rang in der Gray-Folge 00, 01, 11, 10
    druck_.papier(0).phasen(PAAR[FOLGE[code]]);
}

void K6313Ccb::nadelnAuswerten6311()
{
    // D105 B (DDH) = Nadeln 1...8 (Bit 7 = Nadel 1), D105 A (DCH) Bit 0 = Nadel 9, Bit 1 = Freigabe; 0 = aktiv [?].  Das ROM setzt zuerst DDH, dann
    // DCH (1D7EH-1D92H), arbeitet danach die Impulszeit mit CTC D101 Kanal 0 (F4H: 97H/37H = 880 Takte) ab und schaltet im Interrupt (1EDFH) die
    // Freigabe wieder aus.  Gefeuert wird mit der fallenden Flanke der Freigabe mit den dann anliegenden Daten.
    const uint8_t dc = pio105_.pinsA(0), dd = pio105_.pinsB(0);
    const bool offen = !(dc & 0x02);
    if (offen && !tor_offen_) {
        uint16_t aktiv = 0;
        for (int n = 1; n <= 8; ++n)
            if (!((dd >> (8 - n)) & 1)) aktiv |= static_cast<uint16_t>(1u << (n - 1));
        if (!(dc & 0x01)) aktiv |= 0x100;
        nadeln_aktiv_ = aktiv;
        if (aktiv) druck_.feuere(aktiv, true);
    }
    if (!offen) nadeln_aktiv_ = 0;
    tor_offen_ = offen;
}
