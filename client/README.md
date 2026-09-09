# Mirror Shift 1.0 desktop client

[Español](README.es.md) · [Project documentation](../docs/README.md)

Qt desktop client for Mirror Shift 1.0, built on the Shatranj runtime.
Windows, macOS, and Linux use the same implementation and support both
transports:

- **Direct TCP**: a host listens for one guest; the guest connects to the
  host's address and port.
- **MQTT**: both peers join a room through a broker; the broker carries the
  session payloads and presence messages.

The transport-neutral payloads and topic rules are defined in
[`docs/wire-contract.md`](../docs/wire-contract.md). This guide describes how to use and build the Qt client.

## Using the Qt client

TAKEBACK and RESET buttons next to SEND use the same session confirmations.
The Echoes character counter appears inside the input field.

1. Select `Direct` or `MQTT` and enter the required endpoint/room settings.
2. Choose `Host` or `Guest`; the host selects the colour and starts the game.
3. Click one highlighted Lattice square on your turn. That click is one
   Alignment; Mirrorlock flips and forced Silence are resolved automatically.
4. Use the chat box for messages; pressing Enter sends the current line.
5. Use the command forms below when a session control action is needed:

   ```text
   /resign     resign the current game
   /takeback   request undo of the last applied move
   /save [name] save the current position locally
   /load [name] request restoration of a saved position
   ```

6. Use the disk buttons below the move log to save or load one of ten slots.
   Either peer may save or initiate a load once the session is ready; the
   peer must approve a load, and the save's host-side assignment must match
   the current session. Legacy chess version 1 files are
   rejected without changing the current game.

The client remembers connection settings and recent Direct guest addresses,
shows turn/game/move clocks, and exposes an RX/TX log. Once a Direct or MQTT
peer is ready, the bottom status shows its advertised platform as `VS ZX`,
`VS NXT`, `VS SPCX`, `VS MAC`, `VS LNX`, or `VS PC`; legacy peers appear as `VS ?`.
A hardware Spectrum host can be tested with the steps in
[Test Direct TCP with hardware](#test-direct-tcp-with-hardware).

## Architecture

```text
portable common C -> desktop core -> Qt Widgets application
```

The common layer owns Mirror Shift rules plus inherited protocol parsing,
MQTT grammar, session reducers, and the Mirror Shift v2 save-game wire format.
The desktop core adapts those contracts to TCP/MQTT, timing, persistence, and
Qt helpers. The Qt application owns presentation and packaging. CMake target
boundaries prevent a platform-specific client fork.

## Build and test

Install CMake, Qt with Core/Widgets/Network, and a matching C++ compiler.
On Windows, use Visual Studio 2022 with C++ support and the MSVC Qt build;
set `QT_ROOT_DIR` to that Qt installation. The MSVC presets require CMake
3.22 or later. macOS and Linux use their platform compiler and Qt installation.


Use the repository `Makefile` entry points from the project root:

```sh
make client-test   # configure, build, and run Qt tests
make client        # release packaging for the current desktop platform
make tap           # Classic ZX: MIRSHIFT.tap, .OVL, and .DAT
make nex           # Spectrum Next: self-contained MIRSHIFT.nex
make full-check    # host, Spectrum, ABI, and size guards
```

`make client-test` is the supported desktop development loop on Windows,
macOS, and Linux. On Windows, `client\build-pc.cmd` is an equivalent
interactive wrapper; the MSVC CMake presets keep the build tree outside the
repository and provide Qt DLLs to CTest. Do not use a qmake fallback or an
ad-hoc in-tree/raw CMake build.

`make client` produces the Windows executable, deploys a macOS application
bundle, or builds the Linux executable against the system
Qt installation. The **Build Linux AppImage** GitHub Actions workflow packages
and inspects a self-contained x86_64 AppImage and uploads it as a workflow
artifact. To publish it, download that artifact and attach the AppImage
manually to the GitHub release. On macOS,
the command also installs the current bundle at `/Applications/MirrorShift.app`;
set `CLIENT_MAC_APPLICATIONS_DIR` to choose another Applications directory.

The Spectrum targets accept these configuration variables when a configured
build is required:

```sh
PORT=5000 MQTT_HOST=broker.example MQTT_PORT=1883 MQTT_CODE=1234 make tap
```

## Test Direct TCP with hardware

1. Build the matching Classic or Next target (`make tap` or `make nex`).
2. On a Classic host, copy `MIRSHIFT.tap`, `MIRSHIFT.OVL`, and `MIRSHIFT.DAT`
   together; on Next, copy the self-contained `MIRSHIFT.nex`.
3. Start the host and note its LAN address and configured port (the default is
   `5000` for the Classic build).
4. Start the Qt client, select `Direct` and `Guest`, enter the address/port,
   and connect.
5. Wait for the host to start the game, then play when the status says it is
   your turn; chat is available in the same window.

For Spectrum-specific input, the command line accepts `/resign` and
`/takeback`; saving and loading are available from the FILE menu. The
protocol-level expectations remain in the canonical
[`wire contract`](../docs/wire-contract.md) and
[`session contract`](../docs/session-core-contract.md).
