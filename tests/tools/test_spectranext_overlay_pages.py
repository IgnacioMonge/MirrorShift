"""Run the production Page B loader against bounded ROM-vector models."""

import argparse
import os
import re
import shutil
import subprocess
from pathlib import Path


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", default=str(Path(__file__).resolve().parents[2]))
    parser.add_argument("--build-dir", default="build")
    args = parser.parse_args()
    root = Path(args.root).resolve()
    work = Path(args.build_dir).resolve() / "spectranext-overlay-vector"
    work.mkdir(parents=True, exist_ok=True)
    assembler = shutil.which("z80asm") or shutil.which("z88dk-z80asm")
    assert assembler, "z88dk assembler required"
    z88dk = Path(os.environ.get("Z88DK", str(Path(assembler).parents[1])))
    loader = (root / "asm/esxdos/overlay_loader.asm").read_text()
    # Place all production instructions together for ticks' flat memory loader.
    # Only section/ORG/link declarations differ; ROM vectors remain at real addresses.
    flat = re.sub(r"(?m)^(?:SECTION|ORG|EXTERN|PUBLIC)\b[^\n]*", "", loader)
    # The standalone vector relocates the real 20-byte loader BSS and places
    # a canary exactly where CRT BSS_UNINITIALIZED starts in the product link.
    flat = flat.replace("_spxn_rom_held: DEFS 1", "_spxn_rom_held: DEFS 1\ntest_bss_tail: DEFS 2")
    (work / "loader.asm").write_text(flat)
    offsets = [100, 4196] + [4196 + 8 * i for i in range(1, 19)]
    (work / "overlay_atlas_table.asm").write_text(
        "ovl_atlas_count EQU 19\n"
        + "\n".join(f"ovl_atlas_fingerprint_{i} EQU {v}" for i, v in enumerate((11, 22, 33, 44)))
        + "\novl_atlas_table:\n" + "\n".join(f"DW {v}" for v in offsets) + "\n"
    )
    (work / "config_private.inc").write_text(
        "defc __CPU_RABBIT__=0\ndefc __CPU_8085__=0\ndefc __Z80=1\ndefc __Z80_NMOS=1\n"
    )
    helper_dir = z88dk / "libsrc/arch/z80/z80"
    if not helper_dir.is_dir():
        helper_dir = z88dk / "libsrc/_DEVELOPMENT/z80/z80"
    (work / "vector.asm").write_text(
        (root / "tests/spectrum/test_spectranext_overlay_vector.asm").read_text()
        + '\ndefc __bss_user_head = ovl_handle\ndefc __bss_user_size = test_bss_tail - ovl_handle\n'
        + '\nEXTERN asm_z80_push_di, asm_z80_pop_ei\n'
        + '\nINCLUDE "loader.asm"\n'
    )
    binary = work / "vector.bin"
    ram = work / "vector.ram"
    result = subprocess.run(
        [assembler, "-b", "-m", "-r0x8000", "-DNETCHESSZX_SPECTRANEXT", "-I.", "-O=.",
         "-o=" + binary.name, "vector.asm",
         str(helper_dir / "asm_z80_push_di.asm"), str(helper_dir / "asm_z80_pop_ei.asm")],
        cwd=work, text=True, capture_output=True,
    )
    assert result.returncode == 0, result.stdout + result.stderr
    result = subprocess.run(
        [shutil.which("z88dk-ticks") or "z88dk-ticks", "-mz80", "-l", "0x8000",
         "-pc", "8000", "-end", "0", "-int", "10000", "-counter", "3000000",
         "-output", ram.name, binary.name], cwd=work, text=True, capture_output=True,
    )
    assert result.returncode == 0, result.stdout + result.stderr
    memory = ram.read_bytes()
    assert len(memory) >= 65536 and memory[0x7000] == 0, "Page B execution vector failed"
    assert "_spectrum_overlay_context EQU 0x5FF8" in loader
    assert "piece_reflection_set_size EQU 32" in loader
    assert "asset_load_size_classic EQU 908" in loader
    assert "spxn_overlay_chunk EQU 1502" in loader
    assert "_spxn_rom_held: DEFS 1" in loader
    print("[OK] Page B preload, fingerprint, allocation rollback, entry bounds, nested dispatch, frame ROM/IY/IFF restoration")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
