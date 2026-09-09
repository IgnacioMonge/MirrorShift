<p align="center">
  <img src="assets/pc-client/mirrorshift-wordmark.png" alt="Mirror Shift" width="620">
</p>

<p align="center">
  <strong>Two sides. One reality.</strong><br>
  Online Reversi across the ZX Spectrum family and modern desktops.<br>
  <strong>Version 1.0</strong> · Direct TCP or MQTT · GPL-2.0
</p>

<p align="center">
  <a href="README.es.md">Español</a> ·
  <a href="https://github.com/IgnacioMonge/MirrorShift/releases/latest">Download</a> ·
  <a href="docs/README.md">Developer documentation</a> ·
  <a href="client/README.md">Qt client guide</a>
</p>

---

Mirror Shift brings two-player network Reversi to ZX Spectrum Classic,
Spectrum Next, the SpectraNext cartridge, and Windows, macOS, and Linux.
Play Spectrum-to-Spectrum, Spectrum-to-desktop, or desktop-to-desktop using
the same rules and game protocol.

Place a disc on the **Lattice** and capture opposing discs enclosed between
it and your own. Each placement is an **Alignment**; captured discs change
sides through **Mirrorlock**. A forced pass is **Silence**, a tied final score
is **PARADOX**, and **Echoes** carries your chat with the other player.

## Why Mirror Shift

| | |
| --- | --- |
| **Play** | Spectrum ↔ Spectrum, Spectrum ↔ desktop, or desktop ↔ desktop |
| **Connect** | Direct TCP to your opponent, or MQTT through a shared broker and room |
| **Platforms** | Classic, Next, SpectraNext, Windows, macOS, and Linux |
| **In game** | Legal-move hints, SELF/ECHO scores, clocks, chat, takeback, and rematches |
| **Continue later** | Saved settings and synchronized restoration of compatible saved games |
| **Native retro builds** | TAP + OVL + DAT for Classic; one self-contained NEX for Next; cartridge resource for SpectraNext |

## Contents

