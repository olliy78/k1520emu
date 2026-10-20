/**
 * @file paket.cpp
 * @brief Kermit-Paketschicht (siehe paket.h).
 */
#include "kermit/src/paket.h"

#include <algorithm>

namespace k1520::kermit {

// ─── Blockpruefung ─────────────────────────────────────────────────────────

int pruefsumme2(const uint8_t* p, std::size_t n) {
    unsigned s = 0;
    for (std::size_t i = 0; i < n; ++i) s += p[i];
    return static_cast<int>(s & 0x0FFF);
}

int pruefsumme1(const uint8_t* p, std::size_t n) {
    unsigned s = 0;
    for (std::size_t i = 0; i < n; ++i) s += p[i];
    return static_cast<int>(((s & 0xC0) >> 6) + s) & 0x3F;
}

uint16_t crc16Kermit(const uint8_t* p, std::size_t n) {
    uint16_t crc = 0;
    for (std::size_t i = 0; i < n; ++i) {
        crc ^= p[i];
        for (int b = 0; b < 8; ++b) crc = (crc & 1) ? static_cast<uint16_t>((crc >> 1) ^ 0x8408) : static_cast<uint16_t>(crc >> 1);
    }
    return crc;
}

std::string blockpruefung(int typ, const uint8_t* p, std::size_t n) {
    std::string s;
    switch (typ) {
    case 2: {
        const int c = pruefsumme2(p, n);
        s += tochar((c >> 6) & 0x3F);
        s += tochar(c & 0x3F);
        break;
    }
    case 3: {
        const int c = crc16Kermit(p, n);
        s += tochar((c >> 12) & 0x0F);
        s += tochar((c >> 6) & 0x3F);
        s += tochar(c & 0x3F);
        break;
    }
    default:
        s += tochar(pruefsumme1(p, n));
        break;
    }
    return s;
}

static const uint8_t* bytes(const std::string& s) { return reinterpret_cast<const uint8_t*>(s.data()); }

// ─── Rahmen ────────────────────────────────────────────────────────────────

std::string baueRahmen(const Paket& p, int chkt, const RahmenParameter& rp) {
    const int chkl = chktFuerTyp(p.typ, chkt);
    std::string kopf;   // ab LEN, Grundlage der Blockpruefung
    const int normalLen = static_cast<int>(p.daten.size()) + 2 + chkl;
    if (normalLen <= MAX_NORMAL) {
        kopf += tochar(normalLen);
        kopf += tochar(p.seq & 63);
        kopf += p.typ;
    } else {
        const int lenx = static_cast<int>(p.daten.size()) + chkl;
        kopf += tochar(0);
        kopf += tochar(p.seq & 63);
        kopf += p.typ;
        kopf += tochar(lenx / 95);
        kopf += tochar(lenx % 95);
        kopf += tochar(pruefsumme1(bytes(kopf), kopf.size()));
    }
    kopf += p.daten;
    std::string aus(static_cast<std::size_t>(std::max(0, rp.npad)), static_cast<char>(rp.padc));
    aus += static_cast<char>(rp.mark);
    aus += kopf;
    aus += blockpruefung(chkl, bytes(kopf), kopf.size());
    aus += static_cast<char>(rp.eol);
    return aus;
}

// ─── Zerleger ──────────────────────────────────────────────────────────────

PaketZerleger::Ergebnis PaketZerleger::zufuehren(uint8_t b) {
    if (b == mark_) {   // Beginn oder Neusynchronisation
        z_ = Z::Len;
        roh_.clear();
        pruef_.clear();
        return Ergebnis::Weiter;
    }
    switch (z_) {
    case Z::Suche:
        return Ergebnis::Weiter;
    case Z::Len:
        if (b < 32) { z_ = Z::Suche; return Ergebnis::Weiter; }   // Zeilenrest, kein Paket
        roh_ += static_cast<char>(b);
        len_ = unchar(static_cast<char>(b));
        z_ = Z::Seq;
        return Ergebnis::Weiter;
    case Z::Seq:
        if (b < 32) return fehler();
        roh_ += static_cast<char>(b);
        paket_.seq = unchar(static_cast<char>(b));
        if (paket_.seq < 0 || paket_.seq > 63) return fehler();
        z_ = Z::Typ;
        return Ergebnis::Weiter;
    case Z::Typ:
        if (b < 32) return fehler();
        roh_ += static_cast<char>(b);
        paket_.typ = static_cast<char>(b);
        paket_.daten.clear();
        chkl_ = chktFuerTyp(paket_.typ, chkt_);
        if (len_ == 0) { z_ = Z::Lx1; return Ergebnis::Weiter; }
        // NAK: keine Daten, die Pruefungslaenge folgt aus LEN (C-Kermit-Heuristik)
        // B traegt ebenfalls nie Daten: so liest ein Server im Leerlauf (Pruefung 1) auch das
        // wiederholte B einer Transaktion mit Pruefung 2/3, deren Schlussquittung verloren ging.
        if ((paket_.typ == 'N' || paket_.typ == 'B') && len_ - 2 >= 1 && len_ - 2 <= 3) chkl_ = len_ - 2;
        if (len_ < 2 + chkl_) return fehler();
        datenLen_ = len_ - 2 - chkl_;
        z_ = datenLen_ > 0 ? Z::Daten : Z::Pruef;
        return Ergebnis::Weiter;
    case Z::Lx1:
    case Z::Lx2:
        if (b < 32) return fehler();
        roh_ += static_cast<char>(b);
        z_ = (z_ == Z::Lx1) ? Z::Lx2 : Z::Hchk;
        return Ergebnis::Weiter;
    case Z::Hchk: {
        if (b < 32) return fehler();
        const char soll = tochar(pruefsumme1(bytes(roh_), roh_.size()));
        if (static_cast<char>(b) != soll) return fehler();
        roh_ += static_cast<char>(b);
        const int lenx = unchar(roh_[3]) * 95 + unchar(roh_[4]);
        if (lenx < chkl_) return fehler();
        datenLen_ = lenx - chkl_;
        z_ = datenLen_ > 0 ? Z::Daten : Z::Pruef;
        return Ergebnis::Weiter;
    }
    case Z::Daten:
        if (b < 32) return fehler();   // abgeschnittenes Paket (z. B. EOL)
        roh_ += static_cast<char>(b);
        paket_.daten += static_cast<char>(b);
        if (static_cast<int>(paket_.daten.size()) == datenLen_) z_ = Z::Pruef;
        return Ergebnis::Weiter;
    case Z::Pruef:
        if (b < 32) return fehler();
        pruef_ += static_cast<char>(b);
        if (static_cast<int>(pruef_.size()) < chkl_) return Ergebnis::Weiter;
        z_ = Z::Suche;
        if (pruef_ != blockpruefung(chkl_, bytes(roh_), roh_.size())) return Ergebnis::Fehlerhaft;
        paket_.chkl = chkl_;
        return Ergebnis::Paket;
    }
    return Ergebnis::Weiter;
}

// ─── Init-Parameter ────────────────────────────────────────────────────────

std::string initDaten(const InitParameter& ip, bool mitCapas) {
    std::string d;
    d += tochar(std::clamp(ip.maxl, 10, MAX_NORMAL));
    d += tochar(std::clamp(ip.time, 0, 94));
    d += tochar(std::clamp(ip.npad, 0, 94));
    d += static_cast<char>(ctl(ip.padc));
    d += tochar(ip.eol);
    d += ip.qctl;
    d += ip.qbin;
    d += static_cast<char>('0' + ip.chkt);
    d += ip.rept;
    if (mitCapas) {
        d += tochar(ip.capas & 0x3E);
        d += tochar(std::clamp(ip.windo, 0, 31));
        const int mx = std::clamp(ip.maxlx, 0, MAX_LANG);
        d += tochar(mx / 95);
        d += tochar(mx % 95);
    }
    return d;
}

InitParameter zerlegeInit(const std::string& d) {
    InitParameter ip;
    const std::size_t n = d.size();
    auto da = [&](std::size_t i) { return i < n && d[i] != ' '; };
    ip.felder = static_cast<int>(n);
    if (da(0)) {
        ip.maxl = unchar(d[0]);
        if (ip.maxl < 10) ip.maxl = 80;
        if (ip.maxl > MAX_NORMAL) ip.maxl = MAX_NORMAL;
    }
    if (da(1)) ip.time = std::max(0, unchar(d[1]));
    if (da(2)) ip.npad = std::max(0, unchar(d[2]));
    if (n > 3 && ip.npad > 0) ip.padc = ctl(static_cast<uint8_t>(d[3]));
    if (da(4)) {
        const int e = unchar(d[4]);
        ip.eol = static_cast<uint8_t>((e < 2 || e > 31) ? '\r' : e);
    }
    if (da(5)) ip.qctl = d[5];
    if (n > 6) ip.qbin = (d[6] == ' ') ? 'N' : d[6];
    if (da(7)) {
        const int c = d[7] - '0';
        ip.chkt = (c >= 1 && c <= 3) ? c : 1;
    }
    if (n > 8) ip.rept = d[8];
    if (da(9)) {
        ip.capas = unchar(d[9]);
        std::size_t j = 9;
        while ((unchar(d[j]) & 1) && j + 1 < n) ++j;   // Fortsetzungsbytes ueberspringen
        if (j + 1 < n) ip.windo = unchar(d[j + 1]);
        if (j + 3 < n) ip.maxlx = unchar(d[j + 2]) * 95 + unchar(d[j + 3]);
    }
    return ip;
}

InitParameter antwortParameter(const InitParameter& mein, const InitParameter& dein) {
    InitParameter r = mein;
    r.chkt = (dein.chkt >= 1 && dein.chkt <= 3) ? dein.chkt : 1;
    return r;
}

int Vereinbarung::datenBudget() const {
    const int normal = std::max(1, maxl - 2 - chkt);
    if (langpakete && maxlx > 0) return std::max(normal, std::min(maxlx, MAX_LANG) - chkt - 7);
    return normal;
}

Vereinbarung verhandle(const InitParameter& mein, const InitParameter& dein, int eigenesMaxPaket) {
    Vereinbarung v;
    v.chkt = (mein.chkt == dein.chkt && mein.chkt >= 1 && mein.chkt <= 3) ? mein.chkt : 1;
    v.kod.qctlMein = mein.qctl;
    v.kod.qctlDein = (dein.qctl > 32 && dein.qctl < 127) ? dein.qctl : '#';

    const char m = mein.qbin, d = dein.qbin;
    char qbin = 0;
    if (gueltigesPraefix(m) && (d == 'Y' || d == m)) qbin = m;
    else if (gueltigesPraefix(d) && (m == 'Y' || m == d)) qbin = d;
    if (qbin == v.kod.qctlMein || qbin == v.kod.qctlDein) qbin = 0;
    v.kod.qbin = qbin;

    char rept = 0;
    if (gueltigesPraefix(mein.rept) && mein.rept == dein.rept) rept = mein.rept;
    if (rept == qbin || rept == v.kod.qctlMein || rept == v.kod.qctlDein) rept = 0;
    v.kod.rept = rept;

    const int capas = mein.capas & dein.capas;
    v.langpakete = (capas & CAPAS_LANGPAKETE) != 0;
    v.attribute = (capas & CAPAS_ATTRIBUTE) != 0;
    v.maxl = std::clamp(dein.maxl, 10, MAX_NORMAL);
    if (v.langpakete) {
        const int ihr = dein.maxlx > 0 ? dein.maxlx : 500;
        v.maxlx = std::min({ihr, std::max(eigenesMaxPaket, MAX_NORMAL), MAX_LANG});
        if (v.maxlx <= MAX_NORMAL) { v.langpakete = false; v.maxlx = 0; }
    }
    v.rahmen.eol = dein.eol;
    v.rahmen.npad = dein.npad;
    v.rahmen.padc = dein.padc;
    v.time = dein.time;
    return v;
}

// ─── Kodierung ─────────────────────────────────────────────────────────────

static void kodiereZeichen(const Kodierung& k, uint8_t c, std::string& u) {
    const uint8_t a7 = c & 0x7F;
    if (k.qbin && (c & 0x80)) {
        u += k.qbin;
        c = a7;
    }
    if (a7 < 32 || a7 == 127) {
        u += k.qctlMein;
        c = ctl(c);
    } else if (a7 == static_cast<uint8_t>(k.qctlMein) || (k.qbin && a7 == static_cast<uint8_t>(k.qbin)) ||
               (k.rept && a7 == static_cast<uint8_t>(k.rept))) {
        u += k.qctlMein;
    }
    u += static_cast<char>(c);
}

std::size_t kodiereBlock(const Kodierung& k, const uint8_t* src, std::size_t n, std::size_t maxLen, std::string& aus) {
    std::size_t i = 0;
    std::string einheit;
    while (i < n) {
        std::size_t lauf = 1;
        if (k.rept) {
            while (i + lauf < n && src[i + lauf] == src[i] && lauf < 94) ++lauf;
        }
        einheit.clear();
        std::size_t verbraucht;
        if (k.rept && lauf >= 3) {
            einheit += k.rept;
            einheit += tochar(static_cast<int>(lauf));
            kodiereZeichen(k, src[i], einheit);
            verbraucht = lauf;
        } else {
            kodiereZeichen(k, src[i], einheit);
            verbraucht = 1;
        }
        if (aus.size() + einheit.size() > maxLen) break;
        aus += einheit;
        i += verbraucht;
    }
    return i;
}

std::string kodiere(const Kodierung& k, const std::string& s) {
    std::string aus;
    kodiereBlock(k, bytes(s), s.size(), static_cast<std::size_t>(-1), aus);
    return aus;
}

bool dekodiere(const Kodierung& k, const std::string& ein, std::vector<uint8_t>& aus) {
    const std::size_t n = ein.size();
    std::size_t i = 0;
    while (i < n) {
        uint8_t a = static_cast<uint8_t>(ein[i++]);
        int rpt = 1;
        if (k.rept && a == static_cast<uint8_t>(k.rept)) {
            if (i + 1 >= n) return false;
            rpt = unchar(ein[i++]);
            if (rpt <= 0) rpt = 1;   // wie C-Kermit 4.2
            a = static_cast<uint8_t>(ein[i++]);
        }
        uint8_t b8 = 0;
        if (k.qbin && a == static_cast<uint8_t>(k.qbin)) {
            if (i >= n) return false;
            b8 = 0x80;
            a = static_cast<uint8_t>(ein[i++]);
        }
        if (a == static_cast<uint8_t>(k.qctlDein)) {
            if (i >= n) return false;
            a = static_cast<uint8_t>(ein[i++]);
            const uint8_t a7 = a & 0x7F;
            if ((a7 >= 0x40 && a7 <= 0x5F) || a7 == 0x3F) a = ctl(a);
        }
        a |= b8;
        aus.insert(aus.end(), static_cast<std::size_t>(rpt), a);
    }
    return true;
}

std::string dekodiereText(const Kodierung& k, const std::string& ein) {
    std::vector<uint8_t> v;
    if (!dekodiere(k, ein, v)) return ein;
    return std::string(v.begin(), v.end());
}

// ─── Attribute ─────────────────────────────────────────────────────────────

static void attr(std::string& d, char tag, const std::string& wert) {
    d += tag;
    d += tochar(static_cast<int>(std::min<std::size_t>(wert.size(), 94)));
    d += wert.substr(0, 94);
}

std::string attributDaten(const Attribute& a) {
    std::string d;
    if (a.typ == 'A') attr(d, '"', "A");
    else if (a.typ == 'B') attr(d, '"', "B8");
    if (a.laengeK >= 0) attr(d, '!', std::to_string(a.laengeK));
    if (a.laenge >= 0) attr(d, '1', std::to_string(a.laenge));
    if (!a.datum.empty()) attr(d, '#', a.datum);
    return d;
}

static int64_t zahl(const std::string& s) {
    if (s.empty()) return -1;
    int64_t v = 0;
    for (char c : s) {
        if (c < '0' || c > '9') return -1;
        v = v * 10 + (c - '0');
        if (v > (int64_t(1) << 50)) return -1;
    }
    return v;
}

Attribute zerlegeAttribute(const std::string& d) {
    Attribute a;
    std::size_t i = 0;
    while (i + 1 < d.size()) {
        const char tag = d[i];
        const int len = unchar(d[i + 1]);
        if (len < 0 || i + 2 + static_cast<std::size_t>(len) > d.size()) break;
        const std::string w = d.substr(i + 2, static_cast<std::size_t>(len));
        switch (tag) {
        case '1': a.laenge = zahl(w); break;
        case '!': a.laengeK = zahl(w); break;
        case '#': a.datum = w; break;
        case '"':
            if (!w.empty() && (w[0] == 'A' || w[0] == 'B' || w[0] == 'I')) a.typ = (w[0] == 'A') ? 'A' : 'B';
            break;
        default: break;   // unbekannte Attribute werden uebergangen
        }
        i += 2 + static_cast<std::size_t>(len);
    }
    return a;
}

}  // namespace k1520::kermit
