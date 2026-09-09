# Changelog

User-visible changes to Mirror Shift are documented here. Developer and
maintenance details live under [`docs/`](docs/README.md). The inherited
Shatranj chronology and internal conversion history remain in the private
development repository.

---

## [Unreleased]

## [1.0] - 2026-09-09

This is the cumulative change set for the first Mirror Shift release.

### Added

- Shared Reversi rules for every client: legal placements, captures in all
  directions, automatic Silence, end-of-game scoring, and PARADOX ties.
- Lattice board, Alignment placements, Mirrorlock captures, Echoes chat, and
  Mirror Shift artwork, title screens, icons, and About presentation.
- Mirror Shift version 2 saved-game format and `.MSH` files.
- Classic TAP with companion OVL and DAT files.
- Self-contained Spectrum Next NEX with full-colour tiles and hardware sprites.
- Native SpectraNext edition with cartridge networking, XFS storage, UTC clock
  support, and resource installation.
- Three disc sizes, board themes, rotation, legal-move hints, persistent
  configuration, paginated file selection, and saved clocks on Spectrum.
- Shared Qt client for Windows, macOS, and Linux with placement, chat, move
  history, clocks, connection logging, save/load slots, and remembered settings.

### Changed

- Establish `github.com/IgnacioMonge/MirrorShift` as the canonical public
  repository while retaining the development remote for rollback.
- Refer public setup guidance to stable firmware without pinning development
  firmware versions in the README files.
- Present the first release with three-column platform galleries, consistent
  screenshot framing, and the correct SpectraNext resource installation flow.
- Replace inherited chess rules, moves, pieces, and presentation with Reversi
  while retaining the Shatranj session, transport, and desktop foundation.
- Use the standard Reversi opening with black moving first.
- Distribute Spectrum Next as a self-contained NEX instead of the obsolete
  Next TAP route.
- Keep the dark Next cursor visible over occupied squares and extend the
  Spectrum turn shine to up to three discs at the existing cadence.
- Reduce redundant setup and game-start repainting, including restoring ALIGN
  from explicit visibility transitions instead of a blank-pixel probe.
- Reduce Classic/Next UART storage with status-only readiness and defensive
  reads. Classic initialization delegates draining to the transport's existing
  flush; RX pumping after each TX attempt is preserved.
- Refresh the managed ZXESPEmu launchers for the current Classic, Next, and SpectraNext builds.
- Keep release validation compatible with the z88dk 2.4 library layout and
  Qt 6.10 deployment tooling used by GitHub Actions.

### Removed

- Remove inherited chess gameplay, piece assets, notation, and version 1 chess
  save compatibility; incompatible saves fail closed.
- Remove negotiated DRAW requests while retaining naturally tied PARADOX games.

### Fixed

- Preserve the canonical Reality A/B face order after changing the Spectrum
  piece set, including the WOF seed placement on Next and both orientations.
- Keep Qt legal-move hints, dialogs, and board effects synchronized across
  rejected moves, game completion, reset, restore, takeback, and rapid play.
- Preserve separate Direct and MQTT ports in the Qt client, gate save/load
  commands correctly, report inactive games accurately, require Qt 6, and point
  every About view to `github.com/IgnacioMonge/MirrorShift`.
- Ease the Qt 3D disc conversion into both endpoints, removing its abrupt
  midpoint flash, final angular jump, and mismatched final repaint.
- Clear stale ZX and SpectraNext disc pixels after an opponent disconnects by
  restoring NetChessZX's independent cursor and bitmap-clear render modes.
- Separate Direct receive and transmit storage and correct fragmented input,
  readable-data draining on disconnect, timeouts, cancellation, and recovery.
- Correct MQTT action batches, host recovery, peer-loss handling, and retries.
- Reject malformed ACK/NACK without renewing Direct liveness and validate
  supported control-message reasons consistently across targets.
- Prevent incoming TAKEBACK prompts from entering the outgoing retry path or
  being consumed by unrelated acknowledgements.
- Preserve timers and move authors when saving, restoring, or applying a forced
  Silence; require peer approval and matching host-side assignment on restore.
- Reject truncated Spectrum saves and saves with trailing data.
- Correct `.MSH` paths, paginated file selection, descriptor-zero handling,
  and first-time Classic/Next configuration saves that previously reported
  `SAVE FAILED E1`.
- Preserve RTC mode across Next firmware calls and correct UTC/FAT conversion,
  the year 2100 boundary, and timezone or midnight rollover outside 1980–2107.
- Preserve Next and SpectraNext paging, interrupt, IX/IY, and nested-overlay
  contracts across cartridge and MMU calls.
- Correct disc-set morphing, board rotation parity, Next overlay nesting,
  cursor/selection visibility over discs, and setup-row restoration.
- Include product assets, dependency notices, licence texts, and matching
  source material in the packaging flow.

[Unreleased]: https://github.com/IgnacioMonge/MirrorShift/compare/mirrorshift-v1.0...HEAD
[1.0]: https://github.com/IgnacioMonge/MirrorShift/releases/tag/mirrorshift-v1.0
