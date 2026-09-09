# Local ZEsarUX and ZXESPEmu

## Prepared environment

The current Windows machine-local harness is:

```text
C:\dev\ZXESPEmu
```

It is a separate managed harness. Runtime logs and the downloaded/built
emulators are intentionally kept outside this repository.

The reusable patched executable is under:

```text
ZXESPEmu\vendor\zesarux\zesarux.exe
```

Normal runs reuse that local copy. The first run invokes the setup script only
when the vendor executable is absent; do not redownload ZEsarUX for each test.

## Run Mirror Shift

Build the artifacts you need, then validate and sync the consumer-owned
descriptor:

```powershell
make tap nex
& 'C:\Program Files\Python311\python.exe' ..\ZXESPEmu\launcherctl.py validate .\zxespemu-launchers.json
& 'C:\Program Files\Python311\python.exe' ..\ZXESPEmu\launcherctl.py register .\zxespemu-launchers.json
& 'C:\Program Files\Python311\python.exe' ..\ZXESPEmu\launcherctl.py sync mirrorshift
& 'C:\Program Files\Python311\python.exe' ..\ZXESPEmu\launcherctl.py status mirrorshift
```

The descriptor publishes four independent launchers: `Mirror Shift ZX`,
`Mirror Shift Next`, `Mirror Shift SpectraNext`, and `Mirror Shift SpectraNext
Installer`. The last two use FuseX direct and installer modes respectively;
the installer launcher never replaces the normal one. On Windows the manager
owns the Desktop and Start-menu shortcuts. Launchers consume existing artifacts
and do not compile during startup. Both SpectraNext launchers resolve the
versioned local `packaging/spectranext/port.json`; they do not depend on a
temporary port worktree. Use `127.0.0.1` for a Direct desktop peer on the same
machine.

Runtime state and logs are replaced safely under `ZXESPEmu/run/`; inspect
`launcher.log`, `zesarux.log`, and `zxesp.log` when startup or UART traffic
fails.

Before a cartridge run, the host-only conformance gate is:

```powershell
make spectranext-conformance-test
```

It compiles the DIRECT backend, ping/poll timing and full DIRECT parity with
`NETCHESSZX_SPECTRANEXT` at the cartridge's PAL 50 Hz, then runs the black-box
SPCX harness tests. It does not replace the physical cartridge run.

## Virtual ESP scope

The virtual modem covers the commands Mirror Shift uses:

- Wi-Fi preflight and local IP reporting.
- TCP client/server, multiplexing, send, close, and incoming IPD frames.
- MQTT transparent binary passthrough and guarded escape.
- SNTP configuration and time query.

It does not emulate the ESP8266 CPU, radio, Wi-Fi scans, TLS, or UDP.

Run its focused tests with:

```sh
PYTHONPATH=. python3 tests/test_zxesp.py -v
```

## ZEsarUX patches

The local setup applies:

- `patches/zesarux-13-uartbridge.patch` for the required command-line UART
  bridge.
- `patches/zesarux-13-tbblue-ulaplus.patch` for Next ULA+ palette rendering.

Stock ZEsarUX 13.0 conflates TBBlue ULA+ enable (`NextReg 0x68` bit 3) with
ULANext enable (`NextReg 0x43` bit 0). Consequently Mirror Shift's valid private
attributes `0x81` and `0x8A` can appear blue with FLASH in themes 2-5. The patch
selects the ULA+ palette group from attribute bits 6-7. It was reported with
the patch at <https://github.com/chernandezba/zesarux/pull/12>.

This is an emulator correction, not a Mirror Shift wire or rendering workaround.
Real Next hardware remains authoritative for timing, MMU/IFF restoration, UART,
FPGA-core behavior, and final visual acceptance.

