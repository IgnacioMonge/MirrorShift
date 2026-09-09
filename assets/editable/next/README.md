# Editable ZX Spectrum Next graphics

These PNGs are the canonical editable sources for the Next-native graphics.
The normal build reads them directly and regenerates the binary files under
`assets/next/`; never edit those generated binaries by hand.

## Layout

- `pieces/BW-{L,M,S}/reality-{a,b}.png`: six coloured 16x16 discs; every board
  theme uses the same selected L/M/S pair.
- `boards/{theme}-{light,dark}.png`: ten original opaque 16x16 Mirror Shift
  board tiles. Recreate them with `tools/generate_original_next_board_tiles.py`.
- `markers/{hint,cursor,selected}.png`: three canonical 16x16 marker sprites.
  Runtime pattern 2 is generated as `cursor + hint`; it is not a fourth source.
  Two duplicated piece pairs also carry the cursor/selection composited over
  each disc, so occupied squares retain both the piece and the marker.
- `about.png`: the opaque 256x192 Layer 2 About screen.

All visible colours must be exact RGB333 values. Use only these 8-bit channel
levels: `0, 36, 73, 109, 146, 182, 219, 255`. Sprite transparency must be hard
alpha (`0` or `255`), with no antialiasing or partial transparency.

Next piece silhouettes are independent from the corresponding Classic
`assets/editable/checkers/BW-*` masks. The Next setup morph is regenerated
from these full-colour PNGs on every build; the gameplay capture turn is
generated from the same selected pair and played backwards for B-to-A. Edit
colour, material and silhouette here. White-disc rows must remain filled
between their leftmost and rightmost opaque pixels so an accidental transparent
hole cannot expose the board.

Run `make next-assets` after editing. The import fails closed on invalid size,
alpha, RGB333 colour, silhouette, palette count, or binary budget.

`tools/export_next_editable_assets.py` is only a protected bootstrap/recovery
tool. It refuses to overwrite these sources unless explicitly passed `--force`.
