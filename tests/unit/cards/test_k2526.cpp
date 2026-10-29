/**
 * @file test_k2526.cpp
 * @brief Unit tests for the K2526 ZRE (Zentrales Rechenwerk und Erweiterung) card emulation.
 *
 * @details
 * Emulator component under test: **K2526** (`core/cards/k2526/k2526.h`)
 *
 * The K2526 is the central processor card of the Robotron A5120.  It contains:
 *  - **ZVE1** – the main Z80 CPU (boot and application execution)
 *  - **ZVE2** – a secondary DMA/CPU Z80 used for DMA transfers
 *  - **Boot ROM** – 1 KiB at 0x0000–0x03FF, mapped at power-on and disabled by software
 *  - **BS-PIO** – an internal Z80 PIO for control signals (Port A: MEMDI, SPS-Ind; Port B: /LD-ROM,
 *    /INT-BS, /SPS-ESR, /WAIT-ZVE2, /SA)
 *  - **CTC** – a Z80 CTC for timing
 *  - **Q240 SPA (Speicher-/E/A-Schutz)** – 1024 x 1 bit protection RAM (64-byte segments); B2=1
 *    programmes it via AB0, B2=0 arms the protection (ZVE2 is privileged and bypasses it)
 *
 * I/O port map (on the K1520 bus, decoded by K2526):
 *  - 0x00–0x07: U-Bus sub-devices
 *  - 0x00:  /INT-BS latch write
 *  - 0x02:  /RES-SPA (clear Q240 violation + NMI)
 *  - 0x04:  /RES-ZVE2 (bit 0 high = release reset, low = assert reset)
 *  - 0x08–0x0B: BS-PIO (Port A data, A ctrl, Port B data, B ctrl)
 *  - 0x0C–0x0F: CTC (channels 0–3)
 *
 * ## Test groups
 *
 * | Group                    | What is tested                                              |
 * |--------------------------|-------------------------------------------------------------|
 * | ROM power-on state       | ROM enabled by default; data matches ZRE_BOOT_ROM array    |
 * | ROM write ignored        | ROM is read-only; writes have no effect                     |
 * | ROM disable via BS-PIO B0| /LD-ROM (B0=0) removes ROM from bus; B0=1 leaves it active |
 * | MEMDI via BS-PIO A7      | A7=1 asserts MEMDI on bus; A7=0 releases it                 |
 * | clockTick()              | 1000 ticks must not crash                                   |
 * | I/O dispatch             | All ports 0x00–0x0F readable and writable without crash     |
 * | attachToBus              | Port 0x0A routes to BS-PIO and disables ROM correctly       |
 * | InterruptSlave           | hasInterrupt(), setIEI(), getIEO() interface works          |
 * | deviceName / sub-chips   | deviceName(), bsPio(), ctc() return expected values         |
 * | ZVE1 (CPU) interface     | Reset, PC, IFF1, cpuStep(), cpuNMI()                        |
 * | ZVE2 (DMA-CPU)           | Reset/release, step, wait signal, PC starts at 0           |
 * | /WAIT-ZVE2 (BS-PIO B3)  | B3=0 stalls ZVE2 (zve2Step returns 0); B3=1 resumes it     |
 * | Q240 memory protection   | B2=1 AB0 programming, B2=0 arm, SPS-Ind, E/A -> NMI         |
 * | /INT-BS (port 0x00)      | Pulses BS-PIO B1 (active high) -> PIO interrupt             |
 * | /SA power-off (B4)       | B4=0 asserts shutdown request; B4=1 leaves it inactive      |
 *
 * @see core/cards/k2526/k2526.h
 * @see core/cards/k2526/rom_data.h
 * @see core/bus/k1520_bus.h
 */

#include <gtest/gtest.h>
#include "core/util/os_compat.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include "core/cards/k2526/k2526.h"
#include "core/cards/k2526/rom_data.h"
#include "core/cards/k3526/k3526.h"
#include "core/cards/k5122/k5122.h"
#include "core/peripherals/floppy_drive/disk_format.h"
#include "core/peripherals/floppy_drive/format_catalog.h"
#include "tests/support/temp_path.h"

// ─── Helpers ──────────────────────────────────────────────────────────────────

// Write to BS-PIO Port A data register (port 0x08 = base 0x08 + sub-port 0).
static constexpr uint8_t BSPIO_PORTA_DATA = 0x08;
// Write to BS-PIO Port B data register (port 0x0A = base 0x08 + sub-port 2).
static constexpr uint8_t BSPIO_PORTB_DATA = 0x0A;
// BS-PIO Port A control register (port 0x09 = base 0x08 + sub-port 1).
static constexpr uint8_t BSPIO_PORTA_CTRL = 0x09;

// ─── ROM: power-on state ──────────────────────────────────────────────────────

/**
 * @test K2526/IsRomEnabled_InitialFalse
 * @brief isRomEnabled() returns true immediately after construction (before powerOn).
 * @details The ROM enable flag is set by the constructor; ROM is registered with the bus
 *   only after attachToBus() + powerOn().
 * @par Pass criterion  card.isRomEnabled() == true.
 */
TEST(K2526, IsRomEnabled_InitialFalse)
{
    // Before powerOn(), the ROM is enabled by default (member init), but not
    // yet registered. isRomEnabled() reflects the internal flag.
    K1520Bus bus;
    K2526 card(bus);
    EXPECT_TRUE(card.isRomEnabled());
}

/**
 * @test K2526/PowerOn_RomRegistered_MemReadNonFF
 * @brief After attachToBus() + powerOn(), the ROM is accessible at 0x0000 and reads non-0xFF.
 * @par Pass criterion  bus.memRead(0x0000) != 0xFF; bus.memRead(0x0000) == ZRE_BOOT_ROM[0].
 */
TEST(K2526, PowerOn_RomRegistered_MemReadNonFF)
{
    K1520Bus bus;
    K2526 card(bus);
    card.attachToBus(bus);
    card.powerOn();

    // The first byte of the ROM is 0xEE – definitely not 0xFF.
    uint8_t val = bus.memRead(0x0000);
    EXPECT_NE(val, 0xFF);
    EXPECT_EQ(val, ZRE_BOOT_ROM[0]);
}

/**
 * @test K2526/PowerOn_RomData_MatchesArray
 * @brief Every byte of the 1 KiB boot ROM matches the ZRE_BOOT_ROM[] compile-time array.
 * @par Pass criterion  bus.memRead(i) == ZRE_BOOT_ROM[i] for all i in [0, 1023].
 */
TEST(K2526, PowerOn_RomData_MatchesArray)
{
    K1520Bus bus;
    K2526 card(bus);
    card.attachToBus(bus);
    card.powerOn();

    for (uint16_t i = 0; i < 1024; ++i)
        EXPECT_EQ(bus.memRead(i), ZRE_BOOT_ROM[i]) << "mismatch at offset " << i;
}

// ─── Lade-ROM-Fassungen (AP A7, doc/a5120_alt/laderom.md) ───────────────────

/**
 * @test K2526/Laderom_VorgabeIstV09_2
 * @brief Ohne Angabe steckt V0.9_2 (= zre.rom) — bitgleich zu vor AP A7.
 */
TEST(K2526, Laderom_VorgabeIstV09_2)
{
    K2526::A5120Config cfg;
    EXPECT_EQ(cfg.laderom, K2526::Laderom::V09_2);
    K1520Bus bus;
    K2526 card(bus, cfg);
    EXPECT_EQ(card.laderom(), K2526::Laderom::V09_2);
    EXPECT_EQ(&K2526::laderomDaten(K2526::Laderom::V09_2), &ZRE_BOOT_ROM);
}

/**
 * @test K2526/Laderom_JedeFassungLiegtBei0000
 * @brief Jede Fassung liegt nach powerOn() byteweise bei 0000H–03FFH; die eingebetteten
 *        Abzuege tragen ihre CRC-16/CCITT-FALSE in 03FEH/03FFH (laderom.md §1) — damit ist
 *        auch belegt, dass `eprom_to_h.py` die richtige Datei eingebettet hat.
 */
TEST(K2526, Laderom_JedeFassungLiegtBei0000)
{
    struct F { K2526::Laderom l; uint16_t crc; };
    const F faelle[] = { {K2526::Laderom::V09_2, 0xF76B},
                         {K2526::Laderom::V09_1, 0x3FAE},
                         {K2526::Laderom::V06,   0x07F1} };
    for (const auto& f : faelle) {
        K2526::A5120Config cfg;
        cfg.laderom = f.l;
        K1520Bus bus;
        K2526 card(bus, cfg);
        card.attachToBus(bus);
        card.powerOn();
        const uint8_t (&rom)[1024] = K2526::laderomDaten(f.l);
        uint16_t crc = 0xFFFF;
        for (uint16_t i = 0; i < 1024; ++i) {
            ASSERT_EQ(bus.memRead(i), rom[i]) << "Fassung " << int(f.l) << " @" << i;
            if (i >= 0x3FE) continue;
            crc ^= static_cast<uint16_t>(rom[i] << 8);
            for (int b = 0; b < 8; ++b)
                crc = (crc & 0x8000) ? static_cast<uint16_t>((crc << 1) ^ 0x1021)
                                     : static_cast<uint16_t>(crc << 1);
        }
        EXPECT_EQ(crc, f.crc) << "Fassung " << int(f.l);
        EXPECT_EQ((rom[0x3FE] << 8) | rom[0x3FF], f.crc) << "Fassung " << int(f.l);
    }
    // V0.6 ist ein anderes Programm: Indexvektor 0BAH zeigt auf 01DDH statt 01C7H (§3 #1).
    EXPECT_EQ(K2526::laderomDaten(K2526::Laderom::V06)[0xBA], 0xDD);
    EXPECT_EQ(K2526::laderomDaten(K2526::Laderom::V09_2)[0xBA], 0xC7);
}

