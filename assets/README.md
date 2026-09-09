# Mirror Shift checker assets

`assets/editable/checkers/BW-{S,M,L}/reality-{a,b}.png` are the approved
Spectrum silhouettes. They are immutable build inputs: 16x16 RGBA, hard alpha,
no scaling or antialiasing.

Classic uses `assets/spectrum/checker_pieces_16x16.asm`, generated directly
from those six alpha masks by `tools/build_checker_sets.py`. The inherited ABI
still exposes 3 sets of 12 physical slots; the six old chess slots repeat each
set's exact 32-byte A/B pair.

Each Classic set also owns seven editable reflection frames under
`assets/editable/checkers/BW-{S,M,L}/reflection/`. The same generator packs
their exact per-pixel deltas into one 32-byte stream per set at
`assets/spectrum/checker_reflections.bin`.

Next uses its own full-colour sources under
`assets/editable/next/{pieces,boards,markers}`. The build generates
`assets/next/checker_piece_sprites.bin`, `checker_sprite_palette.bin`, and the
matching metadata; pattern count and bank layout remain fixed.

## Classic About

`assets/spectrum/about_classic.scr` is the approved 6912-byte SCREEN$ source.
`tools/make_about_board.py` extracts the 256x144 live ABOUT band and adds the
four credit lines at the bottom; `assets/spectrum/about_board.bin` is the
generated 5184-byte pixels-plus-attributes payload streamed from the DAT file.

## Qt branding

`assets/pc-client/mirrorshift-wordmark.png` is the project-supplied transparent
wordmark used by the Qt `AppBanner`. It remains an external deployed asset so
the executable does not absorb the 252 KB source image.

`assets/pc-client/mirrorshift-icon.png` is the deployed board-piece icon used by
the Qt client.

`assets/pc-client/about/mirrorshift-about.png` is the project-supplied artwork
shown by the Qt About dialog. It is deployed beside the wordmark rather than
embedded in the executable.

## Next About

`assets/next/about_screen.nxi` is the final project-supplied 256x192 Layer 2
screen: 512 bytes of RGB333 palette followed by 49152 indexed pixels. The NEX
pipeline packs it directly and must not regenerate or overpaint its pixels.
