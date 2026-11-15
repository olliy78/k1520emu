# Gastsoftware der SOUND-Karte K1520 (Fixtures fuer `SoundSystem.*`)

Bytegleiche Kopien aus den Archiven in `tools/sound/fremd/original/` (Weitergabe freigegeben, Entwurf 51 §2a;
Herkunftstabelle dort).  Entpackt abgelegt, damit die Tests ohne ZIP-Schritt auskommen; ein Test spielt sie
mit dem DiskTool auf eine `TempDisk`-Kopie einer Systemdiskette (nie auf die Fixture selbst).

| Datei | Herkunft | benutzt von |
|-------|----------|-------------|
| `TGCPM.COM` | `K1520_SOUND_BC.zip` `TGCPM/` (A5120/K8924, Karte 38H) | `SoundSystem.A5120_Cpa_TGCPM_*` |
| `SKTEST4.COM` | `rowikla_SKTEST4_BC/SKTEST4.COM` (neuerer Bau als der im ZIP) | `SoundSystem.A5120_*_SKTEST4_*` |
| `STCTEST1.COM` | `K1520_SOUND_BC.zip` `STCTEST1/` | `SoundSystem.A5120_Cpa_STCTEST1_*` |
| `PLAYSTC.COM`, `SONG.STC` | `K1520_SOUND_BC.zip` `PLAYSTC/` | `SoundSystem.A5120_Cpa_PLAYSTC_*` |
| `pc1715/SKTEST4.COM`, `pc1715/STCTEST1.COM` | `K1520_SOUND_PC1715_CPA.zip` (PC 1715 CP/A, Karte 38H wie bei A5120, Vektor F7F0H) | `SoundSystem.Pc1715_*` |
