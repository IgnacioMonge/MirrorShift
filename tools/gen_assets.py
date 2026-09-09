#!/usr/bin/env python3
import re
import sys
from pathlib import Path

if __package__:
    from .asm_data import label_block as block_from_label
    from .asm_data import parse_defb_with_offsets as parse_defb
else:
    from asm_data import label_block as block_from_label
    from asm_data import parse_defb_with_offsets as parse_defb

EXPECTED_UI_BYTES = 812
VERSION_SLOT = 16
# Next board sprite themes: black&white, blue3, green, brown, wood.
# ULA attrs only tint the board frame; squares are covered by sprites.
NEXT_BOARD_MARK_INKS = [2, 2, 1, 0, 0]
NEXT_BOARD_LIGHT_ATTRS = [0x78, 0x6F, 0x66, 0x77, 0x37]
NEXT_BOARD_DARK_ATTRS = [0x07, 0x4D, 0x20, 0x56, 0x52]
PIECE_SET_BYTES = 64
PIECE_FACE_BYTES = 32
REFLECTION_SET_BYTES = 32
EXPECTED_SOURCE_PIECE_BYTES = PIECE_SET_BYTES * 3
EXPECTED_PIECE_BYTES = EXPECTED_SOURCE_PIECE_BYTES
EXPECTED_REFLECTION_BYTES = REFLECTION_SET_BYTES * 3
CLASSIC_RUNTIME_PIECE_BYTES = PIECE_SET_BYTES + REFLECTION_SET_BYTES
ABOUT_BOARD_WIDTH = 32
ABOUT_PIXEL_ROWS = 144
ABOUT_ATTR_ROWS = 18
ABOUT_PIXEL_BYTES = ABOUT_BOARD_WIDTH * ABOUT_PIXEL_ROWS
EXPECTED_ABOUT_BOARD_BYTES = ABOUT_PIXEL_BYTES + (ABOUT_BOARD_WIDTH * ABOUT_ATTR_ROWS)
EXPECTED_ABOUT_PAYLOAD_BYTES = EXPECTED_ABOUT_BOARD_BYTES
ABOUT_PAYLOAD_OFFSET = EXPECTED_UI_BYTES + CLASSIC_RUNTIME_PIECE_BYTES
ZX_EXTRA_PIECE_OFFSET = ABOUT_PAYLOAD_OFFSET + EXPECTED_ABOUT_PAYLOAD_BYTES
NEXT_EXTRA_PIECE_OFFSET = EXPECTED_UI_BYTES + PIECE_SET_BYTES
EXPECTED_ZX_DAT_BYTES = (
    EXPECTED_UI_BYTES
    + EXPECTED_PIECE_BYTES
    + EXPECTED_REFLECTION_BYTES
    + EXPECTED_ABOUT_PAYLOAD_BYTES
)
EXPECTED_NEXT_DAT_BYTES = EXPECTED_UI_BYTES + EXPECTED_PIECE_BYTES
EXPECTED_UI_OFFSETS = {
    "expand_2x": 0,
    "font_lut": 16,
    "chat_icon_white": 26,
    "chat_icon_black": 32,
    "timer_ikkle_packed": 38,
    "font_packed": 166,
    "badge_pattern": 454,
    "conn_pattern": 462,
    "rank_digit_patterns": 470,
    "file_letter_patterns": 510,
    "title_msg": 550,
    "chat_msg": 563,
    "session_setup_msg": 570,
    "game_setup_msg": 587,
    "preflight_setup_msg": 598,
    "input_prompt_msg": 620,
    "self_turn_msg": 623,
    "echo_turn_msg": 637,
    "self_silence_msg": 651,
    "echo_silence_msg": 664,
    "menu_cursor_masks": 677,
    "tab_label_file": 701,
    "tab_label_discc": 706,
    "tab_label_reset": 712,
    "tab_label_flip": 718,
    "tab_label_theme": 723,
    "tab_label_about": 729,
    "moves_echo_msg": 735,
    "moves_self_msg": 741,
    "banner_info_top_msg": 747,
    "board_theme_mark_inks": 781,
    "board_theme_light_attrs": 786,
    "board_theme_dark_attrs": 791,
    "version_banner_msg": 796,
}


