/**
 * @file mc80_tastatur.cpp
 * @brief Tastatur des MC80.3x auf Zeichenebene.
 * @see mc80_tastatur.h, doc/mc80/egos.md §3
 */

#include "core/peripherals/mc80_tastatur/mc80_tastatur.h"
#include "core/logger.h"

void Mc80Tastatur::reset()
{
    queue_.clear();
    bis_zeichen_ = 0;
    bis_taste_   = 0;
    bereit_ = true;
    dauerton_ = false;
    umschaltung_gesperrt_ = false;
}

void Mc80Tastatur::anschlag(uint8_t praefix, uint8_t code)
{
    // Praefix vor JEDEM Anschlag [Annahme]: INIT setzt die Praefixzelle FB7CH zurueck, eine
    // Tastatur, die nur bei Wechsel sendete, waere danach stumm (Mc80Boot.Kommando…).
    queue_.push_back({praefix, true});
    queue_.push_back({code, false});
}

void Mc80Tastatur::zeichen(uint8_t code) { anschlag(PRAEFIX_ZEICHEN, code & 0x7F); }

void Mc80Tastatur::sondertaste(int n)
{
    if (n < 0 || n > 7) return;
    anschlag(PRAEFIX_SONDER, n == 7 ? 0x00 : static_cast<uint8_t>(0x40 >> n));
}

void Mc80Tastatur::ziffernblock(uint8_t code) { anschlag(PRAEFIX_EBENE2, code & 0x7F); }

void Mc80Tastatur::funktionstaste(uint8_t code) { anschlag(PRAEFIX_FUNKTION, code & 0x7F); }

void Mc80Tastatur::tippe(const char* text)
{
    for (const char* p = text; p && *p; ++p) {
        if (*p == '\n' || *p == '\r') zeichen(ENTER);
        else zeichen(static_cast<uint8_t>(*p));
    }
}

bool Mc80Tastatur::qtTaste(uint32_t k, bool shift, bool ctrl)
{
    if (!bereit_) return false;   // 04H: Matrix gesperrt, bis 05H sie freigibt
    if (umschaltung_gesperrt_) { shift = false; ctrl = false; }   // 0EH
    // Qt::Key_* der Sondertasten (qnamespace.h); Codes der Nicht-Zeichentasten [Forum]
    switch (k) {
        case 0x01000004: case 0x01000005: zeichen(ENTER); return true;   // Return, Enter
        case 0x01000000: zeichen(OFF);   return true;                    // Escape
        case 0x01000003: zeichen(0x08);  return true;                    // Rueckschritt = <-
        case 0x01000001: zeichen(0x09);  return true;                    // Tab
        case 0x01000012: zeichen(0x08);  return true;                    // Kursor links
        case 0x01000013: zeichen(0x0B);  return true;                    // Kursor hoch
        case 0x01000014: zeichen(0x0C);  return true;                    // Kursor rechts
        case 0x01000015: zeichen(0x0A);  return true;                    // Kursor runter
        case 0x01000007: zeichen(0x7F);  return true;                    // Entf = CL
        case TASTE_CRLF:    zeichen(0x0D); return true;
        case TASTE_ZB_CL:   ziffernblock(0x7F); return true;
        case TASTE_ZB_LEER: ziffernblock(0x20); return true;
        case TASTE_ZB_00:   // 1DH, 30H zweimal [Forum]; zweites Paar im Zeichenabstand
            ziffernblock('0');
            queue_.push_back({PRAEFIX_EBENE2, false});
            queue_.push_back({'0', false});
            return true;
        default: break;
    }
    if (k >= TASTE_ZB_ZEICHEN && k < TASTE_ZB_ZEICHEN + 0x80) {
        ziffernblock(static_cast<uint8_t>(k - TASTE_ZB_ZEICHEN));
        return true;
    }
    if (k >= 0x01000030 && k <= 0x01000037) { sondertaste(static_cast<int>(k - 0x01000030)); return true; }
    if (k >= 0x80) return false;   // sonstige Wirtstasten: keine Entsprechung
    uint8_t c = static_cast<uint8_t>(k);
    const bool buchstabe = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
    if (buchstabe) {
        // CTRL: Praefix 1FH, EGOS setzt Bit 7.  Das gesendete Zeichen (Kappe) ist [?].
        if (ctrl) { funktionstaste(static_cast<uint8_t>(c & ~0x20)); return true; }
        c = shift ? static_cast<uint8_t>(c & ~0x20) : static_cast<uint8_t>(c | 0x20);
    }
    if (c < 0x20 && c != 0x08 && c != 0x09 && c != 0x0D) return false;
    zeichen(c == 0x0D ? ENTER : c);
    return true;
}

bool Mc80Tastatur::takt(int takte)
{
    bis_zeichen_ -= takte;
    bis_taste_   -= takte;
    if (queue_.empty() || bis_zeichen_ > 0) return false;
    const Eintrag e = queue_.front();
    if (e.tastenanfang && bis_taste_ > 0) return false;
    if (empfaengerFrei && !empfaengerFrei()) return false;
    queue_.pop_front();
    if (senden) senden(e.byte);
    bis_zeichen_ = TAKTE_JE_ZEICHEN;
    if (queue_.empty() || queue_.front().tastenanfang)   // Anschlag vollstaendig
        bis_taste_ = TAKTE_JE_TASTE;
    return true;
}

void Mc80Tastatur::vomRechner(uint8_t b)
{
    // Steuerworte der Tastatur-Firmware [Forum, Beitrag 037, am Geraet getestet]
    switch (b & 0x7F) {
        case 0x07: ++summer_; dauerton_ = false; break;   // Piep, schaltet Dauerton aus
        case 0x10: dauerton_ = true; break;
        case 0x0E: umschaltung_gesperrt_ = true; break;
        case 0x0F: umschaltung_gesperrt_ = false; break;
        case 0x04: bereit_ = false; antwort(NAK); break;
        case 0x05: bereit_ = true;  antwort(ACK); break;
        case 0x13: case 0x14: break;   // Autorepeat aus/ein: wirkungslos (Firmwarefehler)
        default:
            antwort(CAN);              // jedes andere 7-Bit-Wort: CAN, sonst ohne Funktion
            LOG_DEBUG("MC80-TAST", "Zeichen %02XH vom Rechner: Antwort CAN", b);
            break;
    }
}
