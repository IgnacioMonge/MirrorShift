# Mirror Shift 1.0 for SpectraNext

Packaging uses the generic SpectraNext installer compiler. Product inputs live
in `installer.json`: stem `MIRSHIFT`, loading screen, installer panel centred
one character row below the top with black ink on white paper, and the
TAP/OVL/DAT set from `make tap-spectranext`.

`port.json` is the schema-2 FuseX product descriptor for the integrated Mirror
Shift product. Its `repository.path` name and relative value are retained
because the current FuseX runtime resolves product inputs through that field.

Run the normal build and validation commands from the repository root:

```sh
make tap-spectranext SPXN_DIR=/path/to/SpectraNext/driver
make spectranext-conformance-test SPXN_DIR=/path/to/SpectraNext/driver
make spectranext-resource SPXN_DIR=/path/to/SpectraNext/driver
```

The flat resource is written to `build/spectranext-resource/` and copied to
`release/Spectranext-resource/`. The local preview is
`build/installer-preview.png` and is not published.

## Test through TNFS

From the Mirror Shift repository root, serve the generated resource with the
canonical SpectraNext development tool. It prints the host address to use:

```powershell
C:\path\to\SpectraNext\tools\dev.cmd tnfs --root build/spectranext-resource --slot 2
C:\path\to\SpectraNext\tools\dev.cmd probe 127.0.0.1 --port 16384
```

On the Spectrum, replace any stale slot-2 mount and load the remote BASIC entry
point:

```text
%umount 2
%mount 2, "tnfs://<host-ip>/"
%fs 2
%load "boot.zx"
```

`boot.zx` is a BASIC `.zx` file; do not use `%tapein`. Keep the server running
until installation finishes.

FuseX discovery, profiles and execution belong to SpectraNext tooling. This
consumer does not carry a private launcher or mutate an emulator profile.
Cartridge power-cycle results remain binding.


## Version and installation compatibility

The installer requires the latest stable cartridge firmware. The exact minimum
accepted by the package remains declared in `installer.json`.

`VERSION` is `1.0`; the installer normalizes it to `1.0.0`. Install into a fresh
test destination when moving from development build `1.0.21`: the platform's
installer preserves a numerically newer installed version. Retain existing
saves and configuration; do not remove them to bypass this protection.

Software gates do not replace installation, game, save, and power-cycle checks
on real cartridge hardware.