// ─── ROM: write is ignored (ROM is read-only) ─────────────────────────────────

/**
 * @test K2526/RomWriteIgnored
 * @brief Bus writes to the ROM address range are silently discarded.
 * @par Pass criterion  bus.memRead(0x0010) returns the original ROM byte after a write attempt.
 */
TEST(K2526, RomWriteIgnored)
{
    K1520Bus bus;
    K2526 card(bus);
    card.attachToBus(bus);
    card.powerOn();

    uint8_t original = bus.memRead(0x0010);
    bus.memWrite(0x0010, ~original);       // attempt write
    EXPECT_EQ(bus.memRead(0x0010), original);  // unchanged
}

// ─── ROM: disable via BS-PIO Port B bit 0 (/LD-ROM, active low) ──────────────

/**
 * @test K2526/BSPioPortB_Bit0Low_DisablesRom
 * @brief Writing 0x00 to BS-PIO Port B (B0=0, /LD-ROM asserted) unregisters the ROM from the bus.
 * @par Pass criterion  isRomEnabled() == false; bus.memRead(0x0000) == 0xFF after the write.
 */
TEST(K2526, BSPioPortB_Bit0Low_DisablesRom)
{
    K1520Bus bus;
    K2526 card(bus);
    card.attachToBus(bus);
    card.powerOn();

    ASSERT_TRUE(card.isRomEnabled());
    ASSERT_NE(bus.memRead(0x0000), 0xFF);

    // B0=0 → /LD-ROM asserted (active low) → ROM disabled.
    bus.ioWrite(BSPIO_PORTB_DATA, 0x00);

    EXPECT_FALSE(card.isRomEnabled());
    // After unregistering the ROM, the bus returns 0xFF for the address
    // (no other device registered there).
    EXPECT_EQ(bus.memRead(0x0000), 0xFF);
}

/**
 * @test K2526/BSPioPortB_DisableRom_IsIdempotent
 * @brief Asserting /LD-ROM twice in a row does not crash and leaves ROM disabled.
 * @par Pass criterion  isRomEnabled() == false after the second B0=0 write.
 */
TEST(K2526, BSPioPortB_DisableRom_IsIdempotent)
{
    K1520Bus bus;
    K2526 card(bus);
    card.attachToBus(bus);
    card.powerOn();

    bus.ioWrite(BSPIO_PORTB_DATA, 0x00);  // B0=0 → ROM disabled
    EXPECT_FALSE(card.isRomEnabled());

    // Second write must not double-unregister (would crash or be harmless).
    bus.ioWrite(BSPIO_PORTB_DATA, 0x00);
    EXPECT_FALSE(card.isRomEnabled());
}

/**
 * @test K2526/BSPioPortB_Bit0High_RomStaysEnabled
 * @brief Writing B0=1 to BS-PIO Port B (/LD-ROM not asserted) leaves the ROM active.
 * @par Pass criterion  isRomEnabled() == true; bus.memRead(0x0000) != 0xFF.
 */
TEST(K2526, BSPioPortB_Bit0High_RomStaysEnabled)
{
    K1520Bus bus;
    K2526 card(bus);
    card.attachToBus(bus);
    card.powerOn();

    // B0=1 → /LD-ROM not asserted → ROM stays active.
    bus.ioWrite(BSPIO_PORTB_DATA, 0x01);
    EXPECT_TRUE(card.isRomEnabled());
    EXPECT_NE(bus.memRead(0x0000), 0xFF);
}

// ─── MEMDI signal via BS-PIO Port A bit 7 (MEMDI1/2 output) ─────────────────

/**
 * @test K2526/BSPioPortA_Bit7_TreibtMemdi12NichtBusMemdi
 * @brief BS-PIO Port A Bit 7 ist MEMDI1/2 der Rueckverdrahtung (onMemdi12), NICHT die
 *   Bus-Leitung /MEMDI — die zieht je Zugriff nur ein Vorrangspeicher (EM256).
 * @par Pass criterion  memdi12() und der Rueckruf folgen A7; bus.memdiActive() bleibt
 *   false, der Speicher bleibt erreichbar.
 */
TEST(K2526, BSPioPortA_Bit7_TreibtMemdi12NichtBusMemdi)
{
    K1520Bus bus;
    K2526 card(bus);
    card.attachToBus(bus);
    card.powerOn();
    int  calls = 0;
    bool last  = false;
    card.onMemdi12([&](bool a) { ++calls; last = a; });

    EXPECT_FALSE(card.memdi12());
    bus.ioWrite(BSPIO_PORTA_DATA, 0x80);   // A7=1 → MEMDI1/2 aktiv
    EXPECT_TRUE(card.memdi12());
    EXPECT_TRUE(last);
    EXPECT_FALSE(bus.memdiActive());
    bus.ioWrite(BSPIO_PORTA_DATA, 0x00);   // A7=0 → frei
    EXPECT_FALSE(card.memdi12());
    EXPECT_FALSE(last);
    EXPECT_EQ(calls, 2);
}

// ─── clockTick() ─────────────────────────────────────────────────────────────

/**
 * @test K2526/ClockTick_DoesNotCrash
 * @brief Calling clockTick() 1000 times after powerOn() does not crash.
 * @par Pass criterion  No exception or assertion failure.
 */
TEST(K2526, ClockTick_DoesNotCrash)
{
    K1520Bus bus;
    K2526 card(bus);
    card.attachToBus(bus);
    card.powerOn();

    // Tick many times – must not crash.
    for (int i = 0; i < 1000; ++i)
        card.clockTick();
}

// ─── I/O dispatch: all port ranges accessible without crash ──────────────────

/**
 * @test K2526/IODispatch_AllPortsReadable
 * @brief Reading all ports 0x00–0x0F does not crash after attachToBus().
 * @par Pass criterion  No exception or assertion failure for any port in range.
 */
TEST(K2526, IODispatch_AllPortsReadable)
{
    K1520Bus bus;
    K2526 card(bus);
    card.attachToBus(bus);

    for (uint8_t port = 0x00; port <= 0x0F; ++port)
        (void)bus.ioRead(port);  // must not crash
}

/**
 * @test K2526/IODispatch_AllPortsWritable
 * @brief Writing 0x00 to all ports 0x00–0x0F does not crash after attachToBus().
 * @par Pass criterion  No exception or assertion failure for any port in range.
 */
TEST(K2526, IODispatch_AllPortsWritable)
{
    K1520Bus bus;
    K2526 card(bus);
    card.attachToBus(bus);

    for (uint8_t port = 0x00; port <= 0x0F; ++port)
        bus.ioWrite(port, 0x00);  // must not crash
}

/**
 * @test K2526/IODispatch_UBusRange_0x00_0x07
 * @brief Read/write to U-Bus ports 0x00–0x07 does not crash.
 * @par Pass criterion  No exception or assertion failure.
 */
TEST(K2526, IODispatch_UBusRange_0x00_0x07)
{
    K1520Bus bus;
    K2526 card(bus);
    card.attachToBus(bus);

    // Writing to ports 0x00-0x07 routes to U-Bus and does not crash.
    for (uint8_t port = 0x00; port <= 0x07; ++port) {
        bus.ioWrite(port, 0x00);
        (void)bus.ioRead(port);
    }
}

/**
 * @test K2526/IODispatch_BsPioRange_0x08_0x0B
 * @brief Read/write to BS-PIO ports 0x08–0x0B does not crash.
 * @par Pass criterion  No exception or assertion failure.
 */
TEST(K2526, IODispatch_BsPioRange_0x08_0x0B)
{
    K1520Bus bus;
    K2526 card(bus);
    card.attachToBus(bus);

    // Writing to ports 0x08-0x0B routes to BS-PIO and does not crash.
    for (uint8_t port = 0x08; port <= 0x0B; ++port) {
        bus.ioWrite(port, 0x00);
        (void)bus.ioRead(port);
    }
}

/**
 * @test K2526/IODispatch_CtcRange_0x0C_0x0F
 * @brief Writing a CTC reset control word to ports 0x0C–0x0F does not crash.
 * @par Pass criterion  No exception or assertion failure.
 */
