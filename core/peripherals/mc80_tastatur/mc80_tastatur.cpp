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
    wiederholung_ = true;
    halte_aktiv_ = false;
}

void Mc80Tastatur::anschlag(uint8_t praefix, uint8_t code)
{
    // Praefix vor JEDEM Anschlag [Annahme]: INIT setzt die Praefixzelle FB7CH zurueck, eine
    // Tastatur, die nur bei Wechsel sendete, waere danach stumm (Mc80Boot.Kommando…).
    queue_.push_back({praefix, true});
    queue_.push_back({code, false});
    letzter_praefix_ = praefix;
    letzter_code_ = code;
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

bool Mc80Tastatur::qtTasteIntern(uint32_t k, bool shift, bool ctrl)
{
    if (!bereit_) return false;   // 04H: Matrix gesperrt, bis 05H sie freigibt
    // 0EH (Firmware 0AC2H: r12 |= 24H): CTRL-Zustand fest EIN, Umschalt aus, Modifikatortasten
    // werden ignoriert — jede Taste kommt als 1FH + unumgeschalteter Code.  0FH loest das.
    if (umschaltung_gesperrt_) { shift = false; ctrl = true; }
    // CTRL gilt fuer JEDE Taste des grossen Feldes (Firmware: Praefix 1FH + Code der
    // unumgeschalteten Tabelle), nicht nur fuer Buchstaben.
    auto z = [&](uint8_t code) { if (ctrl) funktionstaste(code); else zeichen(code); };
    // Qt::Key_* der Sondertasten (qnamespace.h); Codes der Nicht-Zeichentasten [Forum]
    switch (k) {
        case 0x01000004: case 0x01000005: z(ENTER); return true;   // Return, Enter
        case 0x01000000: z(OFF);   return true;                    // Escape
        case 0x01000003: z(0x08);  return true;                    // Rueckschritt = <-
        case 0x01000001: z(0x09);  return true;                    // Tab
        case 0x01000012: z(0x08);  return true;                    // Kursor links
        case 0x01000013: z(0x0B);  return true;                    // Kursor hoch
        case 0x01000014: z(0x0C);  return true;                    // Kursor rechts
        case 0x01000015: z(0x0A);  return true;                    // Kursor runter
        case 0x01000007: z(0x7F);  return true;                    // Entf = CL
        case TASTE_CRLF:    z(0x0D); return true;
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
        // CTRL: 1FH + KLEINER Buchstabe (unumgeschaltete Tabelle, Umschalt ignoriert; Firmware 0A4A).
        if (ctrl) { funktionstaste(static_cast<uint8_t>(c | 0x20)); return true; }
        c = shift ? static_cast<uint8_t>(c & ~0x20) : static_cast<uint8_t>(c | 0x20);
    }
    if (c < 0x20 && c != 0x08 && c != 0x09 && c != 0x0D) return false;
    z(c == 0x0D ? ENTER : c);
    return true;
}

bool Mc80Tastatur::qtTaste(uint32_t k, bool shift, bool ctrl)
{
    if (!qtTasteIntern(k, shift, ctrl)) return false;
    // Wiederholung nur fuer Praefix 1CH/1DH (Firmware: Bit 3 nach 1EH/1FH sperrt) und nicht
    // fuer das doppelte „00“ der Bildschirmtastatur.
    halte_aktiv_ = (letzter_praefix_ == PRAEFIX_ZEICHEN || letzter_praefix_ == PRAEFIX_EBENE2)
                   && k != TASTE_ZB_00;
    halte_kode_ = k;
    bis_wiederholung_ = TAKTE_REPEAT_ANFANG;
    return true;
}

void Mc80Tastatur::loslassen(uint32_t k)
{
    if (halte_aktiv_ && halte_kode_ == k) halte_aktiv_ = false;
}

bool Mc80Tastatur::takt(int takte)
{
    bis_zeichen_ -= takte;
    bis_taste_   -= takte;
    if (halte_aktiv_ && wiederholung_ && bereit_) {
        bis_wiederholung_ -= takte;
        if (bis_wiederholung_ <= 0) {
            // Leitung noch belegt (Anschlag steht aus): im naechsten Takt erneut versuchen.
            if (queue_.empty()) {
                anschlag(letzter_praefix_, letzter_code_);
                bis_wiederholung_ = TAKTE_REPEAT_FOLGE;
            } else bis_wiederholung_ = 1;
        }
    }
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
        // Firmware (MC8030KB.bin, 0AEBH/0AE1H): 12H = Wiederholung ein, 14H = aus, beide ohne
        // Antwort.  13H ist KEIN Kommando (→ CAN).  Das Forum hielt 13H/14H fuer wirkungslos.
        case 0x12: wiederholung_ = true;  break;
        case 0x14: wiederholung_ = false; break;
        default:
            antwort(CAN);              // jedes andere 7-Bit-Wort: CAN, sonst ohne Funktion
            LOG_DEBUG("MC80-TAST", "Zeichen %02XH vom Rechner: Antwort CAN", b);
            break;
    }
}
