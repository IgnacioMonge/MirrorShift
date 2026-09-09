#!/usr/bin/env python3
"""Focused host checks for the Next extension-bank and graphics layout."""

from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))

from gen_next_nex import (  # noqa: E402
    BANK_SIZE,
    BundleRegion,
    EXTENSION_TABLE_SIZE,
    append_bundle_region,
    render_bundle,
    validate_graphics_bank,
)


def numeric(text: str, name: str, operator: str) -> int:
    match = re.search(
        rf"(?m)^{re.escape(name)}\s+{re.escape(operator)}\s+(0x[0-9A-Fa-f]+|[0-9]+)\s*$",
        text,
    )
    if not match:
        raise AssertionError(f"{name} not found")
    return int(match.group(1), 0)


def expect_rejected(blob: bytes, offset: int, origin: int, used=()) -> None:
    try:
        validate_graphics_bank(blob, offset, origin, list(used))
    except SystemExit:
        return
    raise AssertionError("invalid extension-bank layout accepted")


def main() -> int:
    from test_next_cursor import check_cursor
    check_cursor(ROOT)
    regions = [BundleRegion("first", 0, b"AB")]
    append_bundle_region(
        regions, BundleRegion("second", 4, b"CD"), "unexpected overlap"
    )
    assert render_bundle(regions, 8) == b"AB\0\0CD\0\0"
    try:
        append_bundle_region(
            regions, BundleRegion("overlap", 1, b"XX"), "expected overlap"
        )
    except SystemExit as exc:
        assert str(exc) == "expected overlap"
    else:
        raise AssertionError("overlapping bundle region accepted")

    origin = 0x2000
    blob = bytearray(256)
    for index in range(EXTENSION_TABLE_SIZE // 3):
        base = index * 3
        blob[base] = 0xC3
        blob[base + 1 : base + 3] = (origin + EXTENSION_TABLE_SIZE).to_bytes(
            2, "little"
        )
    assert validate_graphics_bank(bytes(blob), 0, origin, [(8192, 9000)]) == 256
    expect_rejected(bytes(blob), 1, origin)
    expect_rejected(bytes(blob), 8186, 0x3FFA)
    expect_rejected(bytes(blob), 0, origin, [(100, 256)])
    bad_entry = bytearray(blob)
    bad_entry[3] = 0
    expect_rejected(bytes(bad_entry), 0, origin)

    extension = (ROOT / "asm/next/extension_bank_next.asm").read_text(
        encoding="utf-8"
    )
    layout = (ROOT / "asm/next/extension_bank_layout.asm").read_text(
        encoding="utf-8"
    )
    stubs = "\n".join(
        (ROOT / path).read_text(encoding="utf-8")
        for path in (
            "asm/next/screen_extension_stubs_top.asm",
            "asm/next/screen_extension_stubs_tail.asm",
        )
    )
    loader = (ROOT / "asm/next/overlay_loader_next.asm").read_text(encoding="utf-8")
    uart = (ROOT / "asm/uart/next_uart.asm").read_text(encoding="utf-8")
    makefile = (ROOT / "Makefile").read_text(encoding="utf-8")

    layout_origin = numeric(layout, "next_extension_org", "EQU")
    layout_page = numeric(layout, "next_extension_page", "EQU")
    layout_limit = numeric(layout, "next_extension_code_limit", "EQU")
    layout_table = numeric(layout, "next_extension_table_size", "EQU")
    make_offset = numeric(makefile, "NEXT_GRAPHICS_BANK_OFFSET", ":=")
    make_origin = numeric(makefile, "NEXT_GRAPHICS_BANK_ORG", ":=")
    make_limit = numeric(makefile, "NEXT_GRAPHICS_BANK_LIMIT", ":=")
    assert make_offset == 0
    assert layout_origin == make_origin == 0x2000
    assert layout_page == 32
    assert make_limit == layout_limit - layout_origin == 5376
    assert layout_table == EXTENSION_TABLE_SIZE
    assert numeric(layout, "next_sprite_source_pattern_count", "EQU") == 12
    assert numeric(layout, "next_sprite_pattern_count", "EQU") == 28
    assert numeric(layout, "next_capture_sprite_pattern_base", "EQU") == 28
    assert numeric(layout, "next_capture_sprite_pattern_count", "EQU") == 14
    assert numeric(layout, "next_common_sprite_pattern_base", "EQU") == 42
    assert numeric(layout, "next_marker_sprite_pattern_base", "EQU") == 52
    assert numeric(layout, "next_piece_final_pattern_base", "EQU") == 56
    assert numeric(layout, "next_piece_flash_pattern_base", "EQU") == 58
    assert numeric(layout, "next_piece_cursor_pattern_base", "EQU") == 60
    assert numeric(layout, "next_piece_cursor_pattern_count", "EQU") == 4
    assert numeric(layout, "next_sprite_pal_offset", "EQU") == 32768
    assert numeric(layout, "next_bundle_dat_offset", "EQU") == 8192
    assert numeric(layout, "next_about_pal_offset", "EQU") == 33792
    assert numeric(makefile, "NEXT_RAW_BANK_BASE", ":=") == 16
    assert numeric(makefile, "NEXT_MAX_BUNDLE_BANKS", ":=") == 4
    assert "NEXT_RESIDENT_ASM := asm/next/graphics_bank_next.asm" in makefile
    assert "graphics_bank_next.asm" not in extension

    assert not re.search(r"(?mi)^\s*ei(?:\s|$)", extension)
    assert not re.search(r"(?mi)^\s*rst(?:\s|$)", extension)
    assert stubs.count("call next_extension_restore") == EXTENSION_TABLE_SIZE // 3
    assert not re.search(r"(?m)^\s*[^;\n]*:\s+jp\s+next_ext_", stubs)
    assert "DEFB 0xed, 0x91, next_mmu_slot1, next_extension_page" in loader
    for resident in (loader, uart):
        assert not re.search(r"(?mi)^\s*ld\s+a\s*,\s*i(?:\s|$)", resident)
    hard_reset = uart.split("_net_uart_hard_reset:", 1)[1].split(
        "_net_uart_set_baud_230400:", 1
    )[0]
    assert re.search(r"(?mi)^\s*di\s*$\s*^\s*im\s+1\s*$", hard_reset)
    assert hard_reset.lower().index("im 1") < hard_reset.lower().index("ei")
    assert numeric(layout, "next_about_bank", "EQU") == (
        numeric(makefile, "NEXT_RAW_BANK_BASE", ":=")
        + numeric(makefile, "NEXT_ABOUT_PIXELS_OFFSET", ":=") // BANK_SIZE
    )
    print("Next extension-bank and Mirror graphics layout ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
