# Editable checker PNGs

Canonical approved 16x16 transparent sources for all Spectrum piece sets:

- `BW-S`: 10-pixel disc.
- `BW-M`: 12-pixel disc.
- `BW-L`: 14-pixel disc.

Each directory contains `reality-a.png` (white outline) and
`reality-b.png` (black solid). Keep the canvas at 16x16, preserve transparency,
and use hard pixels without antialiasing for a bit-exact Classic import.

Classic imports only their hard 1-bpp alpha. Next retains its target palette
and material colouring but uses the same alpha silhouettes. The production
horizontal-axis animation also projects these masks directly.

Do not regenerate or overwrite these PNGs. `tools/build_checker_sets.py` reads
their alpha masks and emits the Classic A/B pairs.
