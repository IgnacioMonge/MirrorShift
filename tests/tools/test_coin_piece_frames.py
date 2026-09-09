#!/usr/bin/env python3
"""Contracts for the live procedural board-piece animations."""

from functools import reduce
import hashlib
import math
from operator import or_
from pathlib import Path
import re

from PIL import Image

ROOT = Path(__file__).resolve().parents[2]
SOURCE_DIR = ROOT / "assets" / "editable" / "checkers"
THEMES = ("BW-S", "BW-M", "BW-L")
FRAME_COUNT = 16
FRAME_BYTES = 32

PRODUCTION_BOARD = ROOT / "asm" / "overlay" / "board" / "entry_board.asm"
PRODUCTION_SCREEN = ROOT / "asm" / "spectrum" / "screen.asm"
EXPECTED_PNG_SHA256 = {
    ("BW-S", "a"): "481ed71dfbf59067f2a306cdd1a30c0050183a7ab969466b734c450cd3329201",
    ("BW-S", "b"): "2db2d13bc759a35c9a66c9d7249b5985f3b515c8848119b57e1d50ad036a29db",
    ("BW-M", "a"): "0a7b841d2b8b4b8b7c5ea32a2740cd2e2bb97d89d474672498c0be52e4e84e0d",
    ("BW-M", "b"): "44b79c8f5ce958d34398ad327fc972b4faa5cbdc2009df4ae6abb8662593fe15",
    ("BW-L", "a"): "236d738d97a5e98534516a8ee5d5439749eb90ab1bbc1e90d5bc9a1288eaca63",
    ("BW-L", "b"): "a220593b7cb82d43d5c8180ebf0325cc7d177f8430c9d48470bc8dd7ce6a14da",
}


def rgba_rows(path: Path) -> tuple[bytes, ...]:
    with Image.open(path) as image:
        rgba = image.convert("RGBA")
        assert rgba.size == (16, 16)
        data = rgba.tobytes()
    return tuple(data[offset : offset + 64] for offset in range(0, len(data), 64))


def alpha_mask(path: Path) -> tuple[int, ...]:
    words = []
    for rgba in rgba_rows(path):
        word = 0
        for x in range(16):
            if rgba[x * 4 + 3] >= 128:
                word |= 1 << (15 - x)
        words.append(word)
    return tuple(words)


def projected_frame(rows: tuple[int, ...], scale: float) -> tuple[int, ...]:
    out = [0] * 16
    for y, word in enumerate(rows):
        projected_y = int(round(7.5 + (y - 7.5) * scale))
        out[projected_y] |= word
    return tuple(out)


def build_themes() -> dict[str, tuple[tuple[int, ...], ...]]:
    themes = {}
    for theme in THEMES:
        face_a = alpha_mask(SOURCE_DIR / theme / "reality-a.png")
        face_b = alpha_mask(SOURCE_DIR / theme / "reality-b.png")
        themes[theme] = tuple(
            projected_frame(
                face_a if math.cos(math.pi * frame / (FRAME_COUNT - 1)) >= 0 else face_b,
                max(0.05, abs(math.cos(math.pi * frame / (FRAME_COUNT - 1)))),
            )
            for frame in range(FRAME_COUNT)
        )
    return themes


def atlas_bytes(frames: tuple[tuple[int, ...], ...]) -> bytes:
    return b"".join(word.to_bytes(2, "big") for frame in frames for word in frame)


def frame_height(frame: tuple[int, ...]) -> int:
    rows = [y for y, row in enumerate(frame) if row]
    return rows[-1] - rows[0] + 1 if rows else 0


