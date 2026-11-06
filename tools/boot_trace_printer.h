/**
 * @file boot_trace_printer.h
 * @brief `boot_trace --machine sd1157|k6316|k6316-v24` (Entwurf 49, WP1/WP2a).
 *
 * Eine CPU (U880 der Steuerung), kein Bild.  Optionen wie beim MC80 (@ref K8915TraceOpts):
 * `-c`, `--stall`, `--until`, `--events`, `--csv`, `--itrace`, `-w/-W`, `--watch`, `--watchio`,
 * `-l <listing>`, `--coverage`, `--quiet --json`.  Nicht vorhanden: Tasten, Disketten, Kassette.
 *
 * @license MIT
 */
#pragma once
#include "tools/boot_trace_k8915.h"

/** @brief Faehrt den SD 1157 und berichtet.  Rueckgabe = Exit-Code: 0 = Stillstand (Firmware wartet
 *  ohne ein neues Paar (Tor, Wert) bei einem Schreibzugriff, d. h. in der Leerlaufschleife) bzw. `--until` erfuellt, 1 = Taktgrenze ohne Stillstand,
 *  2 = `--until` nicht erfuellt. */
int bootTraceSd1157(const K8915TraceOpts& o, const prnlst::Listing& prn, int k6316 = 0);