- [Platforms and protocols](#platforms-and-protocols)
- [Download](#download)
- [Installation](#installation)
- [Quick start](#quick-start)
- [Gallery](#gallery)
- [Disc sets and board themes](#disc-sets-and-board-themes)
- [Using Mirror Shift](#using-mirror-shift)
- [Build from source](#build-from-source)
- [Troubleshooting](#troubleshooting)
- [Development](#development)
- [Credits and license](#credits-and-license)

## Platforms and protocols

| Client | Platform | Network modes | Distribution |
| --- | --- | --- | --- |
| Qt desktop | Windows, macOS, Linux | Direct TCP, MQTT | Desktop package |
| ZX Spectrum Classic | 48K ZX Spectrum | Direct TCP, MQTT | `MIRSHIFT.tap` + `MIRSHIFT.OVL` + `MIRSHIFT.DAT` |
| Spectrum Next | ZX Spectrum Next | Direct TCP, MQTT | `MIRSHIFT.nex` |
| SpectraNext | ZX Spectrum with SpectraNext cartridge | Direct TCP, MQTT | Cartridge resource installer |

Direct TCP requires the guest to reach the host's address and port. With
MQTT, both clients connect to the same broker and room; the host does not
need an incoming connection from the guest.

### Spectrum hardware

Classic and Next use a supported UART-to-ESP link with **ESP-AT**.
Classic also needs divMMC/esxDOS for its companion files. A ZX-Uno-compatible
UART requires ESP transmit flow control through CTS; NetManZX configures this
setting. The Next release is self-contained, so only its NEX needs to be copied.

SpectraNext is a separate cartridge platform, not the Spectrum Next. It uses
its own networking and XFS storage. The installer requires the latest stable
cartridge firmware. Its clock support uses a UTC
source rather than a cartridge RTC.

## Download

Download ready-to-run builds from the
[latest public release](https://github.com/IgnacioMonge/MirrorShift/releases/latest).
Choose the package for your platform: Windows portable, macOS application,
Linux x86_64 AppImage, Classic three-file set, Next NEX, or SpectraNext
resource installer.

Use files from the same release. Build output locations and cartridge resource
preparation are documented below and in the
[cartridge guide](packaging/spectranext/README.md). See the
[1.0 release notes](docs/release-notes-1.0.md) and [changelog](CHANGELOG.md)
for the complete release overview.

## Installation

### Desktop

Extract the complete package and start `MirrorShift.exe` on Windows or
`MirrorShift.app` on macOS. On Linux, make the AppImage executable and launch
it. Keep bundled libraries, assets, and license files with the application.
Desktop-to-desktop games do not require Spectrum hardware.

### ZX Spectrum Classic

Copy `MIRSHIFT.tap`, `MIRSHIFT.OVL`, and `MIRSHIFT.DAT` to the same directory
on writable esxDOS storage. Keep their names unchanged and load `MIRSHIFT.tap`.

### Spectrum Next

Copy `MIRSHIFT.nex` to the SD card and launch it from the NextZXOS browser.
No separate OVL or DAT file is needed.

### SpectraNext cartridge

1. Update the cartridge to the **latest stable SpectraNext firmware**, then
   configure the cartridge and Wi-Fi using the
   [official SpectraNext instructions](https://docs.spectranext.net/tutorials/setting-up-mounts).
2. In the SpectraNext menu, select **Load Resource URL** and enter:
   <code>https://ignaciomonge.github.io/MirrorShift/</code>
3. The guided installer installs Mirror Shift in the cartridge's local storage
   and launches it.
4. Afterwards, start `MIRSHIFT.ZX` from local XFS. To update Mirror Shift, use
   **Load Resource URL** again; your configuration and saved games are
   preserved.

<p align="center">
  <img src="docs/screenshots/mirrorshift-1.0-spectranext-installer.png" alt="Mirror Shift guided installer on SpectraNext" width="400"><br>
  <sub>The guided SpectraNext resource installer.</sub>
</p>

A GitHub Release ZIP is not a mountable SpectraNext resource; enter the HTTPS
resource URL above instead. The [cartridge guide](packaging/spectranext/README.md)
documents local TNFS testing and version compatibility.

## Quick start

1. Start Mirror Shift on both clients.
2. Choose **Host** on one and **Guest** on the other.
3. Select **Direct** or **MQTT** on both sides.
4. For Direct, enter the host address and port on the guest. For MQTT, enter
   the same broker, port, and room on both clients.
5. The host chooses the side and starts the game once the peer is ready.
6. Place a disc on a legal square when it is your turn. Use Echoes to chat.

### Direct TCP

The default game port is `5000`. On a LAN, use the host's LAN address and
allow the connection through its firewall. For two Qt instances on the same
computer, use `127.0.0.1` on the guest.

### MQTT

Both players need access to the same broker and must use complementary roles
and the same room code. The broker carries the game and presence messages.

## Gallery

### Desktop

<table>
  <tr>
    <td align="center" width="33%"><strong>Windows</strong><br><img src="docs/screenshots/mirrorshift-1.0-qt-windows.png" alt="Mirror Shift Qt client on Windows" width="300"></td>
    <td align="center" width="33%"><strong>macOS</strong><br><img src="docs/screenshots/mirrorshift-1.0-qt-macos.jpg" alt="Mirror Shift Qt client on macOS" width="300"></td>
    <td align="center" width="33%"><strong>Linux</strong><br><img src="docs/screenshots/mirrorshift-1.0-qt-linux.jpg" alt="Mirror Shift Qt client on Linux" width="300"></td>
  </tr>
</table>

### ZX Spectrum Classic

<table>
  <tr>
    <td align="center" width="33%"><strong>Game setup</strong><br><img src="docs/screenshots/mirrorshift-1.0-classic-setup.png" alt="Mirror Shift game setup on ZX Spectrum Classic" width="300"></td>
    <td align="center" width="33%"><strong>Direct game</strong><br><img src="docs/screenshots/mirrorshift-1.0-classic-game.png" alt="Mirror Shift Direct game on ZX Spectrum Classic" width="300"></td>
    <td align="center" width="33%"><strong>Saved games</strong><br><img src="docs/screenshots/mirrorshift-1.0-classic-saves.png" alt="Mirror Shift saved-game browser on ZX Spectrum Classic" width="300"></td>
  </tr>
</table>

### Spectrum Next

<table>
  <tr>
    <td align="center" width="33%"><strong>Game setup</strong><br><img src="docs/screenshots/mirrorshift-1.0-next-setup.png" alt="Mirror Shift game setup on Spectrum Next" width="300"></td>
    <td align="center" width="33%"><strong>Direct game</strong><br><img src="docs/screenshots/mirrorshift-1.0-next-game.png" alt="Mirror Shift Direct game on Spectrum Next" width="300"></td>
    <td align="center" width="33%"><strong>Takeback request</strong><br><img src="docs/screenshots/mirrorshift-1.0-next-takeback.png" alt="Mirror Shift takeback request on Spectrum Next" width="300"></td>
  </tr>
</table>

### SpectraNext

<table>
  <tr>
    <td align="center" width="33%"><strong>Game setup</strong><br><img src="docs/screenshots/mirrorshift-1.0-spectranext-setup.png" alt="Mirror Shift game setup on SpectraNext" width="300"></td>
    <td align="center" width="33%"><strong>MQTT game</strong><br><img src="docs/screenshots/mirrorshift-1.0-spectranext-game.png" alt="Mirror Shift MQTT game on SpectraNext" width="300"></td>
    <td align="center" width="33%"><strong>About</strong><br><img src="docs/screenshots/mirrorshift-1.0-spectranext-about.png" alt="Mirror Shift About screen on SpectraNext" width="300"></td>
  </tr>
</table>

## Disc sets and board themes

Classic, Next, and SpectraNext offer **BW-L**, **BW-M**, and **BW-S** disc sets
(large, medium, and small). Choose the set during game setup. Themes and board
rotation change presentation without changing the rules or player assignment.

Classic and SpectraNext use five attribute palettes: **Classic**, **Blue**,
**Green**, **Cyan**, and **Magenta**. Next uses full-color board tiles and
hardware sprites. The dark Next cursor remains visible on occupied squares;
on the Spectrum clients, up to three eligible discs shine together to mark
the side to move.

## Using Mirror Shift

Enclose at least one opposing disc in a straight line to make a legal move.
Captures resolve automatically in every affected direction. If you have no
legal move, Silence passes the turn automatically. The game ends when neither
side can move; the side with more discs wins. Equal scores produce PARADOX.

### Desktop controls

| Action | Control |
| --- | --- |
| Configure a session | Choose transport and role, then enter connection settings |
| Place a disc | Click one legal square on your turn |
| Send text | Type in Echoes and press Enter |
| Save or restore | Use the disk buttons below the move log, or `/save [name]` and `/load [name]` |
| Inspect traffic | Open Log to view RX/TX messages |
| Change appearance | Use the client settings |

The desktop client remembers connection settings and recent Direct guest
addresses. See the [Qt guide](client/README.md) for save slots and controls.

### Spectrum controls

| Context | Control |
| --- | --- |
| Setup: move between rows | Cursor Up/Down or `Q`/`A` |
| Setup: change an option | Cursor Left/Right or `O`/`P` |
| Setup: edit or confirm | Space or Enter |
| Board: move the cursor | Cursor keys or `Q`/`A`/`O`/`P` |
| Board: place a disc | Space on a legal square |
| Open and submit text input | Enter |
| Open the in-game menu | EDIT (`Caps Shift` + `1` on a Classic keyboard) |
| FILE menu | `Q`/`A` selects; Enter/Space loads or saves; `E` erases |

The in-game menu provides FILE, DISCONNECT, RESET, FLIP, THEME, and ABOUT.
Use Left/Right or `O`/`P`, then Space/Enter to select an action.

### Text commands

| Input | Result | Availability |
| --- | --- | --- |
| `/resign` | Resign the current game | Qt and Spectrum |
| `/takeback` | Request undo of the last applied move | Qt and Spectrum |
| `/save [name]` | Save locally | Qt; use FILE on Spectrum |
| `/load [name]` | Request restoration of a saved game | Qt; use FILE on Spectrum |

Either peer may initiate restoration; the other must approve it. The save's
host-side assignment must match the current session. Mirror Shift `.MSH`
version 2 saves are incompatible with Shatranj chess saves.

## Build from source

Use the repository Makefile. Spectrum builds require z88dk/SDCC, Python 3,
and Pillow for asset generation. Desktop requirements and commands are in the
[Qt client guide](client/README.md).

```sh
make tap              # Classic TAP + OVL + DAT
make nex              # self-contained Spectrum Next NEX
make client-test      # Qt build and tests
make test             # shared and Spectrum host tests
```

Classic files are written to `release/`; Next writes
`release/Next/MIRSHIFT.nex`. SpectraNext uses its own toolchain checkout:

```sh
make spectranext-resource SPXN_DIR=/path/to/SpectraNext/driver
```

## Troubleshooting

- **Classic cannot load assets:** keep matching TAP, OVL, and DAT files together.
- **Direct cannot connect:** check roles, address, port, and the host firewall.
  `127.0.0.1` only reaches the same computer.
- **MQTT peer is missing:** check the broker, room, and complementary roles.
- **Save is rejected:** use a compatible Mirror Shift v2 save and the same
  host-side assignment; a truncated file or extra bytes also make it invalid.

## Development

The [developer documentation](docs/README.md) covers architecture, protocol,
validation, and release maintenance. The [source layout](docs/source-layout.md)
explains module ownership.

## Credits and license

Mirror Shift is based on Shatranj and retains its networking and Qt desktop
components. Its board tiles, discs, and branding are Mirror Shift assets;
provenance and editable sources are described in the
[asset guide](assets/README.md).

Released under the [GNU General Public License v2.0](LICENSE). Dependencies
retain their own terms, listed in [third-party notices](THIRD_PARTY_NOTICES.md).
Keep the corresponding source and required notices with binary distributions.

## Author

**M. Ignacio Monge Garcia — 2026**

Issues and contributions are welcome in the
[official repository](https://github.com/IgnacioMonge/MirrorShift).

<p align="center"><sub>Two sides. One reality.</sub></p>