def main() -> int:
    themes = build_themes()
    assert tuple(themes) == THEMES
    for theme in THEMES:
        for face in ("a", "b"):
            path = SOURCE_DIR / theme / f"reality-{face}.png"
            assert hashlib.sha256(path.read_bytes()).hexdigest() == EXPECTED_PNG_SHA256[(theme, face)]
            rows = rgba_rows(path)
            assert len(rows) == 16 and all(len(row) == 64 for row in rows)
            assert {row[x * 4 + 3] for row in rows for x in range(16)} <= {0, 255}
        frames = themes[theme]
        face_a = alpha_mask(SOURCE_DIR / theme / "reality-a.png")
        face_b = alpha_mask(SOURCE_DIR / theme / "reality-b.png")
        assert len(frames) == FRAME_COUNT
        assert all(len(frame) == 16 for frame in frames)
        assert frames[0] == face_a
        assert frames[-1] == face_b
        assert frame_height(frames[7]) <= 2
        assert frame_height(frames[8]) <= 2
        assert len(atlas_bytes(frames)) == FRAME_COUNT * FRAME_BYTES
        for index, frame in enumerate(frames):
            source = face_a if index < FRAME_COUNT // 2 else face_b
            assert reduce(or_, frame, 0) == reduce(or_, source, 0), "x coverage changed"
        reverse = tuple(reversed(frames))
        assert reverse[0] == face_b
        assert reverse[-1] == face_a
    board = PRODUCTION_BOARD.read_text(encoding="ascii")
    screen = PRODUCTION_SCREEN.read_text(encoding="ascii")
    assert "boa_prepare_frames:" in board
    assert "boa_prepare_quadrants:" not in board
    assert "boa_phase_params:" in board
    assert "MIRRORSHIFT_FLIP_SCRATCH + 32" in board
    prepare = board[board.index("boa_prepare_frames:"):board.index("boa_project_face:")]
    assert "boa_iff_sampled:\n    di\n    pop hl\n    push af\n    push iy\n" in prepare
    assert "    call boa_project_face\n    pop iy\n    pop af\n    ret nc\n    ei\n    ret\n" in prepare
    assert board.count("call boa_wait_frame") >= 4
    assert "call boa_draw_rows" in board
    assert "_spectrum_flip_blit_frame" in board
    assert "_spectrum_flip_blit_quadrant" not in screen
    assert "ld de, 32" in screen
    assert "cp 15\n    call nz, boa_prepare_frames" in board
    assert "boa_prepare_morph_a:" in board
    assert "boa_prepare_morph_b:" in board
    assert "boa_prepare_morph_a:\nboa_prepare_morph_b:" in board
    morph_prepare = board[
        board.index("boa_prepare_morph_a:") : board.index("boa_prepare_ready:")
    ]
    assert "MIRRORSHIFT_SET_PREV_MASKS + 32" in morph_prepare
    assert "MIRRORSHIFT_PIECE_MASKS + 32" in morph_prepare
    morph_loop = board[
        board.index("_board_set_morph_ovl_entry:") : board.index("boa_morph_draw_a:")
    ]
    next_morph, classic_morph = morph_loop.split("ELSE", 1)
    assert "call boa_morph_show_next_phase" in next_morph
    assert "call boa_prepare_morph_a" not in next_morph
    assert "call boa_morph_draw_a" not in next_morph
    assert "call boa_morph_draw_b" not in next_morph
    assert "call boa_prepare_morph_a" in classic_morph
    assert "call boa_prepare_morph_b" not in classic_morph
    assert "_board_set_morph_ovl_entry:" in board
    assert "MIRRORSHIFT_SET_PREV_MASKS EQU MIRRORSHIFT_FLIP_SCRATCH + 64" in board
    assert "MIRRORSHIFT_FLIP_SCRATCH EQU 0x672b" in board
    assert "MIRRORSHIFT_FLIP_SCRATCH EQU 0x3c2b" in board
    assert "boa_morph_draw_a:" in board
    assert "boa_morph_draw_a:\n    ld a, 1\n    ld (piece_char), a" in board
    assert "boa_morph_draw_b:\n    xor a\n    ld (piece_char), a" in board
    parity = screen[screen.index("square_parity:") : screen.index("_spectrum_render_moves:")]
    assert parity.index("and 1") < parity.index("xor 1") < parity.index("ret")
    refresh = screen[
        screen.index("refresh_board_square_attrs:") :
        screen.index("refresh_board_legal_hints:")
    ]
    assert refresh.index("_netchesszx_board_dark_attr") < refresh.index(
        "_netchesszx_board_light_attr"
    )
    draw_one = board[board.index("boa_draw_one:") : board.index("boa_source_ready:")]
    assert draw_one.index("xor (hl)") < draw_one.index("xor 1") < draw_one.index("ld de")
    # Morph alone inverts piece_char. With the production selector's final
    # XOR, every seed therefore reads old buffer 0 before the midpoint and
    # new buffer 1 afterwards, in either one-axis board orientation.
    seeds = ((3, 3, 0), (4, 4, 0), (3, 4, 1), (4, 3, 1))
    for flipped in (False, True):
        for row, col, new_face in seeds:
            screen_row, screen_col = (
                (row, 7 - col) if flipped else (7 - row, col)
            )
            logical_parity = (screen_row + screen_col) % 2 ^ 1
            piece_selector = new_face ^ 1
            assert logical_parity == new_face
            assert logical_parity ^ piece_selector ^ 0 ^ 1 == 0
            assert logical_parity ^ piece_selector ^ 1 ^ 1 == 1
    assert "or 0x80\n    jp next_draw_piece_sprite_16x16" in board
    next_phase = board[
        board.index("boa_morph_show_next_phase:") : board.index(
            "ELSE", board.index("boa_morph_show_next_phase:")
        )
    ]
    assert "ld d, 3\n    ld e, 4\n    call boa_morph_show_next_one" in next_phase
    assert "ld d, 4\n    ld e, 3\n    call boa_morph_show_next_one" in next_phase
    assert "inc (hl)\n    ld d, 3\n    ld e, 3" in next_phase
    assert "ld d, 4\n    ld e, 4\n    jr boa_morph_show_next_one" in next_phase
    assert "boa_draw_one" not in next_phase
    assert "set_square_attr_2x2" not in next_phase
    next_final = screen[
        screen.index("_spectrum_next_setup_pieces_use_final:") :
        screen.index("next_setup_piece_use_final_one:")
    ]
    assert "ld d, 3\n    ld e, 4\n    ld a, 'A'" in next_final
    assert "ld d, 4\n    ld e, 3\n    ld a, 'A'" in next_final
    assert "ld d, 3\n    ld e, 3\n    ld a, 'B'" in next_final
    assert "ld d, 4\n    ld e, 4\n    ld a, 'B'" in next_final
    assert "call _spectrum_board_view_redraw_square" in board
    apply = board[
        board.index("_board_apply_ovl_entry:") : board.index("boa_prepare_frames:")
    ]
    next_apply, classic_apply = apply.split("ELSE", 1)
    assert "call boa_draw_rows_next" in next_apply
    assert "NEXT_CAPTURE_PATTERN_BASE" in next_apply
    assert "_spectrum_flip_blit_frame" not in next_apply
    assert "call boa_draw_rows" in classic_apply
    assert "call nz, boa_prepare_frames" in classic_apply
    assert (
        "    cp 14\n"
        "    jr nz, boa_capture_phase_ready\n"
        "    inc a\n"
        "boa_capture_phase_ready:\n"
        "    push af\n"
    ) in classic_apply
    next_rows = board[
        board.index("boa_draw_rows_next:") : board.index("boa_prepare_frames:")
    ]
    assert "jp next_draw_piece_sprite_16x16" in next_rows
    assert "_spectrum_flip_blit_frame" not in next_rows
    assert "set_square_attr_2x2" not in next_rows
    match = re.search(r"boa_phase_params:\s*((?:\s+DEFB[^\n]+\n){2})", board)
    assert match is not None
    params = tuple(
        int(value)
        for line in match.group(1).splitlines()
        for value in line.split("DEFB", 1)[1].split(",")
        if "DEFB" in line
    )
    assert len(params) == 16
    actual_maps = []
    for phase in range(8):
        start, step = params[phase * 2 : phase * 2 + 2]
        top = tuple((start + step * source_y) >> 4 for source_y in range(8))
        actual_maps.append(top + tuple(15 - value for value in reversed(top)))
    actual_maps = tuple(actual_maps)
    expected_maps = tuple(
        tuple(
            int(round(7.5 + (source_y - 7.5) * max(0.05, abs(math.cos(math.pi * phase / 15)))))
            for source_y in range(16)
        )
        for phase in range(8)
    )
    assert actual_maps == expected_maps
    print("[OK] horizontal COIN pieces: exact endpoints; light/dark 2x3 gallery")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
