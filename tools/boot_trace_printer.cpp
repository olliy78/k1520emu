/**
 * @file boot_trace_printer.cpp
 * @brief `boot_trace --machine sd1157`: Netz-Ein -> Original-ROM des SD 1157, mit Ereignisprotokoll
 *        (E/A-Schreibzugriffe, Interrupts), PC-Histogramm und Endzustand (Entwurf 49, WP1;
 *        Muster tools/boot_trace_mc80.cpp).
 *
 * Protokolliert werden alle Schreibzugriffe auf E/A-Tore (gefaltet), Interrupts und NMI.  Lesezugriffe
 * nur auf Wunsch (`--watchio`).  Abbruch: `--until`, Stillstand (`--stall` Takte ohne NEUES Paar aus Tor
 * und Wert) oder `-c`.  Die Leerlaufschleife der Firmware schreibt dieselben Werte (0AH/C8H nach F8H/F9H,
 * 15H/01H im Wechsel nach DCH) alle 1638 Takte immer wieder — ein Paar, das schon vorkam, zaehlt darum
 * nicht als Aktivitaet.  Im Ereignisprotokoll fehlen nur unmittelbare Wiederholungen desselben Werts am selben Tor.  Es gibt kein Bild; der Endzustand nennt Lampen, Wagen, Papier und SIO-Ausgabe.
 *
 * @license MIT
 */
#include "tools/boot_trace_printer.h"
#include "tools/coverage_diff.h"
#include "tools/event_bp.h"
#include "tools/z80dis_min.h"
#include "core/machines/printer/printer_machine.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <unordered_map>

namespace {

// E/A-Namen nach doc/printer/sd1157/hardware.md §4.
const char* portName(uint8_t p, int fam = 0) {   // fam: 0 SD 1157, 1 K6316, 2 K6313
    if (fam == 2) {
        // K6313: CCB, eine Adressleitung tief, doc/printer/k6313/hardware.md §2.2
        if (p >= 0x7C && p <= 0x7F) return (p & 2) ? ((p & 1) ? "SIO Steuer B (DTR/RTS)" : "SIO Steuer A (V.24)")
                                                    : ((p & 1) ? "SIO Daten B" : "SIO Daten A (V.24)");
        if (p >= 0xBC && p <= 0xBF) return "PIO D1 Centronics-Kassette (nicht bestueckt)";
        if (p >= 0xDC && p <= 0xDF) return "PIO D105 (Tastenmatrix, Nadeln 1-8, LED ERR)";
        if (p >= 0xEC && p <= 0xEF) return "CTC D102 (Kanal 2 = Baudtakt)";
        if (p >= 0xF4 && p <= 0xF7) return "CTC D101 (Kanal 0 = Wagentakt, 2 = CMT)";
        if (p >= 0xF8 && p <= 0xFB) return "PIO D104 (Wagen-/Formularmotor, PE, TDS, LON, Nadel 9)";
        return "";
    }
    if (fam == 1) {
        // K6316: Decoder D8, doc/printer/k6316/hardware.md §2
        if (p >= 0x80 && p <= 0x83) return "PIO D12 (Papiermotor, Lichtschranken, Wagenmotor, /G1, MHA, PE)";
        if (p >= 0x84 && p <= 0x87) return "CTC D5";
        if (p >= 0x88 && p <= 0x8B) return "CTC D10 (Kanal 2 = Baudtakt)";
        if (p >= 0x8C && p <= 0x8F) return "PIO D14 (Nadeln, Lampen, Tasten, /MELO)";
        if (p >= 0xA0 && p <= 0xA3) return "Schnittstellenmodul (frei)";
        if (p >= 0xA4 && p <= 0xA7) return (p & 2) ? ((p & 1) ? "SIO Steuer B (IFSS)" : "SIO Steuer A (V.24)")
                                                    : ((p & 1) ? "SIO Daten B (IFSS)" : "SIO Daten A (V.24)");
        if (p >= 0xA8 && p <= 0xAB) return "PIO D11 (Vorsteckmotor, Tasten, DIL-Matrix, Magnete)";
        return "";
    }
    if (p == 0x6F) return "Schalterblock 3 (V.24)";
    if (p >= 0x7C && p <= 0x7F) return (p & 2) ? ((p & 1) ? "SIO Steuer B (IFSS)" : "SIO Steuer A (V.24)")
                                                : ((p & 1) ? "SIO Daten B (IFSS)" : "SIO Daten A (V.24)");
    if (p == 0xBC) return "8212-System (Papierende, lpi, Bahnen, Formular)";
    if (p == 0xBE) return "Nadeln 1-8 (low = feuern)";
    if (p == 0xBF) return "Nadel 9 (D7)";
    if (p >= 0xDC && p <= 0xDF) return "CTC Kanal";
    if (p >= 0xEC && p <= 0xEF) return "PIO 3 (Bedienfeld/Schalter)";
    if (p >= 0xF4 && p <= 0xF7) return "PIO 1 (Papier, Raender)";
    if (p >= 0xF8 && p <= 0xFB) return "PIO 2 (Wagen, Druck, Lampen)";
    return "";
}

}  // namespace