TEST(K2526, IODispatch_CtcRange_0x0C_0x0F)
{
    K1520Bus bus;
    K2526 card(bus);
    card.attachToBus(bus);

    // CTC: write a reset control word to each channel, then read.
    for (uint8_t port = 0x0C; port <= 0x0F; ++port) {
        bus.ioWrite(port, 0x02);  // CTC reset
        (void)bus.ioRead(port);
    }
}

// ─── attachToBus registers ports 0x00–0x0F ───────────────────────────────────

/**
 * @test K2526/AttachToBus_RegistersPorts_0x00_to_0x0F
 * @brief After attachToBus(), writing 0x00 to port 0x0A (BS-PIO Port B) disables the ROM.
 * @details Verifies that bus I/O writes are routed to the card's BS-PIO.
 * @par Pass criterion  isRomEnabled() == false after bus.ioWrite(0x0A, 0x00).
 */
TEST(K2526, AttachToBus_RegistersPorts_0x00_to_0x0F)
{
    K1520Bus bus;
    K2526 card(bus);
    card.attachToBus(bus);

    // Writes to ports outside the K2526 range (e.g. 0x10) should not reach
    // the card. Writes to 0x00–0x0F must be dispatched to the card – verified
    // indirectly by checking ROM disable works through the bus I/O path.
    card.powerOn();
    bus.ioWrite(0x0A, 0x00);   // BS-PIO Port B (0x0A): B0=0 → /LD-ROM → ROM disabled
    EXPECT_FALSE(card.isRomEnabled());
}

// ─── InterruptSlave interface ─────────────────────────────────────────────────

/**
 * @test K2526/InterruptSlave_NoInterruptInitially
 * @brief hasInterrupt() returns false before any interrupt source is active.
 * @par Pass criterion  card.hasInterrupt() == false.
 */
TEST(K2526, InterruptSlave_NoInterruptInitially)
{
    K1520Bus bus;
    K2526 card(bus);
    EXPECT_FALSE(card.hasInterrupt());
}

/**
 * @test K2526/InterruptSlave_SetIEI_DoesNotCrash
 * @brief setIEI(true) and setIEI(false) complete without crashing.
 * @par Pass criterion  No exception or assertion failure.
 */
TEST(K2526, InterruptSlave_SetIEI_DoesNotCrash)
{
    K1520Bus bus;
    K2526 card(bus);
    card.setIEI(true);
    card.setIEI(false);
}

/**
 * @test K2526/InterruptSlave_GetIEO_ReturnsBool
 * @brief getIEO() returns a valid bool without crashing after setIEI(true).
 * @par Pass criterion  No exception or assertion failure.
 */
TEST(K2526, InterruptSlave_GetIEO_ReturnsBool)
{
    K1520Bus bus;
    K2526 card(bus);
    card.setIEI(true);
    // IEO is a bool – just make sure it doesn't crash.
    (void)card.getIEO();
}

// ─── deviceName ──────────────────────────────────────────────────────────────

/**
 * @test K2526/DeviceName_IsK2526
 * @brief deviceName() returns the string "K2526".
 * @par Pass criterion  strcmp(card.deviceName(), "K2526") == 0.
 */
TEST(K2526, DeviceName_IsK2526)
{
    K1520Bus bus;
    K2526 card(bus);
    EXPECT_STREQ(card.deviceName(), "K2526");
}

// ─── Sub-chip accessors return correct objects ────────────────────────────────

/**
 * @test K2526/SubchipAccessors_ReturnValidRefs
 * @brief bsPio().deviceName() == "K2526-BS-PIO" and ctc().deviceName() == "K2526-CTC".
 * @par Pass criterion  Both sub-chip deviceName strings match the expected values.
 */
TEST(K2526, SubchipAccessors_ReturnValidRefs)
{
    K1520Bus bus;
    K2526 card(bus);
    // bsPio() and ctc() must return references to the internal sub-chips.
    // The simplest check: their deviceName() matches what we set.
    EXPECT_STREQ(card.bsPio().deviceName(), "K2526-BS-PIO");
    EXPECT_STREQ(card.ctc().deviceName(),   "K2526-CTC");
}

// ─── ZVE1 (CPU) on K2526 card ────────────────────────────────────────────────

/**
 * @test K2526/CpuAccessor_ReturnsSameObject
 * @brief cpu() returns a stable reference; &cpu() == &cpu() across calls.
 * @par Pass criterion  Pointer identity holds for two successive calls.
 */
TEST(K2526, CpuAccessor_ReturnsSameObject)
{
    K1520Bus bus;
    K2526 card(bus);
    // cpu() must return a stable reference (same address across calls).
    EXPECT_EQ(&card.cpu(), &card.cpu());
}

/**
 * @test K2526/CpuReset_SetsPCToZero
 * @brief cpuReset() resets ZVE1's program counter to 0x0000.
 * @par Pass criterion  cpuPC() == 0x0000 after cpuReset().
 */
TEST(K2526, CpuReset_SetsPCToZero)
{
    K1520Bus bus;
    K2526 card(bus);
    card.attachToBus(bus);
    card.powerOn();
    card.cpuReset();
    EXPECT_EQ(card.cpuPC(), 0x0000);
}

/**
 * @test K2526/CpuIFF1_FalseAfterReset
 * @brief ZVE1 starts with interrupts disabled (IFF1 = false) after a hardware reset.
 * @par Pass criterion  cpuIFF1() == false.
 */
TEST(K2526, CpuIFF1_FalseAfterReset)
{
    K1520Bus bus;
    K2526 card(bus);
    card.attachToBus(bus);
    card.powerOn();
    card.cpuReset();
    // After a hardware reset the CPU starts with interrupts disabled.
    EXPECT_FALSE(card.cpuIFF1());
}

/**
 * @test K2526/CpuStep_WithBootRom_AdvancesPC
 * @brief cpuStep() executes one instruction from the boot ROM and advances PC past 0x0000.
 * @par Pass criterion  cpuStep() > 0 cycles; cpuPC() > 0x0000.
 */
TEST(K2526, CpuStep_WithBootRom_AdvancesPC)
{
    K1520Bus bus;
    K2526 card(bus);
    card.attachToBus(bus);
    card.powerOn();
    card.cpuReset();

    // Execute one instruction from the boot ROM at 0x0000.
    int cycles = card.cpuStep();

    // cpuStep() must return a positive cycle count and advance PC past 0.
    EXPECT_GT(cycles, 0);
    EXPECT_GT(card.cpuPC(), 0x0000);
}

/**
 * @test K2526/CpuStep_ReturnsPositiveCycles
 * @brief Ten successive cpuStep() calls each return a positive cycle count.
 * @par Pass criterion  cycles > 0 for all 10 steps.
 */
TEST(K2526, CpuStep_ReturnsPositiveCycles)
{
    K1520Bus bus;
    K2526 card(bus);
    card.attachToBus(bus);
    card.powerOn();
    card.cpuReset();

    // Run 10 steps — each must return > 0 cycles (no infinite loops here
    // because the boot ROM is valid Z80 code).
    for (int i = 0; i < 10; ++i) {
        int c = card.cpuStep();
        EXPECT_GT(c, 0) << "step " << i << " returned 0 cycles";
    }
}

/**
 * @test K2526/CpuNMI_DoesNotCrash
 * @brief cpuNMI() completes without crashing.
 * @par Pass criterion  No exception or assertion failure.
 */
TEST(K2526, CpuNMI_DoesNotCrash)
{
    K1520Bus bus;
    K2526 card(bus);
    card.attachToBus(bus);
    card.powerOn();
    card.cpuReset();
    // Just verify that cpuNMI() does not crash.
    card.cpuNMI();
}

// ─── ZVE2 (DMA-CPU) on K2526 card ────────────────────────────────────────────

/**
 * @test K2526/ZVE2_StartsInReset
 * @brief ZVE2 is held in reset at construction time.
 * @par Pass criterion  isZVE2InReset() == true.
 */
TEST(K2526, ZVE2_StartsInReset)
{
    K1520Bus bus;
    K2526 card(bus);
    EXPECT_TRUE(card.isZVE2InReset());
}

/**
 * @test K2526/ZVE2_Port04_Bit1_ReleasesReset
 * @brief Writing bit 0 = 1 to port 0x04 releases ZVE2 from reset.
 * @par Pass criterion  isZVE2InReset() == false after bus.ioWrite(0x04, 0x01).
 */
TEST(K2526, ZVE2_Port04_Bit1_ReleasesReset)
{
    K1520Bus bus;
    K2526 card(bus);
    card.attachToBus(bus);
    ASSERT_TRUE(card.isZVE2InReset());
    bus.ioWrite(0x04, 0x01);   // bit0=1 → /RES-ZVE2 released
    EXPECT_FALSE(card.isZVE2InReset());
}

/**
 * @test K2526/ZVE2_Port04_Bit0_AssertsReset
 * @brief Writing bit 0 = 0 to port 0x04 re-asserts ZVE2 reset after it was released.
 * @par Pass criterion  isZVE2InReset() == true after bus.ioWrite(0x04, 0x00).
 */
