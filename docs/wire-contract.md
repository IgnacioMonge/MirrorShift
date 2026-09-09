# Wire Contract

This is the client-neutral Mirror Shift wire contract. Spectrum, PC, and future clients must implement these payloads without assuming the peer implementation.

## Ownership

`docs/session-core-contract.md` is authoritative: PC executes the canonical
common reducers; ZX and Next execute the compact Spectrum FSMs. These are the
only two implementation families and must produce the same target-neutral wire
semantics.

## Session

| Transport | Direction | Payload | Notes |
| --- | --- | --- | --- |
| Direct | host to guest | `HELLO DIRECT HOST WHITE=HOST|GUEST` | Host announces which role owns white. |
| Direct | guest to host | `HELLO DIRECT GUEST` | Guest readiness. |
| Direct | occupied host to new guest | `BUSY` | Positive occupancy signal; send before closing only the newcomer connection. |
| Direct | host to guest | `GAME START WHITE=HOST|GUEST` | Direct start carries white owner. |
| MQTT | host bootstrap | `H W|B <sid>` | Retained on the side-neutral `meta` topic. |
| MQTT | guest bootstrap | `J <sid>` | Live on the side-neutral `meta` topic. |
| MQTT | side presence | `O W|B <sid>` | Retained on `pres_w` or `pres_b`. |
| MQTT | correlated disconnect | `F W|B <sid>` | Live on the disconnecting side's presence topic. A matching SID may end that session; an idless or foreign SID may not. Spectrum hosts install this as their non-retained Last Will. Spectrum guests install no Last Will before learning the host SID. |
| MQTT | host to guest | `GAME START` | MQTT color/session comes from presence, not start detail. Receivers may accept `GAME START <detail>` for forward compatibility, but senders should emit plain `GAME START`. |
| Any | receiver to starter | `ACK GAME START` | Start accepted. |
| Any | receiver to starter | `NACK GAME START [reason]` | Start rejected. |

## Game

Reversi has no negotiated draw in Mirror Shift. A natural tied final score is
PARADOX. Legacy `DRAW`, `ACK DRAW`, `NACK DRAW`, and `CANCEL DRAW` payloads are
unsupported: they neither change the game nor refresh peer liveness. `/draw`
is not a chat command; as ordinary chat text it has no control effect.
Unrecognized MQTT payloads are ignored; chat requires the `CHAT` prefix.

| Payload | Meaning |
| --- | --- |
| `MOVE <ply> <square> [notation]` | Mirror Shift placement. `square` is exactly one lowercase Lattice coordinate (`a1`..`h8`); optional `notation` is exactly one non-empty token, and trailing tokens are invalid. Forced Silence never travels on the wire. |
| `ACK <ply> [notation]` | Move or takeback accepted. |
| `NACK <ply> [reason]` | Move or takeback rejected. Reason is optional and advisory. |
| `TAKEBACK <ply>` | Request undo of the last applied ply. Response is generic `ACK/NACK <ply>`. |
| `RESET` | Reset/rematch request. |
| `ACK RESET` / `NACK RESET [reason]` | Reset response. |
| `CANCEL RESET` | Cancel a locally pending reset after its reply deadline. The receiver answers `NACK RESET`. |
| `RESIGN` | Unilateral resignation. Sender retransmits until `ACK RESIGN` arrives. |
| `ACK RESIGN` | Resignation acknowledged. Receivers must ACK every `RESIGN`, including retransmissions. Current peers then synchronize the automatic new game through the existing `RESET` / `ACK RESET` exchange. |
| `CHAT <text>` | Chat text. Max visible text is 42 chars. |
| `MACH <code>` where code is `ZX`, `NXT`, `MAC`, `LNX`, `PC`, or `SPCX` | Optional peer machine identity (`SPCX` identifies the SpectraNext cartridge). Both peers may emit after the link is ready (`peer_ready`). Informational only: never changes game state, color, or control flow. Unknown codes and missing announcements leave the peer platform unknown. Legacy peers ignore `MACH` silently on Direct and Spectrum MQTT. |
| `PING` / `ACK PING` | Keepalive. |
| `BYE` | Peer disconnect. |
| `RQ` | Request permission to restore a saved position. |
| `RY` / `RN` | Accept or reject/cancel a restore exchange. |
| `RS00 <30 Base64URL chars>` | First 30 ASCII characters of the unpadded Base64URL restore encoding. Exactly 35 wire bytes including the `RS00 ` prefix. |
| `RS01 <30 Base64URL chars>` | Final 30 ASCII characters of the unpadded Base64URL restore encoding. Exactly 35 wire bytes including the `RS01 ` prefix. |
| `RA` | Snapshot applied; restore exchange complete. |

## Topic Rules

Direct carries these payloads as newline-delimited TCP lines.
Spectrum transports support at most 47 application payload bytes; their local
48-byte buffers reserve the final byte for the C terminator. `CHAT ` plus the
maximum 42 visible characters exactly reaches that interoperable limit.

Application payload bytes never contain `0x00`. A locally appended C
terminator is not part of the wire payload. A receiver must reject a Direct
line or MQTT PUBLISH containing an embedded NUL instead of dispatching the
prefix as a shorter message.

The compact Spectrum MQTT transport discards an oversized packet with a
representable remaining length through its exact body boundary before parsing
the next packet. The discard body never credits broker/peer liveness or yields
an application payload. A remaining length above the compact 255-byte range,
including a continued third length byte, ends the transport instead of scanning
its unknown body for apparent packet headers. The existing 160-byte accepted
packet limit is unchanged.