int bootTraceSd1157(const K8915TraceOpts& o, const prnlst::Listing& prn, int k6316)
{
    PrinterMachine::Config cfg;
    const bool k13 = k6316 >= 3;   // gleiche Karte K6313Ccb: 3 = K6313 (WP8c), 4 = K6311 (WP8e, Hardware erschlossen [?]), 5/6 = IFSS-Fassungen (WP8g)
    if (k6316 == 5) cfg.typ = PrinterMachine::Config::Typ::K6313_Ifss;
    else if (k6316 == 6) cfg.typ = PrinterMachine::Config::Typ::K6311_Ifss;
    else if (k6316 == 4) cfg.typ = PrinterMachine::Config::Typ::K6311_V24;
    else if (k6316 == 3) cfg.typ = PrinterMachine::Config::Typ::K6313_V24;
    else if (k6316) cfg.typ = k6316 == 2 ? PrinterMachine::Config::Typ::K6316_V24 : PrinterMachine::Config::Typ::K6316_Ifss;
    if (!o.rom_path.empty()) {   // --rom <Datei>: ROM-Satz statt des eingebetteten (K6313/K6316)
        std::vector<uint8_t> d;
        if (FILE* f = fopen(o.rom_path.c_str(), "rb")) {
            int ch; while ((ch = fgetc(f)) != EOF) d.push_back(static_cast<uint8_t>(ch));
            fclose(f);
        }
        if (d.empty()) { fprintf(stderr, "FEHLER: --rom '%s' nicht lesbar\n", o.rom_path.c_str()); return 2; }
        (k13 ? cfg.k6313.rom_extern : k6316 ? cfg.k6316.rom_extern : cfg.sd1157.rom_extern) = std::move(d);
    }
    if ((k6316 == 3 || k6316 == 5) && o.dil >= 0) cfg.k6313.dil = static_cast<uint32_t>(o.dil);
    PrinterMachine m(cfg);
    // Je Geraet eine Steuerung: `sd` (SD 1157), `k6` (K6316) bzw. `k3` (K6313); gemeinsam sind CPU, Druckwerk und SIO-Mitschrift.
    Sd1157Steuerung* sd = k6316 ? nullptr : &m.sd1157();
    K6316Steuerung*  k6 = (k6316 == 1 || k6316 == 2) ? &m.k6316() : nullptr;
    K6313Ccb*        k3 = k13 ? &m.k6313() : nullptr;
    auto sioAus = [&](int kanal) -> const std::string& { return sd ? sd->sioAusgabe(kanal) : k3 ? k3->sioAusgabe(kanal) : k6->sioAusgabe(kanal); };
    auto lampeOn = [&]() { return sd ? sd->lampeOnline() : k3 ? k3->lampe(1) : k6->lampe(5); };
    auto lampeErr = [&]() { return sd ? sd->lampeFehler() : k3 ? k3->lampe(3) : k6->lampe(2); };
    // Tasten, die beim Netz-Ein gehalten werden (--keys, K6313: 1..4 = ON/OFF LINE, LF vor, LF zurueck, FF)
    if (k3) for (char c : o.keys) if (c >= '1' && c <= '4') k3->setTaste(c - '0', true);
    auto rd = [&](uint16_t a) { return m.memReadDebug(a); };
    auto prnTail = [&](uint16_t a) -> std::string {
        const std::string* s = prn.find(a);
        if (!s || !prn.matches(a, rd)) return std::string();
        return "  ; " + *s;
    };
    auto name = [&](uint16_t a) -> std::string {
        std::string l = prn.labelNear(a);
        return l.empty() ? l : " <" + l + ">";
    };

    m.powerOn();
    if (!o.quiet) {
        fprintf(stderr, "=== %s Boot Trace ===\n", PrinterMachine::typName(cfg.typ));
        fprintf(stderr, "Max cycles: %lld   Stillstand nach %lld\n", o.limit, o.stall);
        if (o.until.kind != untilcond::UntilCond::NONE)
            fprintf(stderr, "Until:      %s\n", o.until.text.c_str());
        fprintf(stderr, "\n");
    }

    // ── Ereignisprotokoll mit Faltung (wie MC80) ─────────────────────────────
    FILE* ev = stderr;
    if (!o.events_path.empty()) {
        ev = fopen(o.events_path.c_str(), "w");
        if (!ev) { fprintf(stderr, "WARN: cannot write --events '%s'\n", o.events_path.c_str()); ev = stderr; }
    }
    const bool ev_on = !o.quiet || ev != stderr;
    long ev_lines = 0, ev_total = 0; bool ev_capped = false;
    struct Ev { std::string key, text; uint64_t cyc = 0; uint16_t pc_lo = 0, pc_hi = 0; long n = 0;
                std::string tail; };
    Ev cur; bool have = false;
    auto emit = [&](const Ev& e) {
        if (!ev_on) return;
        if (ev_lines >= o.events_cap) {
            if (!ev_capped) { fprintf(ev, "  [ev] Grenze %ld Zeilen erreicht - Protokoll endet (--events-cap)\n", o.events_cap); ev_capped = true; }
            return;
        }
        char pcs[24];
        if (e.pc_lo == e.pc_hi) snprintf(pcs, sizeof pcs, "PC=%04X", e.pc_lo);
        else snprintf(pcs, sizeof pcs, "PC=%04X..%04X", e.pc_lo, e.pc_hi);
        fprintf(ev, "  [ev c%-10llu %s] %s", (unsigned long long)e.cyc, pcs, e.text.c_str());
        if (e.n > 1) fprintf(ev, "  x%ld", e.n);
        fprintf(ev, "%s\n", e.tail.c_str());
        ++ev_lines;
    };
    auto flush = [&] { if (have) emit(cur); have = false; };
    auto event = [&](const std::string& key, const std::string& text, uint16_t pc) {
        ++ev_total;
        if (have && cur.key == key) {
            ++cur.n; cur.pc_lo = std::min(cur.pc_lo, pc); cur.pc_hi = std::max(cur.pc_hi, pc);
            return;
        }
        flush();
        cur = Ev{key, text, m.totalCycles(), pc, pc, 1, prnTail(pc)}; have = true;
    };

    // ── Beobachter ───────────────────────────────────────────────────────────
    std::unordered_map<uint16_t, uint32_t> hist;
    std::unordered_map<uint16_t, std::string> hist_note;
    uint64_t instr = 0;
    uint64_t io_rd[256] = {0}, io_wr[256] = {0};
    int last_wr[256]; for (int& v : last_wr) v = -1;   // zuletzt geschriebener Wert je Tor (-1 = nie)
    std::vector<bool> gesehen(0x10000, false);          // Paare (Tor, Wert), die schon einmal geschrieben wurden
    uint64_t last_activity = 0;
    bool until_hit = false; uint64_t until_cyc = 0; uint16_t until_pc = 0;
    eventbp::Prev prev;
    long ints = 0;
    FILE* csv = nullptr; long csv_rows = 0;
    if (!o.csv_path.empty()) {
        csv = fopen(o.csv_path.c_str(), "w");
        if (csv) fprintf(csv, "seq,cyc,cpu,pc,bytes,disasm,af,bc,de,hl,ix,iy,sp\n");
        else fprintf(stderr, "WARN: cannot write --csv '%s'\n", o.csv_path.c_str());
    }
    FILE* itr = nullptr; long itr_n = 0;
    if (!o.itrace_path.empty()) {
        itr = fopen(o.itrace_path.c_str(), "w");
        if (itr) fprintf(itr, "seq,cyc,kind,int_pc,isr_pc,sp,vector,device\n");
        else fprintf(stderr, "WARN: cannot write --itrace '%s'\n", o.itrace_path.c_str());
    }
    int win_n = 0;
    uint16_t insn_pc = 0;

    m.setCpuTraceCallback([&](const Z80& z) {
        ++instr; insn_pc = z.PC;
        if (hist[z.PC]++ == 0) hist_note[z.PC] = name(z.PC) + prnTail(z.PC);
        if (csv && (o.win_lo < 0 || (z.PC >= o.win_lo && z.PC <= o.win_hi)) && csv_rows < 5'000'000) {
            z80dis::Insn d = z80dis::decode(rd, z.PC);
            char bytes[16] = {0};
            for (int i = 0; i < d.len && i < 4; ++i) { char b[3]; snprintf(b, 3, "%02X", rd(static_cast<uint16_t>(z.PC + i))); strcat(bytes, b); }
            fprintf(csv, "%ld,%llu,CPU,0x%04X,%s,\"%s\",%04X,%04X,%04X,%04X,%04X,%04X,%04X\n",
                    csv_rows, (unsigned long long)z.cycles, z.PC, bytes, d.text,
                    z.AF, z.BC, z.DE, z.HL, z.IX, z.IY, z.SP);
            ++csv_rows;
        }
        const eventbp::Event e = eventbp::classify(z.PC, z.SP, z.IFF1, prev, true, true, false, 0, 0);
        if (e == eventbp::Event::Interrupt || e == eventbp::Event::NMI) {
            ++ints;
            const uint16_t ret = static_cast<uint16_t>(rd(z.SP) | (rd(static_cast<uint16_t>(z.SP + 1)) << 8));
            const auto& ia = m.bus().lastIntAck();
            const char* dev = e == eventbp::Event::NMI ? "-" : ia.spurious ? "SPURIOUS" : (ia.device ? ia.device : "?");
            char t[160];
            if (e == eventbp::Event::NMI) snprintf(t, sizeof t, "NMI -> ISR 0066 (unterbrochen bei %04X)", ret);
            else snprintf(t, sizeof t, "INT (IM %d) Vektor=%02X von %s -> ISR %04X%s", z.IM, ia.vector, dev,
                          z.PC, name(z.PC).c_str());
            char key[48]; snprintf(key, sizeof key, "I%02X%04X", e == eventbp::Event::NMI ? 0x100 : ia.vector, z.PC);
            event(key, t, ret);
            if (itr) {
                char vec[8] = "-";
                if (e == eventbp::Event::Interrupt) snprintf(vec, sizeof vec, "0x%02X", ia.vector);
                fprintf(itr, "%ld,%llu,%s,0x%04X,0x%04X,0x%04X,%s,%s\n", itr_n, (unsigned long long)m.totalCycles(),
                        e == eventbp::Event::NMI ? "NMI" : "INT", ret, z.PC, z.SP, vec, dev);
                ++itr_n;
            }
        }
        prev.have = true; prev.sp = z.SP; prev.iff1 = z.IFF1;

        if (o.win_lo >= 0 && z.PC >= o.win_lo && z.PC <= o.win_hi && win_n < o.win_cap && !o.quiet) {
            z80dis::Insn d = z80dis::decode(rd, z.PC);
            fprintf(stderr, "  [w%4d] PC=%04X %-16s AF=%04X BC=%04X DE=%04X HL=%04X SP=%04X%s\n", win_n++,
                    z.PC, d.text, z.AF, z.BC, z.DE, z.HL, z.SP, prnTail(z.PC).c_str());
        }
        if (o.until.kind == untilcond::UntilCond::PC || o.until.kind == untilcond::UntilCond::MEM) {
            long v = o.until.kind == untilcond::UntilCond::PC ? z.PC
                   : o.until.word ? (rd(o.until.addr) | (rd(static_cast<uint16_t>(o.until.addr + 1)) << 8))
                                  : rd(o.until.addr);
            if (!until_hit && o.until.compare(v)) {
                until_hit = true; until_cyc = m.totalCycles(); until_pc = z.PC; m.stop();
            }
        }
    });
    m.setBusTrace([&](bool isIO, bool isRead, uint16_t addr, uint8_t data) {
        const uint16_t pc = insn_pc;
        if (!isIO) {
            if (!isRead)
                for (uint16_t w : o.watch)
                    if (w == addr) {
                        char t[64]; snprintf(t, sizeof t, "WR [%04X]=%02X", addr, data);
                        char k[24]; snprintf(k, sizeof k, "W%04X%02X", addr, data);
                        event(k, t, pc);
                    }
            return;
        }
        const uint8_t p = static_cast<uint8_t>(addr);
        (isRead ? io_rd : io_wr)[p]++;
        // Aktivitaet = neues Paar (Tor, Wert); Ereignis = alle Schreibzugriffe ausser der unmittelbaren Wiederholung
        // desselben Werts am selben Tor; Lesen nur mit --watchio.
        bool wiederholung = false;
        if (!isRead) {
            const size_t paar = static_cast<size_t>(p) << 8 | data;
            if (!gesehen[paar]) { gesehen[paar] = true; last_activity = m.totalCycles(); }
            wiederholung = last_wr[p] == data;
            last_wr[p] = data;
        }
        bool gewuenscht = !isRead && !wiederholung;
        for (uint16_t w : o.watchio) if ((w & 0xFF) == p) gewuenscht = true;
        if (!gewuenscht) return;
        char t[200], k[24];
        snprintf(t, sizeof t, "%s (%02XH)=%02X  %s", isRead ? "IN " : "OUT", p, data, portName(p, k13 ? 2 : k6316 ? 1 : 0));
        snprintf(k, sizeof k, "P%c%02X%02X%04X", isRead ? 'r' : 'w', p, data, pc);
        event(k, t, pc);
    });

    // ── Lauf ─────────────────────────────────────────────────────────────────
    const int batch = 20000;
    bool stillstand = false;
    uint64_t next_progress = 10'000'000;
    while (static_cast<long long>(m.totalCycles()) < o.limit) {
        const int n = m.run(batch);
        if (until_hit) break;
        const uint64_t now = m.totalCycles();
        if (now - last_activity >= static_cast<uint64_t>(o.stall)) { stillstand = true; break; }
        if (!o.quiet && now >= next_progress) {
            fprintf(stderr, "[PROGRESS] cycles=%llu PC=%04X instr=%llu\n",
                    (unsigned long long)now, m.cpuPC(), (unsigned long long)instr);
            next_progress = now + 10'000'000;
        }
        if (n == 0) break;
    }
    flush();
    if (ev != stderr) fclose(ev);
    if (csv) { fclose(csv); fprintf(stderr, "--csv: %ld row(s) -> %s\n", csv_rows, o.csv_path.c_str()); }
    if (itr) { fclose(itr); fprintf(stderr, "--itrace: %ld INT/NMI -> %s\n", itr_n, o.itrace_path.c_str()); }

    const uint64_t cycles = m.totalCycles();
    const auto& druck = m.druck();
    if (!o.quiet) {
        fprintf(stderr, "\n=== Drucker Boot Trace Complete: %llu cycles ===\n", (unsigned long long)cycles);
        if (o.until.kind != untilcond::UntilCond::NONE)
            fprintf(stderr, "--until (%s): %s", o.until.text.c_str(), until_hit ? "MET" : "not met\n");
        if (until_hit) fprintf(stderr, " at cycle %llu (PC=0x%04X)\n", (unsigned long long)until_cyc, until_pc);
        if (stillstand)
            fprintf(stderr, "Stillstand:  JA - %lld Takte ohne neues Paar (Tor, Wert) bei einem Schreibzugriff (PC=%04X)\n", o.stall, m.cpuPC());
        fprintf(stderr, "Lampen:      ONLINE %s, FEHLER %s\n", lampeOn() ? "an" : "aus", lampeErr() ? "an" : "aus");
        if (k6)
            fprintf(stderr, "             H1 TEST %d, H3 PAPIER %d, H4 BEREIT %d; Schnitte %d\n", k6->lampe(1), k6->lampe(3),
                    k6->lampe(4), k6->schnitte());
        if (k3)
            fprintf(stderr, "             POW %d, PE %d; Nadeln zuletzt %03X\n", k3->lampe(2), k3->lampe(4), k3->nadeln());
        fprintf(stderr, "Wagen:       Position %d, %s  PV-Schritte Bahn 1/2: %u/%u\n",
                druck.wagen().position(), druck.wagen().laeuft() ? "laeuft" : "steht",
                (unsigned)druck.papier(0).schritte(), (unsigned)(druck.bahnen() > 1 ? druck.papier(1).schritte() : 0u));
        if (sd)
            fprintf(stderr, "Ausgaenge:   2A=%02X 2B=%02X 1A=%02X 1B=%02X  Nadeln BEH=%02X BFH=%02X\n",
                    sd->portAusgabe(2, 0), sd->portAusgabe(2, 1), sd->portAusgabe(1, 0), sd->portAusgabe(1, 1),
                    sd->nad18(), sd->nad9());
        else if (k3)
            fprintf(stderr, "Ausgaenge:   D104 A/B=%02X/%02X D105 A/B=%02X/%02X (Pinpegel), Matrixspalte %d\n",
                    k3->portAusgabe(104, 0), k3->portAusgabe(104, 1), k3->portAusgabe(105, 0), k3->portAusgabe(105, 1), k3->matrixSpalte());
        else
            fprintf(stderr, "Ausgaenge:   D11 A/B=%02X/%02X D12 A/B=%02X/%02X D14 A/B=%02X/%02X (Pinpegel)\n",
                    k6->portAusgabe(11, 0), k6->portAusgabe(11, 1), k6->portAusgabe(12, 0), k6->portAusgabe(12, 1),
                    k6->portAusgabe(14, 0), k6->portAusgabe(14, 1));
        fprintf(stderr, "SIO gesendet: A (V.24) %zu Zeichen, B (IFSS) %zu Zeichen\n",
                sioAus(0).size(), sioAus(1).size());
        fprintf(stderr, "Ereignisse:  %ld (%ld Zeilen), Interrupts: %ld, Befehle: %llu\n",
                ev_total, ev_lines, ints, (unsigned long long)instr);
        const Z80& z = m.cpu();
        fprintf(stderr, "Final CPU:   PC=%04X SP=%04X AF=%04X BC=%04X DE=%04X HL=%04X IM=%d IFF1=%d%s\n",
                z.PC, z.SP, z.AF, z.BC, z.DE, z.HL, z.IM, z.IFF1, prnTail(z.PC).c_str());

        fprintf(stderr, "\nI/O-Ports (rd/wr):\n");
        for (int p = 0; p < 256; ++p)
            if (io_rd[p] || io_wr[p])
                fprintf(stderr, "  port 0x%02X : rd=%-9llu wr=%-9llu %s\n", p,
                        (unsigned long long)io_rd[p], (unsigned long long)io_wr[p], portName(static_cast<uint8_t>(p), k13 ? 2 : k6316 ? 1 : 0));

        std::vector<std::pair<uint32_t, uint16_t>> hs;
        for (auto& kv : hist) hs.push_back({kv.second, kv.first});
        std::sort(hs.rbegin(), hs.rend());
        fprintf(stderr, "\nPC-Histogramm (top 30, %llu Befehle):\n", (unsigned long long)instr);
        for (size_t i = 0; i < hs.size() && i < 30; ++i)
            fprintf(stderr, "  0x%04X : %9u  (%5.2f%%)%s\n", hs[i].second, hs[i].first,
                    100.0 * hs[i].first / static_cast<double>(instr ? instr : 1), hist_note[hs[i].second].c_str());
    }
    if (o.coverage) {
        std::vector<bool> cov(0x10000, false);
        for (auto& kv : hist) {
            int len = z80dis::decode(rd, kv.first).len;
            for (int b = 0; b < len; ++b) cov[static_cast<uint16_t>(kv.first + b)] = true;
        }
        auto ranges = covdiff::collapseRanges(cov);
        fprintf(stderr, "\n=== Code coverage (CPU) ===\n  %zu distinct instr addresses, %zu range(s)\n",
                hist.size(), ranges.size());
        for (size_t i = 0; i < ranges.size() && i < 60; ++i)
            fprintf(stderr, "    0x%04X-0x%04X%s\n", ranges[i].first, ranges[i].second,
                    name(static_cast<uint16_t>(ranges[i].first)).c_str());
        if (!o.coverage_path.empty()) {
            if (FILE* cf = fopen(o.coverage_path.c_str(), "w")) {
                fprintf(cf, "cpu,pc,hits\n");
                std::vector<std::pair<uint16_t, uint32_t>> v(hist.begin(), hist.end());
                std::sort(v.begin(), v.end());
                for (auto& kv : v) fprintf(cf, "CPU,0x%04X,%u\n", kv.first, kv.second);
                fclose(cf);
                fprintf(stderr, "  CSV written -> %s (cpu,pc,hits)\n", o.coverage_path.c_str());
            }
        }
    }
    if (o.dump_lo >= 0 && o.dump_hi > o.dump_lo) {
        if (FILE* df = fopen(o.dump_path.c_str(), "wb")) {
            for (int a = o.dump_lo; a < o.dump_hi; ++a) fputc(rd(static_cast<uint16_t>(a)), df);
            fclose(df);
            fprintf(stderr, "\nRAM dump (CPU-Sicht) 0x%04X-0x%04X -> %s\n", o.dump_lo, o.dump_hi, o.dump_path.c_str());
        }
    }
    if (o.json) {
        fprintf(stderr,
            "{\"machine\":\"%s\",\"stall\":%s,\"cycles\":%llu,\"final_pc\":\"0x%04X\","
            "\"online_lampe\":%s,\"fehler_lampe\":%s,\"wagen\":%d,\"lauf\":%s,"
            "\"instr\":%llu,\"events\":%ld,\"ints\":%ld,",
            k6316 == 6 ? "k6311-ifss" : k6316 == 5 ? "k6313-ifss" : k6316 == 4 ? "k6311" : k6316 == 3 ? "k6313" : k6316 ? (k6316 == 2 ? "k6316-v24" : "k6316") : "sd1157", stillstand ? "true" : "false", (unsigned long long)cycles, m.cpuPC(),
            lampeOn() ? "true" : "false", lampeErr() ? "true" : "false",
            druck.wagen().position(), druck.wagen().laeuft() ? "true" : "false",
            (unsigned long long)instr, ev_total, ints);
        if (o.until.kind != untilcond::UntilCond::NONE)
            fprintf(stderr, "\"until\":{\"set\":true,\"met\":%s,\"cycle\":%llu,\"pc\":\"0x%04X\"}}\n",
                    until_hit ? "true" : "false", (unsigned long long)until_cyc, until_pc);
        else
            fprintf(stderr, "\"until\":{\"set\":false}}\n");
    }
    if (o.until.kind != untilcond::UntilCond::NONE) return until_hit ? 0 : 2;
    return stillstand ? 0 : 1;
}
