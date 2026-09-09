# Mirror Shift Architecture Decisions

## 0001 - Project Name

Decision: visible product name is Mirror Shift.

Reason: Mirror Shift replaces the visible Shatranj identity; inherited
protocol/internal prefixes stay stable for compatibility.

Current generated names:

- Spectrum TAP: `MIRSHIFT.tap` (8.3 basename)
- PC client: `MirrorShift.exe`
- Protocol prefix: `netchesszx/v1` (stable wire compatibility)
- Generated MQTT room prefix: `MS`
- Z80 `netchesszx_*`/`NETCHESSZX_*` symbols: stable ABI compatibility

## 0002 - Mirror Shift Rules Core

Decision: `src/common/reversi/` is the only active rules core. Delete the
inherited chess core and its test-only Spectrum adapters/oracle.

Use this layout instead:

```text
src/
  common/
    reversi/
    mqtt/
    protocol/
  pc/
    client/
  spectrum/
    app/
    board/
    overlay/
    platform/
    transport/
    ui/
asm/
  spectrum/
  uart/
  esxdos/
  platform/
  overlay/rules/
client/
tests/
```

Reason: after the product conversion no shipped or regression path consumes
chess position, FEN, SAN, or perft behavior. Keeping a second rules domain would
only preserve dead source and build cost.

Current contract:

- `src/common/reversi/`: portable deterministic Mirror Shift rules.
- `src/spectrum/board/reversi_board.c`: fixed-low-RAM product binding.
- `tests/rules/test_reversi.c`: product rules regression coverage.
- Legacy chess source, SAN/FEN/perft fixtures, and optional targets are absent.
- `docs/source-layout.md`: canonical source map for current paths.

## 0003 - One Cross-Platform Desktop Client

Decision: Windows, macOS, and Linux use one Qt client and one shared desktop
core. They are build/package variants, not independent applications.

Dependency direction is enforced with separate CMake targets:

```text
mirrorshift-common -> mirrorshift-desktop-core -> mirrorshift-client
```

- `mirrorshift-common` is portable C and has no Qt dependency.
- `mirrorshift-desktop-core` contains rules helpers, session adapters and
  controller, transport framing, and save-game persistence. It uses Qt Core
  and Network but not Widgets.
- `mirrorshift-client` contains the Widgets UI and platform packaging resources.

Reason: a single implementation prevents behavior drift between desktop
platforms while target boundaries catch accidental UI/platform dependencies in
the shared core at compile time. Platform-specific code is added only when a
real OS API requires it; speculative interface hierarchies are rejected.

## 0004 - Spectrum Renderer

Decision: Spectrum screen rendering starts in ASM.

Reason: the C `printf` board was useful for transport proof but too slow and too
large. Screen clear, board drawing, cursor, attributes, and later piece blits
need direct VRAM writes from the beginning.

Boundary:

- C keeps protocol/network orchestration while it remains cheap enough.
- ASM owns rendering hot paths.
- ChessZX is a reference for UI/piece organization, not vendored code.

## 0005 - Spectrum Cold Overlays

Decision: keep the Spectrum resident core 48K-first and add a Spectalk-style
cold overlay file, `MIRSHIFT.OVL`.

Reason: legal move validation, storage, options, and reconnect/resume logic are
cold paths. Keeping them resident would force a premature 128K-only boundary and
make later memory recovery harder.

Current contract:

- Resident owns UI, board drawing, network/protocol, timers, and turn state.
- `asm/esxdos/overlay_loader.asm` loads overlay blocks from `MIRSHIFT.OVL`
  into `_overlay_code_slot`.
- Overlay 0 is reserved for cold Mirror Shift rules UI.
- Local Spectrum moves are not applied locally until the PC returns `ACK <ply>`.
  If the PC rejects a move, it sends `NACK <ply> ...`; the Spectrum keeps the
  turn and board state.

