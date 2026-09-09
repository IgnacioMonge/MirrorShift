#!/usr/bin/env python3
"""Build Next sprite patterns directly from canonical editable RGB333 PNGs.

Layout (fixed sizes, ABI-stable):
  - 3 piece sets (L/M/S): each 12 patterns of 256 bytes
      patterns 0..1  = the editable Reality A/B pair
      patterns 2..5  = A/B with cursor, then A/B with selection
      patterns 6..9  = compatibility copies of the unmarked pair
      patterns 10..11 = A/B with a one-pixel outer flash halo
  - 10 board tiles (5 themes × light/dark) with 1px bevel
  - 4 markers: modest hint dot, cursor, last, selected

The cold Next graphics bank reads the first A/B pair and generates setup and
capture animation patterns; board theme does not select a different disc.
"""

from __future__ import annotations

import json
from pathlib import Path

from PIL import Image


def image_pixels(image: Image.Image):
    return (
        image.get_flattened_data()
        if hasattr(image, "get_flattened_data")
        else image.getdata()
    )


ROOT = Path(__file__).resolve().parents[1]
SELECTED_BOARDS = ROOT / "assets/lichess/selected_next_boards.json"
EDITABLE_ROOT = ROOT / "assets/editable/next"
OUT_DIR = ROOT / "assets/next"
OUT_BIN = OUT_DIR / "checker_piece_sprites.bin"
OUT_PALETTE_BIN = OUT_DIR / "checker_sprite_palette.bin"
OUT_META = OUT_DIR / "checker_piece_sprites.json"

SPRITE_SIZE = 16
ALPHA_THRESHOLD = 128
TRANSPARENT = 0xE3
PALETTE_LIMIT = 160
SPRITE_PALETTE_ENTRIES = 256
SHIMMER_ENTRY_OFFSET = 4
SHIMMER_EXIT_OFFSET = 12
SHIMMER_ENTRY_SAMPLE = (6, 3)
SHIMMER_EXIT_SAMPLE = (10, 9)

STANDARD_ULA_PALETTE = bytes((
    0x00, 0x00, 0x02, 0x01, 0xA0, 0x00, 0xA2, 0x01,
    0x14, 0x00, 0x16, 0x01, 0xB4, 0x00, 0xB6, 0x01,
    0x00, 0x00, 0x02, 0x01, 0xA0, 0x00, 0xA2, 0x01,
    0x14, 0x00, 0x16, 0x01, 0xB4, 0x00, 0xB6, 0x01,
    0x00, 0x00, 0x03, 0x01, 0xE0, 0x00, 0xE3, 0x01,
    0x1C, 0x00, 0x1F, 0x01, 0xFC, 0x00, 0xFF, 0x01,
    0x00, 0x00, 0x03, 0x01, 0xE0, 0x00, 0xE3, 0x01,
    0x1C, 0x00, 0x1F, 0x01, 0xFC, 0x00, 0xFF, 0x01,
    # Group 2 indices 3..7: brighter light/dark pairs for focused swatches.
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xDF, 0x01,
    0xFF, 0x01, 0xFF, 0x01, 0xFF, 0x01, 0xFF, 0x01,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xB7, 0x00,
    0xBB, 0x01, 0x96, 0x01, 0xB3, 0x00, 0xB2, 0x00,
    # Group 3 indices 0..4: dominant light/dark colours from each tile pair.
    0xBB, 0x00, 0xDF, 0x01, 0xDF, 0x00, 0xDB, 0x01,
    0xFB, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x92, 0x01, 0x97, 0x00, 0x72, 0x00, 0x8E, 0x01,
    0x8D, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
))

PIPELINE_VERSION = 13
PIECE_SETS = 3
PIECES_PER_SET = 12
BOARD_THEMES = 5
BOARD_TILES_PER_THEME = 2
MARKER_PATTERNS = 4
BOARD_PATTERN_BASE = PIECES_PER_SET
MARKER_PATTERN_BASE = BOARD_PATTERN_BASE + BOARD_THEMES * BOARD_TILES_PER_THEME

# Disc diameters in pixels (matches BW-L/M/S product names).
PIECE_VARIANTS = (
    ("obs-large", 14, "BW-L"),
    ("obs-medium", 12, "BW-M"),
    ("obs-small", 10, "BW-S"),
)
PIECE_SLOTS = [name for name, _size, _source in PIECE_VARIANTS]
THEME_DIRS = {
    "black&white": "black-and-white",
    "bw": "black-and-white",
    "blue3": "blue3",
    "green": "green",
    "brown": "brown",
    "wood": "wood",
}


