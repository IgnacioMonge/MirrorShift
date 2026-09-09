"""Execute the authority ROM bridge with the consumer-owned held byte."""

import argparse
import os
import re
import shutil
import subprocess
from pathlib import Path


HARNESS = r"""
SECTION code_user
EXTERN _spxn_rom_hlcall, _spxn_rom_ixcall, _spxn_regs, _spxn_rom_error
EXTERN _esx_fclose
PUBLIC _spxn_rom_held
start:
    ld sp, 0xFF00
    ld a, 0xFE
    ld (0x7000), a
    ld a, 0xC3
    ld (0x3FFA), a
    ld (0x3FFD), a
    ld (0x3ED2), a
    ld (0x3EDB), a
    ld hl, trampoline_hl
    ld (0x3FFB), hl
    ld hl, trampoline_ix
    ld (0x3FFE), hl
    ld hl, close_file
    ld (0x3ED3), hl
    ld hl, close_dir
    ld (0x3EDC), hl
    ld ix, 0xABCD
    ld iy, 0x5C3A
    xor a
    ld (_spxn_rom_held), a
    ld (trampolines), a
    ei
    call exercise
    ld a, i
    jp po, fail
    ld a, (trampolines)
    cp 2
    jp nz, fail
    ; Resident subset shares handle ownership with complete overlay adapter.
    ; A failed directory close must retain its kind and never call VCLOSE.
    ld a, 2
    ld (0x5B6C), a
    ld a, 0x17
    ld (0x5B6B), a
    ld a, 1
    ld (close_failure), a
    call _esx_fclose
    ld a, l
    cp 0xFF
    jp nz, fail
    ld a, (0x5B6C)
    cp 2
    jp nz, fail
    ld a, (closed_kind)
    cp 2
    jp nz, fail
    xor a
    ld (close_failure), a
    call _esx_fclose
    ld a, l
    or a
    jp nz, fail
    ld a, (0x5B6C)
    or a
    jp nz, fail
    ld a, 1
    ld (0x5B6C), a
    ld a, 0x17
    ld (0x5B6B), a
    call _esx_fclose
    ld a, l
    or a
    jp nz, fail
    ld a, (closed_kind)
    cp 1
    jp nz, fail
    xor a
    ld (trampolines), a
    ld a, 1
    ld (_spxn_rom_held), a
    di
    call exercise
    ld a, i
    jp pe, fail
    ld a, (trampolines)
    or a
    jp nz, fail
    ld hl, 0
    add hl, sp
    ld de, 0xFF00
    or a
    sbc hl, de
    jp nz, fail
    push ix
    pop hl
    ld de, 0xABCD
    or a
    sbc hl, de
    jp nz, fail
    push iy
    pop hl
    ld de, 0x5C3A
    or a
    sbc hl, de
    jp nz, fail
    xor a
    ld (0x7000), a
    jp 0
fail:
    ld a, 0xFF
    ld (0x7000), a
    jp 0
exercise:
    call arguments
    ld hl, rom_hl
    call _spxn_rom_hlcall
    ld a, l
    cp 2 ; Z, no carry
    jp nz, fail
    ld hl, (_spxn_regs + 1)
    ld de, 3
    or a
    sbc hl, de
    jp nz, fail
    call arguments
    ld hl, rom_ix
    call _spxn_rom_ixcall
    ld a, l
    cp 1 ; carry, nonzero
    jp nz, fail
    call _spxn_rom_error
    ld a, l
    cp 0xFA
    jp nz, fail
    ret
arguments:
    ld a, 0x17
    ld (_spxn_regs), a
    ld hl, 8
    ld (_spxn_regs + 1), hl
    ld hl, 0x6000
    ld (_spxn_regs + 3), hl
    ld hl, 0x6100
    ld (_spxn_regs + 5), hl
    ret
trampoline_hl:
    push af
    ld a, (trampolines)
    inc a
    ld (trampolines), a
    pop af
    jp (hl)
trampoline_ix:
    push af
    ld a, (trampolines)
    inc a
    ld (trampolines), a
    pop af
    jp (ix)
rom_ix:
    push af
    push hl
    ld hl, 0x6100
    ex (sp), hl
    or a
    pop de
    sbc hl, de
    jp nz, fail
    pop af
    ld de, 0x6000
    call check_args
    ld a, 0xFA
    or a
    scf
    ret
rom_hl:
    call check_args
    ld bc, 3
    xor a
    ret
check_args:
    cp 0x17
    jp nz, fail
    ld a, b
    or a
    jp nz, fail
    ld a, c
    cp 8
    jp nz, fail
    ld a, d
    cp 0x60
    jp nz, fail
    ld a, e
    or a
    jp nz, fail
    ld a, i
    jp pe, fail
    ld ix, 0xDEAD
    ld iy, 0xBEEF
    ret
_spxn_rom_held: defb 0
trampolines: defb 0
close_failure: defb 0
closed_kind: defb 0
close_file:
    cp 0x17
    jp nz, fail
    ld a, 1
    jr close_result
close_dir:
    cp 0x17
    jp nz, fail
    ld a, 2
close_result:
    ld (closed_kind), a
    ld a, (close_failure)
    or a
    ret z
    scf
    ret
"""


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", default=str(Path(__file__).resolve().parents[2]))
    parser.add_argument("--build-dir", default="build")
    parser.add_argument("--spxn-dir", default=os.environ.get("SPXN_DIR"))
    args = parser.parse_args()
    root = Path(args.root).resolve()
    driver = Path(args.spxn_dir).resolve() if args.spxn_dir else root.parent / "SpectraNext/driver"
    assert (driver / "spxn_rom.asm").is_file(), "supply --spxn-dir for the authority driver"
    assembler = shutil.which("z80asm") or shutil.which("z88dk-z80asm")
    assert assembler, "z88dk assembler required"
    z88dk = Path(os.environ.get("Z88DK", str(Path(assembler).parents[1])))
    work = Path(args.build_dir).resolve() / "spectranext-rom-held"
    work.mkdir(parents=True, exist_ok=True)
    (work / "vector.asm").write_text(HARNESS)
    (work / "config_private.inc").write_text(
        "defc __CPU_RABBIT__=0\ndefc __CPU_8085__=0\ndefc __Z80=1\ndefc __Z80_NMOS=1\n"
    )
    helper = z88dk / "libsrc/arch/z80/z80"
    result = subprocess.run(
        [assembler, "-b", "-m", "-r0x8000", "-DSPXN_ROM_HELD", "-DSPXN_ROM_HELD_EXTERNAL",
         "-DSPXN_XFS_STATE_BASE=0x5B60", "-DSPXN_XFS_DIR_SCRATCH=0x662B",
         "-I.", "-O=.", "-o=vector.bin", "vector.asm", str(driver / "spxn_rom.asm"),
         str(root / "asm/esxdos/xfs_loader_spectranext.asm"),
         str(helper / "asm_z80_push_di.asm"), str(helper / "asm_z80_pop_ei.asm")],
        cwd=work, text=True, capture_output=True,
    )
    assert result.returncode == 0, result.stdout + result.stderr
    result = subprocess.run(
        [shutil.which("z88dk-ticks") or "z88dk-ticks", "-mz80", "-l", "0x8000",
         "-pc", "8000", "-end", "0", "-counter", "100000", "-output", "vector.ram", "vector.bin"],
        cwd=work, text=True, capture_output=True,
    )
    assert result.returncode == 0, result.stdout + result.stderr
    memory = (work / "vector.ram").read_bytes()
    assert len(memory) >= 65536 and memory[0x7000] == 0, "ROM held bridge vector failed"
    loader = (root / "asm/esxdos/overlay_loader.asm").read_text()
    assert re.search(r"_spxn_rom_held:\s+DEFS 1", loader)
    print("[OK] authority ROM bridge: held/external byte, registers/flags, IX/IY/IFF/SP; resident XFS file/directory close and failed-close ownership")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