TEST(K2526, ZVE2_Port04_Bit0_AssertsReset)
{
    K1520Bus bus;
    K2526 card(bus);
    card.attachToBus(bus);
    bus.ioWrite(0x04, 0x01);   // release
    ASSERT_FALSE(card.isZVE2InReset());
    bus.ioWrite(0x04, 0x00);   // bit0=0 → /RES-ZVE2 asserted
    EXPECT_TRUE(card.isZVE2InReset());
}

/**
 * @test K2526/ZVE2_Released_PCStartsAtZero
 * @brief When ZVE2 is released from reset, its program counter starts at 0x0000.
 * @par Pass criterion  zve2().PC == 0x0000.
 */
TEST(K2526, ZVE2_Released_PCStartsAtZero)
{
    K1520Bus bus;
    K2526 card(bus);
    card.attachToBus(bus);
    bus.ioWrite(0x04, 0x01);   // release ZVE2
    EXPECT_EQ(card.zve2().PC, 0x0000u);
}

/**
 * @test K2526/ZVE2Step_ExecutesInstructions_WhenNotInReset
 * @brief zve2Step() executes one boot ROM instruction and advances ZVE2 PC.
 * @par Pass criterion  zve2Step() > 0 cycles; zve2().PC > pc_before.
 */
TEST(K2526, ZVE2Step_ExecutesInstructions_WhenNotInReset)
{
    K1520Bus bus;
    K2526 card(bus);
    card.attachToBus(bus);
    card.powerOn();   // boot ROM at 0x0000–0x03FF
    card.cpuReset();  // ZVE1 in reset

    bus.ioWrite(0x04, 0x01);   // release ZVE2 — starts at PC=0x0000
    ASSERT_FALSE(card.isZVE2InReset());

    uint16_t pc_before = card.zve2().PC;
    int cycles = card.zve2Step();   // execute first ROM instruction

    EXPECT_GT(cycles, 0);
    EXPECT_GT(card.zve2().PC, pc_before) << "ZVE2 PC must advance after step";
}

/**
 * @test K2526/ZVE2_ResetWhileRunning_FreezesPC
 * @brief Re-asserting ZVE2 reset after execution leaves the PC at its current value.
 * @details PC is not zeroed by the reset signal; it is only zeroed when ZVE2 is
 *   released again (Z80 reset sequence).
 * @par Pass criterion  isZVE2InReset() == true; PC is preserved (not zeroed).
 */
TEST(K2526, ZVE2_ResetWhileRunning_FreezesPC)
{
    K1520Bus bus;
    K2526 card(bus);
    card.attachToBus(bus);
    card.powerOn();
    bus.ioWrite(0x04, 0x01);   // release ZVE2
    card.zve2Step();            // execute one instruction
    ASSERT_GT(card.zve2().PC, 0u);

    bus.ioWrite(0x04, 0x00);   // put ZVE2 back in reset
    EXPECT_TRUE(card.isZVE2InReset());
    // PC is preserved (not zeroed by reset signal; only zeroed when released again)
}

// ─── /WAIT-ZVE2 (BS-PIO B3) ──────────────────────────────────────────────────
//
// Port B output bits (direction mask 0xE2: B0/B2/B3/B4 are outputs, active low):
//   PORTB_SAFE_ROMON  = 0x1D: B0=1(ROM on),  B2=1, B3=1, B4=1  – all others inactive
//   PORTB_SAFE_ROMOFF = 0x1C: B0=0(ROM off), B2=1, B3=1, B4=1  – all others inactive
//   PORTB_WAIT_ACTIVE = 0x15: B0=1(ROM on),  B2=1, B3=0, B4=1  – /WAIT-ZVE2 asserted
//   PORTB_SPSESR_ACT  = 0x18: B0=0(ROM off), B2=0, B3=1, B4=1  – /SPS-ESR asserted
//   PORTB_SA_ACTIVE   = 0x0C: B0=0(ROM off), B2=1, B3=1, B4=0  – /SA asserted

static constexpr uint8_t PORTB_SAFE_ROMON   = 0x1D;  ///< ROM on, all other outputs inactive
static constexpr uint8_t PORTB_SAFE_ROMOFF  = 0x1C;  ///< ROM off, all other outputs inactive
static constexpr uint8_t PORTB_WAIT_ACTIVE  = 0x15;  ///< ROM on, /WAIT-ZVE2 asserted (B3=0)
static constexpr uint8_t PORTB_WAITZVE2_REL = 0x1D;  ///< ROM on, /WAIT-ZVE2 released (B3=1)
static constexpr uint8_t PORTB_SPSESR_ACT   = 0x18;  ///< ROM off, /SPS-ESR asserted (B2=0)
static constexpr uint8_t PORTB_SA_ACTIVE    = 0x0C;  ///< ROM off, /SA asserted (B4=0)

/**
 * @test K2526/ZVE2_WaitInactive_Initially
 * @brief The /WAIT-ZVE2 signal is inactive (ZVE2 not stalled) at power-on.
 * @par Pass criterion  isZVE2Waiting() == false.
 */
TEST(K2526, ZVE2_WaitInactive_Initially)
{
    K1520Bus bus;
    K2526 card(bus);
    card.attachToBus(bus);
    card.powerOn();
    EXPECT_FALSE(card.isZVE2Waiting());
}

/**
 * @test K2526/ZVE2_Wait_PortB3Low_StallsZVE2
 * @brief Setting BS-PIO Port B bit 3 low (/WAIT-ZVE2 asserted) stalls ZVE2; zve2Step() returns 0.
 * @par Pass criterion  isZVE2Waiting() == true; zve2Step() == 0.
 */
TEST(K2526, ZVE2_Wait_PortB3Low_StallsZVE2)
{
    K1520Bus bus;
    K2526 card(bus);
    card.attachToBus(bus);
    card.powerOn();

    // Release ZVE2 from reset so it would run if not stalled.
    bus.ioWrite(0x04, 0x01);
    ASSERT_FALSE(card.isZVE2InReset());

    // Assert /WAIT-ZVE2 (B3=0): ROM on, SPS-ESR inactive, SA inactive.
    bus.ioWrite(BSPIO_PORTB_DATA, PORTB_WAIT_ACTIVE);
    EXPECT_TRUE(card.isZVE2Waiting());

    // zve2Step() must return 0 (no instruction executed) while waiting.
    EXPECT_EQ(card.zve2Step(), 0);
}

/**
 * @test K2526/ZVE2_Wait_PortB3High_ResumesZVE2
 * @brief Releasing /WAIT-ZVE2 (Port B B3=1) allows ZVE2 to execute instructions again.
 * @par Pass criterion  isZVE2Waiting() == false; zve2Step() > 0 after release.
 */
TEST(K2526, ZVE2_Wait_PortB3High_ResumesZVE2)
{
    K1520Bus bus;
    K2526 card(bus);
    card.attachToBus(bus);
    card.powerOn();

    bus.ioWrite(0x04, 0x01);                            // release ZVE2
    bus.ioWrite(BSPIO_PORTB_DATA, PORTB_WAIT_ACTIVE);  // stall ZVE2
    ASSERT_TRUE(card.isZVE2Waiting());

    bus.ioWrite(BSPIO_PORTB_DATA, PORTB_WAITZVE2_REL); // release ZVE2 wait
    EXPECT_FALSE(card.isZVE2Waiting());

    // zve2Step() should now execute an instruction from the boot ROM.
    EXPECT_GT(card.zve2Step(), 0);
}

/**
 * @test K2526/ZVE2_Wait_DoesNotAffectZVE1
 * @brief Stalling ZVE2 via /WAIT-ZVE2 does not prevent ZVE1 from executing instructions.
 * @par Pass criterion  cpuStep() > 0 cycles; cpuPC() > 0x0000 despite isZVE2Waiting().
 */
TEST(K2526, ZVE2_Wait_DoesNotAffectZVE1)
{
    K1520Bus bus;
    K2526 card(bus);
    card.attachToBus(bus);
    card.powerOn();
    card.cpuReset();

    // Stall ZVE2 via /WAIT-ZVE2 – must not prevent ZVE1 from running.
    bus.ioWrite(BSPIO_PORTB_DATA, PORTB_WAIT_ACTIVE);
    ASSERT_TRUE(card.isZVE2Waiting());

    int cycles = card.cpuStep();
    EXPECT_GT(cycles, 0);
    EXPECT_GT(card.cpuPC(), 0x0000u);
}

// ─── Q240 Speicher-/E/A-Schutz (MemIOProtect / spa_), Originalschaltung 6.11 ──
//
// Fixture: K2526 + K3526 RAM am Bus, Lade-ROM abgeschaltet.  cpu().readByte/writeByte/
// readPort/writePort sind die ZVE1-Callbacks MIT Schutzlogik (bus.memWrite umgeht sie).
// Der M1-Befehlsbereich (FF "Befehlsaufruf") wird von cpuStep() gesetzt; die meisten
// Tests setzen ihn direkt ueber spa().befehlsaufruf(pc).
//
//   B2 = 1: Schutz unwirksam, jeder Speicherzugriff schreibt Schutz-RAM[A>>6] := AB0
//   B2 = 0: scharf; Anwender-Befehl (Bereich 2) darf kein Bereich-1-Segment beschreiben,
//           E/A aus Bereich 2 -> NMI; Lesen nie beeinflusst

