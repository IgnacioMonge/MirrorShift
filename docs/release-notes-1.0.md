# Mirror Shift 1.0 release notes

[Español](release-notes-1.0.es.md)

**Two sides. One reality.**

Mirror Shift brings online Reversi to ZX Spectrum Classic, Spectrum Next, the
SpectraNext cartridge, Windows, macOS, and Linux. Every edition shares the same
rules and game protocol, so retro and desktop clients can play each other over
Direct TCP or MQTT.

## First release

- Standard Reversi with automatic captures, forced Silence, legal-move hints,
  SELF/ECHO scoring, and PARADOX ties.
- Direct TCP and MQTT games between any supported Spectrum or desktop client.
- Echoes chat, game and move clocks, takeback, resignation, and rematches.
- Local saved games with synchronized, peer-approved restoration.
- Three Spectrum disc sets, five Classic/SpectraNext palettes, full-colour Next
  themes, board rotation, capture animations, and side-to-move shine.
- Native packages for Classic, Next, SpectraNext, and Qt desktop systems.

## Release files

| Platform | Files |
| --- | --- |
| ZX Spectrum Classic | `MIRSHIFT.tap`, `MIRSHIFT.OVL`, `MIRSHIFT.DAT` |
| ZX Spectrum Next | `MIRSHIFT.nex` |
| SpectraNext cartridge | Complete resource directory containing `boot.zx`, `MIRSHIFT.INS`, `MIRSHIFT.PKG`, and `MIRSHIFT.SCR` |
| Windows | Portable x86_64 ZIP |
| macOS | Application bundle |
| Linux | x86_64 AppImage |
| Source | Source archive matching the release commit |

Use all files from the same release. Classic companion files are not compatible
substitutes for the SpectraNext edition.

## Requirements and compatibility

- Classic requires a 48K ZX Spectrum, writable divMMC/esxDOS storage, and a
  supported UART-to-ESP link running ESP-AT.
- Spectrum Next requires ESP-AT and loads the self-contained NEX.
- SpectraNext requires the latest stable cartridge firmware and installs into
  local XFS storage.
- Mirror Shift v2 saves and Reversi peers are not compatible with Shatranj chess
  saves or clients.

Mirror Shift is released under the GNU General Public License v2.0. See
[`LICENSE`](../LICENSE) and [`THIRD_PARTY_NOTICES.md`](../THIRD_PARTY_NOTICES.md).