def load_selected_boards() -> list[str]:
    data = json.loads(SELECTED_BOARDS.read_text(encoding="utf-8"))
    boards = list(data.get("selected", []))
    if len(boards) != BOARD_THEMES:
        raise SystemExit(
            f"{SELECTED_BOARDS}: expected exactly {BOARD_THEMES} selected boards"
        )
    for name in boards:
        if name not in THEME_DIRS:
            raise SystemExit(f"unsupported editable Next board theme: {name}")
    return boards


def rgb333_level(value: int, path: Path) -> int:
    level = round(value * 7 / 255)
    if round(level * 255 / 7) != value:
        raise SystemExit(
            f"{path}: RGB component {value} is not exactly representable in RGB333"
        )
    return level


def load_editable(path: Path, opaque: bool = False) -> Image.Image:
    with Image.open(path) as source:
        image = source.convert("RGBA")
    if image.size != (SPRITE_SIZE, SPRITE_SIZE):
        raise SystemExit(f"{path}: got {image.size}, expected 16x16")
    alphas = {alpha for _red, _green, _blue, alpha in image_pixels(image)}
    if opaque:
        if alphas != {255}:
            raise SystemExit(f"{path}: board tiles must be fully opaque")
    elif alphas - {0, 255}:
        raise SystemExit(f"{path}: alpha must be hard 0 or 255")
    for red, green, blue, alpha in image_pixels(image):
        if alpha:
            rgb333_level(red, path)
            rgb333_level(green, path)
            rgb333_level(blue, path)
    return image


def collect_board_tiles(boards: list[str]) -> dict[tuple[str, int], Image.Image]:
    tiles: dict[tuple[str, int], Image.Image] = {}
    for name in boards:
        theme = THEME_DIRS[name]
        for parity, face in enumerate(("light", "dark")):
            path = EDITABLE_ROOT / "boards" / f"{theme}-{face}.png"
            tiles[(name, parity)] = load_editable(path, opaque=True)
    return tiles


def collect_piece_images(sets: list[str]) -> dict[tuple[str, str], Image.Image]:
    """Load one editable A/B pair for each piece set."""
    images: dict[tuple[str, str], Image.Image] = {}
    variants = {name: source for name, _diameter, source in PIECE_VARIANTS}
    for set_name in sets:
        source = variants[set_name]
        for side, face in (("w", "a"), ("b", "b")):
            path = EDITABLE_ROOT / "pieces" / source / f"reality-{face}.png"
            images[(set_name, side)] = load_editable(path)
    return images


def build_marker_patterns() -> list[Image.Image]:
    """0=hint, 1=cursor, 2=hint+cursor, 3=selected."""
    hint = load_editable(EDITABLE_ROOT / "markers/hint.png")
    cursor = load_editable(EDITABLE_ROOT / "markers/cursor.png")
    selected = load_editable(EDITABLE_ROOT / "markers/selected.png")
    combined = Image.alpha_composite(cursor, hint)
    return [hint, cursor, combined, selected]


def visible_pixels(images) -> list[tuple[int, int, int]]:
    pixels: list[tuple[int, int, int]] = []
    for image in images:
        for r, g, b, a in image_pixels(image):
            if a >= ALPHA_THRESHOLD:
                pixels.append((r, g, b))
    if not pixels:
        raise SystemExit("no visible pixels in rendered sprites")
    return pixels


def build_palette(pixels: list[tuple[int, int, int]]) -> list[tuple[int, int, int]]:
    palette = list(dict.fromkeys(pixels))
    if len(palette) > PALETTE_LIMIT:
        raise SystemExit(
            f"editable Next sprites use {len(palette)} colours; limit is {PALETTE_LIMIT}"
        )
    return palette


def nearest_index(
    rgb: tuple[int, int, int], palette: list[tuple[int, int, int]]
) -> int:
    r, g, b = rgb
    best_i = 0
    best_d = 1 << 30
    for i, (pr, pg, pb) in enumerate(palette):
        d = (r - pr) * (r - pr) + (g - pg) * (g - pg) + (b - pb) * (b - pb)
        if d < best_d:
            best_i = i
            best_d = d
    return best_i


