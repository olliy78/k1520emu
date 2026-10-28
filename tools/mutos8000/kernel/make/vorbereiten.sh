#!/bin/bash
# Bereitet die Plattenabbilder fuer bau_make.dbg / test_make.dbg vor und
# erzeugt daraus lauffaehige Skripte.  Aufruf: vorbereiten.sh ARBEITSORDNER
# (Kopien der Platten, weil k1520dbg --rw schreibt; die Originale in disks/ bleiben unberuehrt).
set -e
HIER=$(cd "$(dirname "$0")" && pwd)
REPO=$(cd "$HIER/../../../.." && pwd)
W=$(realpath -m "${1:?Arbeitsordner angeben}")
K=$REPO/build/k1520disktool
mkdir -p "$W/cp" "$W/src"
cp "$REPO"/disks/a5120_16_MUTOS/a5120_mutos8000_{usr,root}_mfm.hfe \
   "$REPO"/disks/a5120_16_MUTOS/a5120_mutos8000_boot_udos_k5601.hfe "$W/cp/"
cp "$HIER/make.c" "$HIER"/test/{Makefile,t1.c,t2.c} "$W/src/"
for i in $(seq 1 14); do echo ENDE$i > "$W/src/fertig$i"; done
rm -f "$W/quelle.hfe" "$W/tmp.hfe"
"$K" create "$W/quelle.hfe" --fs mutos8k_26x128_mfm --inodes 40 >/dev/null
"$K" put "$W/quelle.hfe" "$W"/src/* >/dev/null
"$K" create "$W/tmp.hfe" --fs mutos8k_26x128_mfm --inodes 24 >/dev/null
# alte Zeitstempel: die vom Gast erzeugten .o sind dann sicher neuer
for f in make.c Makefile t1.c t2.c; do "$K" attr "$W/quelle.hfe" $f --mtime 543018037 >/dev/null; done
for s in bau_make test_make; do
  sed -e "s#DISKS#$W/cp#; s#QUELLE#$W/quelle.hfe#; s#TMPDISK#$W/tmp.hfe#" "$HIER/$s.dbg" > "$W/$s.dbg"
done
echo "fertig. Lauf:"
echo "  cd $REPO && tools/dev.sh tool k1520dbg --rw --em em256 --laufwerke K5601,K5601,K5601,K5601 $W/cp/a5120_mutos8000_boot_udos_k5601.hfe -x $W/test_make.dbg > $W/test.log"
echo "  danach: $K get $W/quelle.hfe make --to $W/out --fileinfo"
