# Size report

`make size-report` rebuilds the Spectrum artifacts and writes
`build/size_report.json`.

`make nex-size-report` reports the self-contained Next release. Next is shipped
only as the paged NEX; there is no legacy Next-UART TAP target.

`make size-baseline` writes `docs/size_report.baseline.json` after a known-good
build. Commit that baseline before shrink work.

The tracked baseline is a set of independently maintained guardrails, not
necessarily one coherent build snapshot. Do not sum its per-overlay ceilings
or regenerate the whole file unless deliberately establishing a new baseline.

`make size-check` compares the current report with the historical baseline and
always enforces the absolute Classic limits in `z80opt.toml`. It reports
baseline deltas without failing on growth by default; pass
`SIZE_CHECK_FLAGS=--fail-on-growth` when a branch is expected to be
size-neutral or smaller. `make nex-size-report` and `make tap-spectranext`
likewise enforce their target-specific `z80opt.toml` limits.

The `[build].command` in `z80opt.toml` is the stricter optimization gate:
it additionally requires no growth against the retained JSON guardrails.
It is not the release acceptance command. `make full-check` uses the absolute
policy limits and prints historical deltas; a release PASS does not mean
zero growth against every historical JSON value. Do not raise either reference
merely to accept growth.