class K2526Protection : public ::testing::Test {
protected:
    K1520Bus bus;
    K2526    zre{bus};
    K3526    ops;

    void SetUp() override {
        ops.attachToBus(bus);     // RAM 0x0000-0xFFFF
        zre.attachToBus(bus);     // ports 0x00-0x0F
        zre.powerOn();            // Lade-ROM bei 0000H-03FFH; Q240: B2=1
        bus.ioWrite(BSPIO_PORTB_DATA, PORTB_SAFE_ROMOFF);   // ROM aus, B2=1
        ops.fill(0xAA);
    }

    /// Segment als Bereich 1 (BS) programmieren: Zugriff mit ungerader Adresse (AB0=1).
    void markiereBS(uint16_t seg_start) { zre.cpu().readByte(seg_start | 1); }
    /// Segment als Bereich 2 (Anwender) programmieren: gerade Adresse (AB0=0).
    void markiereAnwender(uint16_t seg_start) { zre.cpu().readByte(seg_start & ~1); }
    void scharf()       { bus.ioWrite(BSPIO_PORTB_DATA, PORTB_SPSESR_ACT); }
    void entschaerfen() { bus.ioWrite(BSPIO_PORTB_DATA, PORTB_SAFE_ROMOFF); }
    /// Befehl aus dem Segment von @p pc starten (M1).
    void befehlAus(uint16_t pc) { zre.spa().befehlsaufruf(pc); }
};

/**
 * @test K2526Protection/Reset_StartetMitB2Eins
 * @brief Einschaltzustand: B2 = 1 (Schutz unwirksam), Speicher arbeitet normal.
 */
TEST_F(K2526Protection, Reset_StartetMitB2Eins)
{
    EXPECT_TRUE(zre.spa().programmieren());
    zre.cpu().writeByte(0x0400, 0x55);
    EXPECT_EQ(ops.rawPtr()[0x0400], 0x55);
    EXPECT_EQ(zre.cpu().readByte(0x0400), 0x55);
    EXPECT_FALSE(zre.isSPSViolation());
}

/**
 * @test K2526Protection/SpsEsr_SchaltetScharf
 * @brief B2 = 0 schaltet den Schutz scharf, B2 = 1 wieder aus.
 */
TEST_F(K2526Protection, SpsEsr_SchaltetScharf)
{
    scharf();
    EXPECT_TRUE(zre.spa().scharf());
    entschaerfen();
    EXPECT_TRUE(zre.spa().programmieren());
}

/**
 * @test K2526Protection/B2Eins_ProgrammiertUeberAB0
 * @brief Bei B2 = 1 schreibt jeder ZVE1-Speicherzugriff AB0 an Schutz-RAM[A>>6]; Speicher normal.
 */
TEST_F(K2526Protection, B2Eins_ProgrammiertUeberAB0)
{
    EXPECT_EQ(zre.cpu().readByte(0x0401), 0xAA);        // Lesen liefert RAM, nicht Tabelle
    EXPECT_TRUE(zre.spa().bereich1(0x0400));
    EXPECT_TRUE(zre.spa().bereich1(0x043F));            // gleiches Segment
    EXPECT_FALSE(zre.spa().bereich1(0x0440));           // naechstes Segment unberuehrt

    zre.cpu().readByte(0x0400);                         // gerade Adresse -> "0"
    EXPECT_FALSE(zre.spa().bereich1(0x0400));

    zre.cpu().writeByte(0x0481, 0x12);                  // Schreibzugriff programmiert ebenfalls
    EXPECT_TRUE(zre.spa().bereich1(0x0480));
    EXPECT_EQ(ops.rawPtr()[0x0481], 0x12);
    zre.cpu().writeByte(0x0480, 0x34);
    EXPECT_FALSE(zre.spa().bereich1(0x0480));
}

/**
 * @test K2526Protection/LesenLiefertImmerSpeicher
 * @brief Regression SIOS: bei B2 = 0 wie B2 = 1 liefert Lesen/Opcode-Abruf den Speicher.
 */
TEST_F(K2526Protection, LesenLiefertImmerSpeicher)
{
    bus.memWrite(static_cast<uint16_t>(0x0800), 0x3C);
    EXPECT_EQ(zre.cpu().readByte(0x0800), 0x3C);
    scharf();
    befehlAus(0x0800);
    EXPECT_EQ(zre.cpu().readByte(0x0800), 0x3C);
    EXPECT_FALSE(zre.isSPSViolation());
}

/**
 * @test K2526Protection/Scharf_AnwenderBefehlDarfBSNichtBeschreiben
 * @brief Anwender-Befehl schreibt in Bereich-1-Segment: unterdrueckt, SPS-Ind gesetzt, A3 low, kein NMI.
 */
TEST_F(K2526Protection, Scharf_AnwenderBefehlDarfBSNichtBeschreiben)
{
    markiereBS(0x0400);
    markiereAnwender(0x0800);
    ops.fill(0xBB);
    scharf();
    befehlAus(0x0800);                                  // Befehl aus Bereich 2

    zre.cpu().writeByte(0x0400, 0x55);
    EXPECT_EQ(ops.rawPtr()[0x0400], 0xBB);
    EXPECT_TRUE(zre.isSPSViolation());
    EXPECT_EQ(bus.ioRead(BSPIO_PORTA_DATA) & 0x08, 0) << "SPS-Ind (A3) aktiv low";
    EXPECT_FALSE(bus.isNMI()) << "Speicherverletzung meldet ueber die PIO, nicht per NMI";
}

/** @test K2526Protection/Scharf_AnwenderDarfAnwenderBeschreiben */
TEST_F(K2526Protection, Scharf_AnwenderDarfAnwenderBeschreiben)
{
    markiereBS(0x0400);
    markiereAnwender(0x0800);
    markiereAnwender(0x0C00);
    scharf();
    befehlAus(0x0800);
    zre.cpu().writeByte(0x0C10, 0x66);
    EXPECT_EQ(ops.rawPtr()[0x0C10], 0x66);
    EXPECT_FALSE(zre.isSPSViolation());
}

/** @test K2526Protection/Scharf_BSBefehlDarfUeberallSchreiben */
TEST_F(K2526Protection, Scharf_BSBefehlDarfUeberallSchreiben)
{
    markiereBS(0x0400);
    markiereAnwender(0x0800);
    scharf();
    befehlAus(0x0400);                                  // Befehl aus Bereich 1
    zre.cpu().writeByte(0x0400, 0x11);
    zre.cpu().writeByte(0x0800, 0x22);
    EXPECT_EQ(ops.rawPtr()[0x0400], 0x11);
    EXPECT_EQ(ops.rawPtr()[0x0800], 0x22);
    EXPECT_FALSE(zre.isSPSViolation());
}

/**
 * @test K2526Protection/Scharf_AbrufDurchCpuStep
 * @brief cpuStep() setzt das FF Befehlsaufruf aus der M1-Adresse (PC vor dem Befehl).
 */
TEST_F(K2526Protection, Scharf_AbrufDurchCpuStep)
{
    markiereBS(0x0400);
    markiereAnwender(0x0800);
    ops.fill(0xBB);
    const uint8_t code[] = {0x32, 0x00, 0x04};          // LD (0400H),A
    for (int i = 0; i < 3; ++i) bus.memWrite(static_cast<uint16_t>(0x0800 + i), code[i]);
    for (int i = 0; i < 3; ++i) bus.memWrite(static_cast<uint16_t>(0x0408 + i), code[i]);
    scharf();
    zre.cpuReset();
    zre.cpu().PC = 0x0800;
    zre.cpu().A  = 0x77;
    zre.cpuStep();
    EXPECT_EQ(ops.rawPtr()[0x0400], 0xBB);
    EXPECT_TRUE(zre.isSPSViolation());
    // Derselbe Befehl aus einem BS-Segment geht durch.
    bus.clearNMI();
    zre.spa().resetViolation();
    zre.cpu().PC = 0x0408;
    zre.cpuStep();
    EXPECT_EQ(ops.rawPtr()[0x0400], 0x77);
}

/** @test K2526Protection/B2Eins_SchuetztNicht */
TEST_F(K2526Protection, B2Eins_SchuetztNicht)
{
    markiereBS(0x0400);
    befehlAus(0x0800);
    zre.cpu().writeByte(0x0401, 0x55);
    EXPECT_EQ(ops.rawPtr()[0x0401], 0x55);
    EXPECT_FALSE(zre.isSPSViolation());
}

/**
 * @test K2526Protection/ResSpa_LoeschtIndikator
 * @brief /RES-SPA (Port 02H) loescht SPS-Indikator (A3 wieder high); Schutz-RAM bleibt.
 */
