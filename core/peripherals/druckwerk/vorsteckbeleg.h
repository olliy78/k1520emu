/**
 * @file vorsteckbeleg.h
 * @brief Der Beleg im Vorsteckschacht des K6316 als eigenes Blatt (hardware.md §11.4).
 *
 * Reine Geometrie, ohne Bezug auf die Steuerung: der Beleg hat Laenge @c L (in Vorsteckschritten zu 1/108 Zoll) und
 * Breite @c B (Pixel) und eine Lage **u** = Weg der Belegspitze (Einzugsrichtung vorn) hinter der oberen Lichtschranke
 * FSO, in Schritten.  Wer den Vorsteckmotor treibt, ruft @ref bewege (+ = Einzug = in den Drucker).
 *
 * * FSO ist belegt, solange die Spitze hinter FSO steht und das Ende noch nicht vorbei ist, FSU ebenso um den Abstand
 *   @ref Parameter::abstand_fso_fsu spaeter.
 * * Die Bezugslinie der Druckzeile liegt @ref Parameter::druckzeile Schritte hinter FSO (Firmware-Messung, hardware.md §11.8).  Liegt der Beleg dort, treffen die Nadeln ihn
 *   (@ref unterDruckzeile); die Beleg-Blattzeile der Nadel 1 ist @ref druckzeileY, x relativ zur Linkskante
 *   (@ref Parameter::x_links, in Pixeln des Hauptblatts).
 * * Eigenes @ref Blattmodell (@ref blatt), 216 Zeilen je Zoll (2 je Schritt), 300 Pixel je Zoll in x; Zeile 0 = Oberkante
 *   (Ende, das zuletzt in den Drucker geht).  Das Blatt bleibt nach @ref entnehmen stehen, bis ein neuer Beleg kommt.
 *
 * Alles mit [?] ist Annahme (hardware.md §11.3/§11.4), nicht gemessen.
 */

#pragma once
#include "core/peripherals/druckwerk/blattmodell.h"
#include <memory>

class Vorsteckbeleg {
public:
    struct Parameter {
        int abstand_fso_fsu = 36;   ///< D: FSU liegt D Schritte hinter FSO [H] (Mindestlaenge 72 mm = 306 = D + 270, hardware.md §11.8)
        int druckzeile = 175;       ///< P: Bezugslinie (Unterkante der ersten Zeile = Nadel 8) liegt P Schritte hinter FSO [E/H] (hardware.md §11.8)
        int nadel1_ueber_bezug = 21; ///< Zeilen von Nadel 1 bis zur Bezugslinie: 7 Nadelteilungen zu 3 Zeilen [H]
        int x_links = 395;          ///< Linkskante des Belegs in Pixeln des Hauptblatts: 30 Pixel (0,1 Zoll) vor dem ersten Textpunkt (425) [?] (Handbuch 7.1: Spiel zum Anschlag)
        int einlege_lage = 10;      ///< u beim Einlegen: Spitze hinter FSO, vor FSU [?]
        int u_max = 1500;           ///< Einzug klemmt hier (Motor laeuft leer) [?]
        int u_hinaus = -200;        ///< Spitze weiter oben als das: der Beleg ist ganz hinaus (Auswurf zu Ende) [?]
        int zeilen_je_schritt = 2;  ///< Blattzeilen je Vorsteckschritt: 216 Zeilen je Zoll bei 108 Schritten je Zoll
    };

    Vorsteckbeleg() : Vorsteckbeleg(Parameter{}) {}
    explicit Vorsteckbeleg(const Parameter& p) : p_(p) {}

    /// Neuen Beleg einlegen: @p laenge Schritte, @p breite_px Blattpixel; false = es liegt schon einer im Schacht.
    bool einlegen(int laenge, int breite_px);
    /// Beleg aus dem Schacht nehmen (FSO/FSU frei); das Blatt bleibt zum Ansehen/Exportieren.
    void entnehmen() { da_ = false; }
    bool eingelegt() const { return da_; }

    /// Der Vorsteckmotor hat @p schritte bewegt: + = Einzug (in den Drucker), - = Auswurf.  Ohne Beleg wirkungslos.
    void bewege(int schritte);

    bool fso() const { return da_ && u_ >= 0 && u_ - laenge_ <= 0; }
    bool fsu() const { return da_ && u_ >= p_.abstand_fso_fsu && u_ - laenge_ <= p_.abstand_fso_fsu; }
    int  u() const { return u_; }
    int  laenge() const { return laenge_; }
    int  breitePixel() const { return breite_px_; }
    int  xLinks() const { return p_.x_links; }
    const Parameter& parameter() const { return p_; }

    /// Die Druckzeile liegt auf dem Beleg: Spitze hinter ihr, Ende noch nicht vorbei.
    bool unterDruckzeile() const { return da_ && u_ >= p_.druckzeile && u_ - laenge_ <= p_.druckzeile; }
    /// Beleg-Blattzeile der Druckzeile der Nadel 1 = Abstand vom Ende bis zur Druckzeile; nur sinnvoll bei @ref unterDruckzeile.
    int  druckzeileY() const { return (p_.druckzeile + laenge_ - u_) * p_.zeilen_je_schritt - p_.nadel1_ueber_bezug; }
    /// Liegt Wagenpixel @p x_pixel (Hauptblatt) ueber dem Beleg?
    bool trifftX(int x_pixel) const { return x_pixel >= p_.x_links && x_pixel < p_.x_links + breite_px_; }

    bool hatBlatt() const { return static_cast<bool>(blatt_); }
    Blattmodell&       blatt()       { return *blatt_; }
    const Blattmodell& blatt() const { return *blatt_; }
    /// Versionszaehler, der auch ueber ein neues Einlegen hinweg nur waechst (das Blatt wird dabei neu angelegt).
    uint64_t version() const { return basis_ + (blatt_ ? blatt_->version() : 0); }

private:
    Parameter p_;
    bool da_ = false;
    int  u_ = 0, laenge_ = 0, breite_px_ = 0;
    uint64_t basis_ = 0;
    std::unique_ptr<Blattmodell> blatt_;
};
