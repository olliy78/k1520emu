/**
 * @file leitung_netz.h
 * @brief Netzleitungen fuer Kermit: `TelnetLeitung` und `Rfc2217Leitung`, je als
 *        Client (verbinden) UND Server (lauschen, EINE Verbindung annehmen).
 *
 * @details
 * Doc: doc/design/36_kermit.md §4.3 (AP K3).  Aufgesetzt auf die reinen Codecs des
 * Emulators (`core/serial/telnet_codec.*`, `rfc2217_codec.*`) und die Socket-Huelle
 * (`core/serial/net/*`) — NICHT auf `Wandler`/`SerialHub` (die sind an Maschinentakte
 * und die Gastseite gebunden).  Die Rolle der Leitung (verbinden/lauschen) ist
 * unabhaengig von der Rolle im Kermit-Protokoll (Client/Server).
 *
 * Eigenschaften:
 *   - blockierend mit Frist (`lesen`: Anzahl / 0 Frist oder geweckt / -1 getrennt),
 *     Abbruchschalter `wecken()` aus jedem Faden (auch waehrend `annehmen`);
 *   - Telnet ist 8-Bit-BINAER: `IAC` wird verdoppelt (Codec), die Option BINARY wird
 *     nach dem Verbinden/Annehmen ausgehandelt (kurze Wartezeit `aushandeln_ms`,
 *     danach geht es auch ohne weiter — dann gilt CR → CR NUL des Codecs und
 *     `hinweis()` sagt es).  Wie der Emulator-Hub: er ist als Server `WILL ECHO/SGA/
 *     BINARY` + `DO BINARY`, als Client wartet er; der Client hier bietet BINARY von
 *     sich aus an (nur Telnet; beim RFC-2217-Client kommt es vom Server);
 *   - RFC 2217: Baud/Format gehen als WUNSCH hinaus (Vorgabe 9600 8N1); der Gast ist
 *     massgeblich — die Antwort des Servers wird nur gemerkt (`gegenBaud()`).  Als
 *     Server antwortet die Leitung auf jede Anfrage des Clients mit ihren eigenen
 *     Werten (`NetzOptionen`), ohne etwas umzustellen;
 *   - ein Faden benutzt eine Leitung; `wecken()` darf jeder rufen.
 */
#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "core/serial/net/socket.h"
#include "kermit/src/leitung.h"

namespace serial {
class TelnetCodec;
class Rfc2217Codec;
}  // namespace serial

namespace k1520::kermit {

struct NetzOptionen {
    int baud = 9600;              ///< RFC 2217: Wunsch (Client) bzw. Antwortwert (Server)
    int datenbits = 8;            ///< 5..8
    char paritaet = 'N';          ///< 'N', 'O', 'E'
    int stoppbits = 1;            ///< 1 oder 2
    int aushandeln_ms = 1500;     ///< Wartezeit auf Telnet-BINARY nach dem Verbinden
    int schreibfrist_ms = 30000;  ///< laenger blockiertes Schreiben gilt als getrennt
    std::string signatur = "k1520kermit";   ///< RFC 2217 SIGNATURE (Server)
};

class NetzLeitung : public Leitung {
public:
    enum class Art { Telnet, Rfc2217 };

    ~NetzLeitung() override;

    // --- Leitung ----------------------------------------------------------
    int lesen(uint8_t* buf, std::size_t n, int64_t fristMs) override;
    bool schreiben(const uint8_t* buf, std::size_t n) override;
    void schliessen() override;
    void wecken() override;

    // --- Server -----------------------------------------------------------
    /// Gebundener Port eines lauschenden Servers (0 sonst).
    int port() const { return port_; }
    /// Wartet bis `fristMs` auf EINE Verbindung, nimmt sie an und schliesst den
    /// Lauscher.  false: Frist, `wecken()` oder Fehler (Text in `fehler`).
    bool annehmen(int fristMs, std::string* fehler = nullptr);

    // --- Auskunft ---------------------------------------------------------
    Art art() const { return art_; }
    bool verbunden() const { return verbunden_ && !getrennt_; }
    /// "127.0.0.1:40122" der Gegenstelle (leer ohne Verbindung).
    const std::string& gegenstelle() const { return gegenstelle_; }
    /// Laeuft Telnet-BINARY in BEIDE Richtungen?
    bool binaerAktiv() const;
    /// Hinweis nach dem Verbinden (z. B. BINARY nicht ausgehandelt), sonst leer.
    const std::string& hinweis() const { return hinweis_; }
    /// RFC 2217: Baud, die die Gegenseite auf unseren Wunsch geantwortet hat (0 = keine Antwort).
    uint32_t gegenBaud() const { return gegenBaud_; }

protected:
    NetzLeitung(Art art, const NetzOptionen& o);
    bool verbindeIntern(const std::string& host, int port, int fristMs, std::string* fehler);
    bool lauscheIntern(int port, std::string* fehler);

private:
    enum class Pump { Nichts, Daten, Geweckt, Getrennt };

    void starteCodec(bool alsClient);
    bool aushandeln();
    Pump pumpe(int64_t ms);
    void ereignisse();
    bool schreibeRoh(const std::vector<uint8_t>& v);
    bool flush();

    // Dispatch auf den passenden Codec
    void cEingabe(const uint8_t* d, std::size_t n);
    std::vector<uint8_t> cNutz();
    void cSende(const uint8_t* d, std::size_t n);
    std::vector<uint8_t> cAusgabe();

    Art art_;
    NetzOptionen opt_;
    std::unique_ptr<::serial::TelnetCodec> tel_;
    std::unique_ptr<::serial::Rfc2217Codec> r2_;
    k1520::serial::net::Socket sock_;
    k1520::serial::net::Socket lauscher_;
    k1520::serial::net::Wecker wecker_;
    int port_ = 0;
    bool verbunden_ = false;
    bool getrennt_ = false;
    std::atomic<bool> geweckt_{false};
    std::vector<uint8_t> eingang_;
    std::size_t eingangPos_ = 0;
    std::string gegenstelle_, hinweis_;
    uint32_t gegenBaud_ = 0;
};

/// Telnet (RFC 854, 8-Bit-binaer).
class TelnetLeitung : public NetzLeitung {
public:
    /// Verbindet zu host:port (blockierend, hoechstens fristMs); nullptr bei Fehler.
    static std::unique_ptr<TelnetLeitung> verbinden(const std::string& host, int port, int fristMs,
                                                    const NetzOptionen& o = {}, std::string* fehler = nullptr);
    /// Lauscht auf `port` (0 = frei waehlbar, `port()` fragt ihn ab); danach `annehmen`.
    static std::unique_ptr<TelnetLeitung> lauschen(int port, const NetzOptionen& o = {},
                                                   std::string* fehler = nullptr);

private:
    explicit TelnetLeitung(const NetzOptionen& o) : NetzLeitung(Art::Telnet, o) {}
};

/// Telnet mit COM-PORT-Option (RFC 2217).
class Rfc2217Leitung : public NetzLeitung {
public:
    static std::unique_ptr<Rfc2217Leitung> verbinden(const std::string& host, int port, int fristMs,
                                                     const NetzOptionen& o = {}, std::string* fehler = nullptr);
    static std::unique_ptr<Rfc2217Leitung> lauschen(int port, const NetzOptionen& o = {},
                                                    std::string* fehler = nullptr);

private:
    explicit Rfc2217Leitung(const NetzOptionen& o) : NetzLeitung(Art::Rfc2217, o) {}
};

}  // namespace k1520::kermit