TEST_F(K2526Protection, ResSpa_LoeschtIndikator)
{
    markiereBS(0x0400);
    markiereAnwender(0x0800);
    scharf();
    befehlAus(0x0800);
    zre.cpu().writeByte(0x0400, 0x55);
    ASSERT_TRUE(zre.isSPSViolation());

    bus.ioWrite(0x02, 0x00);
    EXPECT_FALSE(zre.isSPSViolation());
    EXPECT_NE(bus.ioRead(BSPIO_PORTA_DATA) & 0x08, 0) << "A3 Ruhepegel high";
    EXPECT_TRUE(zre.spa().bereich1(0x0400));
}

/**
 * @test K2526Protection/Indikator_HebtSperreAufBisResSpa
 * @brief Rueckkopplung A34/9: nach der ersten Verletzung wird /MEMDI nicht erneut aktiv.
 */
TEST_F(K2526Protection, Indikator_HebtSperreAufBisResSpa)
{
    markiereBS(0x0400);
    markiereAnwender(0x0800);
    scharf();
    befehlAus(0x0800);
    zre.cpu().writeByte(0x0400, 0x11);                  // gesperrt
    zre.cpu().writeByte(0x0400, 0x22);                  // Merkmal steht: geht durch
    EXPECT_EQ(ops.rawPtr()[0x0400], 0x22);
    bus.ioWrite(0x02, 0x00);                            // /RES-SPA (am Bus, ohne E/A-Schutz)
    zre.cpu().writeByte(0x0400, 0x33);                  // Sperre wieder aktiv
    EXPECT_EQ(ops.rawPtr()[0x0400], 0x22);
}

/**
 * @test K2526Protection/Scharf_EAAusAnwenderbereichLoestNMI
 * @brief IN/OUT aus Bereich 2 bei scharfem Schutz: NMI, SPS-Ind unberuehrt, Zugriff ohne Wirkung.
 */
TEST_F(K2526Protection, Scharf_EAAusAnwenderbereichLoestNMI)
{
    markiereAnwender(0x0800);
    scharf();
    befehlAus(0x0800);
    ASSERT_FALSE(bus.isNMI());
    zre.cpu().writePort(0x0003, 0x5A);
    EXPECT_TRUE(bus.isNMI());
    EXPECT_FALSE(zre.isSPSViolation());
    EXPECT_NE(bus.ioRead(BSPIO_PORTA_DATA) & 0x04, 0) << "A2 high = unerlaubter E/A (fluechtig)";
    bus.clearNMI();
    EXPECT_EQ(zre.cpu().readPort(0x0006), 0xFF);        // abgebrochen
    EXPECT_TRUE(bus.isNMI());
}

/** @test K2526Protection/Scharf_EAAusBSBereichErlaubt */
TEST_F(K2526Protection, Scharf_EAAusBSBereichErlaubt)
{
    markiereBS(0x0400);
    scharf();
    befehlAus(0x0400);
    zre.cpu().writePort(0x0003, 0x5A);
    EXPECT_FALSE(bus.isNMI());
}

/** @test K2526Protection/B2Eins_EAErlaubt */
TEST_F(K2526Protection, B2Eins_EAErlaubt)
{
    markiereAnwender(0x0800);
    befehlAus(0x0800);
    zre.cpu().writePort(0x0003, 0x5A);
    EXPECT_FALSE(bus.isNMI());
}

/**
 * @test K2526Protection/Reset_BehaeltTabelle_LoeschtFFsUndB2
 * @brief /RESET (powerOn): Schutz-RAM bleibt, FFs aus, B2 = 1.
 */
TEST_F(K2526Protection, Reset_BehaeltTabelle_LoeschtFFsUndB2)
{
    markiereBS(0x0400);
    markiereAnwender(0x0800);
    scharf();
    befehlAus(0x0800);
    zre.cpu().writeByte(0x0400, 0x55);
    ASSERT_TRUE(zre.isSPSViolation());

    zre.powerOn();
    EXPECT_FALSE(zre.isSPSViolation());
    EXPECT_TRUE(zre.spa().programmieren());
    EXPECT_TRUE(zre.spa().bereich1(0x0400));
    EXPECT_FALSE(zre.spa().bereich1(0x0800));
}

/**
 * @test K2526Protection/ZVE2_BypassesProtection
 * @brief ZVE2 (DMA) wird nicht gefiltert und programmiert das Schutz-RAM nicht.
 */
TEST_F(K2526Protection, ZVE2_BypassesProtection)
{
    markiereBS(0x0400);
    markiereAnwender(0x0800);
    scharf();
    befehlAus(0x0800);
    ops.fill(0xDD);

    zre.zve2().writeByte(0x0400, 0x77);
    EXPECT_EQ(ops.rawPtr()[0x0400], 0x77);
    EXPECT_FALSE(zre.isSPSViolation());
    EXPECT_EQ(zre.zve2().readByte(0x0400), 0x77);

    entschaerfen();
    zre.zve2().readByte(0x0C01);                        // ungerade, aber ZVE2: keine Programmierung
    EXPECT_FALSE(zre.spa().bereich1(0x0C00));
}

/**
 * @test K2526Protection/SegmentBoundary_64ByteAligned
 * @brief Segmente sind 64 Byte breit (Index = A >> 6).
 */
TEST_F(K2526Protection, SegmentBoundary_64ByteAligned)
{
    markiereBS(0x0000);                                 // Segment 0
    markiereAnwender(0x0040);                           // Segment 1
    markiereAnwender(0x0800);
    scharf();
    befehlAus(0x0800);
    zre.cpu().writeByte(0x003F, 0x11);
    EXPECT_TRUE(zre.isSPSViolation());
    bus.ioWrite(0x02, 0x00);
    zre.cpu().writeByte(0x0040, 0x22);
    EXPECT_FALSE(zre.isSPSViolation());
    EXPECT_EQ(ops.rawPtr()[0x0040], 0x22);
}

/**
 * @test K2526Protection/SIOS_Scharfschaltfolge_BleibtImBereich1
 * @brief Nachbildung SIOS 0AFFH: IN A,(0AH); RES 2,A; NOP; OUT (0AH),A; EI; RET.
 *   Opcode-Abruf liefert bei jedem B2 den Speicher; das Segment des Befehlsstroms bleibt "1"
 *   (letzter Abruf vor dem Scharfschalten: Operand 0B05H, ungerade).
 */
TEST_F(K2526Protection, SIOS_Scharfschaltfolge_BleibtImBereich1)
{
    const uint8_t code[] = {0xDB, 0x0A, 0xCB, 0x97, 0x00, 0xD3, 0x0A, 0xFB, 0xC9};
    const uint16_t org = 0x0AFF;
    for (size_t i = 0; i < sizeof(code); ++i) bus.memWrite(static_cast<uint16_t>(org + i), code[i]);
    zre.cpuReset();
    zre.cpu().PC = org;
    zre.cpu().SP = 0x0400;
    for (int i = 0; i < 4; ++i) zre.cpuStep();          // IN, RES, NOP, OUT
    EXPECT_TRUE(zre.spa().scharf());
    EXPECT_EQ(zre.cpuPC(), org + 7);
    EXPECT_TRUE(zre.spa().bereich1(0x0B04));            // Segment 0B00-0B3F: "1"
    EXPECT_TRUE(zre.spa().bereich1(0x0AFF));            // Segment 0AC0-0AFF: "1"
}

// ─── /INT-BS (port 00H) ───────────────────────────────────────────────────────

/**
 * @test K2526/IntBS_Port00_DoesNotCrash
 * @brief Writing to port 0x00 (/INT-BS latch) does not crash and does not trigger Q240 protection.
 * @par Pass criterion  No exception; isSPSViolation() == false.
 */
TEST(K2526, IntBS_Port00_DoesNotCrash)
{
    K1520Bus bus;
    K2526 card(bus);
    card.attachToBus(bus);
    card.powerOn();

    bus.ioWrite(0x00, 0x00);              // write port 00H
    EXPECT_FALSE(card.isSPSViolation());  // must not trigger protection violation
}

/**
 * @test K2526/IntBS_Port00_PulstB1_PIOMerktInterrupt
 * @brief OUT 00H ist ein IMPULS auf B1; die wie SIOS programmierte PIO (Bitmodus, ODER,
 *   aktiv HIGH, Maske nur B1) merkt ihn sich.  Ruhepegel B1 = 0.
 */
TEST(K2526, IntBS_Port00_PulstB1_PIOMerktInterrupt)
{
    K1520Bus bus;
    K2526 card(bus);
    card.attachToBus(bus);
    card.powerOn();
    EXPECT_EQ(bus.ioRead(BSPIO_PORTB_DATA) & 0x02, 0) << "Ruhepegel B1 = 0";

    bus.ioWrite(0x0B, 0x76);   // Vektor
    bus.ioWrite(0x0B, 0xB7);   // Int. ein, ODER, aktiv HIGH, Maske folgt
    bus.ioWrite(0x0B, 0xFD);   // nur B1 ueberwacht
    card.setIEI(true);
    ASSERT_FALSE(card.hasInterrupt());

    bus.ioWrite(0x00, 0x00);
    EXPECT_TRUE(card.hasInterrupt()) << "OUT 00H muss den Interrupt anmelden";
    EXPECT_EQ(bus.ioRead(BSPIO_PORTB_DATA) & 0x02, 0) << "B1 faellt nach dem Impuls zurueck";
    EXPECT_FALSE(card.isSPSViolation());
}