MQTT uses room topics by side. Lateral topics name the direction (`w2b` =
white publishes, black listens; `b2w` the reverse). ACK topics name the
recipient side (`ack_w` carries acknowledgements addressed to white, `ack_b`
to black); the sender publishes to the peer's ACK topic.

Normative per-payload topic table:

| Payload | Sender publishes to | Receivers must accept from |
| --- | --- | --- |
| `H` | `meta` (retained) | `meta` |
| `J` | `meta` (live) | `meta` |
| `O` / `F` | own `pres_w` / `pres_b` | peer and own presence topics |
| `GAME START` | `meta` or own lateral (both canonical, see below) | `meta` and inbound lateral |
| `ACK GAME START` / `NACK GAME START` | `meta` or own lateral (both canonical, see below) | `meta` and inbound lateral |
| Game payloads (`MOVE`, `TAKEBACK`, `RESET`, `RESIGN`, `CHAT`, `PING`, `BYE`, `MACH`) | own lateral (`w2b` white, `b2w` black) | inbound lateral |
| `ACK` / `NACK` responses to game payloads (incl. `ACK PING`) | peer ACK topic (`ack_b` from white, `ack_w` from black) | inbound ACK topic and inbound lateral |
| `RQ`, `RY`, `RN`, `RS00`, `RS01`, `RA` | own lateral, live, never retained | inbound lateral |

`GAME START` and its ACK/NACK are the only payloads with two canonical
emission topics: the Spectrum family publishes them on `meta`, the PC family
on its own lateral. Both are contract-legal until unified; every client must
therefore listen for them on `meta` and on its inbound lateral. Receivers
must process these START replies by payload, not by client type. Receivers
enforce the inbound lateral route for `MOVE` and `MACH`. Either payload on any
other topic is inert and does not credit liveness, mutate state, reach the UI,
or produce a reply. A retained `MACH` is also inert. Other payload families
retain their current payload-dispatched behavior; see
`docs/session-core-contract.md`.

## MQTT RESTORE

Either linked peer may initiate MQTT RESTORE. The ordered exchange is `RQ`,
then `RY` or `RN`; after `RY`, the initiator sends `RS00` followed by `RS01`.
The receiver decodes and applies only after both 30-character chunks form the
complete 60-character, unpadded Base64URL encoding of the 45-byte binary save
record.
No padding or NUL terminator is transmitted. It then answers `RA` on success
or `RN` on rejection/failure.

The control deadline gives each stage bounded retries. While waiting for `RY`,
the initiator retries `RQ`; while waiting for `RA`, it retries the two chunks in
order. The receiver re-sends `RY` for a duplicate accepted `RQ`, waits a bounded
time for missing chunks, and ends the exchange with `RN` on expiry. Before
`RY`, a local cancel sends `RN`; after chunk transmission starts, local cancel
is ignored because the peer may already have applied the snapshot. After a
successful apply, either exact duplicate chunk re-sends `RA`; a conflicting
chunk sends `RN` without applying again.

## Save Record v2

The 60 Base64URL characters carried by RESTORE encode one 45-byte Mirror Shift
record. The unpadded Base64URL and two 30-character chunk sizes are unchanged
from the inherited transport envelope; the binary payload is version 2:

- bytes `0..31`: two Lattice cells per byte, high nibble first;
  `0 = .`, `1 = A`, `2 = B`; every other nibble is invalid;
- byte `32`: side in bit 0 (`0 = A`, `1 = B`), host side in bit 1 with the
  same mapping, game-over state in bit 2, flipped view in bit 6; all other
  bits are zero;
- byte `33`: reserved and zero;
- bytes `34..35`: little-endian placement ply;
- byte `36`: bit 0 active, bit 1 game over; other bits are zero. The
  game-over flag must equal byte 32 bit 2, and active plus game over is invalid;
- bytes `37..42`: game `hh:mm:ss`, then move `hh:mm:ss`; hours are `0..99`,
  minutes and seconds are `0..59`;
- byte `43`: format version `2`;
- byte `44`: CRC-8 over bytes `0..43`, polynomial `0x07`, initial value zero.

Readers reject version 1 chess records and any invalid cell, reserved bit,
reserved byte, timer, state, or CRC before mutating the board. Mixed v1/v2
peers retain the normal RESTORE negotiation but the decoder ends an
incompatible transfer with `RN`; the active game remains unchanged.

## Compatibility Rules

- Parse by payload grammar, not by peer client name.
- Control requests (`RESET`, `TAKEBACK`, `RESIGN`) are retransmitted until answered; receivers must treat duplicates idempotently (re-ACK an already-accepted request instead of NACKing or re-prompting).
- Mixed-version RESTORE: a peer that still treats restore as host-only answers a guest `RQ` with `RN` and does not mutate the board. Host-initiated restore remains interoperable.
- Treat unknown optional tails as advisory text unless the verb requires exact grammar.
- Do not send PC-only or Spectrum-only variants.
- Keep MQTT `GAME START` plain for canonical output.

Mirror Shift deliberately rejects the inherited four/five-character chess
MOVE payload. A mixed Mirror Shift/Shatranj pair may still negotiate a session
and exchange chat/control traffic, but cannot play a compatible game.

Mirror Shift also deliberately rejects inherited version 1 chess save records.
There is no automatic chess-to-Lattice conversion.

`RESIGN` remains wire-compatible with older peers because its acknowledgement
and the following rematch use existing payloads. A current peer automatically
sends or accepts the post-resignation `RESET`; an older peer may still expose
its legacy RESET decision prompt, so automatic rematch is guaranteed only when
both peers implement the current session contract.
