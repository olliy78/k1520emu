#!/bin/bash
# Schutzversuch ohne MMU: zwei "wache" im Hintergrund, dann "schreib"; U8001-Abdeckung je Segment.
# Aufruf: lauf.sh ARBEITSORDNER   (dauert einige Minuten Wanduhr)
set -e
HIER=$(cd "$(dirname "$0")" && pwd); REPO=$(cd "$HIER/../../../.." && pwd)
W=$(realpath -m "${1:?Arbeitsordner}"); K=$REPO/build/k1520disktool
rm -rf "$W"; mkdir -p "$W/cp" "$W/src"
cp "$REPO"/disks/a5120_16_MUTOS/a5120_mutos8000_{usr,root}_mfm.hfe "$REPO"/disks/a5120_16_MUTOS/a5120_mutos8000_boot_udos_k5601.hfe "$W/cp/"
cp "$HIER"/wache.c "$HIER"/schreib.c "$W/src/"
for i in 1 2 3 4; do echo ENDE$i > "$W/src/fertig$i"; done
"$K" create "$W/quelle.hfe" --fs mutos8k_26x128_mfm --inodes 40 >/dev/null; "$K" put "$W/quelle.hfe" "$W"/src/* >/dev/null
"$K" create "$W/tmp.hfe" --fs mutos8k_26x128_mfm --inodes 24 >/dev/null
cat > "$W/lauf.dbg" <<EOT
g 80000000
keys BOOT\sMUTOS\r
g 100000000
eject A
mount A $W/tmp.hfe udos_ss77
mount B $W/cp/a5120_mutos8000_usr_mfm.hfe udos_ss77
mount C $W/cp/a5120_mutos8000_root_mfm.hfe udos_ss77
mount D $W/quelle.hfe udos_ss77
keys r
g 100000000
keys root\r
g 100000000
keys /etc/mount\s/dev/rk1\s/usr\r
g 30000000
keys /etc/mount\s/dev/rk3\s/mnt\r
g 30000000
keys /etc/mount\s/dev/rk0\s/tmp\r
g 30000000
keys cd\s/mnt\r
g 20000000
keys PATH=/bin:/usr/bin:.\r
g 20000000
keys cc\s-o\swache\swache.c;cat\sfertig1\r
gscreen "ENDE1" 2000000000
keys cc\s-o\sschreib\sschreib.c;cat\sfertig2\r
gscreen "ENDE2" 2000000000
cpu u8000
cover on
keys wache\sA\s&\r
g 60000000
keys wache\sB\s&\r
g 60000000
keys schreib;cat\sfertig3\r
gscreen "ENDE3" 400000000
g 200000000
gscreen /heil|ZERSTOERT/ 600000000
g 150000000
cover report $W/cov.txt
screen
q
EOT
cd "$REPO" && tools/dev.sh tool k1520dbg --rw --em em256 --laufwerke K5601,K5601,K5601,K5601 "$W/cp/a5120_mutos8000_boot_udos_k5601.hfe" -x "$W/lauf.dbg" > "$W/lauf.log" 2>&1
awk '{split($1,a,":"); k=a[1]" "$2; n[k]++; s[k]+=$3} END{for(k in n) print k, n[k]" Adressen", s[k]" Befehle"}' "$W/cov.txt" | sort > "$W/segmente.txt"
echo fertig: "$W"