// ─── /SA (power-off, BS-PIO B4) ───────────────────────────────────────────────

/**
 * @test K2526/SA_InitiallyNotRequested
 * @brief isShutdownRequested() returns false before any /SA signal.
 * @par Pass criterion  isShutdownRequested() == false.
 */
TEST(K2526, SA_InitiallyNotRequested)
{
    K1520Bus bus;
    K2526 card(bus);
    EXPECT_FALSE(card.isShutdownRequested());
}

/**
 * @test K2526/SA_PortB4Low_SetsShutdownFlag
 * @brief Setting BS-PIO Port B bit 4 low (/SA asserted, active low) sets the shutdown flag.
 * @par Pass criterion  isShutdownRequested() == true after PORTB_SA_ACTIVE write.
 */
TEST(K2526, SA_PortB4Low_SetsShutdownFlag)
{
    K1520Bus bus;
    K2526 card(bus);
    card.attachToBus(bus);
    card.powerOn();

    // B4=0 (active low) → /SA asserted → power-off requested.
    bus.ioWrite(BSPIO_PORTB_DATA, PORTB_SA_ACTIVE);
    EXPECT_TRUE(card.isShutdownRequested());
}

/**
 * @test K2526/SA_PortB4High_NoShutdown
 * @brief Setting BS-PIO Port B bit 4 high (/SA not asserted) leaves the shutdown flag clear.
 * @par Pass criterion  isShutdownRequested() == false after PORTB_SAFE_ROMON write.
 */
TEST(K2526, SA_PortB4High_NoShutdown)
{
    K1520Bus bus;
    K2526 card(bus);
    card.attachToBus(bus);
    card.powerOn();

    // B4=1 → /SA not asserted (all safe: B2=1, B3=1, B4=1).
    bus.ioWrite(BSPIO_PORTB_DATA, PORTB_SAFE_ROMON);
    EXPECT_FALSE(card.isShutdownRequested());
}

// ─── ZVE2: start-from-reset + multi-round PC re-zero (boot DMA edge cases) ─────

/**
 * @test K2526/ZVE2_StartFromReset_ClearsResetWaitAndZeroesPC
 * @brief zve2StartFromReset() releases reset + /WAIT and restarts ZVE2 at PC=0.
 * @details This is the path the 3rd-stage @OS.COM read relies on: ZVE2 is poised
 *   in reset (OUT 04H bit0=0) with its DMA-routine entry at [0x0000], and the
 *   /BUSRQ of the /STR starts it from PC=0 without an explicit bit0=1.  The
 *   function must clear BOTH the reset latch and /WAIT-ZVE2 and zero the PC.
 * @par Pass criterion  after a run+reset, zve2StartFromReset() →
 *   isZVE2InReset()==false, isZVE2Waiting()==false, PC==0.
 */
TEST(K2526, ZVE2_StartFromReset_ClearsResetWaitAndZeroesPC)
{
    K1520Bus bus;
    K2526 card(bus);
    card.attachToBus(bus);
    card.powerOn();

    bus.ioWrite(0x04, 0x01);    // release + run ZVE2
    card.zve2Step();            // PC advances off 0
    ASSERT_GT(card.zve2().PC, 0u);
    bus.ioWrite(0x04, 0x00);    // hold in reset again (PC preserved by HW)
    ASSERT_TRUE(card.isZVE2InReset());

    card.zve2StartFromReset();
    EXPECT_FALSE(card.isZVE2InReset())  << "reset latch must be cleared";
    EXPECT_FALSE(card.isZVE2Waiting())  << "/WAIT-ZVE2 must be cleared";
    EXPECT_EQ(card.zve2().PC, 0x0000u)  << "ZVE2 must restart from PC=0";
}

/**
 * @test K2526/ZVE2_Port04_Bit1_WhileRunning_RezeroesPC
 * @brief OUT(04H) bit0=1 restarts ZVE2 from PC=0 even when it is already running.
 * @details The boot ROM's CALL 0194 issues OUT(04H) bit0=1 before EVERY DMA
 *   round; ZVE2 must reset to PC=0 each time so it reloads its IDAM registers for
 *   the new cyl/sector — not only on a reset→run edge.  Regression guard for the
 *   multi-round bootloader (see k2526.cpp port-04 handler).
 * @par Pass criterion  after stepping ZVE2 (PC>0), a second bit0=1 zeroes PC.
 */
TEST(K2526, ZVE2_Port04_Bit1_WhileRunning_RezeroesPC)
{
    K1520Bus bus;
    K2526 card(bus);
    card.attachToBus(bus);
    card.powerOn();

    bus.ioWrite(0x04, 0x01);    // release + run
    card.zve2Step();
    card.zve2Step();
    ASSERT_GT(card.zve2().PC, 0u);

    bus.ioWrite(0x04, 0x01);    // bit0=1 AGAIN while running → must re-zero PC
    EXPECT_EQ(card.zve2().PC, 0x0000u)
        << "OUT(04H) bit0=1 must restart ZVE2 from PC=0 every round";
    EXPECT_FALSE(card.isZVE2InReset());
}

// ─── K5122 → K1520 bus → ZVE2 data chain (floppy DMA read path) ───────────────
//
// The whole boot DMA chain — K5122 streams a sector field on port 0x16, the
// K1520 bus routes the IN to it, and the REAL ZVE2 Z80 reads it — was only ever
// exercised by the full multi-million-cycle boot, never as a focused unit test.
// (The Continuous1024 K5122 tests drive ioRead(0x16) directly; the K2526 ZVE2
// tests step ZVE2 over the boot ROM.)  This fixture wires K2526(+ZVE2) + K3526
// RAM + K5122 on one bus and runs an actual ZVE2 program that reads port 0x16,
// proving the field reaches ZVE2 byte-for-byte via the bus — the path where the
// head-select (bit2) bug lived.

namespace {
/// cpa780 aus dem YAML-Katalog (data/formats.yaml, §8.6) holen.
DiskFormat cpa780Format() {
    std::string fatal;
    FormatCatalog cat = FormatCatalog::loadDefault(&fatal);
    EXPECT_TRUE(fatal.empty()) << fatal;
    const DiskFormat* f = cat.find("cpa780");
    EXPECT_NE(f, nullptr) << "Format 'cpa780' fehlt im Katalog";
    return f ? *f : DiskFormat{};
}

// Temporaeres cpa780-Image anlegen.  Der Dateiname enthaelt Prozess-ID UND Testnamen:
// unter `ctest -j` laufen die Tests dieser Fixture in NEBENLAEUFIGEN Prozessen, und ein
// gemeinsamer fester Pfad liesse den einen das Image des anderen ueberschreiben oder im
// TearDown wegloeschen.
std::string makeCpa780Image() {
    const DiskFormat fmt = cpa780Format();
    const auto* info = ::testing::UnitTest::GetInstance()->current_test_info();
    const std::string tag = std::string(info ? info->name() : "unknown")
                            + "_" + std::to_string(k1520::os::processId());
    auto path = k1520test::tempPath(("k2526_chain_cpa780_" + tag + ".img"));
    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    std::vector<uint8_t> buf(fmt.totalBytes(), 0xE5);
    f.write(reinterpret_cast<const char*>(buf.data()), static_cast<std::streamsize>(buf.size()));
    return path;
}
}  // namespace

class K2526ZVE2FloppyChain : public ::testing::Test {
protected:
    K1520Bus bus;
    K2526    zre{bus};
    K3526    ops;
    K5122    afs{bus};
    DiskFormat fmt = cpa780Format();
    std::string img;

    void SetUp() override {
        ops.attachToBus(bus);          // RAM 0x0000–0xFFFF
        zre.attachToBus(bus);          // ports 0x00–0x0F (+ ZVE1/ZVE2)
        bus.registerIO(&afs, 0x10, 9); // K5122 ports 0x10–0x18
        zre.powerOn();
        bus.ioWrite(BSPIO_PORTB_DATA, PORTB_SAFE_ROMOFF);  // ROM off → 0x0000 is RAM
        img = makeCpa780Image();
        ASSERT_TRUE(afs.mountDisk(0, img, fmt));
        afs.ioWrite(0x18, 0x00);       // select drive 0
    }
    void TearDown() override { if (!img.empty()) std::filesystem::remove(img); }

    void seekToCyl(int target) {
        for (int c = 0; c < target; ++c) {
            afs.ioWrite(0x10, 0xFF); afs.ioWrite(0x10, 0x7F); afs.ioWrite(0x10, 0xFF);
        }
    }
};

/**
 * @test K2526ZVE2FloppyChain/ZVE2ReadsK5122FieldViaBus
 * @brief A real ZVE2 program (IN A,(16H) loop) reads the K5122's continuous-read
 *   IDAM field through the K1520 bus, byte-for-byte.
 * @details Seek to the cyl-2 1024B data track, /STR a head-0 read (bit2=1) so the
 *   K5122 builds the field and asserts BUSRQ, then release ZVE2 (PC=0) and step
 *   it.  The ZVE2 code reads 6 bytes from port 0x16 into RAM 0x0100.  They must be
 *   the IDAM [A1 FE cyl head sec size] = A1 FE 02 00 01 03 — exercising
 *   K5122.ioRead(0x16) → bus.ioRead → ZVE2.readPort end to end.
 */
