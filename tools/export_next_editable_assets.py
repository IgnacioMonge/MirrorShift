#!/usr/bin/env python3
"""Export the current Next-native binaries as protected editable PNG sources."""

from __future__ import annotations

import argparse
from pathlib import Path

from PIL import Image


ROOT = Path(__file__).resolve().parents[1]
SPRITES = ROOT / "assets/next/checker_piece_sprites.bin"
SPRITE_PALETTE = ROOT / "assets/next/checker_sprite_palette.bin"
ABOUT = ROOT / "assets/next/about_screen.nxi"
OUT = ROOT / "assets/editable/next"

SETS = ("BW-L", "BW-M", "BW-S")
BOARD_THEMES = ("black-and-white", "blue3", "green", "brown", "wood")
SIDES = ("reality-a", "reality-b")
MARKERS = (("hint", 0), ("cursor", 1), ("selected", 3))
TRANSPARENT = 0xE3


def rgb333_component(value: int) -> int:
    return round(value * 255 / 7)


def decode_palette(raw: bytes) -> list[tuple[int, int, int, int]]:
    if len(raw) % 2:
        raise SystemExit("palette has an odd byte count")
    result = []
    for first, second in zip(raw[0::2], raw[1::2]):
        red = first >> 5
        green = (first >> 2) & 7
        blue = ((first & 3) << 1) | (second & 1)
        result.append(
            (
                rgb333_component(red),
                rgb333_component(green),
                rgb333_component(blue),
                255,
            )
        )
    return result


def write_png(path: Path, image: Image.Image, force: bool) -> None:
    if path.exists() and not force:
        raise SystemExit(f"refusing to overwrite editable source: {path}")
    path.parent.mkdir(parents=True, exist_ok=True)
    image.save(path)


def sprite_image(
    pattern: bytes, palette: list[tuple[int, int, int, int]]
) -> Image.Image:
    if len(pattern) != 256:
        raise SystemExit("sprite pattern is not 256 bytes")
    pixels = []
    for index in pattern:
        if index == TRANSPARENT:
            pixels.append((0, 0, 0, 0))
        elif index < len(palette):
            pixels.append(palette[index])
        else:
            raise SystemExit(f"sprite palette index {index} is out of range")
    image = Image.new("RGBA", (16, 16))
    image.putdata(pixels)
    return image


def export_sprites(force: bool) -> None:
    raw = SPRITES.read_bytes()
    if len(raw) != 50 * 256:
        raise SystemExit(f"{SPRITES}: expected 12800 bytes, got {len(raw)}")
    palette = decode_palette(SPRITE_PALETTE.read_bytes()[: 160 * 2])
    patterns = [raw[offset : offset + 256] for offset in range(0, len(raw), 256)]

    for set_index, set_name in enumerate(SETS):
        for side_index, side in enumerate(SIDES):
            pattern = patterns[set_index * 12 + side_index]
            write_png(
                OUT / "pieces" / set_name / f"{side}.png",
                sprite_image(pattern, palette),
                force,
            )

    for theme_index, theme in enumerate(BOARD_THEMES):
        for parity, face in enumerate(("light", "dark")):
            pattern = patterns[36 + theme_index * 2 + parity]
            write_png(
                OUT / "boards" / f"{theme}-{face}.png",
                sprite_image(pattern, palette),
                force,
            )

    for marker, pattern_index in MARKERS:
        write_png(
            OUT / "markers" / f"{marker}.png",
            sprite_image(patterns[46 + pattern_index], palette),
            force,
        )


def export_about(force: bool) -> None:
    raw = ABOUT.read_bytes()
    if len(raw) != 512 + 256 * 192:
        raise SystemExit(f"{ABOUT}: expected 49664 bytes, got {len(raw)}")
    palette = decode_palette(raw[:512])
    image = Image.new("RGBA", (256, 192))
    image.putdata([palette[index] for index in raw[512:]])
    write_png(OUT / "about.png", image, force)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--force",
        action="store_true",
        help="overwrite existing editable PNGs (never used by the normal build)",
    )
    args = parser.parse_args()
    export_sprites(args.force)
    export_about(args.force)
    print(f"[OK] exported 20 editable Next PNGs under {OUT}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
