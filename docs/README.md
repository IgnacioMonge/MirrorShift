# Mirror Shift technical documentation

[Español](README.es.md) · [User guide](../README.md) · [Qt client guide](../client/README.md)

This directory contains the maintained engineering documentation for Mirror Shift 1.0.
The inherited normative contracts remain transport- and client-neutral:

- [`wire-contract.md`](wire-contract.md) — payloads, Direct newline-delimited
  TCP framing, MQTT topics, restore exchange, and compatibility rules.
- [`session-core-contract.md`](session-core-contract.md) — session state,
  reducers, retries, acknowledgements, and cross-client semantics.

## Architecture and ownership

- [`source-layout.md`](source-layout.md) — module boundaries and ownership.
- [`architecture-decisions.md`](architecture-decisions.md) — accepted
  cross-cutting decisions and their rationale.

The implementation is shared across Qt Windows/macOS/Linux, ZX Spectrum
Classic, Spectrum Next, and the SpectraNext cartridge. Common C owns Reversi rules plus inherited protocol,
session, and save-format code; desktop code adapts them to Qt and the Spectrum
clients use the compact target-specific runtime. Keep protocol parsing/building in common code
and treat the two contracts above as the source of truth.

## Build and validation entry points

Run from the repository root:

```sh
make test          # host tests
make client-test   # Qt build and tests
make tap           # Classic TAP/OVL/DAT
make nex           # Next self-contained NEX
make full-check    # release-level cross-target guards
```

Spectrum configuration variables are `PORT`, `MQTT_HOST`, `MQTT_PORT`, and
`MQTT_CODE`; use them only for a configured Spectrum build. The Linux Qt client
builds against the system Qt. The **Build Linux AppImage** workflow packages x86_64 and keeps
manual-run artifacts; attach the package to the release manually. The public
Qt workflow uses CMake through the repository targets, without a qmake or raw
CMake fallback.

## Product and release reference

- [Release notes](../CHANGELOG.md): user-visible changes in 1.0.
- [SpectraNext guide](../packaging/spectranext/README.md): cartridge resource, firmware, and installation compatibility.

`make full-check` covers host tests, module guards, Classic/Next ABI, and size
policies. Desktop tests use `make client-test`. SpectraNext build and conformance
are separate; use the cartridge guide's commands. A documentation version does
not replace recorded hardware, two-client, or platform acceptance evidence.