TEST_F(K2526ZVE2FloppyChain, ZVE2ReadsK5122FieldViaBus) {
    seekToCyl(2);

    // ZVE2 program at 0x0000: LD HL,0x0100; LD B,9; (IN A,(16);LD(HL),A;INC HL;DJNZ); HALT
    const uint8_t prog[] = { 0x21,0x00,0x01, 0x06,0x09, 0xDB,0x16, 0x77, 0x23, 0x10,0xFA, 0x76 };
    for (size_t i = 0; i < sizeof(prog); ++i)
        bus.memWrite(static_cast<uint16_t>(i), prog[i]);

    // MFM-Lesemodus latchen (0x85 = MFM-Marke + /STR head0 → startReadTransfer baut den
    // treuen 4×A1-MFM-Lese-Stream, setzt transferring_, assertiert BUSRQ).
    afs.ioWrite(0x10, 0xFF);
    afs.ioWrite(0x10, 0x85);   // MFM-Marke, /STR=0, bit2=1 (head 0)
    ASSERT_TRUE(bus.isBUSRQ());
    // MK1-Resync-Strobe: Kopf vor die erste IDAM-Markensequenz (markPos-4 = erstes A1).
    afs.ioWrite(0x10, 0x95);   // MK1 steigende Flanke

    bus.ioWrite(0x04, 0x01);   // release ZVE2 → runs from PC=0
    for (int i = 0; i < 80 && !zre.zve2().halted; ++i) zre.zve2Step();

    // Treuer MFM-Lese-Stream nach Resync: [A1 A1 A1 A1 FE cyl head sec size].
    EXPECT_EQ(bus.memRead(0x0100), 0xA1) << "A1 sync 1/4 via ZVE2";
    EXPECT_EQ(bus.memRead(0x0103), 0xA1) << "A1 sync 4/4";
    EXPECT_EQ(bus.memRead(0x0104), 0xFE) << "IDAM address mark";
    EXPECT_EQ(bus.memRead(0x0105), 0x02) << "cylinder (2)";
    EXPECT_EQ(bus.memRead(0x0106), 0x00) << "head (0)";
    EXPECT_EQ(bus.memRead(0x0107), 0x01) << "sector (1)";
    EXPECT_EQ(bus.memRead(0x0108), 0x03) << "size code (1024B)";
}

/**
 * @test K2526ZVE2FloppyChain/ZVE2ReadsHead1FieldViaBus
 * @brief Same chain for HEAD 1 (the @OS.COM side): bit2=0 must make the K5122
 *   serve a head-1 IDAM to ZVE2 — the exact path the bit2 side-select fix
 *   repaired (head 1 is first used by @OS.COM).
 * @par Pass criterion  ZVE2 reads IDAM head byte == 1 from cyl 2.
 */
TEST_F(K2526ZVE2FloppyChain, ZVE2ReadsHead1FieldViaBus) {
    seekToCyl(2);

    const uint8_t prog[] = { 0x21,0x00,0x01, 0x06,0x09, 0xDB,0x16, 0x77, 0x23, 0x10,0xFA, 0x76 };
    for (size_t i = 0; i < sizeof(prog); ++i)
        bus.memWrite(static_cast<uint16_t>(i), prog[i]);

    // Head-1-Lesemodus ueber das Pfad-/Lese-Steuerwort 0x81 (MFM-Marke + bit2=0 → Kopf 1):
    // es waehlt die Seite (setHead), armiert den head-1-Read (startReadTransfer baut den
    // treuen 4×A1-MFM-Stream) und assertiert als /STR-Fallflanke BUSRQ.  Die Seitenwahl
    // traegt seit dem Fix NUR das Pfad-Byte, nicht der /STR-Strobe (dessen bit2 inzidentell).
    afs.ioWrite(0x10, 0xFF);
    afs.ioWrite(0x10, 0x81);   // MFM-Marke + /STR head1 (bit2=0) → head-1 Read armiert
    ASSERT_TRUE(bus.isBUSRQ());
    // MK1-Resync-Strobe: Kopf vor die erste IDAM-Markensequenz (bit2 hier egal — kein Flip).
    afs.ioWrite(0x10, 0x91);   // MK1 steigende Flanke → Resync auf erstes A1

    bus.ioWrite(0x04, 0x01);
    for (int i = 0; i < 80 && !zre.zve2().halted; ++i) zre.zve2Step();

    EXPECT_EQ(bus.memRead(0x0103), 0xA1) << "A1 sync (4/4)";
    EXPECT_EQ(bus.memRead(0x0104), 0xFE) << "IDAM mark";
    EXPECT_EQ(bus.memRead(0x0105), 0x02) << "cylinder (2)";
    EXPECT_EQ(bus.memRead(0x0106), 0x01) << "head (1) — bit2=0 side-select via the bus";
}

// ─── BS-PIO Port A: /WR als Schreib-Strobe (A5) ──────────────────────────────
//
// Die Speicher-Ausbaumessung des Lade-ROMs (0040H–005AH) und HARDYs MEMDI-RDY-Test
// schaerfen Port A im Bitmodus mit Maske 9FH (A5 /WR AND A6 /RDY, aktiv-LOW), geben
// `EI` und erwarten den Interrupt DURCH den nachfolgenden Schreibbefehl; ihre ISR
// prueft, ob das Testbyte im Speicher gelandet ist.  Lag /WR dauerhaft aktiv, kam
// der Interrupt schon vor dem Schreiben — bei frischem DRAM (0xFF) unauffaellig,
// bei einem Neustart aus dem laufenden Betrieb meldete die Messung „kein Speicher".

class K2526WriteStrobe : public ::testing::Test {
protected:
    K1520Bus bus;
    K2526    zre{bus};
    K3526    ops;

    void SetUp() override {
        ops.attachToBus(bus);
        zre.attachToBus(bus);
        zre.powerOn();
        bus.ioWrite(BSPIO_PORTB_DATA, PORTB_SAFE_ROMOFF);   // ROM aus → RAM ab 0x0000
        zre.setIEI(true);   // Daisy-Chain freigeben (in der Maschine macht das der Bus)
    }
    /// Port A wie das Lade-ROM scharf schalten: D7H (IE, AND, aktiv-LOW, Maske folgt) + Maske 9FH.
    void armPortA() {
        bus.ioWrite(BSPIO_PORTA_CTRL, 0xD7);
        bus.ioWrite(BSPIO_PORTA_CTRL, 0x9F);
    }
};

/**
 * @test K2526WriteStrobe/ArmingAloneRequestsNoInterrupt
 * @brief Scharfschalten allein loest nichts aus — /WR ruht (HIGH), bis geschrieben wird.
 */
TEST_F(K2526WriteStrobe, ArmingAloneRequestsNoInterrupt) {
    armPortA();
    EXPECT_FALSE(zre.hasInterrupt())
        << "Port A meldet Interrupt ohne Schreibzugriff — /WR (A5) ist faelschlich "
           "dauerhaft aktiv; die ROM-Ausbaumessung liefe dann VOR ihrem Testschreiben";
}

/**
 * @test K2526WriteStrobe/Zve1WriteRequestsInterrupt
 * @brief Ein ZVE1-Schreibzugriff pulst /WR und fordert damit den Interrupt an.
 */
TEST_F(K2526WriteStrobe, Zve1WriteRequestsInterrupt) {
    armPortA();
    zre.cpu().writeByte(0x0800, 0xFF);
    EXPECT_TRUE(zre.hasInterrupt())
        << "Schreibzugriff loeste den /WR-Strobe-Interrupt nicht aus";
    EXPECT_EQ(bus.memRead(0x0800), 0xFF)
        << "das Testbyte muss VOR der ISR im Speicher stehen — genau das prueft sie";
}

/**
 * @test K2526WriteStrobe/DisablingClearsPendingRequest
 * @brief Das Sperren (Steuerwort 47H) verwirft die anstehende Anforderung.
 *
 * Die ISR des Lade-ROMs sperrt Port A als erstes wieder — und wird damit die
 * Anforderung los, die der Stack-Push der Interruptannahme gerade neu gesetzt hat.
 * Ohne dieses Verwerfen bliebe sie stehen und schluege beim naechsten `EI` los.
 */
TEST_F(K2526WriteStrobe, DisablingClearsPendingRequest) {
    armPortA();
    zre.cpu().writeByte(0x0800, 0xFF);
    ASSERT_TRUE(zre.hasInterrupt());
    bus.ioWrite(BSPIO_PORTA_CTRL, 0x47);   // Interruptsteuerwort, IE=0
    EXPECT_FALSE(zre.hasInterrupt())
        << "gesperrter Port A haelt die Anforderung weiter — sie schluege beim "
           "naechsten EI los (Boot-Kette entgleist)";
}
