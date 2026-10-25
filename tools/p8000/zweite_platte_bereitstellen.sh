#!/usr/bin/env bash
# Erzeugt die beiden Auslieferungsabbilder fuer die zweite WEGA-3.1-Festplatte (Merkposten p8000 Nr. 65):
#   disks/p8000_wega31/p8000_wega31_platte.avr.img.gz   (fuer zwei Platten vorbereitet)
#   disks/p8000_wega31/p8000_wega31_usr2.avr.img.gz     (zweite Platte: sa.format + mkfs + labelit /usr2)
# am Emulator (Test P8000WegaZweitePlatte.Bereitstellen, langer Weg des Handbuchs + bedingte Einbindung).
#
# Ausgang ist die UNVERAENDERTE WEGA-3.1-Platte (Stand vor Nr. 65, Git-Stand c8a7a744).  Aufruf:
#   tools/p8000/zweite_platte_bereitstellen.sh [ausgangsplatte.img.gz] [ausgabeordner]
# Ohne Argumente: Ausgang aus Git (c8a7a744), Ausgabe nach disks/p8000_wega31.
# Dauer: ~30 Minuten Rechenzeit beim ersten Lauf (Formatieren, sysgen; die eingefrorenen Zustaende
# `zp31_*` in ~/.cache/k1520emu/p8000_wega beschleunigen Folgelaeufe, wenn Groesse UND Zeit der Ausgangsplatte
# gleich bleiben — `cp -p` benutzen), danach ~5 Minuten.  Im HINTERGRUND starten.
set -euo pipefail
cd "$(dirname "$0")/../.."
AUSGANG="${1:-}"
AUS="${2:-disks/p8000_wega31}"
if [ -z "$AUSGANG" ]; then
    AUSGANG="$(mktemp -d)/p8000_wega31_platte.avr.img.gz"
    git show c8a7a744:disks/p8000_wega31/p8000_wega31_platte.avr.img.gz > "$AUSGANG"
fi
K1520_ZP_AUSGANG="$(realpath "$AUSGANG")" K1520_ZP_AUS="$(realpath -m "$AUS")" \
    tools/dev.sh tool k1520_test_p8000_wega_zweite_platte --gtest_filter='*Bereitstellen'