def encode_pattern(image: Image.Image, palette: list[tuple[int, int, int]]) -> bytes:
    out = bytearray()
    for r, g, b, a in image_pixels(image):
        out.append(
            TRANSPARENT if a < ALPHA_THRESHOLD else nearest_index((r, g, b), palette)
        )
    if len(out) != SPRITE_SIZE * SPRITE_SIZE:
        raise SystemExit("bad sprite size")
    return bytes(out)


def encode_flash_outline(
    image: Image.Image, palette: list[tuple[int, int, int]]
) -> bytes:
    """Preserve the source and add an 8-connected, one-pixel outer halo."""
    pixels = list(image_pixels(image))
    out = bytearray()
    for y in range(SPRITE_SIZE):
        for x in range(SPRITE_SIZE):
            red, green, blue, alpha = pixels[y * SPRITE_SIZE + x]
            if alpha >= ALPHA_THRESHOLD:
                out.append(nearest_index((red, green, blue), palette))
                continue
            neighbours = (
                pixels[ny * SPRITE_SIZE + nx][3]
                for ny in range(max(0, y - 1), min(SPRITE_SIZE, y + 2))
                for nx in range(max(0, x - 1), min(SPRITE_SIZE, x + 2))
                if nx != x or ny != y
            )
            out.append(
                PALETTE_LIMIT - 1
                if any(value >= ALPHA_THRESHOLD for value in neighbours)
                else TRANSPARENT
            )
    return bytes(out)


def rgb333_pair(rgb: tuple[int, int, int]) -> tuple[int, int]:
    r, g, b = (round(c * 7 / 255) for c in rgb)
    return ((r << 5) | (g << 2) | (b >> 1), b & 1)


def shimmer_colour(rgb: tuple[int, int, int]) -> tuple[int, int, int]:
    """Lift one RGB333 step; cool pure white so its entry phase remains visible."""
    levels = [round(component * 7 / 255) for component in rgb]
    if levels == [7, 7, 7]:
        levels = [6, 7, 7]
    else:
        levels = [min(7, level + 1) for level in levels]
    return tuple(round(level * 255 / 7) for level in levels)


def emit_palette(
    palette: list[tuple[int, int, int]],
    piece_images: dict[tuple[str, str], Image.Image],
) -> None:
    values = [byte for rgb in palette for byte in rgb333_pair(rgb)]
    raw = bytearray(SPRITE_PALETTE_ENTRIES * 2)
    raw[: len(values)] = bytes(values)
    entry_colours = {
        image.getpixel(SHIMMER_ENTRY_SAMPLE)[:3] for image in piece_images.values()
    }
    exit_colours = {
        image.getpixel(SHIMMER_EXIT_SAMPLE)[:3] for image in piece_images.values()
    }
    for offset, active_colours in (
        (SHIMMER_ENTRY_OFFSET, entry_colours),
        (SHIMMER_EXIT_OFFSET, exit_colours),
    ):
        base = offset * 16
        for index, rgb in enumerate(palette):
            phase_rgb = shimmer_colour(rgb) if rgb in active_colours else rgb
            position = (base + index) * 2
            raw[position : position + 2] = bytes(rgb333_pair(phase_rgb))
    # Entry 159 is the runtime theme-flash colour. Boot defaults to RGB333
    # cyan; screen.asm rewrites this pair whenever the board theme changes.
    raw[318:320] = bytes(rgb333_pair((0, 255, 255)))
    OUT_PALETTE_BIN.write_bytes(bytes(raw) + STANDARD_ULA_PALETTE)


def pack_piece_set(
    set_name: str,
    piece_images: dict[tuple[str, str], Image.Image],
    palette: list[tuple[int, int, int]],
    markers: list[Image.Image],
) -> bytes:
    """Reuse two duplicated pairs for cursor/selection without growing the bank."""
    data = bytearray()
    for pair in range(BOARD_THEMES):
        for side in ("w", "b"):
            image = piece_images[(set_name, side)]
            if pair in (1, 2):
                image = Image.alpha_composite(image, markers[1 if pair == 1 else 3])
            data.extend(encode_pattern(image, palette))
    # Patterns 10-11 preserve the source pixels and add only the outer halo.
    data.extend(encode_flash_outline(piece_images[(set_name, "w")], palette))
    data.extend(encode_flash_outline(piece_images[(set_name, "b")], palette))
    expected = PIECES_PER_SET * SPRITE_SIZE * SPRITE_SIZE
    if len(data) != expected:
        raise SystemExit(f"set {set_name}: got {len(data)} bytes, expected {expected}")
    return bytes(data)