def parse_screen_asset_equ(text):
    offsets = {}
    for raw in text.splitlines():
        line = raw.split(";", 1)[0].strip()
        match = re.match(
            r"^([A-Za-z_][A-Za-z0-9_]*)\s+EQU\s+NETCHESSZX_ASSET_BASE(?:\s+\+\s+([0-9]+))?$",
            line,
        )
        if match:
            offsets[match.group(1)] = int(match.group(2) or "0", 10)
    return offsets


def parse_loader_constant(text, name):
    match = re.search(rf"(?m)^{re.escape(name)}\s+EQU\s+([0-9]+)\s*$", text)
    if not match:
        raise SystemExit(f"{name} not found in overlay loader")
    return int(match.group(1), 10)


def parse_loader_asset_size(text, is_next=False):
    platform_name = "asset_load_size_next" if is_next else "asset_load_size_classic"
    if re.search(rf"(?m)^{platform_name}\s+EQU\s+[0-9]+\s*$", text):
        return parse_loader_constant(text, platform_name)
    return parse_loader_constant(text, "asset_load_size")


def parse_loader_about_size(text):
    return parse_loader_constant(text, "about_board_size")


def build_about_payload(raw):
    if len(raw) != EXPECTED_ABOUT_BOARD_BYTES:
        raise ValueError(
            f"About asset has {len(raw)} bytes, expected {EXPECTED_ABOUT_BOARD_BYTES}"
        )

    return raw


def validate_offsets(label, got, expected):
    for name, offset in expected.items():
        if got.get(name) != offset:
            raise SystemExit(
                f"{label}: {name} offset got {got.get(name)}, expected {offset}"
            )


def runtime_piece_sets(pieces, label):
    if len(pieces) != EXPECTED_SOURCE_PIECE_BYTES:
        raise SystemExit(
            f"{label}: got {len(pieces)} piece bytes, "
            f"expected {EXPECTED_SOURCE_PIECE_BYTES}"
        )
    return bytes(pieces)


def patch_slot(ui, offset, slot, text, label):
    data = text.encode("ascii") + b"\0"
    if len(data) > slot:
        raise SystemExit(f"{label}: '{text}' needs {len(data)} bytes, slot is {slot}")
    ui[offset : offset + slot] = data.ljust(slot, b"\0")


