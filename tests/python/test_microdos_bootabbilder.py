"""Die hinterlegten MicroDOS-Bootabbilder (data/bootsektoren/boot_microdos_*.bin) booten im Emulator.

Waechter fuer doc/merkposten/microdos.md (Abschnitt "Bootabbilder des DiskTools"): eine mit
`create --fs microdos800|microdos200|microdos1715 --boot <abbild>` gebaute Diskette -- danach mit `put`
befuellt -- bootet bis `A>`, und zwar mit GENAU der Bestueckung, auf die das Abbild generiert ist:

  boot_microdos_1526.bin            800K  Tastatur K7637 (seriell), 4 x K5601
  boot_microdos_1526_k7636.bin      800K  Tastatur K7636 (parallel), 4 x K5601
  boot_microdos_1526_200k.bin       200K  K7637, 2 x K5600.10 (40 Spuren einseitig)
  boot_microdos_1526_200k_k7636.bin 200K  K7636, 2 x K5600.10
  boot_microdos_1715.bin            PC 1715, K5601, Bildschirm 2K (K7222)

Das 200K-System bootet NICHT im Vorgabe-K5601 und NICHT im K5600.20 (bleibt bei den Ladepunkten
stehen, gemessen 2026-10-31): in den Systemspuren steht die Laufwerkstabelle der Generierung.
Der Boot dauert rund 110 M Takte; INITIAL.SUB fehlt (kein MKEY-Hinweis), der Prompt kommt von selbst.
"""
import sys

import pytest

from conftest import PROJECT_ROOT, requires_core, requires_disk

pytestmark = [requires_core, requires_disk]

sys.path.insert(0, str(PROJECT_ROOT / "tools" / "microdos"))
import ablauf  # noqa: E402
import ablauf1715  # noqa: E402

BOOT = PROJECT_ROOT / "data" / "bootsektoren"
K5600 = ["K5600.10"] * 2 + [None, None]

#: (Abbild, Profil, Tastatur, Laufwerke)
FAELLE = [
    ("boot_microdos_1526.bin", "microdos800", "k7637", None),
    ("boot_microdos_1526_k7636.bin", "microdos800", "k7636", None),
    ("boot_microdos_1526_200k.bin", "microdos200", "k7637", K5600),
    ("boot_microdos_1526_200k_k7636.bin", "microdos200", "k7636", K5600),
]


def _baue(tmp_path, profil, abbild):
    from app.core_binding.k1520disk import DiskTool
    ziel = tmp_path / "boot.hfe"
    with DiskTool.create(ziel, profil, "", str(BOOT / abbild)) as d:
        # eine per `put` eingefuegte Datei darf das Booten nicht stoeren
        quelle = tmp_path / "q"
        quelle.mkdir()
        (quelle / "HALLO.COM").write_bytes(bytes(range(256)) * 4)
        d.insert_all(quelle)
        d.flush()
    return ziel


@pytest.mark.parametrize("abbild,profil,tastatur,lw", FAELLE, ids=[f[0] for f in FAELLE])
def test_1526_bootabbild_bootet_mit_seiner_bestueckung(tmp_path, abbild, profil, tastatur, lw):
    ziel = _baue(tmp_path, profil, abbild)
    s = ablauf.Sitzung(tastatur, str(ziel), laufwerke=lw)
    s.kaltstart_bis_prompt()
    assert "48K MicroDOS Vers 2.6" in s.bild()
    verz = s.kommando("D")           # geht nur, wenn die Tastatur zum System passt
    assert "MicroDOS.SYS" in verz and "HALLO" in verz, verz


def test_1715_bootabbild_bootet_am_pc1715(tmp_path):
    from app.core_binding.k1520disk import DiskTool
    ziel = tmp_path / "boot1715.hfe"
    with DiskTool.create(ziel, "microdos1715", "", str(BOOT / "boot_microdos_1715.bin")) as d:
        quelle = tmp_path / "q"
        quelle.mkdir()
        (quelle / "HALLO.COM").write_bytes(bytes(range(256)) * 4)
        d.insert_all(quelle)
        d.flush()
    s = ablauf1715.Sitzung(str(ziel))
    s.kaltstart_bis_prompt()
    assert "48K MicroDOS Vers 2.6" in s.bild()
    verz = s.kommando("D")
    assert "MICRODOS.SYS" in verz and "HALLO" in verz, verz
