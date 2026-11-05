/**
 * @file nadeldruckwerk.cpp
 * @brief Nadeldruckwerk — Zusammenspiel von Wagen, Papier, Kopf und Blatt.
 * @see nadeldruckwerk.h
 */

#include "core/peripherals/druckwerk/nadeldruckwerk.h"
#include <utility>

Nadeldruckwerk::Nadeldruckwerk(const NadeldruckParameter& p, std::unique_ptr<Wagen> wagen,
                               const Papierweg::Parameter& papier)
    : p_(p), wagen_(std::move(wagen)), kopf_(p)
{
    // Bahnenzahl auf 1 oder 2 begrenzen (der Zugriff maskiert mit bahnen()-1)
    p_.bahnen = p_.bahnen >= 2 ? 2 : 1;
    for (int i = 0; i < p_.bahnen; ++i) {
        papier_.emplace_back(papier);
        blatt_.push_back(std::make_unique<Blattmodell>(p_.blatt_breite * p_.x_je_takt));
    }
}

void Nadeldruckwerk::netzEin(int wagen_pos)
{
    wagen_->netzEin(wagen_pos);
    for (auto& pw : papier_) pw.netzEin();
    for (auto& b : blatt_) b->leeren();
    kopf_.zaehlerLoeschen();
    for (int i = 0; i < bahnen(); ++i) blattStrecken(i);
}

void Nadeldruckwerk::halt()
{
    wagen_->halt();
    for (auto& pw : papier_) pw.halt();
}

void Nadeldruckwerk::blattStrecken(int bahn)
{
    // das Blatt reicht bis unter die unterste Nadel der aktuellen Papierlage
    blatt_[bahn]->streckeBis(papierY(bahn) + p_.nadel1_y + Nadelkopf::NADELN * p_.nadel_teilung_y);
}

bool Nadeldruckwerk::takt(int ticks)
{
    bool d = wagen_->takt(ticks);
    for (int i = 0; i < bahnen(); ++i) {
        const int lage = papier_[i].lage();
        d |= papier_[i].takt(ticks);
        if (papier_[i].lage() != lage) blattStrecken(i);
    }
    return d;
}

int Nadeldruckwerk::feuere(uint16_t muster, bool kopf_anliegend)
{
    return kopf_.feuere(muster, wagen_->position(), papierY(0), wagen_->richtung(), kopf_anliegend, *blatt_[0]);
}

void Nadeldruckwerk::blattLeeren()
{
    for (auto& b : blatt_) b->leeren();
    for (int i = 0; i < bahnen(); ++i) blattStrecken(i);
}