def main(argv):
    version = None
    is_next = False
    args = [argv[0]]
    it = iter(argv[1:])
    for arg in it:
        if arg == "--version":
            version = next(it, None)
            if version is None:
                raise SystemExit("--version requires a value")
        elif arg == "--next":
            is_next = True
        else:
            args.append(arg)
    argv = args
    if len(argv) != 7 or version is None:
        raise SystemExit(
            "usage: gen_assets.py <ui_assets.asm> <pieces.asm> "
            "<screen.asm> <overlay_loader.asm> <about_board.bin> <out.dat> "
            "--version <version> [--next]"
        )
    if re.fullmatch(
        r"[0-9]+\.[0-9]+(?:\.[0-9]+)?(?:-dev(?:[0-9]{3}|ESP))?", version
    ) is None:
        raise SystemExit(
            "version must contain major.minor[-devNNN|-devESP] or "
            "major.minor.patch[-devNNN|-devESP]"
        )

    ui_path = Path(argv[1])
    pieces_path = Path(argv[2])
    screen_path = Path(argv[3])
    loader_path = Path(argv[4])
    about_path = Path(argv[5])
    out_path = Path(argv[6])

    ui, ui_offsets = parse_defb(ui_path.read_text(encoding="ascii"))
    ui = bytearray(ui)
    if len(ui) != EXPECTED_UI_BYTES:
        raise SystemExit(
            f"{ui_path}: got {len(ui)} UI bytes, expected {EXPECTED_UI_BYTES}"
        )
    validate_offsets(
        str(ui_path),
        ui_offsets,
        {k: v for k, v in EXPECTED_UI_OFFSETS.items() if k != "piece_sprites_16x16"},
    )

    screen_offsets = parse_screen_asset_equ(screen_path.read_text(encoding="ascii"))
    validate_offsets(str(screen_path), screen_offsets, EXPECTED_UI_OFFSETS)

    if is_next:
        for name, values in (
            ("board_theme_mark_inks", NEXT_BOARD_MARK_INKS),
            ("board_theme_light_attrs", NEXT_BOARD_LIGHT_ATTRS),
            ("board_theme_dark_attrs", NEXT_BOARD_DARK_ATTRS),
        ):
            off = EXPECTED_UI_OFFSETS[name]
            ui[off : off + len(values)] = bytes(values)
    banner_version = version if "-dev" in version else f"VERSION {version}."
    patch_slot(
        ui,
        EXPECTED_UI_OFFSETS["version_banner_msg"],
        VERSION_SLOT,
        banner_version,
        "version_banner_msg",
    )

    pieces_text = pieces_path.read_text(encoding="ascii")
    pieces, _ = parse_defb(
        block_from_label(pieces_text, "netchesszx_piece_sprites_16x16")
    )
    piece_sets = runtime_piece_sets(pieces, pieces_path)
    loader_text = loader_path.read_text(encoding="ascii")
    loader_about_size = parse_loader_about_size(loader_text)
    if is_next and loader_about_size != 0:
        raise SystemExit("Next requires the NEX loader (about_board_size must be 0)")
    if is_next:
        runtime_pieces = piece_sets[:PIECE_SET_BYTES]
        extra_pieces = piece_sets[PIECE_SET_BYTES:]
    else:
        reflection_path = pieces_path.with_name("checker_reflections.bin")
        reflections = reflection_path.read_bytes()
        if len(reflections) != EXPECTED_REFLECTION_BYTES:
            raise SystemExit(
                f"{reflection_path}: got {len(reflections)} reflection bytes, "
                f"expected {EXPECTED_REFLECTION_BYTES}"
            )
        bundles = [
            piece_sets[index * PIECE_SET_BYTES : (index + 1) * PIECE_SET_BYTES]
            + reflections[
                index * REFLECTION_SET_BYTES : (index + 1) * REFLECTION_SET_BYTES
            ]
            for index in range(3)
        ]
        runtime_pieces = bundles[0]
        extra_pieces = b"".join(bundles[1:])
    runtime_total = len(ui) + len(runtime_pieces)
    loader_ui_size = parse_loader_constant(loader_text, "asset_ui_size")
    if loader_ui_size != len(ui):
        raise SystemExit(
            f"{loader_path}: asset_ui_size got {loader_ui_size}, "
            f"expected UI payload {len(ui)}"
        )
    piece_offset_match = re.search(
        r"(?m)^asset_piece_offset\s+EQU\s+([0-9]+)\s*$", loader_text
    )
    if piece_offset_match and int(piece_offset_match.group(1), 10) != len(ui):
        raise SystemExit(
            f"{loader_path}: asset_piece_offset got "
            f"{piece_offset_match.group(1)}, expected {len(ui)}"
        )
    loader_size = parse_loader_asset_size(loader_text, is_next)
    if loader_size != runtime_total:
        raise SystemExit(
            f"{loader_path}: asset_load_size got {loader_size}, "
            f"expected runtime load {runtime_total}"
        )
    if loader_about_size == EXPECTED_ABOUT_PAYLOAD_BYTES:
        about_raw = about_path.read_bytes()
        try:
            about = build_about_payload(about_raw)
        except ValueError as exc:
            raise SystemExit(f"{about_path}: {exc}") from exc
    elif loader_about_size == 0:
        about_raw = b""
        about = b""
    else:
        raise SystemExit(
            f"{loader_path}: about_board_size got {loader_about_size}, "
            f"expected {EXPECTED_ABOUT_PAYLOAD_BYTES} or 0"
        )

    total = runtime_total + len(about) + len(extra_pieces)
    expected_total = EXPECTED_NEXT_DAT_BYTES if is_next else EXPECTED_ZX_DAT_BYTES
    if total != expected_total:
        raise AssertionError(f"bad DAT size {total}, expected {expected_total}")

    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_path.write_bytes(ui + runtime_pieces + about + extra_pieces)
    print(
        f"[OK] {out_path}: {total} bytes "
        f"(About {len(about_raw)} -> {len(about)})"
    )


if __name__ == "__main__":
    main(sys.argv)
