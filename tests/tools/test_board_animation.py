"""Execute production BOARD pulse, capture and seed sequencing in z88dk-ticks."""

import argparse
import math
from pathlib import Path
import shutil
import subprocess


def check_dispatch_guard(root: Path) -> None:
    screen = (root / "asm/spectrum/screen.asm").read_text(encoding="utf-8")
    wrapper = screen.split("_spectrum_animate_last_flip:\n", 1)[1]
    assert wrapper.startswith(
        "IFNDEF NETCHESSZX_NEXT\nIFNDEF NETCHESSZX_SPECTRANEXT\n"
        "    ld a, (MIRRORSHIFT_LAST_SQUARE)\n    or a\n    ret m\n"
        "ENDIF\nENDIF\n    ld a, SPECTRUM_OVL_BOARD\n"
        "    ld e, SPECTRUM_OVL_BOARD_APPLY\n    jp call_overlay_ae\n"
    ), "Classic invalid placement must return before overlay I/O"


def classic_piece_sets(root: Path) -> tuple[bytes, ...]:
    source = (root / "assets/spectrum/checker_pieces_16x16.asm").read_text(
        encoding="ascii"
    ).split("netchesszx_piece_sprites_16x16:", 1)[1]
    data = bytearray()
    for line in source.splitlines():
        line = line.split(";", 1)[0].strip()
        if line.upper().startswith("DEFB"):
            data.extend(int(token.strip(), 0) for token in line[4:].split(","))
    assert len(data) == 3 * 64
    return tuple(bytes(data[offset : offset + 64]) for offset in range(0, 192, 64))


def projected_mask(source: bytes, phase: int) -> bytes:
    assert len(source) == 32 and 1 <= phase <= 14
    scale = max(0.05, abs(math.cos(math.pi * phase / 15)))
    rows = [int.from_bytes(source[y * 2 : y * 2 + 2], "big") for y in range(16)]
    projected = [0] * 16
    for source_y, word in enumerate(rows):
        target_y = int(round(7.5 + (source_y - 7.5) * scale))
        projected[target_y] |= word
    return b"".join(word.to_bytes(2, "big") for word in projected)


def expected_morph_log(root: Path) -> bytes:
    sets = classic_piece_sets(root)
    expected = bytearray()
    seeds = ((3, 3, 1), (4, 4, 1), (3, 4, 0), (4, 3, 0))
    for selected_set in range(3):
        old_face = sets[(selected_set - 1) % 3][32:]
        new_face = sets[selected_set][32:]
        for flipped in (0, 1):
            expected += b"M" + bytes((selected_set, flipped))
            for phase in range(1, 15):
                old_frame = projected_mask(old_face, phase)
                new_frame = projected_mask(new_face, phase)
                half = int(phase >= 8)
                for row, col, piece in seeds:
                    screen_row, screen_col = (
                        (row, 7 - col) if flipped else (7 - row, col)
                    )
                    parity = ((screen_row + screen_col) & 1) ^ 1
                    source = new_frame if parity ^ piece ^ half ^ 1 else old_frame
                    expected += b"F" + bytes((
                        phase, screen_row, screen_col, piece
                    )) + source
                expected += b"DUTU"
            assert expected[-(4 + 37 * 4):-4].startswith(b"F" + bytes((14,)))
    return bytes(expected)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", default=".")
    parser.add_argument("--build-dir", default="build")
    args = parser.parse_args()
    root = Path(args.root).resolve()
    check_dispatch_guard(root)
    work = Path(args.build_dir).resolve() / "board-animation"
    work.mkdir(parents=True, exist_ok=True)
    binary = work / "board_animation.bin"
    ram = work / "board_animation.ram"
    for path in (binary, ram):
        path.unlink(missing_ok=True)
    subprocess.run(
        [shutil.which("z80asm") or "z80asm", "-b", "-r0x8000", "-O=.",
         "-o=" + binary.name,
         str(root / "tests/spectrum/test_board_animation_vector.asm"),
         str(root / "asm/overlay/board/entry_board.asm"),
         str(root / "assets/spectrum/checker_pieces_16x16.asm")],
        cwd=work, check=True,
    )
    subprocess.run(
        [shutil.which("z88dk-ticks") or "z88dk-ticks", "-mz80", "-l", "0x8000",
         "-pc", "8000", "-end", "0", "-counter", "10000000", "-output",
         ram.name, binary.name], cwd=work, check=True,
        stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
    )
    image = ram.read_bytes()
    assert len(image) >= 65536 and image[0x7000] == 0, "IX/IY/SP corruption"
    end = int.from_bytes(image[0x7002:0x7004], "little")
    actual = image[0xa000:end]
    expected = bytearray()
    for flipped, visible, frame in ((False, 1, b"EUTU"), (True, 0, b"DUTU")):
        redraw = b"R" + bytes((2, 3, visible))
        pulse = b"P" + bytes((2 if flipped else 5, 4 if flipped else 3, 255, ord("B")))
        expected += b"A" + redraw + (pulse + frame * 6 + redraw + frame * 6) * 2
        expected += frame * 30  # Two reveal frames, fourteen two-frame phases.
    expected += b"AAA"  # ABOUT, file browser, invalid last square: no callbacks.
    for _ in range(2):
        expected += b"SCHV"
        for row, col in ((3, 3), (3, 4), (4, 3), (4, 4)):
            expected += b"R" + bytes((row, col, 1)) + b"DUTU" * 5
    expected += expected_morph_log(root)
    assert actual == expected, next(
        (f"event {i}: got {a:#04x}, expected {b:#04x}"
         for i, (a, b) in enumerate(zip(actual, expected)) if a != b),
        f"event length {len(actual)} != {len(expected)}",
    )
    print("[OK] BOARD: pulse/seed/morph exact bitplanes for sets 0..2 and both orientations; cadence, IFF, IX/IY/SP")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
