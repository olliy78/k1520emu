# K1520emu

K1520emu is a collection of hardware-accurate emulators and tools for computers
based on the East German K1520 bus system, as well as the EAW P8000. It emulates
the hardware: boot ROMs, BIOSes, and operating systems run as real machine code
instead of being replaced by operating-system simulations.

## Emulated computers

| Computer | Operating systems |
|---|---|
| robotron A5120 (also as A5130/K8924, A5120.16 with EM064/EM256/EM1024, early version) | CP/A, SCPX, UDOS, BCU 880, MUTOS 8000 |
| robotron K8915 (V3 and V2) | SCPX 8915 |
| PRG 710 / PRG 710-1 | UDOS, SCPX |
| robotron PC 1715 / PC 1715W | SCP 1715, CP/A, CP/Z, UDOS 1715, SCP 3.0 |
| MC80.30 / MC80.21 / MC80.22 | EGOS, BSYS |
| EAW P8000 (incl. Compact) | UDOS, OS/M, WEGA (UNIX) |

The project also includes DiskTool for reading and writing disk images, the
`k1520dbg` debugger, and Kermit file transfer for WEGA. Ready-to-use packages for
Windows and Linux are available on the
[releases page](https://github.com/olliy78/k1520emu/releases).

## Documentation

- [Emulator architecture](Architecture)
- [Emulator handbook](Emulator-Handbook)
- [DiskTool handbook](DiskTool-Handbook)
- [K1520 Kermit handbook](Kermit-Handbook)
- [Debugger handbook](Debugger-Handbook)

See the [project README](https://github.com/olliy78/k1520emu/blob/main/README.md)
for more information. The program code is licensed under the MIT License.
Bundled disk images, EPROM dumps, and digitized documents remain subject to the
rights of their respective owners.

[[Home|Deutsch]]
