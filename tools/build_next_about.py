#!/usr/bin/env python3
"""Build the final 256x192 RGB333 Next Layer 2 About image from PNG."""

from __future__ import annotations

from pathlib import Path

from PIL import Image


ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "assets/editable/next/about.png"
OUTPUT = ROOT / "assets/next/about_screen.nxi"


def component3(value: int) -> int:
    level = round(value * 7 / 255)
    if round(level * 255 / 7) != value:
        raise SystemExit(
            f"{SOURCE}: RGB component {value} is not exactly representable in RGB333"
        )
    return level


def rgb333_pair(rgb: tuple[int, int, int]) -> tuple[int, int]:
    red, green, blue = (component3(value) for value in rgb)
    return ((red << 5) | (green << 2) | (blue >> 1), blue & 1)


def main() -> int:
    with Image.open(SOURCE) as source:
        image = source.convert("RGBA")
    if image.size != (256, 192):
        raise SystemExit(f"{SOURCE}: got {image.size}, expected 256x192")
    pixels = list(image.getdata())
    if any(alpha != 255 for _red, _green, _blue, alpha in pixels):
        raise SystemExit(f"{SOURCE}: About must be fully opaque")

    palette: list[tuple[int, int, int]] = []
    indexes: dict[tuple[int, int, int], int] = {}
    encoded = bytearray()
    for red, green, blue, _alpha in pixels:
        rgb = (red, green, blue)
        rgb333_pair(rgb)
        if rgb not in indexes:
            if len(palette) == 256:
                raise SystemExit(f"{SOURCE}: more than 256 colours")
            indexes[rgb] = len(palette)
            palette.append(rgb)
        encoded.append(indexes[rgb])

    palette_bytes = bytes(
        value for rgb in palette for value in rgb333_pair(rgb)
    ).ljust(512, b"\x00")
    OUTPUT.write_bytes(palette_bytes + encoded)
    print(f"[OK] {OUTPUT}: 49664 bytes; {len(palette)} RGB333 colours")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
