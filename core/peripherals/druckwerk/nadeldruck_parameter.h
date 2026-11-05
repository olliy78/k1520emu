/**
 * @file nadeldruck_parameter.h
 * @brief Parametersatz des Nadeldruckwerks (Entwurf 49 §3 Nr. 3): ein Satz je Geraet.
 *
 * `[?]` = Annahme, nicht belegt (doc/printer/sd1157/hardware.md §14).  Die Rasterwerte bestimmen nur, wie
 * das Blatt in Punkten abgebildet wird; die Firmware sieht sie nie.
 *
 * **Raster des Blatts:** x in `x_je_takt` Pixeln je Takt der Taktscheibe (1/200" bei 2), y in 1/y_je_zoll Zoll.
 * 144 ist das kleinste Raster, in dem der Papierschritt (1/48" = 3), die Nadelteilung (1/72" = 2) und die
 * Zeilenabstaende 6/8 Schritt (18/24) ganzzahlig sind.  x braucht zwei Pixel je Takt, weil der Kopf eine
 * **Flugzeit** hat (siehe versatz_vor/versatz_rueck): vorwaerts und rueckwaerts landet ein Punkt einen halben
 * Takt nach bzw. vor der Position, in der er ausgeloest wurde.
 */

#pragma once

struct NadeldruckParameter {
    // ─── Raster ──────────────────────────────────────────────────────────────
    int takte_je_zoll = 100;   ///< [?] hardware.md §10.1/§14 Nr. 2: 10 Zeichen/Zoll bei 10 Takten je Zelle
    int x_je_takt = 2;         ///< Blattpixel je Takt (Raster x = 1/200") — die Flugzeit ist 0,5 Takt, s. u.
    int y_je_zoll = 144;       ///< Blattraster 1/144" [?]
    int pv_schritt_y = 3;      ///< Papierschritt 1/48" = 3 Einheiten [?] hardware.md §10.2
    // ─── Nadeln ──────────────────────────────────────────────────────────────
    int nadel_teilung_y = 2;   ///< [?] Nadelabstand 1/72" = 2 Einheiten (9 Nadeln in 1/8", hardware.md §10.5)
    int nadel1_y = 0;          ///< [?] Lage der Nadel 1 (obere Zeile) zur Papierlage, Einheiten
    int punkt_breite = 2;      ///< [?] Punktmass in Pixeln (2 = ein Takt = 1/100")
    int punkt_hoehe = 1;       ///< [?] Punktmass in Einheiten
    /// Flugzeit der Nadel: der Punkt landet `versatz_vor` Pixel rechts (Vorwaertsdruck) bzw. `versatz_rueck` links
    /// (Rueckwaertsdruck, negativ) von der Wagenposition der Ausloesung.  **[F] an der Firmware gemessen:** deren
    /// Rueckwaertsdruck loest einen Takt spaeter aus als ein symmetrischer; beide Richtungen landen nur dann auf
    /// denselben Punkten, wenn die Flugzeit 0,5 Takt (~0,28 ms) betraegt, also 1 Pixel bei 2 Pixeln je Takt.
    int versatz_vor = 1;
    int versatz_rueck = -1;
    // ─── Blatt ───────────────────────────────────────────────────────────────
    int bahnen = 1;            ///< 1 oder 2 Papierbahnen (SD 1157/267: 2)
    int blatt_breite = 1500;   ///< Blattbreite in TAKTEN (Papier ist breiter als der Druckbereich) [?]; das Blatt hat x_je_takt Pixel je Takt
    int rand_oben_y = 36;      ///< Blattrand ueber der Papierlage 0 (Einheiten); fuer Rueckzuege ueber 0 hinaus [?]
};