This gives a safe protocol guard immediately while the full rules overlay is
filled in.


## 0006 - One Semantics, Two Implementations, One Judge

Decision: the portable reducers are the canonical PC/reference implementation,
while Spectrum and Next retain compact state machines optimized for their memory
budget. Both implementations must satisfy the same transcript corpus and wire
contract.

Reason: linking the generic reducer into Spectrum was measured at roughly
31 KiB of additional resident code and is not a viable way to obtain parity.
Shared judges enforce behavior without imposing the same runtime layout.
Target-specific expected results are forbidden: disagreement means one
implementation or the contract is wrong. Transport and UI adapters translate
events and execute actions; they do not decide session policy.

## 0007 - Save Record v2 Reuses the Inherited Envelope

Decision: retain the 45-byte binary record, unpadded 60-character Base64URL
encoding, file storage, and `RS00`/`RS01` RESTORE framing, but replace the
record contents with Mirror Shift state and version it as 2.

Version 2 stores only `.`, `A`, and `B` cells plus side, terminal state, ply,
session flags, timers, host side, and view. Readers reject version 1 chess
records before applying a board; there is no chess-to-Lattice conversion.

Reason: this preserves the inherited persistence and synchronized-restore UX
without a second protocol or filesystem path. Treating castle/en-passant bytes
as Reversi state is semantically undefined and unsafe, while automatic
conversion cannot preserve a chess position's meaning.

## 0008 - Next Keeps ULA+ Enabled

Decision: initialize the standard ULA+ palette and enable ULA+ once during the
Next graphics-bank bootstrap. Do not toggle ULA+ when the board theme or setup
screen changes.

Palette groups 0/1 mirror the classic ULA normal/bright colours, so the default
theme keeps its classic appearance. Themes 2-5 use private group-2 coordinate
and frame colours. Setup swatches use light/dark pairs in group 3 and the free
group-2 indices 3-7 for brighter focused copies; indices 0-2 remain owned by
board coordinates. The Next piece flash uses hardware sprites and does not
depend on classic ULA FLASH semantics.

Reason: switching palette interpretation independently in setup and theme code
allowed the attribute file to be rendered with stale state. One application-
wide mode removes that failure path and avoids repeated resident/overlay
NextReg writes. Reapplying the selected theme on START was rejected as a
workaround that retained two possible hardware states.

## 0009 - Either Peer May Offer Restore

Decision: Host and Guest may both load a save and offer it. The wire sequence
stays `RQ` / `RY`/`RN` / `RS00` / `RS01` / `RA`. `host_color` in the save
record still has to match the current seating.

Reason: restore is a negotiated snapshot, not a host privilege. The previous
host-only gates were policy leftovers: incoming `RQ` was auto-`RN` on the host
and local `RESTORE` was rejected for Guest. Crossed offers remain busy-`RN`.
Older hosts still refuse a guest `RQ` without mutating the board.

## 0010 - Next NEX Uses Paged Overlays and a Fixed Renderer Extension

Decision (user-authorized, 2026-09-05): retain Classic's 2 KiB execution slot;
Next NEX uses 4 KiB overlay code in 8 KiB MMU pages. The upper 4 KiB replicates
immutable resident code. All mutable resident sections and the stack remain
above 0x8000. A fixed slot-1 extension contains inherited renderer routines;
Mirror Shift's 64-disc rendering, animation bodies and context at 0x5ff8 stay.

Reason: pagination addresses the architectural memory constraint. Maintaining
the old Next slot by micro-optimizing or enabling ultra-aggressive allocation
flags adds risk without the same benefit. Measured Next stack margin rises
from 620 to 4681 bytes with these fixes. Extension headroom is 50 bytes,
checked against its actual 5376-byte capacity, not a speculative reserve.

SpectraNext is a separate, canonically gated port; this Next acceptance does
not certify its loader or hardware. Real MMU/IRQ/storage/palette validation
remains pending for the new NEX.
