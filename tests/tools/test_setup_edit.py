#!/usr/bin/env python3
"""Runtime check for the SETUP overlay's inline buffer editor."""

import argparse
import shutil
import subprocess
from pathlib import Path


PROBE = r"""
SECTION code_user

PUBLIC _edit_buf
PUBLIC _edit_max
PUBLIC _edit_len
PUBLIC _edit_pos

EXTERN _spectrum_gui_edit_bind
EXTERN _spectrum_gui_edit_key

TEST_STATUS EQU 0x7000
TEST_SP     EQU 0x7ff0

test_start:
    ld sp, TEST_SP
    xor a
    ld (TEST_STATUS), a
    ld hl, initial
    ld de, buffer
    ld bc, 7
    ldir
    ld hl, buffer
    ld (_edit_buf), hl
    ld a, 5
    ld (_edit_max), a
    ld a, 0xff
    ld (_edit_pos), a
    call _spectrum_gui_edit_bind
    ld a, (_edit_len)
    cp 5
    jp nz, test_done
    ld a, (_edit_pos)
    cp 4
    jp nz, test_done
    ld a, 1
    ld (TEST_STATUS), a

    ld l, 0x88
    call _spectrum_gui_edit_key
    ld a, l
    cp 1
    jp nz, test_done
    ld a, (_edit_pos)
    or a
    jp nz, test_done
    ld a, 2
    ld (TEST_STATUS), a

    ld l, 0x84
    call _spectrum_gui_edit_key
    ld l, 'X'
    call _spectrum_gui_edit_key
    ld hl, buffer
    ld a, (hl)
    cp '1'
    jp nz, test_done
    inc hl
    ld a, (hl)
    cp 'X'
    jp nz, test_done
    ld a, (_edit_len)
    cp 5
    jp nz, test_done
    ld a, 3
    ld (TEST_STATUS), a

    ld l, 0x89
    call _spectrum_gui_edit_key
    ld a, (_edit_pos)
    cp 4
    jp nz, test_done
    ld l, 8
    call _spectrum_gui_edit_key
    ld a, (_edit_len)
    cp 4
    jp nz, test_done
    ld a, (_edit_pos)
    cp 3
    jp nz, test_done
    ld a, (buffer + 4)
    or a
    jp nz, test_done
    ld a, 4
    ld (TEST_STATUS), a

    ld l, 0x88
    call _spectrum_gui_edit_key
    ld l, 0x84
    call _spectrum_gui_edit_key
    ld l, 'Y'
    call _spectrum_gui_edit_key
    ld hl, expected_insert
    ld de, buffer
    ld b, 6
test_compare:
    ld a, (de)
    cp (hl)
    jp nz, test_done
    inc de
    inc hl
    djnz test_compare
    ld a, (buffer + 6)
    cp 0xa5
    jp nz, test_done
    ld a, 5
    ld (TEST_STATUS), a

    ld l, 0x89
    call _spectrum_gui_edit_key
    ld l, 'Z'
    call _spectrum_gui_edit_key
    ld a, (buffer + 4)
    cp 'Z'
    jp nz, test_done
    ld a, (_edit_len)
    cp 5
    jp nz, test_done
    ld a, 6
    ld (TEST_STATUS), a

    xor a
    ld (buffer), a
    ld a, 0xff
    ld (_edit_pos), a
    ld hl, buffer
    call _spectrum_gui_edit_bind
    ld l, '7'
    call _spectrum_gui_edit_key
    ld a, (buffer)
    cp '7'
    jp nz, test_done
    ld a, (buffer + 1)
    or a
    jp nz, test_done
    ld a, (_edit_len)
    cp 1
    jp nz, test_done
    ld hl, 0
    add hl, sp
    ld de, TEST_SP
    or a
    sbc hl, de
    jp nz, test_done
    ld a, 7
    ld (TEST_STATUS), a

test_done:
    jp 0

initial:
    DEFM "12345"
    DEFB 0, 0xa5
expected_insert:
    DEFM "1YX34"
    DEFB 0
buffer:
    DEFS 7
_edit_buf:
    DEFS 2
_edit_max:
    DEFS 1
_edit_len:
    DEFS 1
_edit_pos:
    DEFS 1
"""


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", default=".")
    parser.add_argument("--build-dir", default="build")
    args = parser.parse_args()
    root = Path(args.root).resolve()
    work = Path(args.build_dir).resolve() / "setup_edit_probe"
    work.mkdir(parents=True, exist_ok=True)
    source = work / "setup_edit_probe.asm"
    binary = work / "setup_edit_probe.bin"
    ram = work / "setup_edit_probe.ram"
    source.write_text(PROBE, encoding="ascii")
    for old in (binary, ram):
        old.unlink(missing_ok=True)

    z80asm = shutil.which("z80asm") or "z80asm"
    ticks = shutil.which("z88dk-ticks") or "z88dk-ticks"
    kernel = root / "asm" / "overlay" / "edit" / "edit_buf.asm"
    result = subprocess.run(
        [z80asm, "-b", "-r0x8000", "-O=.", "-o=" + binary.name,
         source.name, str(kernel)],
        cwd=work, text=True, stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT, check=False,
    )
    if result.returncode:
        print(result.stdout, end="")
        print("[ERR] SETUP editor probe assembly failed")
        return 1
    result = subprocess.run(
        [ticks, "-mz80", "-l", "0x8000", "-pc", "8000", "-end", "0",
         "-counter", "100000", "-output", ram.name, binary.name],
        cwd=work, text=True, stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT, check=False,
    )
    if result.returncode or not ram.exists():
        print(result.stdout, end="")
        print("[ERR] SETUP editor probe execution failed")
        return 1
    image = ram.read_bytes()
    step = image[0x7000] if len(image) >= 65536 else 0
    if step != 7:
        print(f"[ERR] SETUP editor runtime failed after checkpoint {step}")
        return 1
    print("[OK] SETUP editor: HOME/END, replace, delete, insert, empty, SP")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
