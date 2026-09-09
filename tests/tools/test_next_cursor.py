"""Execute the production Next marker/piece path and inspect sprite port writes."""
from pathlib import Path
import re
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[2]

def check_cursor(root=ROOT):
    screen = (root / "asm/spectrum/screen.asm").read_text()
    def block(start, end):
        return start + screen.split(start, 1)[1].split(end, 1)[0]
    code = block("next_marker_set_mark_current_square:", "_spectrum_next_sprites_hide_all:")
    code += block("next_draw_piece_sprite_16x16:", "; fastcall spec: display row")
    code += block("next_square_sprite_slot:", "; Repoint the four setup discs")
    code = code.replace("out (c), a", "call log_out")
    constants = "\n".join(re.findall(r"(?m)^(?:NEXT\w+|NETCHESSZX_GAME_BOARD_\w+) EQU .+$", screen))
    cases = [(row, col, flip, piece, flags) for flip in (0, 1)
             for row in range(8) for col in range(8)
             for piece in (65, 66) for flags in (1, 2, 6)]
    cases += [(3, 2, flip, 46, flags) for flip in (0, 1) for flags in (1, 2, 3, 6)]
    vectors = []
    expected = bytearray()
    def writes(row, col, pattern):
        return bytes((0x3b, 64+row*8+col, 0x57, 40+col*16,
                      0x57, 72+row*16, 0x57, 0, 0x57, 128+pattern))
    def hide(row, col):
        return bytes((0x3b, 64+row*8+col, 0x57, 0, 0x57, 0, 0x57, 0, 0x57, 0))
    for row, col, flip, piece, flags in cases:
        logical = ((row if flip else 7-row)*8 + (7-col if flip else col))
        vectors.append(f"defb {row},{col},{flip},{logical},{piece},{flags}")
        if piece != 46:
            if flags & 2:
                expected += writes(row, col, 60+piece-65+(2 if flags & 4 else 0))
            expected += writes(row, col, 56+piece-65)
        else:
            expected += writes(row, col, {1:52, 2:53, 3:54, 6:55}[flags])
            expected += hide(row, col)*2
    source = """
org 0x8000
ld sp, 0x7ff0
ld iy, 0x1357
ld hl, 0xa000
ld (log_ptr), hl
ld hl, marker_slot_squares
ld de, marker_slot_squares+1
ld bc, 31
ld (hl), 255
ldir
ld hl, cases
case_loop:
ld a, (hl)
cp 255
jr z, done
ld (board_row), a
inc hl
ld a, (hl)
ld (board_col), a
inc hl
ld a, (hl)
ld (_spectrum_board_view_flipped), a
inc hl
push hl
ld hl, MIRRORSHIFT_BOARD_STATE
ld de, MIRRORSHIFT_BOARD_STATE+1
ld bc, 63
ld (hl), '.'
ldir
pop hl
ld a, (hl)
inc hl
add a, 0x60
ld e, a
ld d, 0x5f
ld a, (hl)
ld (de), a
ld (piece_char), a
inc hl
ld a, (hl)
inc hl
push hl
call next_marker_or_current_square
call next_marker_release_current_square
ld a, (piece_char)
call next_draw_piece_sprite_16x16
pop hl
jr case_loop
done:
ld hl, (log_ptr)
ld (0x7002), hl
ld (0x7004), sp
ld (0x7006), iy
jp 0
log_out:
push af
push hl
ld hl, (log_ptr)
ld (hl), c
inc hl
ld (hl), a
inc hl
ld (log_ptr), hl
pop hl
pop af
ret
MIRRORSHIFT_BOARD_STATE EQU 0x5f60
""" + constants + "\n" + code + """
log_ptr: defw 0
board_row: defb 0
board_col: defb 0
piece_char: defb 0
mark_mode: defb 0
tmp_char: defb 0
tmp_attr: defb 0
_spectrum_board_view_flipped: defb 0
marker_slot_squares: defs 32
marker_slot_flags: defs 32
cases:
""" + "\n".join(vectors) + "\ndefb 255\n"
    work = root / "build/next-cursor-test"
    work.mkdir(parents=True, exist_ok=True)
    (work/"cursor.asm").write_text(source)
    subprocess.run([shutil.which("z80asm") or "z80asm", "-b", "-r0x8000",
                    "-O=.", "-o=cursor.bin", "cursor.asm"], cwd=work, check=True)
    subprocess.run([shutil.which("z88dk-ticks") or "z88dk-ticks", "-mz80",
                    "-l", "0x8000", "-pc", "8000", "-end", "0",
                    "-counter", "10000000", "-output", "cursor.ram", "cursor.bin"],
                   cwd=work, check=True, stdout=subprocess.DEVNULL)
    ram = (work/"cursor.ram").read_bytes()
    end = int.from_bytes(ram[0x7002:0x7004], "little")
    assert ram[0x7004:0x7008] == bytes((0xf0,0x7f,0x57,0x13)), "SP/IY corrupted"
    assert ram[0xa000:end] == expected, "Cursor missing, wrong piece, or stale marker on release"
    print(f"Next cursor Z80: {len(cases)} occupied/empty/selected/flipped cases PASS")

if __name__ == "__main__":
    check_cursor()
