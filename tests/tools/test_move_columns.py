"""Exercise the production log initializer and two-column renderer on Z80."""
import shutil
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
PROBE = r"""
SECTION code_user
    ld sp, 0x7ff0
    ld hl, 0x7100
    ld b, 64
    ld a, ' '
clear_screen:
    ld (hl), a
    inc hl
    djnz clear_screen
    ld hl, 0x7000
    call _clear_move_line
    ld hl, restored
    ld de, RESTORE_SLOT
    ld bc, 9
    ldir
    ld hl, 0x7000
    ld (move_ptr), hl
    call render_move_current_line
    jp 0
restored: DEFB "RESTORED", 0
move_ptr: DEFW 0
tmp_scan: DEFB 0
current_attr: DEFB 0
ATTR_TEXT EQU 7
ATTR_ERROR EQU 2
NETCHESSZX_INFO_TEXT_COL EQU 0
NETCHESSZX_MOVE_WHITE_OFFSET EQU 18
NETCHESSZX_MOVE_WHITE_COL EQU 14
draw_ikkle_text_abs_y:
    ld b, 0
    ld de, 0x7100
    ex de, hl
    add hl, bc
    ex de, hl
draw_loop:
    ld a, e
    cp 64
    ret nc
    ld a, (hl)
    or a
    ret z
    ld (de), a
    inc hl
    inc de
    jr draw_loop
"""


def main():
    log = (ROOT / "asm/overlay/gui_log/entry_gui_log.asm").read_text()
    screen = (ROOT / "asm/spectrum/screen.asm").read_text()
    initializer = log[log.index("_clear_move_line:"):log.index("_scroll_move_lines:")]
    renderer = screen[screen.index("render_move_current_line:"):
                      screen.index("_spectrum_render_moves_scroll:")]
    for offset, column in ((0, 0), (18, 14)):
        with tempfile.TemporaryDirectory(prefix="mirrorshift-move-columns-") as tmp:
            work = Path(tmp)
            probe = PROBE.replace("RESTORE_SLOT", str(0x7000 + offset))
            (work / "probe.asm").write_text(probe + initializer + renderer)
            subprocess.run([shutil.which("z80asm") or "z80asm", "-b", "-r0x8000",
                            "-o=probe.bin", "probe.asm"], cwd=work, check=True)
            subprocess.run([shutil.which("z88dk-ticks") or "z88dk-ticks", "-mz80",
                            "-l", "0x8000", "-pc", "8000", "-end", "0",
                            "-counter", "100000", "-output", "probe.ram", "probe.bin"],
                           cwd=work, check=True, stdout=subprocess.DEVNULL)
            ram = (work / "probe.ram").read_bytes()
            row = ram[0x7100:0x7140].rstrip(b"\0 ")
            assert row == b" " * column + b"RESTORED", repr(row)
            assert ram[0x700d] == 0 and ram[0x701f] == 0
    print("[OK] production Z80 move columns: RESTORED rendered once, both slots bounded")


if __name__ == "__main__":
    main()
