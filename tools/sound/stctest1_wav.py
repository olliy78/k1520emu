#!/usr/bin/env python3
"""Erzeugt `out/sound/stctest1_emu.wav`: A5120 + CP/A, SOUND-Karte 38H, `STCTEST1` laeuft komplett durch.

Zum Hoeren/Vergleichen mit `STCTEST1.mp3` (Aufnahme der echten Karte, im ZIP
`tools/sound/fremd/original/K1520_SOUND_BC.zip` unter `STCTEST1/Quelle STCTEST1/`).

Ablauf: Kopie der CP/A-Systemdiskette (`tests/fixtures/disks/cpa_cpa780_k5601_noclock.hfe`) in einem
Temp-Ordner, `STCTEST1.COM` (`tools/sound/fremd/a5120/`) mit dem DiskTool daraufschreiben, booten, WAV-Mitschrift
starten (Maschinenzeit, 48 kHz Stereo, kein Wirtsgeraet), `STCTEST1` + Return tippen und laufen, bis das Lied
(2 Wiederholungen) zu Ende ist (alle AY-Lautstaerkeregister R8-R10 zwei Sekunden lang 0).

Aufruf (nach `tools/dev.sh build`):  python3 tools/sound/stctest1_wav.py [ausgabe.wav]
"""
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

WURZEL = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(WURZEL))

TASTE_HALTEN = 1_000_000      # Takte, wie tests/support/keyboard.h (die K7637 braucht ihre Zeit)
TASTE_LOESEN = 300_000
RETURN = 0x01000004


def main() -> int:
    ziel = Path(sys.argv[1]) if len(sys.argv) > 1 else WURZEL / "out" / "sound" / "stctest1_emu.wav"
    ziel.parent.mkdir(parents=True, exist_ok=True)

    from app.core_binding.k1520 import K1520Emulator

    with tempfile.TemporaryDirectory(prefix="k1520_stctest1_") as tmp:
        platte = Path(tmp) / "cpa.hfe"
        shutil.copyfile(WURZEL / "tests/fixtures/disks/cpa_cpa780_k5601_noclock.hfe", platte)
        r = subprocess.run([str(WURZEL / "tools/dev.sh"), "tool", "k1520disktool", "put", str(platte),
                            str(WURZEL / "tools/sound/fremd/a5120/STCTEST1.COM"), "--no-backup"],
                           capture_output=True, text=True)
        if r.returncode != 0:
            print("DiskTool put:", r.stdout, r.stderr, file=sys.stderr)
            return 1

        emu = K1520Emulator(machine="a5120", sound={"basis": 0x38})
        if not emu.mount_disk(0, str(platte), "cpa780", False):
            print("mount:", emu.last_error(), file=sys.stderr)
            return 1
        emu.power_on()

        def bis(text, grenze):
            t = 0
            while t < grenze:
                t += emu.run(100_000)
                if text in emu.screen_text():
                    return True
            return False

        def laufe(takte):
            for _ in range(takte // 5000):
                emu.run(5000)

        def taste(code):
            emu.key_press(code, False, False)
            laufe(TASTE_HALTEN)
            emu.key_release(code)
            laufe(TASTE_LOESEN)

        if not bis("A>", 90_000_000):
            print("kein Prompt:\n" + emu.screen_text(), file=sys.stderr)
            return 1
        laufe(2_000_000)
        if not emu.audio_wav_start(str(ziel)):
            print("WAV:", emu.audio_fehler(), file=sys.stderr)
            return 1
        for c in "STCTEST1":
            taste(ord(c))
        taste(RETURN)

        takt = emu.nenntakt_hz()
        still = 0.0
        gesamt = 0.0
        while gesamt < 240.0:                       # Sicherheitsgrenze
            laufe(takt)
            gesamt += 1.0
            regs = emu.sound_regs() or [0] * 16
            still = still + 1.0 if (regs[8] | regs[9] | regs[10]) & 0x1F == 0 else 0.0
            if gesamt > 5 and still >= 2.0:
                break
        emu.audio_wav_stop()
        print(f"{ziel}: {gesamt:.0f} s Maschinenzeit")
    return 0


if __name__ == "__main__":
    sys.exit(main())
