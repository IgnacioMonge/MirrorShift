#!/usr/bin/env python3
"""Build the Classic About ULA band from a SCREEN$ plus credit lines."""

from __future__ import annotations

import argparse
from pathlib import Path

if __package__:
    from .asm_data import parse_defb_block
else:
    from asm_data import parse_defb_block

try:
    from PIL import Image
except ModuleNotFoundError:
    Image = None


SCREEN_W_BYTES = 32
SCREEN_W = SCREEN_W_BYTES * 8
SCREEN_H = 192
SOURCE_TOP = 24
BAND_TOP = 32
BAND_H = 144
ATTR_ROWS = BAND_H // 8
ABOUT_BYTES = (SCREEN_W_BYTES * BAND_H) + (SCREEN_W_BYTES * ATTR_ROWS)
ATTR_WHITE = 0x07
ATTR_YELLOW = 0x06
IMAGE_SHIFT = 8
TEXT_TOP = 104
TEXT_LINES = [
    ("MIRRORSHIFT", ATTR_WHITE, 110),
    ("(C) 2026 M.I. MONGE GARCIA", ATTR_WHITE, 116),
    ("GITHUB.COM/IGNACIOMONGE/MIRRORSHIFT", ATTR_YELLOW, 122),
    ("LICENSE: GNU GPL V2.0", ATTR_YELLOW, 128),
]


def scr_bitmap_offset(y: int, col: int) -> int:
    third = y // 64
    y8 = y % 8
    row = (y // 8) % 8
    return (third * 2048) + (y8 * 256) + (row * 32) + col


def extract_band(scr: bytes) -> tuple[bytearray, bytearray]:
    if len(scr) != 6912:
        raise SystemExit(f"SCREEN$ must be 6912 bytes, got {len(scr)}")
    pixels = bytearray()
    for y in range(SOURCE_TOP, SOURCE_TOP + BAND_H):
        for col in range(SCREEN_W_BYTES):
            pixels.append(scr[scr_bitmap_offset(y, col)])
    attr_top = SOURCE_TOP // 8
    attrs = bytearray(
        scr[
            6144 + (attr_top * SCREEN_W_BYTES) :
            6144 + ((attr_top + ATTR_ROWS) * SCREEN_W_BYTES)
        ]
    )
    shift_bytes = IMAGE_SHIFT * SCREEN_W_BYTES
    pixels[shift_bytes:] = pixels[:-shift_bytes]
    pixels[:shift_bytes] = bytes(shift_bytes)
    attrs[SCREEN_W_BYTES:] = attrs[:-SCREEN_W_BYTES]
    attrs[:SCREEN_W_BYTES] = bytes(SCREEN_W_BYTES)
    return pixels, attrs


def text_origin_x(text: str) -> int:
    width = len(text) * 4
    if width > SCREEN_W:
        raise SystemExit(f"credit line '{text}' is {width}px, screen is {SCREEN_W}px")
    return (SCREEN_W - width) // 2


def clear_text_band(pixels: bytearray, attrs: bytearray) -> None:
    for y in range(TEXT_TOP, BAND_H):
        start = y * SCREEN_W_BYTES
        pixels[start : start + SCREEN_W_BYTES] = bytes(SCREEN_W_BYTES)
    attr_start = (TEXT_TOP // 8) * SCREEN_W_BYTES
    attrs[attr_start:] = bytes(len(attrs) - attr_start)


def put_ikkle_text(
    pixels: bytearray, font: bytes, attrs: bytearray, y: int, text: str, color: int
) -> None:
    x = text_origin_x(text)
    col0 = x // 8
    col1 = (x + len(text) * 4 - 1) // 8
    for attr_row in range(y // 8, ((y + 3) // 8) + 1):
        for col in range(col0, col1 + 1):
            attrs[attr_row * SCREEN_W_BYTES + col] = color
    for ch in text.upper():
        code = ord(ch)
        if 33 <= code < 128:
            if code >= 96:
                code -= 32
            index = (code - 32) * 2
            if index + 1 < len(font):
                rows = (
                    font[index] >> 4,
                    font[index] & 0x0F,
                    font[index + 1] >> 4,
                    font[index + 1] & 0x0F,
                )
                for row, bits in enumerate(rows):
                    for bit in range(4):
                        if bits & (0x08 >> bit):
                            px = x + bit
                            offset = (y + row) * SCREEN_W_BYTES + (px // 8)
                            pixels[offset] |= 0x80 >> (px & 7)
        x += 4


def build_board(source: Path, ui_assets: Path) -> bytes:
    pixels, attrs = extract_band(source.read_bytes())
    clear_text_band(pixels, attrs)
    font = parse_defb_block(
        ui_assets.read_text(encoding="ascii"),
        "timer_ikkle_packed",
        "font_packed",
    )
    for line, color, y in TEXT_LINES:
        put_ikkle_text(pixels, font, attrs, y, line, color)
    out = bytes(pixels + attrs)
    if len(out) != ABOUT_BYTES:
        raise SystemExit(f"bad board payload size: {len(out)}")
    return out


def band_preview(raw: bytes) -> Image.Image:
    pixels = raw[: SCREEN_W_BYTES * BAND_H]
    attrs = raw[SCREEN_W_BYTES * BAND_H :]
    colors = [
        (0, 0, 0), (0, 0, 192), (192, 0, 0), (192, 0, 192),
        (0, 192, 0), (0, 192, 192), (192, 192, 0), (192, 192, 192),
        (0, 0, 0), (0, 0, 255), (255, 0, 0), (255, 0, 255),
        (0, 255, 0), (0, 255, 255), (255, 255, 0), (255, 255, 255),
    ]
    image = Image.new("RGB", (SCREEN_W, BAND_H))
    output = image.load()
    for y in range(BAND_H):
        for col in range(SCREEN_W_BYTES):
            bits = pixels[y * SCREEN_W_BYTES + col]
            attr = attrs[(y // 8) * SCREEN_W_BYTES + col]
            ink = colors[(attr & 7) + (8 if attr & 0x40 else 0)]
            paper = colors[((attr >> 3) & 7) + (8 if attr & 0x40 else 0)]
            for bit in range(8):
                output[col * 8 + bit, y] = ink if bits & (0x80 >> bit) else paper
    return image


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("source", type=Path)
    parser.add_argument("ui_assets", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--preview", type=Path)
    args = parser.parse_args()

    payload = build_board(args.source, args.ui_assets)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(payload)
    print(f"[OK] {args.output}: {ABOUT_BYTES} bytes")
    if args.preview:
        if Image is None:
            raise SystemExit("--preview requires Pillow")
        args.preview.parent.mkdir(parents=True, exist_ok=True)
        band = band_preview(payload)
        band.resize((SCREEN_W * 2, BAND_H * 2), Image.Resampling.NEAREST).save(
            args.preview
        )
        screen = Image.new("RGB", (SCREEN_W, SCREEN_H), (0, 0, 0))
        screen.paste(band, (0, BAND_TOP))
        screen.resize((SCREEN_W * 2, SCREEN_H * 2), Image.Resampling.NEAREST).save(
            args.preview.with_name(args.preview.stem + "_screen.png")
        )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