def main() -> int:
    sets = PIECE_SLOTS
    if len(sets) != PIECE_SETS:
        raise SystemExit(f"expected exactly {PIECE_SETS} piece sets")
    boards = load_selected_boards()
    piece_images = collect_piece_images(sets)
    board_tiles = collect_board_tiles(boards)
    marker_images = build_marker_patterns()

    for image in piece_images.values():
        if image.getpixel(SHIMMER_ENTRY_SAMPLE)[3] < ALPHA_THRESHOLD:
            raise SystemExit("Next shimmer entry sample must stay opaque")
        if image.getpixel(SHIMMER_EXIT_SAMPLE)[3] < ALPHA_THRESHOLD:
            raise SystemExit("Next shimmer exit sample must stay opaque")

    all_images = (
        [piece_images[(set_name, side)] for set_name in sets for side in ("w", "b")]
        + list(board_tiles.values())
        + marker_images
    )
    palette = build_palette(visible_pixels(all_images))
    OUT_DIR.mkdir(parents=True, exist_ok=True)

    data = bytearray()
    for set_name in sets:
        data.extend(pack_piece_set(set_name, piece_images, palette, marker_images))
    for board in boards:
        for parity in (0, 1):
            data.extend(encode_pattern(board_tiles[(board, parity)], palette))
    for image in marker_images:
        data.extend(encode_pattern(image, palette))

    expected_total = (
        PIECE_SETS * PIECES_PER_SET
        + BOARD_THEMES * BOARD_TILES_PER_THEME
        + MARKER_PATTERNS
    ) * SPRITE_SIZE * SPRITE_SIZE
    if len(data) != expected_total:
        raise SystemExit(f"bank size {len(data)} != expected {expected_total}")

    OUT_BIN.write_bytes(data)
    emit_palette(palette, piece_images)
    OUT_META.write_text(
        json.dumps(
            {
                "pipeline": PIPELINE_VERSION,
                "material": "editable-rgb333",
                "sets": [source for _name, _diameter, source in PIECE_VARIANTS],
                "boards": boards,
                "pieces_per_set": PIECES_PER_SET,
                "pattern_layout": "A,B,cursorA,cursorB,selectedA,selectedB,A,B,A,B,outlineA,outlineB",
                "editable_piece_sources": PIECE_SETS * 2,
                "bytes_per_pattern": SPRITE_SIZE * SPRITE_SIZE,
                "disc_diameters": {source: diameter for _name, diameter, source in PIECE_VARIANTS},
                "bytes_per_set": PIECES_PER_SET * SPRITE_SIZE * SPRITE_SIZE,
                "piece_sets": PIECE_SETS,
                "board_pattern_base": BOARD_PATTERN_BASE,
                "board_patterns": BOARD_THEMES * BOARD_TILES_PER_THEME,
                "marker_pattern_base": MARKER_PATTERN_BASE,
                "marker_patterns": MARKER_PATTERNS,
                "marker_pattern_layout": "hint,cursor,hint+cursor,selected",
                "total_bytes": len(data),
                "transparent_index": TRANSPARENT,
                "palette_entries": len(palette),
                "flash_palette_index": 159,
                "sprite_palette_entries": SPRITE_PALETTE_ENTRIES,
                "shimmer_mode": "per-sprite-palette-offset",
                "shimmer_entry_sample": list(SHIMMER_ENTRY_SAMPLE),
                "shimmer_exit_sample": list(SHIMMER_EXIT_SAMPLE),
                "shimmer_palette_offsets": {
                    "entry": SHIMMER_ENTRY_OFFSET,
                    "exit": SHIMMER_EXIT_OFFSET,
                },
                "source": "canonical-editable-next-rgb333-pngs",
            },
            indent=2,
        )
        + "\n",
        encoding="utf-8",
    )
    print(
        f"[OK] {OUT_BIN}: {len(data)} bytes; palette {len(palette)}; "
        f"{PIECE_SETS} shared editable piece sets"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
